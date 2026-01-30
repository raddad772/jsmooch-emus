//
// Created by . on 2/11/25.
//

#pragma once

#include "helpers/debugger/debuggerdefs.h"
#include "helpers/debugger/debugger.h"
#include "helpers/better_irq_multiplexer.h"
#include "helpers/int.h"
#include "helpers/debug.h"
#include "r3000_multiplier.h"
#include "helpers/scheduler.h"
#include "gte.h"
namespace R3000 {
struct REGS {
    u32 R[32];
    u32 COP0[32];
    u32 PC;
};

enum MN {
    MN_SPECIAL = 0, MN_BcondZ = 1, MN_J = 2, MN_JAL = 3, MN_BEQ = 4,
    MN_BNE = 5, MN_BLEZ = 6, MN_BGTZ = 7, MN_ADDI = 8, MN_ADDIU = 9,
    MN_SLTI = 10, MN_SLTIU = 11, MN_ANDI = 12, MN_ORI = 13, MN_XORI = 14,
    MN_LUI = 15, MN_COP0 = 16, MN_COP1 = 17, MN_COP2 = 18, MN_COP3 = 19,
    MN_LB = 20, MN_LH = 21, MN_LWL = 22, MN_LW = 23, MN_LBU = 24,
    MN_LHU = 25, MN_LWR = 26, MN_SB = 27, MN_SH = 28, MN_SWL = 29,
    MN_SW = 30, MN_SWR = 31, MN_SWC0 = 32, MN_SWC1 = 33, MN_SWC2 = 34,
    MN_SWC3 = 35, MN_SLL = 36, MN_SRL = 37, MN_SRA = 38, MN_SLLV = 39,
    MN_SRLV = 40, MN_SRAV = 41, MN_JR = 42, MN_JALR = 43, MN_SYSCALL = 44,
    MN_BREAK = 45, MN_MFHI = 46, MN_MTHI = 47, MN_MFLO = 48, MN_MTLO = 49,
    MN_MULT = 50, MN_MULTU = 51, MN_DIV = 52, MN_DIVU = 53, MN_ADD = 54,
    MN_ADDU = 55, MN_SUB = 56, MN_SUBU = 57, MN_AND = 58, MN_OR = 59,
    MN_XOR = 60, MN_NOR = 61, MN_SLT = 62, MN_SLTU = 63, MN_NA = 64,
    MN_LWCx = 65, MN_SWCx = 66, MN_COPx = 67, MN_MFC = 68, MN_CFC = 69,
    MN_MTC = 70, MN_CTC = 71, MN_BCF = 72, MN_BCT = 73, MN_COPimm = 74,
    MN_RFE = 75
};

struct OPCODE;
struct core;
typedef void (core::*insfunc)(u32 opcode, OPCODE *op);

struct OPCODE {
    MN mn;
    u32 opcode;
    insfunc func;
    i32 arg;
};

struct pipeline_item {
    i32 target;
    u32 value;
    u32 opcode, new_PC, addr;
    OPCODE *op;
};

struct pipeline {
    u32 base, num_items;
    pipeline_item current, empty_item, item0, item1;
};

struct R3000 {
    REGS regs;
    pipeline pipe;
    R3000_multiplier multiplier;
    jsm_string console;

    scheduler_t *scheduler;
    u64 *clock;
    u64 *waitstates;

    struct {
        u32 IRQ;
    } pins;

    struct {
        jsm_debug_read_trace strct;
        jsm_string str;
        u32 ok;
        u32 ins_PC;
        i32 source_id;
    } trace;

    struct {
        IRQ_multiplexer_b *I_STAT;
        u32 I_MASK;
    } io;

    GTE gte;

    opcode decode_table[0x7F];

    void *fetch_ptr, *read_ptr, *write_ptr, *update_sr_ptr;
    u32 (*fetch_ins)(void *ptr, u32 addr, u32 sz);
    u32 (*read)(void *ptr, u32 addr, u32 sz, u32 has_effect);
    void (*write)(void *ptr, u32 addr, u32 sz, u32 val);
    void (*update_sr)(void *ptr, R3000 *core, u32 val);


    DBG_START
    console_view *console;
    DBG_END
};
}