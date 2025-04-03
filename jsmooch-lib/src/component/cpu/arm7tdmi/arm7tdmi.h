//
// Created by . on 12/4/24.
//

#ifndef JSMOOCH_EMUS_ARM7TDMI_H
#define JSMOOCH_EMUS_ARM7TDMI_H

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

enum ARM7TDMI_modes {
    ARM7M_old_user = 0,
    ARM7M_old_fiq = 1,
    ARM7M_old_irq = 2,
    ARM7_old_supervisor = 3,
    ARM7_user = 16,
    ARM7_fiq = 17,
    ARM7_irq = 18,
    ARM7_supervisor = 19,
    ARM7_abort = 23,
    ARM7_undefined = 27,
    ARM7_system = 31
};

enum ARM7TDMI_condition_codes {
    ARM7CC_EQ = 0, // Z=1
    ARM7CC_NE = 1, // Z=0
    ARM7CC_CS_HS = 2, // C=1
    ARM7CC_CC_LO = 3, // C=0
    ARM7CC_MI = 4, // N=1
    ARM7CC_PL = 5, // N=0
    ARM7CC_VS = 6, // V=1
    ARM7CC_VC = 7, // V=0
    ARM7CC_HI = 8, // C=1 and Z=0
    ARM7CC_LS = 9, // C=0 or Z=1
    ARM7CC_GE = 10, // N=V
    ARM7CC_LT = 11, // N!=V
    ARM7CC_GT = 12, // Z=0 and N=V
    ARM7CC_LE = 13, // Z=1 or N!=V
    ARM7CC_AL = 14, // "always"
    ARM7CC_NV = 15 // never. don't execute opcode
};

struct ARM7TDMI_regs {
    u32 R[16];
    u32 R_fiq[7];
    u32 R_svc[2];
    u32 R_abt[2];
    u32 R_irq[2];
    u32 R_und[2];

    u32 R_invalid[16];

    u32 SPSR_fiq;
    u32 SPSR_svc;
    u32 SPSR_abt;
    u32 SPSR_irq;
    u32 SPSR_und;
    u32 SPSR_invalid;
    union ARM7TDMI_CPSR {
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
        u32 u;
    } CPSR;
    union {
        struct {
        };
        u32 u;
    } SPSR;

    u32 IRQ_line;
};

struct ARM7TDMI;
struct ARM7_ins {
    void (*exec)(struct ARM7TDMI*, u32 opcode);
};

struct ARM7TDMI {
    struct scheduler_t *scheduler;
    u32 sch_irq_sch;
    struct ARM7TDMI_regs regs;
    struct {
        u32 opcode[2];
        u32 addr[2];
        u32 access;
        u32 flushed;
    } pipeline;

    u32 *regmap[16];
    u32 carry; // temp for instructions

    u64 *waitstates;
    u64 *master_clock;

    struct ARM7_ins *arm7_ins;

    struct {
        struct jsm_debug_read_trace strct;
        struct jsm_string str;
        struct jsm_string str2;
        u32 ok;
        u64 *cycles;
        u32 ins_PC;
        i32 source_id;

        struct {
            struct dbglog_view *view;
            u32 id;
        } dbglog;
    } trace;


    u32 testing;

    void *fetch_ptr, *read_ptr, *write_ptr;
    u32 (*fetch_ins)(void *ptr, u32 addr, u32 sz, u32 access);
    u32 (*read)(void *ptr, u32 addr, u32 sz, u32 access, u32 has_effect);
    void (*write)(void *ptr, u32 addr, u32 sz, u32 access, u32 val);

    struct ARM7_ins opcode_table_arm[4096];
    struct thumb_instruction opcode_table_thumb[65536];

    DBG_START
    DBG_EVENT_VIEW
    DBG_TRACE_VIEW
    DBG_END
};


void ARM7TDMI_init(struct ARM7TDMI *, u64 *master_clock, u64 *waitstates, struct scheduler_t *scheduler);
void ARM7TDMI_delete(struct ARM7TDMI *);

void ARM7TDMI_reset(struct ARM7TDMI *);
void ARM7TDMI_disassemble_entry(struct ARM7TDMI*, struct disassembly_entry* entry);
void ARM7TDMI_setup_tracing(struct ARM7TDMI*, struct jsm_debug_read_trace *strct, u64 *trace_cycle_pointer, i32 source_id);
void ARM7TDMI_run_noIRQcheck(struct ARM7TDMI *);
void ARM7TDMI_IRQcheck(struct ARM7TDMI *, u32 do_sched);
void ARM7TDMI_flush_pipeline(struct ARM7TDMI *);
void ARM7TDMI_fill_regmap(struct ARM7TDMI *);
void ARM7TDMI_reload_pipeline(struct ARM7TDMI *);
void ARM7TDMI_idle(struct ARM7TDMI*this, u32 num);


u32 ARM7TDMI_fetch_ins(struct ARM7TDMI *, u32 addr, u32 sz, u32 access);
u32 ARM7TDMI_read(struct ARM7TDMI *, u32 addr, u32 sz, u32 access, u32 has_effect);
void ARM7TDMI_write(struct ARM7TDMI *, u32 addr, u32 sz, u32 access, u32 val);
void ARM7TDMI_schedule_IRQ_check(struct ARM7TDMI *);

#endif //JSMOOCH_EMUS_ARM7TDMI_H
