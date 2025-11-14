//
// Created by . on 6/12/25.
//
#include <cassert>
#include <cstring>
#include <cstdio>

#include "helpers/debug.h"
#include "fail"
#include "huc6280.h"
#include "huc6280_disassembler.h"

static void timer_schedule(HUC6280 *this, u64 cur);


void HUC6280_init(HUC6280 *this, scheduler_t *scheduler, u64 clocks_per_second)
{
    memset(this, 0, sizeof(*this));
    this->scheduler = scheduler;
    this->timer.sch_interval = clocks_per_second / 6992;
    printf("\nTIMER INTERVAL: %lld", this->timer.sch_interval);


    jsm_string_init(&this->trace.str, 100);
    jsm_string_init(&this->trace.str2, 100);

    HUC6280_PSG_init(&this->psg);

    DBG_EVENT_VIEW_INIT;
    DBG_TRACE_VIEW_INIT;
}

void HUC6280_delete(HUC6280 *this)
{

    jsm_string_delete(&this->trace.str);
    jsm_string_delete(&this->trace.str2);
    HUC6280_PSG_delete(&this->psg);
}

// IRQD IS INVERTED!

void HUC6280_reset(HUC6280 *this)
{
    this->regs.MPR[7] = 0;
    this->regs.clock_div = 12;
    this->current_instruction = HUC6280_decoded_opcodes[0][0x100];
    this->regs.TCU = 0;
    this->regs.P.T = 0;
    this->pins.D = 0x100; // special RESET code
    this->regs.S = 0;
    HUC6280_PSG_reset(&this->psg);
}

void HUC6280_setup_tracing(HUC6280* this, jsm_debug_read_trace *strct, u64 *trace_cycle_ptr, i32 source_id)
{
    this->trace.strct.ptr = this;
    this->trace.strct.read_trace = strct->read_trace;
    this->trace.ok = 1;
    this->trace.cycles = trace_cycle_ptr;
    this->trace.source_id = source_id;
}
// Poll during second-to-last cycle

void HUC6280_poll_IRQs(HUC6280_regs *regs, HUC6280_pins *pins)
{
    regs->do_IRQ = (regs->IRQD.u & regs->IRQR.u) && !regs->P.I;
    regs->IRQR_polled.u = regs->IRQR.u;
}


void timer_tick(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct HUC6280 *this = (HUC6280 *)ptr;
    u64 cur = clock - jitter;
    timer_schedule(this, cur);
    if (!this->regs.timer_startstop) return;
    if (this->timer.counter == 0) {
        this->timer.counter = this->timer.reload;
        this->regs.IRQR.TIQ = this->regs.timer_startstop;
    }
    else this->timer.counter = (this->timer.counter - 1) & 0x7F;
}

static void timer_schedule(HUC6280 *this, u64 cur)
{
    if (this->timer.still_sch) {
        scheduler_delete_if_exist(this->scheduler, this->timer.sch_id);
    }
    this->timer.sch_id = scheduler_only_add_abs(this->scheduler, cur + this->timer.sch_interval, 0, this, &timer_tick, &this->timer.still_sch);
}

static u32 longpc(HUC6280 *this)
{
    u32 mpc = (this->regs.PC - 1) & 0xFFFF;
    return this->regs.MPR[mpc >> 13] | (mpc & 0x1FFF);
}

static void trace_format(HUC6280 *this, u32 opcode)
{
    u32 do_dbglog = 0;

    if (this->dbg.dvptr) {
        do_dbglog = this->dbg.dvptr->ids_enabled[this->dbg.dv_id];
    }
    //u32 do_tracething = (this->dbg.tvptr && dbg.trace_on && dbg.traces.huc6280.instruction);
    if (do_dbglog) { //}) || do_tracething) {
        jsm_string_quickempty(&this->trace.str);
        jsm_string_quickempty(&this->trace.str2);
        if (this->regs.IR < 0x100) {
            u32 mpc = this->pins.Addr;

            HUC6280_disassemble(this, &mpc, &this->trace.strct, &this->trace.str);

            // Now add context...
            jsm_string_sprintf(&this->trace.str2, "A:%02x  X:%02x  Y:%02x  S:%02x  P:%c%c%c%c%c%c%c%c  PC:%04x M_0:%02x 1:%02x 2:%02x 3:%02x 4:%02x 5:%02x 6:%02x 7:%02x",
                               this->regs.A, this->regs.X, this->regs.Y, this->regs.S,
                               this->regs.P.N ? 'N' : 'n',
                               this->regs.P.V ? 'V' : 'v',
                               this->regs.P.T ? 'T' : 't',
                               this->regs.P.B ? 'B' : 'b',
                               this->regs.P.D ? 'D' : 'd',
                               this->regs.P.I ? 'I' : 'i',
                               this->regs.P.Z ? 'Z' : 'z',
                               this->regs.P.C ? 'C' : 'c',
                               this->regs.PC,
                               this->regs.MPR[0] >> 13,
                               this->regs.MPR[1] >> 13,
                               this->regs.MPR[2] >> 13,
                               this->regs.MPR[3] >> 13,
                               this->regs.MPR[4] >> 13,
                               this->regs.MPR[5] >> 13,
                               this->regs.MPR[6] >> 13,
                               this->regs.MPR[7] >> 13
            );
        }
        else {
            switch(opcode) {
                case 0x100: // RESET
                    jsm_string_sprintf(&this->trace.str, "RESET");
                    break;
                case 0x101: // IRQ2
                    jsm_string_sprintf(&this->trace.str, "IRQ2");
                    break;
                case 0x102: // IRQ1
                    jsm_string_sprintf(&this->trace.str, "IRQ1");
                    //printf("\nIRQ1 @ %lld", *this->trace.cycles);
                    break;
                case 0x103: // TIQ
                    jsm_string_sprintf(&this->trace.str, "TIQ");
                    break;
            }
        }
        u64 tc;
        if (!this->trace.cycles) tc = 0;
        else tc = *this->trace.cycles;

        if (do_dbglog) {
            struct dbglog_view *dv = this->dbg.dvptr;
            dbglog_view_add_printf(dv, this->dbg.dv_id, tc, DBGLS_TRACE, "%06x  %s", longpc(this), this->trace.str.ptr);
            dbglog_view_extra_printf(dv, "%s", this->trace.str2.ptr);
        }
    }
}

void HUC6280_cycle(HUC6280 *this)
{
    this->regs.TCU++;
    //printf("\nEXEC TCU %d PC %04x", this->regs.TCU, this->regs.PC);
    if (this->regs.TCU == 1) {
        this->PCO = (this->regs.PC - 1) & 0xFFFF;
        this->regs.IR = this->pins.D;
        if (this->regs.do_IRQ) {
            //printf("\nDO IRQ! %lld", *this->trace.cycles);
            this->regs.do_IRQ = 0;
            // timer > IRQ1 > IRQ2
            if (this->regs.IRQD.TIQ & this->regs.IRQR_polled.TIQ) { // TIQ is 103
                this->regs.IR = 0x103;
                this->regs.IRQR.TIQ = 0;
                DBG_EVENT(this->dbg.events.TIQ);
                //printf("\nTIQ!");
            }
            else if (this->regs.IRQD.IRQ1 & this->regs.IRQR_polled.IRQ1) { // IRQ1 is 102
                //printf("\nIRQ1");
                this->regs.IR = 0x102;
                DBG_EVENT(this->dbg.events.IRQ1);
                if (dbg.trace_on) {
                    static int a = 0;
                    if (!a) {
                        a++;
                        dbg_break("WOOHOO", 0);
                    }
                }
                //dbg_break("HAHAHA", 0);
            }
            else if (this->regs.IRQD.IRQ2 & this->regs.IRQR_polled.IRQ2) { // IRQ2 is 101
                //printf("\nIRQ2");
                this->regs.IR = 0x101;
                DBG_EVENT(this->dbg.events.IRQ2);
            }
            else {
                assert(1==2);
            }
        }
        if ((this->regs.MPR[this->regs.PC >> 13]) > 0x1FE000) {
            dbg_break("PHNO", 0);
        }
        trace_format(this, this->regs.IR);
        this->current_instruction = HUC6280_decoded_opcodes[this->regs.P.T][this->regs.IR];
#ifdef TG16_LYCODER2
        dbg_printf("PC:%04X I:%02X A:%02X X:%02X Y:%02X P:%02X S:%02X MPR0-7:%02X %02X %02X %02X %02X %02X %02X %02x\n",
            this->PCO, this->regs.IR, this->regs.A, this->regs.X, this->regs.Y, this->regs.P.u, this->regs.S,
            this->regs.MPR[0] >> 13, this->regs.MPR[1] >> 13, this->regs.MPR[2] >> 13,
            this->regs.MPR[3] >> 13, this->regs.MPR[4] >> 13, this->regs.MPR[5] >> 13,
            this->regs.MPR[6] >> 13, this->regs.MPR[7] >> 13);
        //if (this->PCO == 0xE247) {
        //}
#endif
#ifdef HUC6280_TESTING
        this->ins_decodes++;
#endif
    }
    // NEXT: check HUC6270 IRQ vs. normal
    // NEXT: check HUC6270 timing
    this->current_instruction(&this->regs, &this->pins);
    this->trace.my_cycles++;
}

static u32 internal_read(HUC6280 *this, u32 addr, u32 has_effect)
{
    u32 k = addr & 0x1C00;
    u32 val = 0xFF;
    switch(k) {
        case 0x0800: // PSG.
            if (this->pins.BM) return 0;
            val = this->io.buffer;
            break;
        case 0x0C00: // 1FEC00 to 1FEFFF is timer
            if (this->pins.BM) return 0;
            val = this->timer.counter;
            val |= this->io.buffer & 0x80;
            break;
        case 0x1000: // IO
            if (this->pins.BM) return 0;
            val = this->read_io_func(this->read_io_ptr) & 15;
            val |= 0b00110000; // TurboGrafx-16
            // TODO: cdrom addon would set upper bit
            break;
        case 0x1400: // IRQ
            if (this->pins.BM) return 0;
            switch(addr & 3) {
                case 0:
                case 1:
                    val = this->io.buffer;
                    break;
                case 2:
                    val = (this->regs.IRQD.u ^ 7) | (this->io.buffer & 0b11111000);
                    break;
                case 3:
                    val = this->regs.IRQR.u | (this->io.buffer & 0b11111000);
                    break;
            }
            break;
        case 0x1800: {// CD IO
            static int a = 0;
            if (!a) {
                printf("\nWARNING READ FROM TURBOCD!!!");
                a = 1;
            }
            val = 0xFF;
            break;}
        case 0x1C00: // unmapped
            val = 0xFF;
            break;
    }
#ifdef TG16_LYCODER2
    dbg_printf("CPU read %06X: %02X\n", addr, val);
#endif
    return val;
}

static void internal_write(HUC6280 *this, u32 addr, u32 val)
{
#ifdef TG16_LYCODER2
    dbg_printf("CPU write %06X: %02X\n", addr, val);
#endif
    u32 k = addr & 0x1C00;

    switch(k) {
        case 0x0800:
            HUC6280_PSG_write(&this->psg, addr, val);
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
                this->regs.IRQD.u = (val & 7) ^ 7;
            }
            else if ((addr & 3) == 3) {
                this->regs.IRQR.TIQ = 0;
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

static void trace_write(HUC6280 *this) {
    u32 do_dbglog = 0;
    if (this->dbg.dvptr) {
        do_dbglog = this->dbg.dvptr->ids_enabled[this->trace.dbglog.id_write];
    }
    if (do_dbglog) {
        jsm_string_quickempty(&this->trace.str);
        jsm_string_quickempty(&this->trace.str2);
        struct dbglog_view *dv = this->dbg.dvptr;
        u64 tc;
        if (!this->trace.cycles) tc = 0;
        else tc = *this->trace.cycles;
        dbglog_view_add_printf(dv, this->trace.dbglog.id_write, tc, DBGLS_TRACE, "%06x    (write) %02x", this->pins.Addr, this->pins.D);
    }
}

static void trace_read(HUC6280 *this)
{
    u32 do_dbglog = 0;
    if (this->dbg.dvptr) {
        do_dbglog = this->dbg.dvptr->ids_enabled[this->trace.dbglog.id_read];
    }
    if (do_dbglog) {
        jsm_string_quickempty(&this->trace.str);
        jsm_string_quickempty(&this->trace.str2);
        struct dbglog_view *dv = this->dbg.dvptr;
        u64 tc;
        if (!this->trace.cycles) tc = 0;
        else tc = *this->trace.cycles;
        dbglog_view_add_printf(dv, this->trace.dbglog.id_read, tc, DBGLS_TRACE, "%06x    (read) %02x", this->pins.Addr, this->pins.D);
        dbglog_view_extra_printf(dv, "");
    }
}

void HUC6280_internal_cycle(void *ptr, u64 key, u64 clock, u32 jitter) {
    struct HUC6280* this = (HUC6280 *)ptr;
    u64 cur = clock - jitter;
    this->extra_cycles = 0;
    if (this->pins.RD) {
        if (this->pins.Addr >= 0x1FE800)
            this->pins.D = internal_read(this, this->pins.Addr, 1);
        else {
            if (this->pins.Addr >= 0x1FE000)
                this->extra_cycles = 1;
            this->pins.D = this->read_func(this->read_ptr, this->pins.Addr, this->pins.D, 1);
#ifdef TG16_LYCODER2
            dbg_printf("CPU read %06X: %02X\n", this->pins.Addr, this->pins.D);
#endif
        }
        trace_read(this);
    }

    HUC6280_cycle(this);
    assert(this->pins.Addr < 0x200000);

    if (this->pins.WR) {
        trace_write(this);
        if (this->pins.Addr >= 0x1FE800)
            internal_write(this, this->pins.Addr, this->pins.D);
        else {
            if (this->pins.Addr >= 0x1FE000)
                this->extra_cycles = 1;
#ifdef TG16_LYCODER2
            if ((this->regs.IR != 0x03) && (this->regs.IR != 0x13) && (this->regs.IR != 0x23))
                dbg_printf("CPU write %06X: %02X\n", this->pins.Addr, this->pins.D);
#endif
            this->write_func(this->write_ptr, this->pins.Addr, this->pins.D);
        }
    }
    u32 sch_for = this->regs.clock_div * (this->extra_cycles + 1);
    //scheduler_from_event_adjust_master_clock(this->scheduler, this->regs.clock_div);
    scheduler_only_add_abs(this->scheduler, cur + sch_for, 0, this, &HUC6280_internal_cycle, NULL);
}


void HUC6280_schedule_first(HUC6280 *this, u64 clock)
{
    scheduler_only_add_abs(this->scheduler, clock + this->regs.clock_div, 0, this, &HUC6280_internal_cycle, NULL);
    timer_schedule(this, clock);
}