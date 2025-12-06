//
// Created by . on 6/12/25.
//
#include <cassert>
#include <cstring>
#include <cstdio>

#include "helpers/debug.h"
#include "huc6280.h"
#include "huc6280_disassembler.h"

namespace HUC6280 {

static void timer_schedule(HUC6280 *this, u64 cur);


core::core(HUC6280 *this, scheduler_t *scheduler, u64 clocks_per_second)
{
    timer.sch_interval = clocks_per_second / 6992;
    printf("\nTIMER INTERVAL: %lld", timer.sch_interval);



    HUC6280_PSG_init(&psg);

    DBG_EVENT_VIEW_INIT;
    DBG_TRACE_VIEW_INIT;
}

void HUC6280_delete(HUC6280 *this)
{

    jsm_string_delete(&trace.str);
    jsm_string_delete(&trace.str2);
    HUC6280_PSG_delete(&psg);
}

// IRQD IS INVERTED!

void HUC6280_reset(HUC6280 *this)
{
    regs.MPR[7] = 0;
    regs.clock_div = 12;
    current_instruction = HUC6280_decoded_opcodes[0][0x100];
    regs.TCU = 0;
    regs.P.T = 0;
    pins.D = 0x100; // special RESET code
    regs.S = 0;
    HUC6280_PSG_reset(&psg);
}

void HUC6280_setup_tracing(HUC6280* this, jsm_debug_read_trace *strct, u64 *trace_cycle_ptr, i32 source_id)
{
    trace.strct.ptr = this;
    trace.strct.read_trace = strct->read_trace;
    trace.ok = 1;
    trace.cycles = trace_cycle_ptr;
    trace.source_id = source_id;
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
    if (!regs.timer_startstop) return;
    if (timer.counter == 0) {
        timer.counter = timer.reload;
        regs.IRQR.TIQ = regs.timer_startstop;
    }
    else timer.counter = (timer.counter - 1) & 0x7F;
}

static void timer_schedule(HUC6280 *this, u64 cur)
{
    if (timer.still_sch) {
        scheduler_delete_if_exist(scheduler, timer.sch_id);
    }
    timer.sch_id = scheduler_only_add_abs(scheduler, cur + timer.sch_interval, 0, this, &timer_tick, &timer.still_sch);
}

static u32 longpc(HUC6280 *this)
{
    u32 mpc = (regs.PC - 1) & 0xFFFF;
    return regs.MPR[mpc >> 13] | (mpc & 0x1FFF);
}

static void trace_format(HUC6280 *this, u32 opcode)
{
    u32 do_dbglog = 0;

    if (dbg.dvptr) {
        do_dbglog = dbg.dvptr->ids_enabled[dbg.dv_id];
    }
    //u32 do_tracething = (dbg.tvptr && dbg.trace_on && dbg.traces.huc6280.instruction);
    if (do_dbglog) { //}) || do_tracething) {
        jsm_string_quickempty(&trace.str);
        jsm_string_quickempty(&trace.str2);
        if (regs.IR < 0x100) {
            u32 mpc = pins.Addr;

            HUC6280_disassemble(this, &mpc, &trace.strct, &trace.str);

            // Now add context...
            jsm_string_sprintf(&trace.str2, "A:%02x  X:%02x  Y:%02x  S:%02x  P:%c%c%c%c%c%c%c%c  PC:%04x M_0:%02x 1:%02x 2:%02x 3:%02x 4:%02x 5:%02x 6:%02x 7:%02x",
                               regs.A, regs.X, regs.Y, regs.S,
                               regs.P.N ? 'N' : 'n',
                               regs.P.V ? 'V' : 'v',
                               regs.P.T ? 'T' : 't',
                               regs.P.B ? 'B' : 'b',
                               regs.P.D ? 'D' : 'd',
                               regs.P.I ? 'I' : 'i',
                               regs.P.Z ? 'Z' : 'z',
                               regs.P.C ? 'C' : 'c',
                               regs.PC,
                               regs.MPR[0] >> 13,
                               regs.MPR[1] >> 13,
                               regs.MPR[2] >> 13,
                               regs.MPR[3] >> 13,
                               regs.MPR[4] >> 13,
                               regs.MPR[5] >> 13,
                               regs.MPR[6] >> 13,
                               regs.MPR[7] >> 13
            );
        }
        else {
            switch(opcode) {
                case 0x100: // RESET
                    jsm_string_sprintf(&trace.str, "RESET");
                    break;
                case 0x101: // IRQ2
                    jsm_string_sprintf(&trace.str, "IRQ2");
                    break;
                case 0x102: // IRQ1
                    jsm_string_sprintf(&trace.str, "IRQ1");
                    //printf("\nIRQ1 @ %lld", *trace.cycles);
                    break;
                case 0x103: // TIQ
                    jsm_string_sprintf(&trace.str, "TIQ");
                    break;
            }
        }
        u64 tc;
        if (!trace.cycles) tc = 0;
        else tc = *trace.cycles;

        if (do_dbglog) {
            struct dbglog_view *dv = dbg.dvptr;
            dbglog_view_add_printf(dv, dbg.dv_id, tc, DBGLS_TRACE, "%06x  %s", longpc(this), trace.str.ptr);
            dbglog_view_extra_printf(dv, "%s", trace.str2.ptr);
        }
    }
}

void HUC6280_cycle(HUC6280 *this)
{
    regs.TCU++;
    //printf("\nEXEC TCU %d PC %04x", regs.TCU, regs.PC);
    if (regs.TCU == 1) {
        PCO = (regs.PC - 1) & 0xFFFF;
        regs.IR = pins.D;
        if (regs.do_IRQ) {
            //printf("\nDO IRQ! %lld", *trace.cycles);
            regs.do_IRQ = 0;
            // timer > IRQ1 > IRQ2
            if (regs.IRQD.TIQ & regs.IRQR_polled.TIQ) { // TIQ is 103
                regs.IR = 0x103;
                regs.IRQR.TIQ = 0;
                DBG_EVENT(dbg.events.TIQ);
                //printf("\nTIQ!");
            }
            else if (regs.IRQD.IRQ1 & regs.IRQR_polled.IRQ1) { // IRQ1 is 102
                //printf("\nIRQ1");
                regs.IR = 0x102;
                DBG_EVENT(dbg.events.IRQ1);
                if (dbg.trace_on) {
                    static int a = 0;
                    if (!a) {
                        a++;
                        dbg_break("WOOHOO", 0);
                    }
                }
                //dbg_break("HAHAHA", 0);
            }
            else if (regs.IRQD.IRQ2 & regs.IRQR_polled.IRQ2) { // IRQ2 is 101
                //printf("\nIRQ2");
                regs.IR = 0x101;
                DBG_EVENT(dbg.events.IRQ2);
            }
            else {
                assert(1==2);
            }
        }
        if ((regs.MPR[regs.PC >> 13]) > 0x1FE000) {
            dbg_break("PHNO", 0);
        }
        trace_format(this, regs.IR);
        current_instruction = HUC6280_decoded_opcodes[regs.P.T][regs.IR];
#ifdef TG16_LYCODER2
        dbg_printf("PC:%04X I:%02X A:%02X X:%02X Y:%02X P:%02X S:%02X MPR0-7:%02X %02X %02X %02X %02X %02X %02X %02x\n",
            PCO, regs.IR, regs.A, regs.X, regs.Y, regs.P.u, regs.S,
            regs.MPR[0] >> 13, regs.MPR[1] >> 13, regs.MPR[2] >> 13,
            regs.MPR[3] >> 13, regs.MPR[4] >> 13, regs.MPR[5] >> 13,
            regs.MPR[6] >> 13, regs.MPR[7] >> 13);
        //if (PCO == 0xE247) {
        //}
#endif
#ifdef HUC6280_TESTING
        ins_decodes++;
#endif
    }
    // NEXT: check HUC6270 IRQ vs. normal
    // NEXT: check HUC6270 timing
    current_instruction(&regs, &pins);
    trace.my_cycles++;
}

static u32 internal_read(HUC6280 *this, u32 addr, u32 has_effect)
{
    u32 k = addr & 0x1C00;
    u32 val = 0xFF;
    switch(k) {
        case 0x0800: // PSG.
            if (pins.BM) return 0;
            val = io.buffer;
            break;
        case 0x0C00: // 1FEC00 to 1FEFFF is timer
            if (pins.BM) return 0;
            val = timer.counter;
            val |= io.buffer & 0x80;
            break;
        case 0x1000: // IO
            if (pins.BM) return 0;
            val = read_io_func(read_io_ptr) & 15;
            val |= 0b00110000; // TurboGrafx-16
            // TODO: cdrom addon would set upper bit
            break;
        case 0x1400: // IRQ
            if (pins.BM) return 0;
            switch(addr & 3) {
                case 0:
                case 1:
                    val = io.buffer;
                    break;
                case 2:
                    val = (regs.IRQD.u ^ 7) | (io.buffer & 0b11111000);
                    break;
                case 3:
                    val = regs.IRQR.u | (io.buffer & 0b11111000);
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
            HUC6280_PSG_write(&psg, addr, val);
            return;
        case 0x0C00: // 1FEC00 to 1FEFFF is timer
            io.buffer = val;
            if ((addr & 1) == 0) {
                timer.reload = val & 0x7F;
            }
            else {
                if (!regs.timer_startstop && (val & 1)) {
                    timer.counter = timer.reload;
                    //timer_schedule(this, *scheduler->clock);
                }
                regs.timer_startstop = val & 1;
                /*if (((val & 1) ^ 1) && timer.still_sch) {
                    //scheduler_delete_if_exist(scheduler, timer.still_sch);
                }*/
            }
            return;
        case 0x1000: // IO
            io.buffer = val;
            write_io_func(write_io_ptr, val & 3);
            return;
        case 0x1400: // IRQ stuff
            io.buffer = val;
            if ((addr & 3) == 2) {
                regs.IRQD.u = (val & 7) ^ 7;
            }
            else if ((addr & 3) == 3) {
                regs.IRQR.TIQ = 0;
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
    if (dbg.dvptr) {
        do_dbglog = dbg.dvptr->ids_enabled[trace.dbglog.id_write];
    }
    if (do_dbglog) {
        jsm_string_quickempty(&trace.str);
        jsm_string_quickempty(&trace.str2);
        struct dbglog_view *dv = dbg.dvptr;
        u64 tc;
        if (!trace.cycles) tc = 0;
        else tc = *trace.cycles;
        dbglog_view_add_printf(dv, trace.dbglog.id_write, tc, DBGLS_TRACE, "%06x    (write) %02x", pins.Addr, pins.D);
    }
}

static void trace_read(HUC6280 *this)
{
    u32 do_dbglog = 0;
    if (dbg.dvptr) {
        do_dbglog = dbg.dvptr->ids_enabled[trace.dbglog.id_read];
    }
    if (do_dbglog) {
        jsm_string_quickempty(&trace.str);
        jsm_string_quickempty(&trace.str2);
        struct dbglog_view *dv = dbg.dvptr;
        u64 tc;
        if (!trace.cycles) tc = 0;
        else tc = *trace.cycles;
        dbglog_view_add_printf(dv, trace.dbglog.id_read, tc, DBGLS_TRACE, "%06x    (read) %02x", pins.Addr, pins.D);
        dbglog_view_extra_printf(dv, "");
    }
}

void HUC6280_internal_cycle(void *ptr, u64 key, u64 clock, u32 jitter) {
    struct HUC6280* this = (HUC6280 *)ptr;
    u64 cur = clock - jitter;
    extra_cycles = 0;
    if (pins.RD) {
        if (pins.Addr >= 0x1FE800)
            pins.D = internal_read(this, pins.Addr, 1);
        else {
            if (pins.Addr >= 0x1FE000)
                extra_cycles = 1;
            pins.D = read_func(read_ptr, pins.Addr, pins.D, 1);
#ifdef TG16_LYCODER2
            dbg_printf("CPU read %06X: %02X\n", pins.Addr, pins.D);
#endif
        }
        trace_read(this);
    }

    HUC6280_cycle(this);
    assert(pins.Addr < 0x200000);

    if (pins.WR) {
        trace_write(this);
        if (pins.Addr >= 0x1FE800)
            internal_write(this, pins.Addr, pins.D);
        else {
            if (pins.Addr >= 0x1FE000)
                extra_cycles = 1;
#ifdef TG16_LYCODER2
            if ((regs.IR != 0x03) && (regs.IR != 0x13) && (regs.IR != 0x23))
                dbg_printf("CPU write %06X: %02X\n", pins.Addr, pins.D);
#endif
            write_func(write_ptr, pins.Addr, pins.D);
        }
    }
    u32 sch_for = regs.clock_div * (extra_cycles + 1);
    //scheduler_from_event_adjust_master_clock(scheduler, regs.clock_div);
    scheduler_only_add_abs(scheduler, cur + sch_for, 0, this, &HUC6280_internal_cycle, NULL);
}


void HUC6280_schedule_first(HUC6280 *this, u64 clock)
{
    scheduler_only_add_abs(scheduler, clock + regs.clock_div, 0, this, &HUC6280_internal_cycle, NULL);
    timer_schedule(this, clock);
}
}