//
// Created by . on 4/16/25.
//

#include <cstdio>
#include <cstring>
#include "wdc65816.h"
#include "wdc65816_disassembler.h"
/*
 * interrupt notes
During WAI, When the Status Register I
flag is set (IRQB disabled) the IRQB interrupt will cause the next instruction (following the WAI instruction) to
be executed without going to the IRQB interrupt handler. This method results in the highest speed response
to an IRQB input.

 IRQB is level-sensitive
 NMIB is edge-sensitive
 *
 */
namespace WDC65816 {
extern ins_func decoded_opcodes[5][0x104];

void core::eval_WAI()
{
    if (regs.interrupt_pending) regs.WAI = 0;
}

void core::set_IRQ_level(u32 level)
{
    regs.IRQ_pending = level;
    if (level)
        regs.interrupt_pending = 1;
    else
        regs.interrupt_pending = regs.NMI_pending;
    eval_WAI();
}

void core::set_NMI_level(u32 level)
{
    if ((regs.NMI_old == 0) && level) { // 0->1
        regs.NMI_pending = 1;
        regs.interrupt_pending = 1;
    }
    regs.NMI_old = level;
    eval_WAI();
}

ins_func core::get_decoded_opcode()
{
    if (regs.E) {
        return decoded_opcodes[4][regs.IR];
    }
    u32 flag = (regs.P.M) | (regs.P.X << 1);

    ins_func ret = decoded_opcodes[flag][regs.IR];
    if (ret == nullptr) ret = decoded_opcodes[0][regs.IR];
    return ret;
}

void core::pprint_context(jsm_string *out)
{
    out->sprintf("%c%c  A:%04x  D:%04x  X:%04x  Y:%04x  DBR:%02x  PBR:%02x  S:%04x, P:%c%c%c%c%c%c",
        regs.P.M ? 'M' : 'm',
        regs.P.X ? 'X' : 'x',
        regs.C, regs.D,
        regs.X, regs.Y,
        regs.DBR, regs.PBR,
        regs.S,
        regs.P.C ? 'C' : 'c',
        regs.P.Z ? 'Z' : 'z',
        regs.P.I ? 'I' : 'i',
        regs.P.D ? 'D' : 'd',
        regs.P.V ? 'V' : 'v',
        regs.P.N ? 'N' : 'n'
        );
}

void core::trace_format()
{
    if (trace.dbglog.view && trace.dbglog.view->ids_enabled[trace.dbglog.id]) {
        // addr, regs, e, m, x, rt, out
        trace.str.quickempty();
        trace.str2.quickempty();
        dbglog_view *dv = trace.dbglog.view;
        u64 tc;
        if (!master_clock) tc = 0;
        else tc = *master_clock;


        if (regs.IR > 255) {
            switch(regs.IR) {
                case OP::RESET:
                    trace.str.sprintf("RESET");
                    break;
                case OP::IRQ:
                    trace.str.sprintf("IRQ");
                    break;
                case OP::NMI:
                    trace.str.sprintf("NMI");
                    break;
                default:
                    trace.str.sprintf("UKN %03x", regs.IR);
                    break;

            }
            dv->add_printf(trace.dbglog.id, tc, DBGLS_TRACE, "%s", trace.str.ptr);
            dv->extra_printf("%s", trace.str2.ptr);
            return;
        }
        ctxt ct;
        disassemble(trace.ins_PC, regs, regs.E, regs.P.M, regs.P.X, trace.strct, trace.str, &ct);
        pprint_context(&trace.str2);

        dv->add_printf(trace.dbglog.id, tc, DBGLS_TRACE, "%06x  %s", trace.ins_PC, trace.str.ptr);
        dv->extra_printf("%s", trace.str2.ptr);
    }
}

void core::irqdump(u32 nmi)
{
    printf("\nAT %s. PC:%06x  D:%04x  X:%04x  Y:%04x  S:%04x  E:%d  WAI:%d  cyc:%lld", nmi ? "NMI" : "IRQ", (regs.PBR << 16) | regs.PC, regs.D, regs.X, regs.Y, regs.S, regs.E, regs.WAI, *master_clock);
}

void core::cycle()
{
    int_clock++;
    if (regs.STP) return;

    regs.TCU++;
    if (regs.TCU == 1) {
        if (regs.NMI_pending || (regs.IRQ_pending && !regs.P.I)) {
            if (regs.NMI_pending) {
                regs.NMI_pending = 0;
                regs.IR = OP::NMI;
                DBG_EVENT(dbg.events.NMI);
                regs.interrupt_pending = regs.IRQ_pending;
            }
            else if (regs.IRQ_pending) {
                regs.IR = OP::IRQ;
                DBG_EVENT(dbg.events.IRQ);
            }
            regs.WAI = 0;
        }
        else {
            trace.ins_PC = (pins.BA << 16) | pins.Addr;
            regs.IR = pins.D;
            if ((regs.IR == 0) || ((trace.ins_PC >= 0x003113) && (trace.ins_PC < 0x003cff))) {
                printf("\nCRAP!");
                dbg_break("IR=0", *master_clock);
            }
            /*if (trace.ins_PC == 0x8B88D2) {
                dbg_break("BKPT!", *master_clock);
            }*/
        }
        regs.old_I = regs.P.I;
        ins = get_decoded_opcode();
        trace_format();
    }

    ins(regs, pins);
}

core::core(u64 *m_clock) : master_clock(m_clock) {
    dbg.events.IRQ = -1;
    dbg.events.NMI = -1;
    regs.S = 0x1FF;
}

void core::reset()
{
    pins.RES = 0;
    regs.RES_pending = 0;
    regs.TCU = 0;
    if (regs.STP) {
        regs.STP = 0;
    }
    else {
        pins.D = OP::RESET;
    }
}

void core::setup_tracing(jsm_debug_read_trace *strct)
{
    jsm_copy_read_trace(&trace.strct, strct);;
    trace.ok = 1;
}
}