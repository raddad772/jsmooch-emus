//
// Created by . on 1/18/25.
//

#ifndef JSMOOCH_EMUS_ARM946ES_H
#define JSMOOCH_EMUS_ARM946ES_H

#include "helpers/debugger/debuggerdefs.h"
#include "helpers/debugger/debugger.h"
#include "helpers/debug.h"
#include "helpers/int.h"
#include "helpers/cvec.h"

#include "thumb2_instructions.h"
#define ARM9P_nonsequential 0
#define ARM9P_sequential 1
#define ARM9P_code 2
#define ARM9P_dma 4
#define ARM9P_lock 8

enum ARM946ES_modes {
    ARM9M_old_user = 0,
    ARM9M_old_fiq = 1,
    ARM9M_old_irq = 2,
    ARM9_old_supervisor = 3,
    ARM9_user = 16,
    ARM9_fiq = 17,
    ARM9_irq = 18,
    ARM9_supervisor = 19,
    ARM9_abort = 23,
    ARM9_undefined = 27,
    ARM9_system = 31
};

enum ARM946ES_condition_codes {
    ARM9CC_EQ = 0, // Z=1
    ARM9CC_NE = 1, // Z=0
    ARM9CC_CS_HS = 2, // C=1
    ARM9CC_CC_LO = 3, // C=0
    ARM9CC_MI = 4, // N=1
    ARM9CC_PL = 5, // N=0
    ARM9CC_VS = 6, // V=1
    ARM9CC_VC = 7, // V=0
    ARM9CC_HI = 8, // C=1 and Z=0
    ARM9CC_LS = 9, // C=0 or Z=1
    ARM9CC_GE = 10, // N=V
    ARM9CC_LT = 11, // N!=V
    ARM9CC_GT = 12, // Z=0 and N=V
    ARM9CC_LE = 13, // Z=1 or N!=V
    ARM9CC_AL = 14, // "always"
    ARM9CC_NV = 15 // never. don't execute opcode
};

struct ARM946ES_regs {
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
    union ARM946ES_CPSR {
        struct {
            u32 mode : 5;
            u32 T: 1; // T - state bit. 0 = ARM, 1 = THUMB
            u32 F: 1; // F - FIQ disable
            u32 I: 1; // I - IRQ disable
            //u32 A: 1; // A - abort disable. _2 is : 14 (or 15) if uncommented
            //u32 E: 1; // E - endian. _2 is : 15 if uncommented
            u32 _2 : 16;
            u32 J: 1; // J - Jazelle (Java) mode
            u32 _3 : 2; // this is 2 because Q. it makes sense.
            u32 Q: 1; // Sticky overflow, ARmv5TE and up only.
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

struct ARM946ES;
struct ARM9_ins {
    void (*exec)(struct ARM946ES*, u32 opcode);
};

struct ARM946ES {
    struct ARM946ES_regs regs;
    struct {
        u32 opcode[2];
        u32 addr[2];
        u32 access;
        u32 flushed;
    } pipeline;

    u32 *regmap[16];
    u32 carry; // temp for instructions

    u32 *waitstates;

    struct ARM9_ins *ARM9_ins;

    struct {
        struct jsm_debug_read_trace strct;
        struct jsm_string str;
        struct jsm_string str2;
        u32 ok;
        u64 *cycles;
        u32 ins_PC;
        i32 source_id;
    } trace;


    u32 testing;

    void *fetch_ptr, *read_ptr, *write_ptr;
    u32 (*fetch_ins)(void *ptr, u32 addr, u32 sz, u32 access);
    u32 (*read)(void *ptr, u32 addr, u32 sz, u32 access, u32 has_effect);
    void (*write)(void *ptr, u32 addr, u32 sz, u32 access, u32 val);

    struct ARM9_ins opcode_table_arm[4096];
    struct thumb2_instruction opcode_table_thumb[65536];

    DBG_START
            DBG_EVENT_VIEW
    DBG_TRACE_VIEW
            DBG_END
};


void ARM946ES_init(struct ARM946ES *, u32 *waitstates);
void ARM946ES_delete(struct ARM946ES *);

void ARM946ES_reset(struct ARM946ES *);
void ARM946ES_disassemble_entry(struct ARM946ES*, struct disassembly_entry* entry);
void ARM946ES_setup_tracing(struct ARM946ES*, struct jsm_debug_read_trace *strct, u64 *trace_cycle_pointer, i32 source_id);
void ARM946ES_run(struct ARM946ES*);
void ARM946ES_flush_pipeline(struct ARM946ES *);
void ARM946ES_fill_regmap(struct ARM946ES *);
void ARM946ES_reload_pipeline(struct ARM946ES *);
void ARM946ES_idle(struct ARM946ES*this, u32 num);

u32 ARM946ES_fetch_ins(struct ARM946ES *, u32 addr, u32 sz, u32 access);
u32 ARM946ES_read(struct ARM946ES *, u32 addr, u32 sz, u32 access, u32 has_effect);
void ARM946ES_write(struct ARM946ES *, u32 addr, u32 sz, u32 access, u32 val);

#endif //JSMOOCH_EMUS_ARM946ES_H
