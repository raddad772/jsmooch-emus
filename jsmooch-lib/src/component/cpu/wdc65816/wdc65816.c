//
// Created by . on 4/16/25.
//


#include <string.h>
#include "wdc65816.h"

extern WDC65816_ins_func wdc65816_decoded_opcodes[8][0x104];

WDC65816_ins_func get_decoded_opcode(struct WDC65816 *this)
{
    if (this->regs.E) {
        return wdc65816_decoded_opcodes[7][this->regs.IR];
    }
    u32 flag = (this->regs.P.M << 1) | (this->regs.P.X << 2);
    WDC65816_ins_func ret = wdc65816_decoded_opcodes[flag][this->regs.IR];
    if (ret == NULL) ret = wdc65816_decoded_opcodes[0][this->regs.IR];
    return ret;
}

void WDC65816_cycle(struct WDC65816* this)
{
    if (this->regs.STP) return;

    this->regs.TCU++;
    if (this->regs.TCU == 1) {
        this->trace.ins_PC = this->pins.Addr;
        this->regs.IR = this->pins.D;


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
