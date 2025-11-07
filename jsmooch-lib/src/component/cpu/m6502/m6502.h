//
// Created by Dave on 2/4/2024.
//

#ifndef JSMOOCH_EMUS_M6502_H
#define JSMOOCH_EMUS_M6502_H

#include "helpers_c/int.h"
#include "helpers_c/debug.h"
#include "helpers_c/debugger/debuggerdefs.h"

#include "m6502_misc.h"

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
    struct M6502_P P;

    u32 TCU, IR;
    i32 TA, TR;

    u32 HLT;
    u32 do_IRQ;
    u32 WAI;
    u32 STP;

    u32 NMI_old, NMI_level_detected, do_NMI;
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


    u32 PCO;

    DBG_EVENT_VIEW_ONLY_START
    IRQ, NMI
    DBG_EVENT_VIEW_ONLY_END

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
void M6502_poll_IRQs(struct M6502_regs *regs, struct M6502_pins *pins);
void M6502_poll_NMI_only(struct M6502_regs *regs, struct M6502_pins *pins);
struct serialized_state;
void M6502_serialize(struct M6502*, struct serialized_state *state);
void M6502_deserialize(struct M6502*, struct serialized_state *state);

#endif //JSMOOCH_EMUS_M6502_H
