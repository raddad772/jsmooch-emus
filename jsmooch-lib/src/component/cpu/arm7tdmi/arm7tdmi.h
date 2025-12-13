//
// Created by . on 12/4/24.
//

#pragma once

#include "helpers/scheduler.h"
#include "helpers/debugger/debuggerdefs.h"
#include "helpers/debugger/debugger.h"
#include "helpers/debug.h"
#include "helpers/int.h"
#include "helpers/cvec.h"

#include "thumb_instructions.h"
#define ARM7P_nonsequential 0
#define ARM7P_sequential 1
#define ARM7P_code 2
#define ARM7P_dma 4
#define ARM7P_lock 8

namespace ARM7TDMI {

enum modes {
    M_old_user = 0,
    M_old_fiq = 1,
    M_old_irq = 2,
    M_old_supervisor = 3,
    M_user = 16,
    M_fiq = 17,
    M_irq = 18,
    M_supervisor = 19,
    M_abort = 23,
    M_undefined = 27,
    M_system = 31
};

enum condition_codes {
    CC_EQ = 0, // Z=1
    CC_NE = 1, // Z=0
    CC_CS_HS = 2, // C=1
    CC_CC_LO = 3, // C=0
    CC_MI = 4, // N=1
    CC_PL = 5, // N=0
    CC_VS = 6, // V=1
    CC_VC = 7, // V=0
    CC_HI = 8, // C=1 and Z=0
    CC_LS = 9, // C=0 or Z=1
    CC_GE = 10, // N=V
    CC_LT = 11, // N!=V
    CC_GT = 12, // Z=0 and N=V
    CC_LE = 13, // Z=1 or N!=V
    CC_AL = 14, // "always"
    CC_NV = 15 // never. don't execute opcode
};

struct regs {
    [[nodiscard]] int condition_passes(int which) const;
    u32 R[16]{};
    u32 R_fiq[7]{};
    u32 R_svc[2]{};
    u32 R_abt[2]{};
    u32 R_irq[2]{};
    u32 R_und[2]{};

    u32 R_invalid[16]{};

    u32 SPSR_fiq{};
    u32 SPSR_svc{};
    u32 SPSR_abt{};
    u32 SPSR_irq{};
    u32 SPSR_und{};
    u32 SPSR_invalid{};
    union M_CPSR {
        struct {
            u32 mode : 5;
            u32 T: 1; // T - state bit. 0 = ARM, 1 = THUMB
            u32 F: 1; // F - FIQ disable
            u32 I: 1; // I - IRQ disable
            //u32 A: 1; // A - abort disable. _2 is : 14 (or 15) if uncommented
            //u32 E: 1; // E - endian. _2 is : 15 if uncommented
            u32 _2 : 16;
            u32 J: 1; // J - Jazelle (Java) mode
            u32 _3 : 3;
            //u32 Q: 1; // Sticky overflow, ARmv5TE and up only. _3 is : 2 if uncommented
            u32 V: 1; // 0 = no overflow, 1 = overflow
            u32 C: 1; // 0 = borrow/no carry, 1 = carry/no borrow
            u32 Z: 1; // 0 = not zero, 1 = zero
            u32 N: 1; // 0 = not signed, 1 = signed
        };
        u32 u{};
    } CPSR{};
    union {
        u32 rr;
#if !defined(_MSC_VER) // error C2016: C requires that a struct or union have at least one member
#endif
        u32 u{};
    } SPSR{};

    u32 IRQ_line{};
};

struct core;
struct ARM7_ins {
    void (*exec)(core*, u32 opcode){};
};

struct core {
    explicit core(u64 *master_clock, u64 *waitstates, scheduler_t *scheduler);
    scheduler_t *scheduler{};
    u32 sch_irq_sch{};
    regs regs{};
    struct {
        u32 opcode[2]{};
        u32 addr[2]{};
        u32 access{};
        u32 flushed{};
    } pipeline{};

    u32 *regmap[16]{};
    u32 carry{}; // temp for instructions

    u64 *waitstates{};
    u64 *master_clock{};

    ARM7_ins *arm7_ins{};

    struct {
        jsm_debug_read_trace strct{};
        jsm_string str{100};
        jsm_string str2{100};
        u32 ok{};
        u64 *cycles{};
        u32 ins_PC{};
        i32 source_id{};

        struct {
            dbglog_view *view{};
            u32 id{};
        } dbglog{};
    } trace{};


    u32 testing{};

    void *read_ins_ptr, *read_data_ptr, *write_data_ptr{};
    u32 (*read_ins)(void *ptr, u32 addr, u8 sz, u8 access){};
    u32 (*read_data)(void *ptr, u32 addr, u8 sz, u8 access, bool has_effect){};
    void (*write_data)(void *ptr, u32 addr, u8 sz, u8 access, u32 val){};

    ARM7_ins opcode_table_arm[4096]{};
    thumb_instruction opcode_table_thumb[65536]{};

private:
    u32 fetch_ins(u32 sz);
    u32 go_fetch_ins(u32 addr, u32 sz, u32 access);
    void do_IRQ();
    static void sch_check_irq(void *ptr, u64 key, u64 timecode, u32 jitter);
    void bad_trace(u32 r, u32 sz);
    void print_context(struct ARMctxt *ct, jsm_string *out);
    void armv4_trace_format(u32 opcode, u32 addr, u32 T);
    void fill_arm_table();
    void decode_and_exec_thumb(u32 opcode, u32 opcode_addr);
    void decode_and_exec_arm(u32 opcode, u32 opcode_addr);

public:
    void schedule_IRQ_check();
    void write_reg(u32 *r, u32 v);
    void fill_regmap();
    void flush_pipeline();
    void idle(u32 n);
    u32 read(u32 addr, u32 sz, u32 access, u32 has_effect);
    void write(u32 addr, u32 sz, u32 access, u32 val);
    void IRQcheck(bool do_sched);
    void run_noIRQcheck();
    void do_FIQ();
    void disassemble_entry(disassembly_entry& entry);
    void reload_pipeline();
    void reset();
    void setup_tracing(jsm_debug_read_trace *strct, u64 *trace_cycle_pointer, i32 source_id);
    DBG_START
    DBG_EVENT_VIEW
    DBG_TRACE_VIEW
    DBG_END
};



}