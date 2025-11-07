//
// Created by . on 2/11/25.
//

#ifndef JSMOOCH_EMUS_R3000_H
#define JSMOOCH_EMUS_R3000_H

#include "helpers_c/debugger/debuggerdefs.h"
#include "helpers_c/debugger/debugger.h"
#include "helpers_c/better_irq_multiplexer.h"
#include "helpers_c/int.h"
#include "helpers_c/debug.h"
#include "r3000_multiplier.h"
#include "helpers_c/scheduler.h"
#include "gte.h"

struct R3000_regs {
    u32 R[32];
    u32 COP0[32];
    u32 PC;
};

enum R3000_MN {
    R3000_MN_SPECIAL = 0, R3000_MN_BcondZ = 1, R3000_MN_J = 2, R3000_MN_JAL = 3, R3000_MN_BEQ = 4,
    R3000_MN_BNE = 5, R3000_MN_BLEZ = 6, R3000_MN_BGTZ = 7, R3000_MN_ADDI = 8, R3000_MN_ADDIU = 9,
    R3000_MN_SLTI = 10, R3000_MN_SLTIU = 11, R3000_MN_ANDI = 12, R3000_MN_ORI = 13, R3000_MN_XORI = 14,
    R3000_MN_LUI = 15, R3000_MN_COP0 = 16, R3000_MN_COP1 = 17, R3000_MN_COP2 = 18, R3000_MN_COP3 = 19,
    R3000_MN_LB = 20, R3000_MN_LH = 21, R3000_MN_LWL = 22, R3000_MN_LW = 23, R3000_MN_LBU = 24,
    R3000_MN_LHU = 25, R3000_MN_LWR = 26, R3000_MN_SB = 27, R3000_MN_SH = 28, R3000_MN_SWL = 29,
    R3000_MN_SW = 30, R3000_MN_SWR = 31, R3000_MN_SWC0 = 32, R3000_MN_SWC1 = 33, R3000_MN_SWC2 = 34,
    R3000_MN_SWC3 = 35, R3000_MN_SLL = 36, R3000_MN_SRL = 37, R3000_MN_SRA = 38, R3000_MN_SLLV = 39,
    R3000_MN_SRLV = 40, R3000_MN_SRAV = 41, R3000_MN_JR = 42, R3000_MN_JALR = 43, R3000_MN_SYSCALL = 44,
    R3000_MN_BREAK = 45, R3000_MN_MFHI = 46, R3000_MN_MTHI = 47, R3000_MN_MFLO = 48, R3000_MN_MTLO = 49,
    R3000_MN_MULT = 50, R3000_MN_MULTU = 51, R3000_MN_DIV = 52, R3000_MN_DIVU = 53, R3000_MN_ADD = 54,
    R3000_MN_ADDU = 55, R3000_MN_SUB = 56, R3000_MN_SUBU = 57, R3000_MN_AND = 58, R3000_MN_OR = 59,
    R3000_MN_XOR = 60, R3000_MN_NOR = 61, R3000_MN_SLT = 62, R3000_MN_SLTU = 63, R3000_MN_NA = 64,
    R3000_MN_LWCx = 65, R3000_MN_SWCx = 66, R3000_MN_COPx = 67, R3000_MN_MFC = 68, R3000_MN_CFC = 69,
    R3000_MN_MTC = 70, R3000_MN_CTC = 71, R3000_MN_BCF = 72, R3000_MN_BCT = 73, R3000_MN_COPimm = 74,
    R3000_MN_RFE = 75
};

struct R3000;
struct R3000_opcode;
typedef void (*R3000_ins)(u32 opcode, struct R3000_opcode *op, struct R3000 *core);

struct R3000_opcode {
    enum R3000_MN mn;
    u32 opcode;
    R3000_ins func;
    i32 arg;
};

struct R3000_pipeline_item {
    i32 target;
    u32 value;
    u32 opcode, new_PC, addr;
    struct R3000_opcode *op;
};

struct R3000_pipeline {
    u32 base, num_items;
    struct R3000_pipeline_item current, empty_item, item0, item1;
};

struct R3000 {
    struct R3000_regs regs;
    struct R3000_pipeline pipe;
    struct R3000_multiplier multiplier;
    struct jsm_string console;

    struct scheduler_t *scheduler;
    u64 *clock;
    u64 *waitstates;

    struct {
        u32 IRQ;
    } pins;

    struct {
        struct jsm_debug_read_trace strct;
        struct jsm_string str;
        u32 ok;
        u32 ins_PC;
        i32 source_id;
    } trace;

    struct {
        struct IRQ_multiplexer_b *I_STAT;
        u32 I_MASK;
    } io;

    struct R3000_GTE gte;

    struct R3000_opcode decode_table[0x7F];

    void *fetch_ptr, *read_ptr, *write_ptr, *update_sr_ptr;
    u32 (*fetch_ins)(void *ptr, u32 addr, u32 sz);
    u32 (*read)(void *ptr, u32 addr, u32 sz, u32 has_effect);
    void (*write)(void *ptr, u32 addr, u32 sz, u32 val);
    void (*update_sr)(void *ptr, struct R3000 *core, u32 val);


    DBG_START
    struct console_view *console;
    DBG_END
};

struct R3000_pipeline_item *R3000_pipe_move_forward(struct R3000_pipeline *);
void R3000_init(struct R3000 *, u64 *master_clock, u64 *waitstates, struct scheduler_t *scheduler, struct IRQ_multiplexer_b *IRQ_multiplexer);
void R3000_delete(struct R3000 *);
void R3000_setup_tracing(struct R3000*, struct jsm_debug_read_trace *strct, u64 *trace_cycle_pointer, i32 source_id);
void R3000_reset(struct R3000*);
void R3000_exception(struct R3000 *, u32 code, u32 branch_delay, u32 cop0);
void R3000_flush_pipe(struct R3000 *);
void R3000_cycle(struct R3000 *, i32 howmany);
void R3000_check_IRQ(struct R3000 *);
void R3000_update_I_STAT(struct R3000 *);
void R3000_write_reg(struct R3000 *, u32 addr, u32 sz, u32 val);
u32 R3000_read_reg(struct R3000 *, u32 addr, u32 sz);
void R3000_idle(struct R3000 *this, u32 howlong);
#endif //JSMOOCH_EMUS_R3000_H
