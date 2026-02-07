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
    u32 R[32]{};
    u32 COP0[32]{};
    u32 PC{};
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
    MN mn{};
    u32 opcode{};
    insfunc func{};
    i32 arg{};
};

struct pipeline_item {
    void clear();
    i32 target{};
    u32 value{};
    u32 opcode{}, new_PC, addr{};
    OPCODE *op{};
};

struct pipeline {
    void clear();
    void reset();
    pipeline_item *push();
    pipeline_item *move_forward();
    u32 base{}, num_items{};
    pipeline_item current{}, empty_item{}, item0{}, item1{};
};

struct ctxt;
struct core {
    core(u64 *master_clock_in, u64 *waitstates_in, scheduler_t *scheduler_in, IRQ_multiplexer_b *IRQ_multiplexer_in);
    void setup_tracing(jsm_debug_read_trace &strct, u64 *trace_cycle_pointer, i32 source_id);
    void add_to_console(u32 ch);
    void delay_slots(pipeline_item &which);
    void flush_pipe();
    void exception(u32 code, u32 branch_delay, u32 cop0);
    void decode(u32 IR, pipeline_item &current);
    void fetch_and_decode();
    void print_context(ctxt &ct, jsm_string &out);
    void lycoder_trace_format(jsm_string &out);
    void trace_format(jsm_string &out);
    void check_IRQ();
    void cycle(i32 howmany);
    void reset();
    void update_I_STAT();
    void write_reg(u32 addr, u8 sz, u32 val);
    u32 read_reg(u32 addr, u8 sz);
    void idle(u32 howlong);

    REGS regs{};
    pipeline pipe{};
    multiplier multiplier{};
    jsm_string console{4096};

    scheduler_t *scheduler{};
    u64 *clock{};
    u64 *waitstates{};

    struct {
        u32 IRQ{};
    } pins{};

    struct {
        jsm_debug_read_trace strct{};
        jsm_string str{100};
        bool ok{};
        u32 ins_PC{};
        i32 source_id{};
    } trace;

    struct {
        IRQ_multiplexer_b *I_STAT{};
        u32 I_MASK{};
    } io{};

    GTE::core gte{};

    OPCODE decode_table[0x7F]{};

    void *fetch_ptr{}, *read_ptr{}, *write_ptr{}, *update_sr_ptr{};
    u32 (*fetch_ins)(void *ptr, u32 addr, u8 sz){};
    u32 (*read)(void *ptr, u32 addr, u8 sz, u32 has_effect){};
    void (*write)(void *ptr, u32 addr, u8 sz, u32 val){};
    void (*update_sr)(void *ptr, core *mcore, u32 val){};
    DBG_START
    console_view *console{};
    DBG_END

private:
    void wait_for(u64 timecode);
    void do_decode_table();
    void fNA(u32 opcode, OPCODE *op);
    void fSLL(u32 opcode, OPCODE *op);
    void fSRL(u32 opcode, OPCODE *op);
    void fSRA(u32 opcode, OPCODE *op);
    void fSLLV(u32 opcode, OPCODE *op);
    void fSRLV(u32 opcode, OPCODE *op);
    void fSRAV(u32 opcode, OPCODE *op);
    void fJR(u32 opcode, OPCODE *op);
    void fJALR(u32 opcode, OPCODE *op);
    void fSYSCALL(u32 opcode, OPCODE *op);
    void fBREAK(u32 opcode, OPCODE *op);
    void fMFHI(u32 opcode, OPCODE *op);
    void fMFLO(u32 opcode, OPCODE *op);
    void fMTHI(u32 opcode, OPCODE *op);
    void fMTLO(u32 opcode, OPCODE *op);
    void fMULT(u32 opcode, OPCODE *op);
    void fMULTU(u32 opcode, OPCODE *op);
    void fDIV(u32 opcode, OPCODE *op);
    void fDIVU(u32 opcode, OPCODE *op);
    void fADD(u32 opcode, OPCODE *op);
    void fADDU(u32 opcode, OPCODE *op);
    void fSUB(u32 opcode, OPCODE *op);
    void fSUBU(u32 opcode, OPCODE *op);
    void fAND(u32 opcode, OPCODE *op);
    void fOR(u32 opcode, OPCODE *op);
    void fXOR(u32 opcode, OPCODE *op);
    void fNOR(u32 opcode, OPCODE *op);
    void fSLT(u32 opcode, OPCODE *op);
    void fSLTU(u32 opcode, OPCODE *op);
    void fBcondZ(u32 opcode, OPCODE *op);
    void fJ(u32 opcode, OPCODE *op);
    void fJAL(u32 opcode, OPCODE *op);
    void fBEQ(u32 opcode, OPCODE *op);
    void fBNE(u32 opcode, OPCODE *op);
    void fBLEZ(u32 opcode, OPCODE *op);
    void fBGTZ(u32 opcode, OPCODE *op);
    void fADDI(u32 opcode, OPCODE *op);
    void fADDIU(u32 opcode, OPCODE *op);
    void fSLTI(u32 opcode, OPCODE *op);
    void fSLTIU(u32 opcode, OPCODE *op);
    void fANDI(u32 opcode, OPCODE *op);
    void fORI(u32 opcode, OPCODE *op);
    void fXORI(u32 opcode, OPCODE *op);
    void fLUI(u32 opcode, OPCODE *op);
    void fCOP(u32 opcode, OPCODE *op);
    void fLB(u32 opcode, OPCODE *op);
    void fLH(u32 opcode, OPCODE *op);
    void fLWL(u32 opcode, OPCODE *op);
    void fLW(u32 opcode, OPCODE *op);
    void fLBU(u32 opcode, OPCODE *op);
    void fLHU(u32 opcode, OPCODE *op);
    void fLWR(u32 opcode, OPCODE *op);
    void fSB(u32 opcode, OPCODE *op);
    void fSH(u32 opcode, OPCODE *op);
    void fSWL(u32 opcode, OPCODE *op);
    void fSW(u32 opcode, OPCODE *op);
    void fSWR(u32 opcode, OPCODE *op);
    void fLWC(u32 opcode, OPCODE *op);
    void fSWC(u32 opcode, OPCODE *op);
    void fMFC(u32 opcode, OPCODE *op, u32 copnum);
    void fMTC(u32 opcode, OPCODE *op, u32 copnum);
    void fCOP0_RFE(u32 opcode, OPCODE *op);

    void COP_write_reg(u32 COP, u32 num, u32 val);
    u32 COP_read_reg(u32 COP, u32 num);
    void branch(i64 rel, u32 doit, u32 link, u32 link_reg);
    void jump(u32 new_addr, u32 doit, u32 link, u32 link_reg);
    u32 fs_reg_delay_read(i32 target);
    u64 current_clock() { return *clock + *waitstates; }

    void fs_reg_delay(const i32 target, const u32 value)
    {
        auto *p = &pipe.item0;
        p->target = target;
        p->value = value;

        if (pipe.current.target == target) {
            pipe.current.target = -1;
        }
    }

    void fs_reg_write(u32 target, u32 value)
    {
        if (target != 0) regs.R[target] = value;

        //if (trace_on) debug_reg_list.push(target);

        // cancel in-pipeline write to register
        auto *p = &pipe.current;
        if (p->target == target) p->target = -1;
    }

};
}