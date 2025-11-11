//
// Created by Dave on 2/7/2024.
//

#ifndef JSMOOCH_EMUS_Z80_H
#define JSMOOCH_EMUS_Z80_H

#include "helpers/cvec.h"
#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/debugger/debuggerdefs.h"
#include "helpers/serialize/serialize.h"

#define Z80_S_IRQ 0x100
#define Z80_S_RESET 0x101
#define Z80_S_DECODE 0x102
#define Z80_MAX_OPCODE 0x101

struct Z80_regs_F {
    u32 S, Z, Y, H, X, PV, N, C;
};

void Z80_regs_F_init(Z80_regs_F*);
void Z80_regs_F_setbyte(Z80_regs_F*, u32 val);
u32 Z80_regs_F_getbyte(Z80_regs_F*);

enum Z80P {
  Z80P_HL,
  Z80P_IX,
  Z80P_IY
};

struct Z80_regs {
    u16 reset_vector;
    u32 IR; // Instruction Register
    u32 TCU; // Internal instruction cycle timer register (not on real Z80 under this name)

    u32 A;
    u32 B;
    u32 C;
    u32 D;
    u32 E;
    u32 H;
    u32 L;
    struct Z80_regs_F F;
    u32 I; // Iforget
    u32 R; // Refresh counter

    // Shadow registers
    u32 AF_;
    u32 BC_;
    u32 DE_;
    u32 HL_;

    // Junk calculations
    u32 TR, TA;

    // Temps for register swapping
    u32 AFt;
    u32 BCt;
    u32 DEt;
    u32 HLt;
    u32 Ht;
    u32 Lt;

    // 16-bit registers
    u32 PC;
    u32 SP;
    u32 IX;
    u32 IY;

    u32 t[10];
    u32 WZ;
    u32 EI;
    u32 P;
    u32 Q;
    u32 IFF1;
    u32 IFF2;
    u32 IM;
    u32 HALT;

    u32 data;

    i32 IRQ_vec;
    enum Z80P rprefix;
    u32 prefix;

    u32 poll_IRQ;
};

void Z80_regs_init(Z80_regs*);
void Z80_regs_exchange_shadow_af(Z80_regs*);
void Z80_regs_exchange_de_hl(Z80_regs*);
void Z80_regs_exchange_shadow(Z80_regs*);

struct Z80_pins {
    u32 Addr;
    u32 D;

    u32 IRQ_maskable;
    u32 RD, WR, IO, MRQ;

    u32 M1, WAIT; // M1 pin
};

void Z80_pins_init(Z80_pins*);

typedef void (*Z80_ins_func)(Z80_regs*, Z80_pins*);

struct Z80 {
    struct Z80_regs regs;
    struct Z80_pins pins;
    u32 CMOS;
    u32 IRQ_pending;
    u32 NMI_pending;
    u32 NMI_ack;

    struct {
        u32 ok;
        u64 *cycles;
        u64 last_cycle;
        u64 my_cycles;
    } trace;

    Z80_ins_func current_instruction;

    struct jsm_debug_read_trace read_trace;

    DBG_EVENT_VIEW_ONLY_START
    IRQ, NMI
    DBG_EVENT_VIEW_ONLY_END

    u32 PCO;
};

void Z80_init(Z80*, u32 CMOS);
u32 Z80_parity(u32 val);
void Z80_reset(Z80*);
void Z80_cycle(Z80*);
void Z80_notify_NMI(Z80*, u32 level);
void Z80_notify_IRQ(Z80*, u32 level);
void Z80_setup_tracing(Z80*, jsm_debug_read_trace* dbg_read_trace, u64 *trace_cycle_pointer);
void Z80_serialize(Z80*, serialized_state *state);
void Z80_deserialize(Z80*, serialized_state *state);
void Z80_trace_format(Z80 *);
void Z80_printf_trace(Z80* this);

#endif //JSMOOCH_EMUS_Z80_H
