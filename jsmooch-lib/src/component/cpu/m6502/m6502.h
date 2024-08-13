//
// Created by Dave on 2/4/2024.
//

#ifndef JSMOOCH_EMUS_M6502_H
#define JSMOOCH_EMUS_M6502_H

#include "helpers/int.h"
#include "helpers/debug.h"
#include "m6502_misc.h"
#include "helpers/debugger/debugger.h"

struct M6502_P {
    u32 C;
    u32 Z;
    u32 I;
    u32 D;
    u32 B;
    u32 V;
    u32 N;
};

void M6502_P_init(struct M6502_P *);
void M6502_regs_P_setbyte(struct M6502_P*, u32 val);
u32 M6502_regs_P_getbyte(struct M6502_P*);

struct M6502_regs {
    u32 A, X, Y;
    u32 PC, S;
    u32 old_I;
    struct M6502_P P;

    u32 TCU, IR;
    i32 TA, TR;

    u32 HLT;
    u32 IRQ_pending;
    u32 NMI_pending;
    u32 new_I;
    u32 WAI;
    u32 STP;
};

struct M6502_pins {
    u32 Addr;
    u32 D;
    u32 RW;

    u32 IRQ;
    u32 NMI;
    u32 RST;

    u32 RDY;
};

void M6502_regs_init(struct M6502_regs*);
void M6502_pins_init(struct M6502_pins*);

struct M6502 {
    struct M6502_regs regs;
    struct M6502_pins pins;

    u32 NMI_ack, NMI_old, IRQ_count;

    u32 PCO;

    struct {
        struct {
            struct cvec_ptr view;
            struct cvec_ptr IRQ, NMI;
        } event;
    } dbg;

    struct {
        u32 ok;
        u64 *cycles;
        u64 my_cycles;
        struct jsm_debug_read_trace strct;
    } trace;

    u32 first_reset;

    M6502_ins_func current_instruction;
    M6502_ins_func *opcode_table;
};

void M6502_init(struct M6502 *, M6502_ins_func *opcode_set);
void M6502_cycle(struct M6502*);
void M6502_reset(struct M6502*);
void M6502_setup_tracing(struct M6502*, struct jsm_debug_read_trace* dbg_read_trace, u64 *trace_cycles);

#endif //JSMOOCH_EMUS_M6502_H
