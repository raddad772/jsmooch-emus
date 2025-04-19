//
// Created by . on 4/16/25.
//

#include <stdio.h>
#include <string.h>
#include "wdc65816.h"

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

void WDC65816_set_IRQ_level(struct WDC65816 *this, u32 level)
{
    this->regs.IRQ_pending = level;
    if (level)
        this->regs.interrupt_pending = 1;
    else
        this->regs.interrupt_pending = this->regs.NMI_pending;
}

void WDC65816_set_NMI_level(struct WDC65816 *this, u32 level)
{
    if ((this->regs.NMI_old == 0) && level) { // 0->1
        this->regs.NMI_pending = 1;
        this->regs.interrupt_pending = 1;
    }
    this->regs.NMI_old = level;
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

void WDC65816_cycle(struct WDC65816* this)
{
    if (this->regs.STP) return;

    this->regs.TCU++;
    if (this->regs.TCU == 1) {
        if (this->regs.interrupt_pending) {
            if (this->regs.NMI_pending) {
                this->regs.IR = WDC65816_OP_NMI;
            }
            else if (this->regs.IRQ_pending) {
                if (this->regs.P.I) {
                    this->regs.WAI = 0;
                    printf("\nWAI exit because IRQ with I=1!");
                }
                else {
                    this->regs.IR = WDC65816_OP_IRQ;
                }
            }
        }
        else {
            this->trace.ins_PC = this->pins.Addr;
            this->regs.IR = this->pins.D;

        }
        this->regs.old_I = this->regs.P.I;
        this->ins = get_decoded_opcode(this);
        //ins_trace_format(this);
    }

    this->ins(&this->regs, &this->pins);
}

void WDC65816_init(struct WDC65816* this, u64 *master_clock)
{
    memset(this, 0, sizeof(*this));
    this->master_clock = master_clock;

    jsm_string_init(&this->trace.str, 1000);

    DBG_EVENT_VIEW_INIT;
}

void WDC65816_delete(struct WDC65816* this)
{
    jsm_string_delete(&this->trace.str);
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
