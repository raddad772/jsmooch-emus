//
// Created by RadDad772 on 3/8/24.
//

#include <cstdlib>
#include <cstdio>
#include "tmu.h"
#include "sh4_interpreter.h"
#include "sh4_interrupts.h"

namespace SH4 {

#define SH4_CYCLES_PER_SEC 200000000
#define tmu_underflow 0x0100
#define tmu_UNIE      0x0020
#define TCUSE *cpu->trace.cycles

void scheduled_tmu_callback(void *ptr, u64 key, u64 sch_cycle, u32 jitter);

/* A lot of the structure and logic here very closely follows Reicast */

#define TOTAL_EXPECTED 14
static u32 expected_TCNT[] = {
        0xffffffcf,
        0xfffea023,
        0xfffea000,
        0xfffea000,
        0xfffe9fee,
        0xfffe9fee,
        0xfffe9fcd,
        0xfffe9fcd,
        0xfffe9fbb,
        0xfffe9fbb,
        0xfffe9faf,
        0xfffe9faf,
        0xfffe9faa,
        0xfffe9faa,
        0xfffe9fa5,
};

//#define REGTCNT (((u32)(base[ch] - ((TCUSE >> shift[ch])& mask[ch])))) - 0x80A
#define REGTCNT (((u32)(base[ch] - ((TCUSE >> shift[ch])& mask[ch]))))

u32 TMU::read_TCNT(u32 ch, bool is_regread)
{
#ifdef REICAST_DIFF
    static int num = 0;
    static s64 last_diff = 0;
    if ((ch == 0) && (clock.trace_cycles > 0) && is_regread) {
        num++;
        if (num <= TOTAL_EXPECTED) {
            u32 r = expected_TCNT[num - 1];
            u32 diff = r - REGTCNT;
            printf("\nDIFF: %08x %08lld ch:%d", diff, clock.timer_cycles - last_diff, ch);
            last_diff = clock.timer_cycles;
            return expected_TCNT[num - 1];
        }
    }
#endif
    return REGTCNT;
}

i64 TMU::read_TCNT64(u32 ch) const {
    return static_cast<i64>(base64[ch]) - static_cast<i64>((TCUSE >> shift[ch]) & mask64[ch]);
}


void TMU::sched_chan_tick(u32 ch)
{
    u32 togo = read_TCNT(ch, false) / 8;
    if (togo > SH4_CYCLES_PER_SEC)
        togo = SH4_CYCLES_PER_SEC;

    u32 cycles = togo << shift[ch];

    if (cycles > SH4_CYCLES_PER_SEC)
        togo = SH4_CYCLES_PER_SEC;

    if (mask[ch]) {
        i64 tcode = *cpu->trace.cycles + cycles;
        cpu->scheduler->add_or_run_abs(tcode, ch, this, &scheduled_tmu_callback, nullptr);
    }
    else {
        cpu->scheduler->add_or_run_abs(-1, ch, this, &scheduled_tmu_callback, nullptr);
    }
}


void TMU::write_TCNT(u32 ch, u32 data)
{
    base[ch] = data + ((TCUSE >> shift[ch]) & mask[ch]);
    base64[ch] = data + ((TCUSE >> shift[ch]) & mask64[ch]);

    sched_chan_tick(ch);
}


void TMU::turn_onoff(u32 ch, u32 on)
{
    u32 mTCNT = read_TCNT(ch, false);
    //printf("\nTURN CH%d TO %d cyc:%llu tcyc:%llu", ch, on, clock.trace_cycles, clock.timer_cycles);
    mask[ch] = on ? 0xFFFFFFFF : 0;
    mask64[ch] = on ? 0xFFFFFFFFFFFFFFFF : 0;
    write_TCNT(ch, mTCNT);

    sched_chan_tick(ch);
}

void TMU::write_TSTR(u32 val)
{
    TSTR = val;
    for (u32 i = 0; i < 3; i++) {
        turn_onoff(i, val & (1 << i));
    }
}

void TMU::update_counts(u32 ch)
{
    //u32 irq_num = 10 + ch;
    // TODO: IRQ stuff
    cpu->interrupt_pend(static_cast<IRQ_SOURCES>(IRQ_tmu0_tuni0 + ch), (TCR[ch] & 0x100) && (TCR[ch] & 0x20)); // underflow
    if  ((TCR[ch] & 0x100) && (TCR[ch] & 0x200)) {
        //printf("\nRAISING TCR INTERRUPT!");
        TCR[ch] &= 0xFF;
    }


    //SH4_interrupt_mask(0x400 + (0x20 * ch), TCR[ch] & 0x200);
    //InterruptPend(tmu_intID[reg], TMU_TCR(reg) & 0x100);
    //InterruptMask(tmu_intID[reg], TMU_TCR(reg) & 0x200);


    if (old_mode[ch] == (TCR[ch] & 7))
        return;
    
    old_mode[ch] = TCR[ch] & 7;
    u32 mTCNT = read_TCNT(ch, 0);
    switch (TCR[ch] & 0x7) // mode
    {
        case 0: //4
            shift[ch] = 2;
            break;

        case 1: //16
            shift[ch] = 4;
            break;

        case 2: //64
            shift[ch] = 6;
            break;

        case 3: //256
            shift[ch] = 8;
            break;

        case 4: //1024
            shift[ch] = 10;
            break;

        case 5: //reserved
            printf("TMU ch%d - TCR%d mode is reserved (5)", ch, ch);
            break;

        case 6: //RTC
            printf("TMU ch%d - TCR%d mode is RTC (6), can't be used on Dreamcast", ch, ch);
            break;

        case 7: //external
            printf("TMU ch%d - TCR%d mode is External (7), can't be used on Dreamcast", ch, ch);
            break;
    }
    shift[ch] += 2;
    write_TCNT(ch, mTCNT);
    sched_chan_tick(ch);
}

void TMU::reset()
{
    TOCR = TSTR = 0;
    TCOR[0] = TCOR[1] = TCOR[2] = 0xFFFFFFFF;
    TCR[0] = TCR[1] = TCR[2] = 0;

    update_counts(0);
    update_counts(1);
    update_counts(2);

    write_TSTR(0);

    for (u32 i = 0; i < 3; i++)
        write_TCNT(i, 0xffffffff);
}

void TMU::write_TCR(u32 ch, u32 val)
{
    TCR[ch] = val & 0xFFFF;
    update_counts(ch);
}

void TMU::write(u32 addr, u64 val, u8 sz, bool* success)
{
    switch(addr | 0xF0000000) {
        case 0xFFD80000: // TOCR
            TOCR = val & 0xFF;
            break;
        case 0xFFD80004: // TSTR
            write_TSTR(val);
            break;
        case 0xFFD80010: // TCR0
            write_TCR(0, val);
            break;
        case 0xFFD8001C: // TCR1
            write_TCR(1, val);
            break;
        case 0xFFD80028: // TCR2
            write_TCR(2, val);
            break;
        case 0xFFD8000C: // TCNT0
            write_TCNT(0, val);
            break;
        case 0xFFD80018: // TCNT1
            write_TCNT(1, val);
            break;
        case 0xFFD80024: // TCNT2
            write_TCNT(2, val);
            break;
        case 0xFFD80008: // TCOR0
            TCOR[0] = val;
            break;
        case 0xFFD80014: // TCOR1
            TCOR[1] = val;
            break;
        case 0xFFD80020: // TCOR2
            TCOR[2] = val;
            break;
    }
}

u64 TMU::read(u32 addr, u8 sz, bool* success)
{
    switch(addr | 0xF0000000) {
        case 0xFFD80004: // TSTR
            return TSTR;
        case 0xFFD80010: // TCR0
            return TCR[0];
        case 0xFFD8001C: // TCR1
            return TCR[1];
        case 0xFFD8000C: // TCNT0
            return read_TCNT(0, true);
        case 0xFFD80018: // TCNT1
            return read_TCNT(1, true);
        case 0xFFD80024: // TCNT2
            return read_TCNT(2, true);
        case 0xFFD80028: // TCR2
            return TCR[2];
            break;
    }
    *success = 0;
    return 0;
}

void scheduled_tmu_callback(void *ptr, u64 key, u64 sch_cycle, u32 jitter)
{
    TMU *th = static_cast<TMU *>(ptr);
    u32 ch = key;
    //printf("\nSCHEDULED CALLBACK! %d %08x cyc:%llu tcyc:%llu", ch, mask[ch], clock.trace_cycles, clock.timer_cycles);
    if (th->mask[ch]) {
        u32 tcnt = th->read_TCNT(ch, 0);

        s64 tcnt64 = (s64)th->read_TCNT64(ch);

        u32 tcor = th->TCOR[ch];

        //64 bit maths to differentiate big values from overflows
        if (tcnt64 <= jitter) {
            //raise interrupt, timer counted down
            th->TCR[ch] |= tmu_underflow;

            th->cpu->interrupt_pend(static_cast<IRQ_SOURCES>(IRQ_tmu0_tuni0+ch), 1);
            //InterruptPend(tmu_intID[ch], 1);

            //printf("\nInterrupt for %d, %lld cycles\n", ch, sch_cycle);
            //printf("\nHIGHEST INTERRUPT: %d", interrupt_highest_priority);
            //printf("\nIMASK: %d", regs.SR.IMASK);

            //schedule next trigger by writing the TCNT register
            th->write_TCNT(ch, tcor + tcnt);
        }
        else {

            //schedule next trigger by writing the TCNT register
            th->write_TCNT(ch, tcnt);
        }

        return;	//has already been scheduled by TCNT write
    }
}
}