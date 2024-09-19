//
// Created by Dave on 2/4/2024.
//

#include "stdio.h"

#include "m6502.h"
#include "m6502_disassembler.h"
#include "nesm6502_opcodes.h"
#include "helpers/debug.h"



#define M6502_OP_RESET 0x100
#define M6502_OP_NMI 0x101
#define M6502_OP_IRQ 0x102

void M6502_P_init(struct M6502_P* this)
{
    this->C = this->Z = this->I = this->D = this->B = this->V = this->N = 0;
}

void M6502_regs_P_setbyte(struct M6502_P* this, u32 val) {
    this->C = val & 1;
    this->Z = (val & 0x02) >> 1;
    this->I = (val & 0x04) >> 2;
    this->D = (val & 0x08) >> 3;
    this->B = 1; // Confirmed via Visual6502
    this->V = (val & 0x40) >> 6;
    this->N = (val & 0x80) >> 7;
}

u32 M6502_regs_P_getbyte(struct M6502_P* this)
{
    return this->C | (this->Z << 1) | (this->I << 2) | (this->D << 3) | (this->B << 4) | 0x20 | (this->V << 6) | (this->N << 7);
}

void M6502_regs_init(struct M6502_regs* this)
{
    this->A = this->X = this->Y = 0;
    this->PC = this->S = this->old_I = 0;
    M6502_P_init(&this->P);
    this->TCU = this->IR = 0;
    this->TA = this->TR = 0;
    this->HLT = this->IRQ_pending = this->NMI_pending = 0;
    this->WAI = this->STP = 0;
}

void M6502_pins_init(struct M6502_pins* this)
{
    this->Addr = this->D = this->RW = 0;

    this->IRQ = this->NMI = 0;

    this->RST = 0;
    this->RDY = 0;
}

void M6502_init(struct M6502 *this, M6502_ins_func *opcode_table)
{
    M6502_pins_init(&this->pins);
    M6502_regs_init(&this->regs);
    this->opcode_table = opcode_table;
    this->current_instruction = opcode_table[0];

    this->NMI_ack = this->NMI_old = 0;

    this->trace.ok = 0;
    this->PCO = 0;
    this->trace.cycles = &this->trace.my_cycles;
    this->trace.my_cycles = 0;

    DBG_EVENT_VIEW_INIT;
    this->dbg.events.IRQ = -1;
    this->dbg.events.NMI = -1;

    this->IRQ_count = 0;
    this->first_reset = 1;
    this->trace.strct.ptr = NULL;
    this->trace.strct.read_trace = NULL;
}

static void M6502_power_on(struct M6502* this)
{
    // Initial values from Visual6502
    this->regs.A = 0xCC;
    this->regs.S = 0xFD;
    this->pins.D = 0x60;
    this->pins.RW = 0;
    this->pins.RDY = 0;
    this->regs.X = this->regs.Y = 0;
    this->regs.P.I = 1;
    this->regs.P.Z = 1;
    this->regs.PC = 0;
}

void M6502_reset(struct M6502* this) {
    this->pins.RST = 0;
    this->regs.TCU = 0;
    this->pins.RDY = 0;
    this->regs.P.B = 1;
    this->regs.P.D = 0;
    this->regs.P.I = 1;
    this->regs.WAI = 0;
    this->regs.STP = 0;
    if (this->first_reset) M6502_power_on(this);
    this->first_reset = 0;
    this->pins.RW = 0;
    this->pins.D = M6502_OP_RESET;
}

void M6502_setup_tracing(struct M6502* this, struct jsm_debug_read_trace *strct, u64 *trace_cycle_pointer)
{
    jsm_copy_read_trace(&this->trace.strct, strct);
    this->trace.ok = 1;
    this->trace.cycles = trace_cycle_pointer;
}

void M6502_disable_tracing(struct M6502* this)
{
}

void M6502_cycle(struct M6502* this)
{
    // Perform 1 processor cycle
    if (this->regs.HLT || this->regs.STP) return;
    if ((this->pins.IRQ) && (this->regs.P.I == 0)) {
        this->IRQ_count++;
        this->regs.IRQ_pending = this->IRQ_count >= 1 || this->regs.IRQ_pending;
    }
    else {
        this->IRQ_count = 0;
        this->regs.IRQ_pending = false;
    }

    // Edge-sensitive 0->1
    if (this->pins.NMI != this->NMI_old) {
        if (this->pins.NMI == 1) this->regs.NMI_pending = true;
        this->NMI_ack = false;
        this->NMI_old = this->pins.NMI;
    }

    this->regs.TCU++;
    if (this->regs.TCU == 1) { // T0
        this->PCO = this->pins.Addr; // Capture PC before it runs away
        this->regs.IR = this->pins.D;
        if (this->regs.NMI_pending && !this->NMI_ack) {
            this->NMI_ack = true;
            this->regs.NMI_pending = false;
            this->regs.IR = M6502_OP_NMI;
            DBG_EVENT(this->dbg.events.NMI);
            if (dbg.brk_on_NMIRQ) {
                dbg_break("M6502 NMI", *this->trace.cycles);
            }
        } else if (this->regs.IRQ_pending) {
            this->regs.IRQ_pending = false;
            this->regs.IR = M6502_OP_IRQ;
            DBG_EVENT(this->dbg.events.IRQ);
            if (dbg.brk_on_NMIRQ) {
                dbg_break("M6502 IRQ", *this->trace.cycles);
            }
        }
        this->current_instruction = this->opcode_table[this->regs.IR];
        if (this->current_instruction == this->opcode_table[2]) { // TODO: this doesn't work with illegal opcodes or m65c02
            printf("\nINVALID OPCODE %02x", this->regs.IR);
        }
    }
    this->NMI_ack = false;

    this->current_instruction(&this->regs, &this->pins);
    this->trace.my_cycles++;
}