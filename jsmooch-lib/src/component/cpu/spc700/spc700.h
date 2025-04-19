//
// Created by . on 4/16/25.
//

#ifndef JSMOOCH_EMUS_SPC700_H
#define JSMOOCH_EMUS_SPC700_H

#include "helpers/int.h"

#include "helpers/debugger/debuggerdefs.h"
#include "helpers/debugger/debugger.h"
#include "helpers/debug.h"


union SPC700_regs_P {
    struct {
        u32 C : 1;
        u32 Z : 1;
        u32 I : 1;
        u32 H : 1;
        u32 B : 1;
        u32 P : 1;
        u32 V : 1;
        u32 N : 1;
        u32 DO : 16;
    };
    u32 v;
};

static inline u8 SPC700_P_getbyte(union SPC700_regs_P *P)
{
    return P->v & 0xFF;
}

static inline void SPC700_P_setbyte(union SPC700_regs_P *P, u8 val)
{
    P->v = val;
    P->DO = P->P << 8;
}

struct SPC700_regs {
    union SPC700_regs_P P;

    u32 IR;
    i32 TR, TA, TA2;

    i32 opc_cycles;

    u8 A, X, Y;
    u16 SP, PC;
};

struct SNES_clock;

struct SPC700 {
    struct SPC700_regs regs;

    void *read_ptr, *write_ptr;
    u8 (*read_apu)(void *, u16 addr);
    void (*write_apu)(void *, u16 addr, u8 val);

    struct SNES_clock *clock;

    i32 cycles;
    u32 WAI, STP;


    struct {
        u32 ROM_readable;
        u32 T0_enable, T1_enable, T2_enable;
        u16 DSPADDR, DSPDATA;

        u8 CPUI[4];
        u8 CPUO[4];
    } io;

    struct {
        i32 divider, counter;
        struct {
            u32 target, out;
        };
    } timers[3];

    struct {
        struct jsm_debug_read_trace strct;
        struct jsm_string str;
        u32 ins_PC;
        i32 source_id;

        struct {
            struct dbglog_view *view;
            u32 id;
        } dbglog;

    } trace;

    u8 RAM[0x10000];

    DBG_START
        DBG_EVENT_VIEW
        DBG_TRACE_VIEW
    DBG_END

};

typedef void (*SPC700_ins_func)(struct SPC700 *);

void SPC700_init(struct SPC700 *, struct SNES_clock *clock);
void SPC700_reset(struct SPC700 *);
void SPC700_cycle(struct SPC700 *this, i64 how_many);

u8 SPC700_read8(struct SPC700 *, u32 addr);
u8 SPC700_read8D(struct SPC700 *, u32 addr);
void SPC700_write8(struct SPC700 *, u32 addr, u32 val);
void SPC700_write8D(struct SPC700 *, u32 addr, u32 val);

#endif //JSMOOCH_EMUS_SPC700_H
