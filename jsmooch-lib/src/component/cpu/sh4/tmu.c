//
// Created by David Schneider on 3/8/24.
//

#include "tmu.h"
#include "sh4_interpreter.h"
#include "stdio.h"

#define SH4_CYCLES_PER_SEC 200000000
#define tmu_underflow 0x0100
#define tmu_UNIE      0x0020


u32 scheduled_tmu_callback(void *ptr, u32 sch_cycle, u32 jitter);

/* A lot of the structure and logic here very closely follows Reicast */

static u32 read_TMU_TCNT(struct SH4* this, u32 ch)
{
    return this->tmu.base[ch] - ((this->trace_cycles >> this->tmu.shift[ch])& this->tmu.mask[ch]);
}

static i64 read_TMU_TCNT64(struct SH4* this, u32 ch)
{
    return (i64)this->tmu.base64[ch] - (i64)((this->trace_cycles >> this->tmu.shift[ch])& this->tmu.mask64[ch]);
}


static void sched_chan_tick(struct SH4* this, u32 ch)
{
    u32 togo = read_TMU_TCNT(this, ch);
    if (togo > SH4_CYCLES_PER_SEC)
        togo = SH4_CYCLES_PER_SEC;

    u32 cycles = togo << this->tmu.shift[ch];

    if (cycles > SH4_CYCLES_PER_SEC)
        togo = SH4_CYCLES_PER_SEC;

    if (this->tmu.mask[ch])
        schedule_event(this->scheduler, &this->tmu.scheduled_funcs[ch], cycles);
    else
        schedule_event(this->scheduler, &this->tmu.scheduled_funcs[ch], -1);
}


static void write_TMU_TCNT(struct SH4* this, u32 ch, u32 data)
{
    this->tmu.base[ch] = data + ((this->trace_cycles >> this->tmu.shift[ch])& this->tmu.mask[ch]);
    this->tmu.base64[ch] = data + ((this->trace_cycles >> this->tmu.shift[ch])& this->tmu.mask64[ch]);

    sched_chan_tick(this, ch);
}


static void turn_onoff(struct SH4* this, u32 ch, u32 on)
{
    u32 TCNT = read_TMU_TCNT(this, ch);
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
    //InterruptPend(tmu_intID[reg], TMU_TCR(reg) & tmu_underflow);
    //InterruptMask(tmu_intID[reg], TMU_TCR(reg) & tmu_UNIE);

    if (this->tmu.old_mode[ch] == (this->tmu.TCR[ch] & 7))
        return;
    
    this->tmu.old_mode[ch] = this->tmu.TCR[ch] & 7;
    u32 TCNT = read_TMU_TCNT(this, ch);
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
    for (u32 i = 0; i < 3; i++) {
        this->tmu.scheduled_func_structs[i].sh4 = this;
        this->tmu.scheduled_func_structs[i].ch = i;
        this->tmu.scheduled_funcs[i].ptr = (void *)&this->tmu.scheduled_func_structs[i];
        this->tmu.scheduled_funcs[i].func = &scheduled_tmu_callback;
    }
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
            return read_TMU_TCNT(this, 0);
        case 0xFFD80018: // TCNT1
            return read_TMU_TCNT(this, 1);
        case 0xFFD80024: // TCNT2
            return read_TMU_TCNT(this, 2);
    }
    *success = 0;
    return 0;
}

u32 scheduled_tmu_callback(void *ptr, u32 sch_cycle, u32 jitter)
{
    struct SH4_tmu_int_struct* is = (struct SH4_tmu_int_struct*) ptr;
    struct SH4* this = is->sh4;
    u32 ch = is->ch;
    if (this->tmu.mask[ch]) {

        u32 tcnt = read_TMU_TCNT(this, ch);

        s64 tcnt64 = (s64)read_TMU_TCNT64(this, ch);

        u32 tcor = this->tmu.TCOR[ch];

        //64 bit maths to differentiate big values from overflows
        if (tcnt64 <= jitter) {
            //raise interrupt, timer counted down
            // TODO: IRQ stuff
            this->tmu.TCR[ch] |= tmu_underflow;
            //InterruptPend(tmu_intID[ch], 1);

            printf("Interrupt for %d, %d cycles\n", ch, sch_cycle);

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
