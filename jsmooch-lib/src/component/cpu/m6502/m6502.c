//
// Created by Dave on 2/4/2024.
//

#include "stdio.h"

#include "m6502.h"
#include "m6502_disassembler.h"
#include "helpers/debug.h"
#include "helpers/serialize/serialize.h"

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
    this->PC = this->S = 0;
    M6502_P_init(&this->P);
    this->TCU = this->IR = 0;
    this->TA = this->TR = 0;
    this->HLT = this->do_IRQ = this->do_NMI = 0;
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

    this->regs.NMI_old = this->regs.do_NMI = this->regs.do_IRQ = 0;

    this->trace.ok = 0;
    this->PCO = 0;
    this->trace.cycles = &this->trace.my_cycles;
    this->trace.my_cycles = 0;

    DBG_EVENT_VIEW_INIT;
    this->dbg.events.IRQ = -1;
    this->dbg.events.NMI = -1;

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

void M6502_cycle(struct M6502* this)
{
    // Perform 1 processor cycle
    if (this->regs.HLT || this->regs.STP) return;

    // Edge-sensitive 0->1
    if (this->pins.NMI != this->regs.NMI_old) {
        if (this->pins.NMI == 1) this->regs.NMI_level_detected = 1;
        this->regs.NMI_old = this->pins.NMI;
    }

    this->regs.TCU++;
    if (this->regs.TCU == 1) { // T0, instruction decode
        this->PCO = this->pins.Addr; // Capture PC before it runs away
        this->regs.IR = this->pins.D;
        if (this->regs.do_NMI) {
            this->regs.do_NMI = false;
            this->regs.IR = M6502_OP_NMI;
            DBG_EVENT(this->dbg.events.NMI);
            if (dbg.brk_on_NMIRQ) {
                dbg_break("M6502 NMI", *this->trace.cycles);
            }
        } else if (this->regs.do_IRQ) {
            this->regs.do_IRQ = false;
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

    this->current_instruction(&this->regs, &this->pins);
    this->trace.my_cycles++;
}

void M6502_poll_NMI_only(struct M6502_regs *regs, struct M6502_pins *pins)
{
    if (regs->NMI_level_detected) {
        regs->do_NMI = 1;
        regs->NMI_level_detected = 0;
    }
}

// Poll during second-to-last cycle
void M6502_poll_IRQs(struct M6502_regs *regs, struct M6502_pins *pins)
{
    if (regs->NMI_level_detected) {
        regs->do_NMI = 1;
        regs->NMI_level_detected = 0;
    }

    regs->do_IRQ = pins->IRQ && !regs->P.I;
}

#define S(x) Sadd(state, &this-> x, sizeof(this-> x))
static void serialize_regs(struct M6502_regs *this, struct serialized_state *state) {
    S(A);
    S(X);
    S(Y);
    S(PC);
    S(S);
    S(P.C);
    S(P.Z);
    S(P.I);
    S(P.D);
    S(P.B);
    S(P.V);
    S(P.N);
    S(TCU);
    S(IR);
    S(TA);
    S(TR);
    S(HLT);
    S(do_IRQ);
    S(WAI);
    S(STP);
    S(NMI_old);
    S(NMI_level_detected);
    S(do_NMI);
}

static void serialize_pins(struct M6502_pins *this, struct serialized_state *state) {
    S(Addr);
    S(D);
    S(RW);
    S(IRQ);
    S(NMI);
    S(RST);
    S(RDY);
}

void M6502_serialize(struct M6502 *this, struct serialized_state *state)
{
    serialize_regs(&this->regs, state);
    serialize_pins(&this->pins, state);
    S(PCO);
    S(first_reset);
}
#undef S

#define L(x) Sload(state, &this-> x, sizeof(this-> x))

static void deserialize_regs(struct M6502_regs *this, struct serialized_state *state) {
    L(A);
    L(X);
    L(Y);
    L(PC);
    L(S);
    L(P.C);
    L(P.Z);
    L(P.I);
    L(P.D);
    L(P.B);
    L(P.V);
    L(P.N);
    L(TCU);
    L(IR);
    L(TA);
    L(TR);
    L(HLT);
    L(do_IRQ);
    L(WAI);
    L(STP);
    L(NMI_old);
    L(NMI_level_detected);
    L(do_NMI);
}

static void deserialize_pins(struct M6502_pins *this, struct serialized_state *state) {
    L(Addr);
    L(D);
    L(RW);
    L(IRQ);
    L(NMI);
    L(RST);
    L(RDY);
}

void M6502_deserialize(struct M6502 *this, struct serialized_state *state)
{
    deserialize_regs(&this->regs, state);
    deserialize_pins(&this->pins, state);
    L(PCO);
    L(first_reset);
    this->current_instruction = this->opcode_table[this->regs.IR];
}

