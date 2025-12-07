//
// Created by . on 6/12/25.
//
#include <cassert>
#include <cstring>
#include <cstdio>

#include "helpers/debugger/debugger.h"
#include "helpers/debug.h"
#include "huc6280.h"
#include "huc6280_disassembler.h"

namespace HUC6280 {

core::core(scheduler_t *scheduler_in, u64 clocks_per_second) : scheduler(scheduler_in) {
    timer.sch_interval = clocks_per_second / 6992;
    printf("\nTIMER INTERVAL: %lld", timer.sch_interval);

    DBG_TRACE_VIEW_INIT;
}

// IRQD IS INVERTED!
void core::reset()
{
    regs.MPR[7] = 0;
    regs.clock_div = 12;
    current_instruction = decoded_opcodes[0][0x100];
    regs.TCU = 0;
    regs.P.T = 0;
    pins.D = 0x100; // special RESET code
    regs.S = 0;
    psg.reset();
}

void core::setup_tracing(jsm_debug_read_trace *strct, u64 *trace_cycle_ptr, i32 source_id)
{
    trace.strct.ptr = this;
    trace.strct.read_trace = strct->read_trace;
    trace.ok = 1;
    trace.cycles = trace_cycle_ptr;
    trace.source_id = source_id;
}
// Poll during second-to-last cycle

void core::poll_IRQs()
{
    regs.do_IRQ = (regs.IRQD.u & regs.IRQR.u) && !regs.P.I;
    regs.IRQR_polled.u = regs.IRQR.u;
}


void core::timer_tick(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto &th = *static_cast<core *>(ptr);
    u64 cur = clock - jitter;
    th.timer_schedule(cur);
    if (!th.regs.timer_startstop) return;
    if (th.timer.counter == 0) {
        th.timer.counter = th.timer.reload;
        th.regs.IRQR.TIQ = th.regs.timer_startstop;
    }
    else th.timer.counter = (th.timer.counter - 1) & 0x7F;
}

void core::timer_schedule(u64 cur)
{
    if (timer.still_sch) {
        scheduler->delete_if_exist(timer.sch_id);
    }
    timer.sch_id = scheduler->only_add_abs(cur + timer.sch_interval, 0, this, &timer_tick, &timer.still_sch);
}

u32 core::longpc() const {
    const u32 mpc = (regs.PC - 1) & 0xFFFF;
    return regs.MPR[mpc >> 13] | (mpc & 0x1FFF);
}

void core::trace_format(u32 opcode)
{
    u32 do_dbglog = 0;

    if (dbg.dvptr) {
        do_dbglog = dbg.dvptr->ids_enabled[dbg.dv_id];
    }
    //u32 do_tracething = (dbg.tvptr && dbg.trace_on && dbg.traces.huc6280.instruction);
    if (do_dbglog) { //}) || do_tracething) {
        trace.str.quickempty();
        trace.str2.quickempty();
        if (regs.IR < 0x100) {
            u32 mpc = pins.Addr;

            disassemble(*this, mpc, trace.strct, trace.str);

            // Now add context...
            trace.str2.sprintf("A:%02x  X:%02x  Y:%02x  S:%02x  P:%c%c%c%c%c%c%c%c  PC:%04x M_0:%02x 1:%02x 2:%02x 3:%02x 4:%02x 5:%02x 6:%02x 7:%02x",
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
                    trace.str.sprintf("RESET");
                    break;
                case 0x101: // IRQ2
                    trace.str.sprintf("IRQ2");
                    break;
                case 0x102: // IRQ1
                    trace.str.sprintf("IRQ1");
                    //printf("\nIRQ1 @ %lld", *trace.cycles);
                    break;
                case 0x103: // TIQ
                    trace.str.sprintf("TIQ");
                    break;
                default:
                    NOGOHERE;
            }
        }
        u64 tc;
        if (!trace.cycles) tc = 0;
        else tc = *trace.cycles;

        if (do_dbglog) {
            dbglog_view *dv = dbg.dvptr;
            dv->add_printf(dbg.dv_id, tc, DBGLS_TRACE, "%06x  %s", longpc(), trace.str.ptr);
            dv->extra_printf("%s", trace.str2.ptr);
        }
    }
}

void core::cycle()
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
                if (::dbg.trace_on) {
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
        trace_format(regs.IR);
        current_instruction = decoded_opcodes[regs.P.T][regs.IR];
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
    current_instruction(*this);
    trace.my_cycles++;
}

u8 core::internal_read(u32 addr, bool has_effect) const
{
    const u32 k = addr & 0x1C00;
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
                default:
                    NOGOHERE;
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
        default:
    }
#ifdef TG16_LYCODER2
    dbg_printf("CPU read %06X: %02X\n", addr, val);
#endif
    return val;
}

void core::internal_write(u32 addr, u8 val)
{
#ifdef TG16_LYCODER2
    dbg_printf("CPU write %06X: %02X\n", addr, val);
#endif
    switch(addr & 0x1C00) {
        case 0x0800:
            psg.write(addr, val);
            return;
        case 0x0C00: // 1FEC00 to 1FEFFF is timer
            io.buffer = val;
            if ((addr & 1) == 0) {
                timer.reload = val & 0x7F;
            }
            else {
                if (!regs.timer_startstop && (val & 1)) {
                    timer.counter = timer.reload;
                    //timer_schedule(*scheduler->clock);
                }
                regs.timer_startstop = val & 1;
                /*if (((val & 1) ^ 1) && timer.still_sch) {
                    //scheduler->delete_if_exist(timer.still_sch);
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
        default:
    }

    printf("\nUNHANDLED INTERNAL WRITE! %06x - %02x", addr, val);
}

void core::trace_write() {
    u32 do_dbglog = 0;
    if (dbg.dvptr) {
        do_dbglog = dbg.dvptr->ids_enabled[trace.dbglog.id_write];
    }
    if (do_dbglog) {
        trace.str.quickempty();
        trace.str2.quickempty();
        dbglog_view *dv = dbg.dvptr;
        u64 tc;
        if (!trace.cycles) tc = 0;
        else tc = *trace.cycles;
        dv->add_printf(trace.dbglog.id_write, tc, DBGLS_TRACE, "%06x    (write) %02x", pins.Addr, pins.D);
    }
}

void core::trace_read()
{
    u32 do_dbglog = 0;
    if (dbg.dvptr) {
        do_dbglog = dbg.dvptr->ids_enabled[trace.dbglog.id_read];
    }
    if (do_dbglog) {
        trace.str.quickempty();
        trace.str2.quickempty();
        dbglog_view *dv = dbg.dvptr;
        u64 tc;
        if (!trace.cycles) tc = 0;
        else tc = *trace.cycles;
        dv->add_printf(trace.dbglog.id_read, tc, DBGLS_TRACE, "%06x    (read) %02x", pins.Addr, pins.D);
        dv->extra_printf("");
    }
}

void core::internal_cycle(void *ptr, u64 key, u64 clock, u32 jitter) {
     auto &th = *static_cast<core *>(ptr);
    u64 cur = clock - jitter;
    th.extra_cycles = 0;
    if (th.pins.RD) {
        if (th.pins.Addr >= 0x1FE800)
            th.pins.D = th.internal_read(th.pins.Addr, true);
        else {
            if (th.pins.Addr >= 0x1FE000)
                th.extra_cycles = 1;
            th.pins.D = th.read_func(th.read_ptr, th.pins.Addr, th.pins.D, true);
#ifdef TG16_LYCODER2
            dbg_printf("CPU read %06X: %02X\n", pins.Addr, pins.D);
#endif
        }
        th.trace_read();
    }

    th.cycle();
    assert(th.pins.Addr < 0x200000);

    if (th.pins.WR) {
        th.trace_write();
        if (th.pins.Addr >= 0x1FE800)
            th.internal_write(th.pins.Addr, th.pins.D);
        else {
            if (th.pins.Addr >= 0x1FE000)
                th.extra_cycles = 1;
#ifdef TG16_LYCODER2
            if ((regs.IR != 0x03) && (regs.IR != 0x13) && (regs.IR != 0x23))
                dbg_printf("CPU write %06X: %02X\n", pins.Addr, pins.D);
#endif
            th.write_func(th.write_ptr, th.pins.Addr, th.pins.D);
        }
    }
    u32 sch_for = th.regs.clock_div * (th.extra_cycles + 1);
    //scheduler->from_event_adjust_master_clock(regs.clock_div);
    th.scheduler->only_add_abs(cur + sch_for, 0, &th, &internal_cycle, nullptr);
}


void core::schedule_first(u64 clock)
{
    scheduler->only_add_abs(clock + regs.clock_div, 0, this, &internal_cycle, nullptr);
    timer_schedule(clock);
}
}