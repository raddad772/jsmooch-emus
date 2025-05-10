//
// Created by . on 4/16/25.
//

#include <stdio.h>
#include <string.h>
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

extern WDC65816_ins_func wdc65816_decoded_opcodes[5][0x104];

static void eval_WAI(struct WDC65816 *this)
{
    if (this->regs.interrupt_pending) this->regs.WAI = 0;
}

void WDC65816_set_IRQ_level(struct WDC65816 *this, u32 level)
{
    this->regs.IRQ_pending = level;
    if (level)
        this->regs.interrupt_pending = 1;
    else
        this->regs.interrupt_pending = this->regs.NMI_pending;
    eval_WAI(this);
}

void WDC65816_set_NMI_level(struct WDC65816 *this, u32 level)
{
    if ((this->regs.NMI_old == 0) && level) { // 0->1
        this->regs.NMI_pending = 1;
        this->regs.interrupt_pending = 1;
    }
    this->regs.NMI_old = level;
    eval_WAI(this);
}

static WDC65816_ins_func get_decoded_opcode(struct WDC65816 *this)
{
    if (this->regs.E) {
        return wdc65816_decoded_opcodes[4][this->regs.IR];
    }
    u32 flag = (this->regs.P.M) | (this->regs.P.X << 1);

    WDC65816_ins_func ret = wdc65816_decoded_opcodes[flag][this->regs.IR];
    if (ret == NULL) ret = wdc65816_decoded_opcodes[0][this->regs.IR];
    return ret;
}

static void pprint_context(struct WDC65816 *this, struct jsm_string *out)
{
    jsm_string_sprintf(out, "%c%c  A:%04x  D:%04x  X:%04x  Y:%04x  DBR:%02x  PBR:%02x  S:%04x, P:%c%c%c%c%c%c",
        this->regs.P.M ? 'M' : 'm',
        this->regs.P.X ? 'X' : 'x',
        this->regs.C, this->regs.D,
        this->regs.X, this->regs.Y,
        this->regs.DBR, this->regs.PBR,
        this->regs.S,
        this->regs.P.C ? 'C' : 'c',
        this->regs.P.Z ? 'Z' : 'z',
        this->regs.P.I ? 'I' : 'i',
        this->regs.P.D ? 'D' : 'd',
        this->regs.P.V ? 'V' : 'v',
        this->regs.P.N ? 'N' : 'n'
        );
}

static void wdc_trace_format(struct WDC65816 *this)
{
    if (this->trace.dbglog.view && this->trace.dbglog.view->ids_enabled[this->trace.dbglog.id]) {
        // addr, regs, e, m, x, rt, out
        jsm_string_quickempty(&this->trace.str);
        jsm_string_quickempty(&this->trace.str2);
        struct dbglog_view *dv = this->trace.dbglog.view;
        u64 tc;
        if (!this->master_clock) tc = 0;
        else tc = *this->master_clock;


        if (this->regs.IR > 255) {
            switch(this->regs.IR) {
                case WDC65816_OP_RESET:
                    jsm_string_sprintf(&this->trace.str, "RESET");
                    break;
                case WDC65816_OP_IRQ:
                    jsm_string_sprintf(&this->trace.str, "IRQ");
                    break;
                case WDC65816_OP_NMI:
                    jsm_string_sprintf(&this->trace.str, "NMI");
                    break;
                default:
                    jsm_string_sprintf(&this->trace.str, "UKN %03x", this->regs.IR);
                    break;

            }
            dbglog_view_add_printf(dv, this->trace.dbglog.id, tc, DBGLS_TRACE, "%s", this->trace.str.ptr);
            dbglog_view_extra_printf(dv, "%s", this->trace.str2.ptr);
            return;
        }
        struct WDC65816_ctxt ct;
        WDC65816_disassemble(this->trace.ins_PC, &this->regs, this->regs.E, this->regs.P.M, this->regs.P.X, &this->trace.strct, &this->trace.str, &ct);
        pprint_context(this, &this->trace.str2);

        dbglog_view_add_printf(dv, this->trace.dbglog.id, tc, DBGLS_TRACE, "%06x  %s", this->trace.ins_PC, this->trace.str.ptr);
        dbglog_view_extra_printf(dv, "%s", this->trace.str2.ptr);
    }
}

static void irqdump(struct WDC65816 *this, u32 nmi)
{
    printf("\nAT %s. PC:%06x  D:%04x  X:%04x  Y:%04x  S:%04x  E:%d  WAI:%d  cyc:%lld", nmi ? "NMI" : "IRQ", (this->regs.PBR << 16) | this->regs.PC, this->regs.D, this->regs.X, this->regs.Y, this->regs.S, this->regs.E, this->regs.WAI, *this->master_clock);
}

void WDC65816_cycle(struct WDC65816* this)
{
    if (this->regs.STP) return;

    this->regs.TCU++;
    if (this->regs.TCU == 1) {
        if (this->regs.NMI_pending || (this->regs.IRQ_pending && !this->regs.P.I)) {
            if (this->regs.NMI_pending) {
                this->regs.NMI_pending = 0;
                this->regs.IR = WDC65816_OP_NMI;
                //irqdump(this, 1);
                this->regs.interrupt_pending = this->regs.IRQ_pending;
            }
            else if (this->regs.IRQ_pending) {
                this->regs.IR = WDC65816_OP_IRQ;
                //irqdump(this, 0);
            }
            this->regs.WAI = 0;
        }
        else {
            this->trace.ins_PC = (this->pins.BA << 16) | this->pins.Addr;
            this->regs.IR = this->pins.D;
            if ((this->regs.IR == 0) || ((this->trace.ins_PC >= 0x003113) && (this->trace.ins_PC < 0x003cff))) {
                printf("\nCRAP!");
                dbg_break("IR=0", *this->master_clock);
            }
        }
        this->regs.old_I = this->regs.P.I;
        this->ins = get_decoded_opcode(this);
        wdc_trace_format(this);
    }

    this->ins(&this->regs, &this->pins);
}

void WDC65816_init(struct WDC65816* this, u64 *master_clock)
{
    memset(this, 0, sizeof(*this));
    this->master_clock = master_clock;

    jsm_string_init(&this->trace.str, 1000);
    jsm_string_init(&this->trace.str2, 200);

    DBG_EVENT_VIEW_INIT;
}

void WDC65816_delete(struct WDC65816* this)
{
    jsm_string_delete(&this->trace.str);
    jsm_string_delete(&this->trace.str2);
}

void WDC65816_reset(struct WDC65816* this)
{
    this->pins.RES = 0;
    this->regs.RES_pending = 0;
    this->regs.TCU = 0;
    if (this->regs.STP) {
        this->regs.STP = 0;
    }
    else {
        this->pins.D = WDC65816_OP_RESET;
    }
}

void WDC65816_setup_tracing(struct WDC65816* this, struct jsm_debug_read_trace *strct)
{
    this->trace.strct.read_trace_m68k = strct->read_trace_m68k;
    this->trace.strct.ptr = strct->ptr;
    this->trace.strct.read_trace = strct->read_trace;
    this->trace.ok = 1;
}
