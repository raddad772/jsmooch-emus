//
// Created by . on 1/18/25.
//

#ifndef JSMOOCH_EMUS_ARM946ES_H
#define JSMOOCH_EMUS_ARM946ES_H

#include "helpers_c/scheduler.h"
#include "helpers_c/debugger/debuggerdefs.h"
#include "helpers_c/debugger/debugger.h"
#include "helpers_c/debug.h"
#include "helpers_c/int.h"
#include "helpers_c/cvec.h"

#include "thumb2_instructions.h"
#include "nds_cp15.h"
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

    u32 EBR; // Exception Base Register, my own kinda abstraction
    union {
#if !defined(_MSC_VER) // error C2016: C requires that a struct or union have at least one member
        struct {
        };
#endif
        u32 u;
    } SPSR;

    u32 IRQ_line;
};

struct ARM946ES;
struct arm9_ins {
    void (*exec)(struct ARM946ES*, u32 opcode);
    u32 valid;
};

struct ARM946ES {
    struct ARM946ES_regs regs;
    struct scheduler_t *scheduler;
    u32 sch_irq_sch;
    struct {
        u32 opcode[2];
        u32 addr[2];
        u32 access;
        u32 flushed;
    } pipeline;

    struct {
        struct {
            union {
                struct {
                    // Bits 0...7
                    u32 mmu_pu_enable : 1;
                    u32 alignment_fault_check : 1;
                    u32 data_unified_cache: 1;
                    u32 write_buffer : 1;
                    u32 exception_handling : 1;
                    u32 address_faults26 : 1;
                    u32 abort_model : 1;
                    u32 endian : 1;

                    // Bits 8...15
                    u32 system_protection : 1;  // 8
                    u32 rom_protection : 1;     // 9
                    u32 _imp1 : 1;              // 10
                    u32 branch_prediction : 1;  // 11
                    u32 instruction_cache : 1;  // 12
                    u32 exception_vectors : 1;  // 13
                    u32 cache_replacement: 1;   // 14
                    u32 pre_armv5_mode : 1;     // 15

                    // Bits 16-23
                    u32 dtcm_enable : 1;            // 16
                    u32 dtcm_load_mode : 1;         // 17
                    u32 itcm_enable : 1;            // 18
                    u32 itcm_load_mode : 1;         // 19
                    u32 _res : 2;                // 20,21
                    u32 unaligned_access : 1;    // 22
                    u32 extended_page_table : 1; // 23


                    // Bits 24-31
                    u32 _res2: 1;               // 24
                    u32 cpsr_e_on_exceptions: 1;// 25
                    u32 _res3: 1;               // 26
                    u32 fiq_behavior: 1;        // 27
                    u32 tex_remap : 1;          // 28
                    u32 force_ap : 1;           // 29
                    u32 _res4: 2;               // 30, 31
                };
                u32 u;
            }control;
            u32 pu_data_cacheable;
            u32 pu_instruction_cacheable;
            u32 pu_data_cached_write;
            u32 pu_data_rw;
            u32 pu_code_rw;
            u32 pu_region[8];
            u32 dtcm_setting;
            u32 itcm_setting;
            u32 trace_process_id;
        } regs;

        u32 rng_seed;
        struct {
             u8 data[DTCM_SIZE];
             u32 size, base_addr, end_addr, mask;
        } dtcm;

        struct {
            u8 data[ITCM_SIZE];
            u32 size, base_addr, end_addr, mask;
        } itcm;
    } cp15;

    u32 *regmap[16];
    u32 carry; // temp for instructions

    u64 *waitstates;
    u64 *master_clock;
    u32 waitstates9; // 66MHz waitstates

    struct arm9_ins *arm9_ins;

    u32 halted;

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

    struct arm9_ins opcode_table_arm[4096];
    struct arm9_ins opcode_table_arm_never[4096];
    struct thumb2_instruction opcode_table_thumb[65536];

    DBG_START
        DBG_EVENT_VIEW
        DBG_TRACE_VIEW
        DBG_LOG_VIEW
    DBG_END
};


void ARM946ES_init(struct ARM946ES *, u64 *master_clock, u64 *waitstates, struct scheduler_t *scheduler);
void ARM946ES_delete(struct ARM946ES *);

void ARM946ES_reset(struct ARM946ES *);
void ARM946ES_disassemble_entry(struct ARM946ES*, struct disassembly_entry* entry);
void ARM946ES_setup_tracing(struct ARM946ES*, struct jsm_debug_read_trace *strct, u64 *trace_cycle_pointer, i32 source_id);
void ARM946ES_run_noIRQcheck(struct ARM946ES*);
void ARM946ES_IRQcheck(struct ARM946ES*, u32 do_sched);
void ARM946ES_flush_pipeline(struct ARM946ES *);
void ARM946ES_fill_regmap(struct ARM946ES *);
void ARM946ES_reload_pipeline(struct ARM946ES *);
void ARM946ES_idle(struct ARM946ES*this, u32 num);
void ARM946ES_NDS_direct_boot(struct ARM946ES *);
u32 ARM946ES_fetch_ins(struct ARM946ES *, u32 addr, u32 sz, u32 access);
u32 ARM946ES_read(struct ARM946ES *, u32 addr, u32 sz, u32 access, u32 has_effect);
void ARM946ES_write(struct ARM946ES *, u32 addr, u32 sz, u32 access, u32 val);
void ARM946ES_schedule_IRQ_check(struct ARM946ES *);

#endif //JSMOOCH_EMUS_ARM946ES_H
