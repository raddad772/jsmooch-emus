//
// Created by . on 6/12/25.
//
#include <assert.h>
#include <string.h>
#if !defined(_MSC_VER)
#include <printf.h>
#else
#include <stdio.h>
#endif

#include "huc6280.h"

void HUC6280_init(struct HUC6280 *this, struct scheduler_t *scheduler, u64 clocks_per_second)
{
    memset(this, 0, sizeof(*this));
    this->scheduler = scheduler;
    this->timer.sch_interval = clocks_per_second / 6992;
}

void HUC6280_delete(struct HUC6280 *this)
{

}

// IRQD IS INVERTED!

void HUC6280_reset(struct HUC6280 *this)
{
    this->regs.MPR[7] = 0;
    this->regs.clock_div = 12;
    this->current_instruction = HUC6280_decoded_opcodes[0][0x100];
    this->regs.TCU = 0;
    this->regs.P.T = 0;
    this->pins.D = 0x100;
}

void HUC6280_setup_tracing(struct HUC6280* this, struct jsm_debug_read_trace *strct)
{
    this->trace.strct.ptr = this;
    this->trace.strct.read_trace = strct->read_trace;
    this->trace.ok = 1;
}
// Poll during second-to-last cycle

void HUC6280_poll_IRQs(struct HUC6280_regs *regs, struct HUC6280_pins *pins)
{
    regs->do_IRQ = (regs->IRQD.u & regs->IRQR.u) && !regs->P.I;
}


static void timer_schedule(struct HUC6280 *this, u64 cur)
{
    /*if (this->timer.still_sch) {
        scheduler_delete_if_exist(this->scheduler, this->timer.sch_id);
    }
    this->timer.sch_id = scheduler_only_add_abs(this->scheduler, cur + this->timer.sch_interval, 0, this, &timer_tick, &this->timer.still_sch);*/
}

void HUC6280_tick_timer(struct HUC6280 *this)
{
    if (!this->regs.timer_startstop) return;
    if (this->timer.counter == 0) {
        this->timer.counter = this->timer.reload;
        this->regs.IRQR.TIQ = this->regs.timer_startstop;
    }
    else this->timer.counter = (this->timer.counter - 1) & 0x7F;

    //u64 cur = clock - jitter;
    //timer_schedule(this, cur);
}

void HUC6280_cycle(struct HUC6280 *this)
{
    this->regs.TCU++;
    //printf("\nEXEC TCU %d PC %04x", this->regs.TCU, this->regs.PC);
    if (this->regs.TCU == 1) {
        this->PCO = this->pins.Addr;
        this->regs.IR = this->pins.D;
        if (this->regs.do_IRQ) {
            this->regs.do_IRQ = 0;
            // timer > IRQ1 > IRQ2
            if (this->regs.IRQD.TIQ & this->regs.IRQR.TIQ) { // TIQ is 103
                this->regs.IR = 0x103;
            }
            else if (this->regs.IRQD.IRQ1 & this->regs.IRQR.IRQ1) { // IRQ1 is 102
                this->regs.IR = 0x102;

            }
            else if (this->regs.IRQD.IRQ2 & this->regs.IRQR.IRQ2) { // IRQ2 is 101
                this->regs.IR = 0x101;
            }
            else {
                assert(1==2);
            }
        }
        this->current_instruction = HUC6280_decoded_opcodes[this->regs.P.T][this->regs.IR];
#ifdef HUC6280_TESTING
        this->ins_decodes++;
#endif
    }
    this->current_instruction(&this->regs, &this->pins);
    this->trace.my_cycles++;
}

static u32 internal_read(struct HUC6280 *this, u32 addr, u32 has_effect)
{
    u32 k = addr & 0x1C00;
    u32 val;
    switch(k) {
        case 0x0800: // PSG. TODO: this
            if (this->pins.BM) return 0;
            return this->io.buffer;
        case 0x0C00: // 1FEC00 to 1FEFFF is timer
            if (this->pins.BM) return 0;
            val = this->timer.counter;
            val |= this->io.buffer & 0x80;
            return val;
        case 0x1000: // IO
            if (this->pins.BM) return 0;
            val = this->read_io_func(this->read_io_ptr) & 15;
            val |= 0b00110000; // TurboGrafx-16
            // TODO: cdrom addon would set upper bit
            return val;
        case 0x1400: // IRQ
            if (this->pins.BM) return 0;
            switch(addr & 3) {
                case 0:
                case 1:
                    return this->io.buffer;
                case 2:
                    return this->regs.IRQD.u | (this->io.buffer & 0b11111000);
                case 3:
                    return this->regs.IRQR.u | (this->io.buffer & 0b11111000);
            }
        case 0x1800: {// CD IO
            static int a = 0;
            if (!a) {
                printf("\nWARNING READ FROM TURBOCD!!!");
                a = 1;
            }
            return 0xFF; }
        case 0x1C00: // unmapped
            return 0xFF;

    }

    printf("\nUNHANDLED INTERNAL READ %06x", addr);
    return 0xFF;
}

static void internal_write(struct HUC6280 *this, u32 addr, u32 val)
{
    u32 k = addr & 0x1C00;

    switch(k) {
        case 0x0800: // PSG. TODO: this
            return;
        case 0x0C00: // 1FEC00 to 1FEFFF is timer
            this->io.buffer = val;
            if ((addr & 1) == 0) {
                this->timer.reload = val & 0x7F;
            }
            else {
                if (!this->regs.timer_startstop && (val & 1)) {
                    this->timer.counter = this->timer.reload;
                    //timer_schedule(this, *this->scheduler->clock);
                }
                this->regs.timer_startstop = val & 1;
                /*if (((val & 1) ^ 1) && this->timer.still_sch) {
                    //scheduler_delete_if_exist(this->scheduler, this->timer.still_sch);
                }*/
            }
            return;
        case 0x1000: // IO
            this->io.buffer = val;
            this->write_io_func(this->write_io_ptr, val & 3);
            return;
        case 0x1400: // IRQ stuff
            this->io.buffer = val;
            if ((addr & 3) == 2) {
                this->regs.IRQD.u = val & 7;
            }
            return;
        case 0x1800: {// CD IO
            static int a = 0;
            if (!a) {
                printf("\nWARNING WRITE TO TURBOCD!!!");
                a = 1;
            }
            return; }
        case 0x1C00: // nothin
            return;
    }

    printf("\nUNHANDLED INTERNAL WRITE! %06x - %02x", addr, val);
}

void HUC6280_internal_cycle(void *ptr, u64 key, u64 clock, u32 jitter) {
    struct HUC6280* this = (struct HUC6280 *)ptr;
    u64 cur = clock - jitter;
    if (this->pins.RD) {
        if (this->pins.Addr >= 0xFF0800)
            this->pins.D = internal_read(this, this->pins.Addr, 1);
        else
            this->pins.D = this->read_func(this->read_ptr, this->pins.Addr, this->pins.D, 1);
    }

    HUC6280_cycle(this);

    if (this->pins.WR) {
        if (this->pins.Addr >= 0xFF0800)
            internal_write(this, this->pins.Addr, this->pins.D);
        else
            this->write_func(this->write_ptr, this->pins.Addr, this->pins.D);
    }

    //scheduler_from_event_adjust_master_clock(this->scheduler, this->regs.clock_div);
    scheduler_only_add_abs(this->scheduler, cur + this->regs.clock_div, 0, this, &HUC6280_internal_cycle, NULL);
}


void HUC6280_schedule_first(struct HUC6280 *this, u64 clock)
{
    scheduler_only_add_abs(this->scheduler, clock + this->regs.clock_div, 0, this, &HUC6280_internal_cycle, NULL);
    timer_schedule(this, clock);
}