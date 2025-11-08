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

void M6502_P::setbyte(u8 val) {
    C = val & 1;
    Z = (val & 0x02) >> 1;
    I = (val & 0x04) >> 2;
    D = (val & 0x08) >> 3;
    B = 1; // Confirmed via Visual6502
    V = (val & 0x40) >> 6;
    N = (val & 0x80) >> 7;
}

u8 M6502_P::getbyte() const
{
    return C | (Z << 1) | (I << 2) | (D << 3) | (B << 4) | 0x20 | (V << 6) | (N << 7);
}

M6502::M6502(M6502_ins_func *opcode_table) : opcode_table(opcode_table) {
    current_instruction = opcode_table[0];
    trace.cycles = &trace.my_cycles;

    dbg.events.IRQ = -1;
    dbg.events.NMI = -1;
}

void M6502::power_on()
{
    // Initial values from Visual6502
    regs.A = 0xCC;
    regs.S = 0xFD;
    pins.D = 0x60;
    pins.RW = 0;
    pins.RDY = 0;
    regs.X = regs.Y = 0;
    regs.P.I = 1;
    regs.P.Z = 1;
    regs.PC = 0;
}

void M6502::reset() {
    pins.RST = 0;
    regs.TCU = 0;
    pins.RDY = 0;
    regs.P.B = 1;
    regs.P.D = 0;
    regs.P.I = 1;
    regs.WAI = 0;
    regs.STP = 0;
    if (first_reset) power_on();
    first_reset = 0;
    pins.RW = 0;
    pins.D = M6502_OP_RESET;
}

void M6502::setup_tracing(struct jsm_debug_read_trace *strct, u64 *trace_cycle_pointer)
{
    jsm_copy_read_trace(&trace.strct, strct);
    trace.ok = 1;
    trace.cycles = trace_cycle_pointer;
}

void M6502::cycle()
{
    // Perform 1 processor cycle
    if (regs.HLT || regs.STP) return;

    // Edge-sensitive 0->1
    if (pins.NMI != regs.NMI_old) {
        if (pins.NMI == 1) regs.NMI_level_detected = 1;
        regs.NMI_old = pins.NMI;
    }

    regs.TCU++;
    if (regs.TCU == 1) { // T0, instruction decode
        PCO = pins.Addr; // Capture PC before it runs away
        regs.IR = pins.D;
        if (regs.do_NMI) {
            regs.do_NMI = false;
            regs.IR = M6502_OP_NMI;
            DBG_EVENT(dbg.events.NMI);
            if (::dbg.brk_on_NMIRQ) {
                dbg_break("M6502 NMI", *trace.cycles);
            }
        } else if (regs.do_IRQ) {
            regs.do_IRQ = false;
            regs.IR = M6502_OP_IRQ;
            DBG_EVENT(dbg.events.IRQ);
            if (::dbg.brk_on_NMIRQ) {
                dbg_break("M6502 IRQ", *trace.cycles);
            }
        }
        current_instruction = opcode_table[regs.IR];
        if (current_instruction == opcode_table[2]) { // TODO: this doesn't work with illegal opcodes or m65c02
            printf("\nINVALID OPCODE %02x", regs.IR);
        }
    }

    current_instruction(&regs, &pins);
    trace.my_cycles++;
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

#define S(x) Sadd(state, & x, sizeof( x))
void M6502_regs::serialize(struct serialized_state &state) {
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

void M6502_pins::serialize(struct serialized_state &state) {
    S(Addr);
    S(D);
    S(RW);
    S(IRQ);
    S(NMI);
    S(RST);
    S(RDY);
}

void M6502::serialize(struct serialized_state &state)
{
    regs.serialize(state);
    pins.serialize(state);
    S(PCO);
    S(first_reset);
}
#undef S

#define L(x) Sload(state, & x, sizeof( x))

void M6502_regs::deserialize(struct serialized_state &state) {
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

void M6502_pins::deserialize(struct serialized_state &state) {
    L(Addr);
    L(D);
    L(RW);
    L(IRQ);
    L(NMI);
    L(RST);
    L(RDY);
}

void M6502::deserialize(struct serialized_state &state)
{
    regs.deserialize(state);
    pins.deserialize(state);
    L(PCO);
    L(first_reset);
    current_instruction = opcode_table[regs.IR];
}

