//
// Created by RadDad772 on 3/8/24.
//

#include "stdlib.h"
#include "tmu.h"
#include "sh4_interpreter.h"
#include "sh4_interrupts.h"
#include "stdio.h"

#define SH4_CYCLES_PER_SEC 200000000
#define tmu_underflow 0x0100
#define tmu_UNIE      0x0020
#define TCUSE this->clock.timer_cycles

u32 scheduled_tmu_callback(void *ptr, u64 key, i64 sch_cycle, u32 jitter);

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

#define REGTCNT (((u32)(this->tmu.base[ch] - ((TCUSE >> this->tmu.shift[ch])& this->tmu.mask[ch])))) - 0x80A

static u32 read_TMU_TCNT(struct SH4* this, u32 ch, u32 is_regread)
{
#ifdef REICAST_DIFF
    static int num = 0;
    static s64 last_diff = 0;
    if ((ch == 0) && (this->clock.trace_cycles > 0) && is_regread) {
        num++;
        if (num <= TOTAL_EXPECTED) {
            u32 r = expected_TCNT[num - 1];
            u32 diff = r - REGTCNT;
            printf("\nDIFF: %08x %08lld ch:%d", diff, this->clock.timer_cycles - last_diff, ch);
            last_diff = this->clock.timer_cycles;
            return expected_TCNT[num - 1];
        }
    }
#endif
    return REGTCNT;
}

static i64 read_TMU_TCNT64(struct SH4* this, u32 ch)
{
    return (i64)this->tmu.base64[ch] - (i64)((TCUSE >> this->tmu.shift[ch])& this->tmu.mask64[ch]);
}


static void sched_chan_tick(struct SH4* this, u32 ch)
{
    u32 togo = read_TMU_TCNT(this, ch, 0);
    if (togo > SH4_CYCLES_PER_SEC)
        togo = SH4_CYCLES_PER_SEC;

    u32 cycles = togo << this->tmu.shift[ch];

    if (cycles > SH4_CYCLES_PER_SEC)
        togo = SH4_CYCLES_PER_SEC;

    if (this->tmu.mask[ch])
        scheduler_add(this->scheduler, this->clock.trace_cycles + cycles, SE_bound_function, ch, scheduler_bind_function(&scheduled_tmu_callback, this));
    else
        scheduler_add(this->scheduler, -1, SE_bound_function, ch, scheduler_bind_function(&scheduled_tmu_callback, this));
}


static void write_TMU_TCNT(struct SH4* this, u32 ch, u32 data)
{
    this->tmu.base[ch] = data + ((TCUSE >> this->tmu.shift[ch]) & this->tmu.mask[ch]);
    this->tmu.base64[ch] = data + ((TCUSE >> this->tmu.shift[ch]) & this->tmu.mask64[ch]);

    sched_chan_tick(this, ch);
}


static void turn_onoff(struct SH4* this, u32 ch, u32 on)
{
    u32 TCNT = read_TMU_TCNT(this, ch, 0);
    this->tmu.mask[ch] = on ? 0xFFFFFFFF : 0;
    this->tmu.mask64[ch] = on ? 0xFFFFFFFFFFFFFFFF : 0;
    write_TMU_TCNT(this, ch, TCNT);

    sched_chan_tick(this,ch);
}

static void write_TSTR(struct SH4* this, u32 val)
{
    this->tmu.TSTR = val;
    for (u32 i = 0; i < 3; i++) {
        turn_onoff(this, i, val & (1 << i));
    }
}

static void UpdateTMUCounts(struct SH4* this, u32 ch)
{
    u32 irq_num = 10 + ch;
    // TODO: IRQ stuff
    SH4_interrupt_pend(this, 0x400 + (0x20 * ch), (this->tmu.TCR[ch] & 0x100) && (this->tmu.TCR[ch] & 0x20)); // underflow
    if  ((this->tmu.TCR[ch] & 0x100) && (this->tmu.TCR[ch] & 0x200)) {
        printf("\nRAISING TCR INTERRUPT!");
        this->tmu.TCR[ch] &= 0xFF;
    }


    //SH4_interrupt_mask(this, 0x400 + (0x20 * ch), this->tmu.TCR[ch] & 0x200);
    //InterruptPend(tmu_intID[reg], TMU_TCR(reg) & 0x100);
    //InterruptMask(tmu_intID[reg], TMU_TCR(reg) & 0x200);


    if (this->tmu.old_mode[ch] == (this->tmu.TCR[ch] & 7))
        return;
    
    this->tmu.old_mode[ch] = this->tmu.TCR[ch] & 7;
    u32 TCNT = read_TMU_TCNT(this, ch, 0);
    switch (this->tmu.TCR[ch] & 0x7) // mode
    {
        case 0: //4
            this->tmu.shift[ch] = 2;
            break;

        case 1: //16
            this->tmu.shift[ch] = 4;
            break;

        case 2: //64
            this->tmu.shift[ch] = 6;
            break;

        case 3: //256
            this->tmu.shift[ch] = 8;
            break;

        case 4: //1024
            this->tmu.shift[ch] = 10;
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
    this->tmu.shift[ch] += 2;
    write_TMU_TCNT(this, ch, TCNT);
    sched_chan_tick(this, ch);
}

void TMU_init(struct SH4* this)
{
}

void TMU_reset(struct SH4* this)
{
    this->tmu.TOCR = this->tmu.TSTR = 0;
    this->tmu.TCOR[0] = this->tmu.TCOR[1] = this->tmu.TCOR[2] = 0xFFFFFFFF;
    this->tmu.TCR[0] = this->tmu.TCR[1] = this->tmu.TCR[2] = 0;

    UpdateTMUCounts(this, 0);
    UpdateTMUCounts(this, 1);
    UpdateTMUCounts(this, 2);

    write_TSTR(this, 0);

    for (u32 i = 0; i < 3; i++)
        write_TMU_TCNT(this, i, 0xffffffff);
}

static void write_TCR(struct SH4* this, u32 ch, u32 val)
{
    this->tmu.TCR[ch] = val & 0xFFFF;
    UpdateTMUCounts(this, ch);
}

void TMU_write(struct SH4* this, u32 addr, u64 val, u32 sz, u32* success)
{
    switch(addr | 0xF0000000) {
        case 0xFFD80000: // TOCR
            this->tmu.TOCR = val & 0xFF;
            break;
        case 0xFFD80004: // TSTR
            write_TSTR(this, val);
            break;
        case 0xFFD80010: // TCR0
            write_TCR(this, 0, val);
            break;
        case 0xFFD8001C: // TCR1
            write_TCR(this, 1, val);
            break;
        case 0xFFD80028: // TCR2
            write_TCR(this, 2, val);
            break;
        case 0xFFD8000C: // TCNT0
            write_TMU_TCNT(this, 0, val);
            break;
        case 0xFFD80018: // TCNT1
            write_TMU_TCNT(this, 1, val);
            break;
        case 0xFFD80024: // TCNT2
            write_TMU_TCNT(this, 2, val);
            break;
        case 0xFFD80008: // TCOR0
            this->tmu.TCOR[0] = val;
            break;
        case 0xFFD80014: // TCOR1
            this->tmu.TCOR[1] = val;
            break;
        case 0xFFD80020: // TCOR2
            this->tmu.TCOR[2] = val;
            break;
    }
}

u64 TMU_read(struct SH4* this, u32 addr, u32 sz, u32* success)
{
    switch(addr | 0xF0000000) {
        case 0xFFD80004: // TSTR
            return this->tmu.TSTR;
        case 0xFFD8000C: // TCNT0
            return read_TMU_TCNT(this, 0, 1);
        case 0xFFD80018: // TCNT1
            return read_TMU_TCNT(this, 1, 1);
        case 0xFFD80024: // TCNT2
            return read_TMU_TCNT(this, 2, 1);
    }
    *success = 0;
    return 0;
}

struct sh4ifunc_struct {
    struct SH4* sh4;
    u32 ch;
    u32 set_or_clear; // 0 = clear, 1 = set
};

//typedef u32 (*scheduler_callback)(void*,u64,i64,u32);
static u32 sh4ifunc(void *ptr, u64 key, i64 timecode, u32 jitter)
{
    struct sh4ifunc_struct *ifs = (struct sh4ifunc_struct *)ptr;
    struct SH4* this = ifs->sh4;
    u32 ch = ifs->ch;

    // TODO: this
    printf("\nPlease implement this delayed interrupt set/clear function sh4ifunc.");
    free(ptr);
}

u32 scheduled_tmu_callback(void *ptr, u64 key, i64 sch_cycle, u32 jitter)
{
    struct SH4* this = (struct SH4*)ptr;
    printf("\nSCHEDULED CALLBACK!");
    u32 ch = key;
    if (this->tmu.mask[ch]) {

        u32 tcnt = read_TMU_TCNT(this, ch, 0);

        s64 tcnt64 = (s64)read_TMU_TCNT64(this, ch);

        u32 tcor = this->tmu.TCOR[ch];

        //64 bit maths to differentiate big values from overflows
        if (tcnt64 <= jitter) {
            //raise interrupt, timer counted down
            this->tmu.TCR[ch] |= tmu_underflow;
            /*struct sh4ifunc_struct* f = malloc(sizeof(sh4ifunc_struct));
            f->sh4 = this;
            f->ch = ch;
            scheduler_add(&this->scheduler, this->clock.trace_cycles+1, SE_bound_function, ch, scheduler_bind_function(&sh4ifunc, f));*/
            u32 c = sh4i_tmu0_tuni0;
            if (ch == 1) c = sh4i_tmu1_tuni1;
            if (ch == 2) c = sh4i_tmu2_tuni2;
            SH4_interrupt_pend(this, c, 1);
            //InterruptPend(tmu_intID[ch], 1);

            printf("Interrupt for %d, %lld cycles\n", ch, sch_cycle);

            //schedule next trigger by writing the TCNT register
            write_TMU_TCNT(this, ch, tcor + tcnt);
        }
        else {

            //schedule next trigger by writing the TCNT register
            write_TMU_TCNT(this, ch, tcnt);
        }

        return 0;	//has already been scheduled by TCNT write
    }
    else {
        return 0;	//this channel is disabled, no need to schedule next event
    }
}
