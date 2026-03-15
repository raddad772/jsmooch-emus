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
static constexpr bool DIVMUL_INSTANT = true;
struct REGS {
    u32 R[32]{};
    u32 COP0[32]{};
    u32 PC{};
    u32 PC_next{};
    u32 IR{};
};

    union CAUSE {
        struct {
            u32 _ : 2;
            u32 exception_code : 5;
            u32 __ : 1;
            u32 IP : 8;
            u32 ___ : 12;
            u32 CE : 2;
            u32 BT : 1;
            u32 BD : 1;
        };
        u32 u{};
    };

struct OPCODE;
struct core;
typedef void (core::*insfunc)(u32 opcode, OPCODE *op);

struct OPCODE {
    u32 opcode{};
    insfunc func{};
    i32 arg{};
};


    struct ctxt;
struct core {
    core(u64 *master_clock_in, u64 *waitstates_in, scheduler_t *scheduler_in, IRQ_multiplexer_b *IRQ_multiplexer_in);
    void setup_tracing(jsm_debug_read_trace &strct, u64 *trace_cycle_pointer, i32 source_id);
    void add_to_console(u32 ch);
    void exception(u32 code, u32 cop0);
    void after_ins();
    void decode(u32 IR);
    bool fetch_and_decode();
    void print_context(ctxt &ct, jsm_string &out);
    void trace_format_console(jsm_string &out);
    void trace_format();
    void dbglog_exception(u32 code, u32 vector, u32 raddr);
    void check_IRQ();
    void cycle(i32 howmany);
    void instruction();
    void reset();
    void update_I_STAT();
    void write_reg(u32 addr, u8 sz, u32 val);
    u32 read_reg(u32 addr, u8 sz);
    [[nodiscard]] u32 peek_next_instruction() const;
    void idle(u32 howlong);

    REGS regs{};
    multiplier multiplier{};
    jsm_string console{4096};

    struct {
        struct {
            i32 target{-1};
            u32 value{0};
        } load[2]{};

        struct {
            bool slot{false};
            bool taken{false};
            u32 target{};
        } branch[2]{};
    } delay{};

    scheduler_t *scheduler{};
    u64 *clock{};
    u64 *waitstates{};

    struct {
        u32 IRQ{};
    } pins{};

    struct {
        jsm_debug_read_trace strct{};
        jsm_string str{100};
        jsm_string str2{100};
        bool ok{};
        u32 ins_PC{};
        i32 source_id{};
        u32 irq_id{};
        u32 rfe_id{};
        u32 exception_id{};
        u32 I_STAT_write{};
        u32 I_MASK_write{};
        u32 console_log_id{};
    } trace{};

    struct {
        IRQ_multiplexer_b *I_STAT{};
        u32 I_MASK{};
    } io{};

    GTE::core gte{};

    OPCODE *cur_ins{};

    OPCODE decode_table[0x7F]{};

    void *fetch_ins_ptr{}, *read_ptr{}, *write_ptr{}, *peek_ins_ptr{}, *update_sr_ptr{}, *khook_ptr{};
    u32 (*fetch_ins)(void *ptr, u32 addr){};
    u32 (*peek_ins)(void *ptr, u32 addr);
    u32 (*read)(void *ptr, u32 addr, u8 sz){};
    void (*khook)(void *ptr);
    void (*write)(void *ptr, u32 addr, u8 sz, u32 val){};
    void (*update_sr)(void *ptr, core *mcore, u32 val){};
    DBG_START
    console_view *console{};
    DBG_LOG_VIEW
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
    void branch(i64 rel, bool doit, bool link, u32 link_reg);
    void jump(u32 new_addr, bool link, u32 link_reg);
    u32 fs_reg_delay_read(i32 target);
    u64 current_clock() { return *clock + *waitstates; }

    void fs_reg_delay(const i32 target, const u32 value)
    {
        delay.load[1].target = target;
        delay.load[1].value = value;

        if (delay.load[0].target == target) { delay.load[0] = {}; }
    }

    void fs_reg_write(u32 target, u32 value)
    {
        // cancel in-pipeline write to register
        if (delay.load[0].target == target) delay.load[0] = {};

        regs.R[target] = value;
    }

};
}