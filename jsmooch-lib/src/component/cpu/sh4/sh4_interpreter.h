//
// Created by Dave on 2/10/2024.
//

#ifndef JSMOOCH_EMUS_SH4_INTERPRETER_H
#define JSMOOCH_EMUS_SH4_INTERPRETER_H

#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/scheduler.h"

// One of these for external mem system to write/read registers here,
// One for this to read/write main memory
struct SH4_memaccess_t {
    void *ptr;
    u64 (*read)(void*,u32,u32,u32*);
    void (*write)(void*,u32,u64,u32,u32*);
};


struct SH4_FV {
    float a, b, c, d;
};

struct SH4_mtx {
    float xf0, xf1, xf2, xf3;
    float xf4, xf5, xf6, xf7;
    float xf8, xf9, xf10, xf11;
    float xf12, xf13, xf14, xf15;
};

struct SH4_regs_SR {
    u32 MD; // bit 30. 1 = privileged modeRB, BL, FD, M, Q, IMASK, S, T;
    u32 RB; //     29. register bank select (privileged only)
    u32 BL; //     28. Exception/interrupt bit
    u32 FD; //     15. FPU disable
    u32 M, Q; //  9,8. Used in DIV instructions
    u32 IMASK; // 7-4. Interrupt mas bits
    u32 S; //       1. Saturation for MAC instruction
    u32 T; //       0. True/false or carry/borrow flag
    u32 data; // other bits
};

struct SH4_regs_FPSCR {
    u32 FR; //      21. Register bank select
    u32 SZ; //      20. Size-transfer mode. 0=FMOV is 32-bit, 1=FMOV is 64-bit
    u32 PR; //      19. Precision mode. 0 = single precision, 1 = double
    u32 DN; //      18. Denormalization mode. 0 = do it, 1 = treat it as 0
    u32 Cause; //17-12. exception cause     FPU error(E), Invalid Op(V), /0(Z), Overflow(O), Underflow(U), Inexact(I)
    u32 Enable;// 11-7. exception enable            None, Invalid Op(V), /0(Z), Overflow(O), Underflow(U), Inexact(I)
    u32 Flag;   // 6-2. exception flag              None, Invalid Op(V), /0(Z), Overflow(O), Underflow(U), Inexact(I)
    u32 RM;     // 1-0. Rounding mode. 0=Nearest, 1=Zero, 2=Reserved, 3=Reserved
    u32 data;  // other bits
};

void SH4_regs_SR_reset(struct SH4_regs_SR* this);
u32 SH4_regs_SR_get(struct SH4_regs_SR* this);

void SH4_regs_FPSCR_reset(struct SH4_regs_FPSCR* this);

struct SH4_regs {
    u32 R[16]; // registers
    u32 R_[8]; // shadow registers

    // -- accessed any time
    u32 GBR; // Global Bank Register
    struct SH4_regs_SR SR;

    // -- accessed in privileged
    u32 SSR; // Saved Status Register
    u32 SPC; // Saved PC
    u32 VBR; // Vector Base Register
    u32 SGR; // Saved reg15
    u32 DBR; // Debug base register

    // System registers, any mode
    u32 MACL; // MAC-lo
    u32 MACH; // MAC-hi
    u32 PR; // Procedure register
    u32 PC; // Program Counter
    struct SH4_regs_FPSCR FPSCR; // Floating Point Status Control Register
    union {
        float f;
        u32 u;
    } FPUL;

    union {
        u32 U32[16];
        u64 U64[8];
        float FP32[16];
        float FP64[8];
        struct SH4_FV FV[4];
        struct SH4_mtx MTX;
    } fb[3];

    u32 QACR[2];  // 0xFF000038 + 3C
    // Emulator-internal registers
    u32 currently_banked_rb;
#include "generated/regs_decls.h"
};

u32 SH4_regs_FPSCR_get(struct SH4_regs_FPSCR* this);
void SH4_regs_FPSCR_set(struct SH4_regs* this, u32 val);
void SH4_regs_FPSCR_bankswitch(struct SH4_regs* this);

struct SH4_tmu_int_struct {
    struct SH4* sh4;
    u32 ch;
};

struct SH4_clock {
    i64 trace_cycles;
    i64 timer_cycles;
    i64 trace_cycles_blocks;
};

struct SH4 {
    struct SH4_regs regs;
    struct SH4_clock clock;

    u32 IRL_irq_level;

    i32 cycles;

    i32 pp_last_m;
    i32 pp_last_n;
    u8 SQ[2][32]; // store queues!
    u8 OC[8 * 1024]; // Operand Cache!

    struct {
        u32 TOCR;
        u32 TSTR;
        u32 TCOR[3];
        u32 TCNT[3];
        u32 TCR[3];
        u32 shift[3];
        u32 old_mode[3];
        u32 base[3];
        u64 base64[3];
        u32 mask[3];
        u64 mask64[3];

        struct SH4_tmu_int_struct scheduled_func_structs[3];
        struct scheduled_bound_function scheduled_funcs[3];
    } tmu;

    void *mptr;
    u32 (*fetch_ins)(void*,u32);
    u64 (*read)(void*,u32,u32);
    void (*write)(void*,u32,u64,u32);

    struct jsm_string console;

    struct scheduler_t* scheduler;
};

void SH4_init(struct SH4* this, struct scheduler_t* scheduler);
void SH4_delete(struct SH4* this);
void SH4_reset(struct SH4* this);
void SH4_run_cycles(struct SH4* this, u32 howmany);
void SH4_fetch_and_exec(struct SH4* this, u32 is_delay_slot);
void SH4_SR_set(struct SH4* this, u32 val);
void SH4_set_IRL_irq_level(struct SH4* this, u32 level);
void SH4_give_memaccess(struct SH4* this, struct SH4_memaccess_t* to);
void SH4_interrupt_pend(struct SH4* this, u32 level, u32 onoff);
void SH4_interrupt_mask(struct SH4* this, u32 level, u32 onoff);

#endif //JSMOOCH_EMUS_SH4_INTERPRETER_H
