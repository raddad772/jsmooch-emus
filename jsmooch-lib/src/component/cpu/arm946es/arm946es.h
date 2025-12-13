//
// Created by . on 1/18/25.
//

#pragma once

#include "helpers/scheduler.h"
#include "helpers/debugger/debuggerdefs.h"
#include "helpers/debugger/debugger.h"
#include "helpers/debug.h"
#include "helpers/int.h"
#include "helpers/cvec.h"

#include "thumb2_instructions.h"
#include "nds_cp15.h"
#include "armv5_disassembler.h"

#define ARM9P_nonsequential 0
#define ARM9P_sequential 1
#define ARM9P_code 2
#define ARM9P_dma 4
#define ARM9P_lock 8

namespace ARM946ES {
#include "helpers/multisize_memaccess.cpp"
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
    union ARM946ES_CPSR {
        struct {
            u32 mode : 5;
            u32 T: 1; // T - state bit. 0 = ARM{}, 1 = THUMB
            u32 F: 1; // F - FIQ disable
            u32 I: 1; // I - IRQ disable
            //u32 A: 1; // A - abort disable. _2 is : 14 (or 15) if uncommented
            //u32 E: 1; // E - endian. _2 is : 15 if uncommented
            u32 _2 : 16;
            u32 J: 1; // J - Jazelle (Java) mode
            u32 _3 : 2; // this is 2 because Q. it makes sense.
            u32 Q: 1; // Sticky overflow{}, ARmv5TE and up only.
            u32 V: 1; // 0 = no overflow{}, 1 = overflow
            u32 C: 1; // 0 = borrow/no carry{}, 1 = carry/no borrow
            u32 Z: 1; // 0 = not zero{}, 1 = zero
            u32 N: 1; // 0 = not signed{}, 1 = signed
        };
        u32 u{};
    } CPSR{};

    u32 EBR{}; // Exception Base Register{}, my own kinda abstraction
    union {
        u32 b;
        u32 u{};
    } SPSR{};

    u32 IRQ_line{};
};

struct core;

struct arm9_ins {
    void (core::*exec)(u32 opcode){};
    bool valid{};
};

struct thumb2_instruction;
struct thumb2_instruction {
    void (core::*func)(const thumb2_instruction &){};
    u16 sub_opcode{};
    u16 opcode{};
    u16 Rd{}, Rs{};
    union {
        u16 Ro{};
        u16 Rm;
    };
    union {
        u16 Rn{};
        u16 Rb;
    };
    union {
        u16 L{};
        u16 I;
        u16 B;
        u16 SP;
        u16 S;
        u16 PC_LR;
    };
    union {
        u32 imm{};
        u16 rlist;
        u16 comment;
    };
};


struct core {
    explicit core(scheduler_t *scheduler, u64 *master_clock_in, u64 *waitstates_in);
    void update_dtcm();
    void update_itcm();
    void reset();
    void run_noIRQcheck();
    void setup_tracing(jsm_debug_read_trace *strct, u64 *trace_cycle_pointer, i32 source_id);
    void idle(u32 num);
    u32 read(u32 addr, u8 sz, u8 access, bool has_effect);
    void write(u32 addr, u8 sz, u8 access, u32 val);
    void fill_arm_table();
    void NDS_CP_reset();
    void NDS_direct_boot();
    u32 *get_SPSR_by_mode();
    u32 *old_getR(u32 num);
    void fill_regmap();
    u32 MUL(u32 product, u32 multiplicand, u32 multiplier, u32 S);

    void NDS_CP_write(u32 num, u32 opcode, u32 Cn, u32 Cm, u32 CP, u32 val);
    u32 NDS_CP_read(u32 num, u32 opcode, u32 Cn, u32 Cm, u32 CP) const;

    u32 fetch_ins(u8 sz);
    u32 do_fetch_ins(u32 addr, u8 sz, u8 access);
    void do_IRQ();
    void do_FIQ();
    bool condition_passes(condition_codes which) const;
    void reload_pipeline();
    void print_context(ARMctxt *ct, jsm_string *out, bool taken) const;
    void armv5_trace_format(u32 opcode, u32 addr, bool T, bool taken);
    void decode_and_exec_thumb(u32 opcode, u32 opcode_addr);
    void decode_and_exec_arm(u32 opcode, u32 opcode_addr);
    static void sch_check_irq(void *ptr, u64 key, u64 timecode, u32 jitter);
    void schedule_IRQ_check();
    void IRQcheck(bool do_sched);
    u32 TEST(u32 v, u32 S);
    u32 ADD(u32 Rnd, u32 Rmd, u32 carry, u32 S);
    u32 SUB(u32 Rn, u32 Rm, u32 carry, u32 S);
    u32 ALU(u32 Rn, u32 Rm, u32 alu_opcode, u32 S, u32 *out);
    u32 LSL(u32 v, u32 amount);
    u32 LSR(u32 v, u32 amount);
    u32 ASR(u32 v, u32 amount);
    u32 ROR(u32 v, u32 amount);
    u32 RRX(u32 v);
    void undefined_exception();
    u32 tLSL(u32 v, u32 amount, bool set_flags);
    u32 tLSR(u32 v, u32 amount, bool set_flags);
    u32 tASR(u32 v, u32 amount, bool set_flags);
    u32 tADD(u32 op1, u32 op2);
    u32 tSUB(u32 op1, u32 op2, bool set_flags);
    u32 tROR(u32 v, u32 amount);

    void ins_MUL_MLA(u32 opcode);
    void ins_MULL_MLAL(u32 opcode);
    void ins_SWP(u32 opcode);
    void ins_LDRH_STRH(u32 opcode);
    void ins_LDRSB_LDRSH(u32 opcode);
    void ins_MRS(u32 opcode);
    void ins_MSR_reg(u32 opcode);
    void ins_MSR_imm(u32 opcode);
    void ins_BX(u32 opcode);
    void ins_data_proc_immediate_shift(u32 opcode);
    void ins_data_proc_register_shift(u32 opcode);
    void ins_undefined_instruction(u32 opcode);
    void ins_data_proc_immediate(u32 opcode);
    void ins_LDR_STR_immediate_offset(u32 opcode);
    void ins_LDR_STR_register_offset(u32 opcode);
    void ins_LDM_STM(u32 opcode);
    void ins_STC_LDC(u32 opcode);
    void ins_CDP(u32 opcode);
    void ins_MCR_MRC(u32 opcode);
    void ins_SWI(u32 opcode);
    void ins_INVALID(u32 opcode);
    void ins_PLD(u32 opcode);
    void ins_SMLAxy(u32 opcode);
    void ins_SMLAWy(u32 opcode);
    void ins_SMULWy(u32 opcode);
    void ins_SMLALxy(u32 opcode);
    void ins_SMULxy(u32 opcode);
    void ins_LDRD_STRD(u32 opcode);
    void ins_CLZ(u32 opcode);
    void ins_BLX_reg(u32 opcode);
    void ins_QADD_QSUB_QDADD_QDSUB(u32 opcode);
    void ins_BKPT(u32 opcode);
    void ins_B_BL(u32 opcode);
    void ins_BLX(u32 opcode);
    void THUMB_ins_INVALID(const thumb2_instruction &ins);
    void THUMB_ins_ADD_SUB(const thumb2_instruction &ins);
    void THUMB_ins_LSL_LSR_ASR(const thumb2_instruction &ins);
    void THUMB_ins_MOV_CMP_ADD_SUB(const thumb2_instruction &ins);
    void THUMB_ins_data_proc(const thumb2_instruction &ins);
    void THUMB_ins_BX_BLX(const thumb2_instruction &ins);
    void THUMB_ins_ADD_CMP_MOV_hi(const thumb2_instruction &ins);
    void THUMB_ins_LDR_PC_relative(const thumb2_instruction &ins);
    void THUMB_ins_LDRH_STRH_reg_offset(const thumb2_instruction &ins);
    void THUMB_ins_LDRSH_LDRSB_reg_offset(const thumb2_instruction &ins);
    void THUMB_ins_LDR_STR_reg_offset(const thumb2_instruction &ins);
    void THUMB_ins_LDRB_STRB_reg_offset(const thumb2_instruction &ins);
    void THUMB_ins_LDR_STR_imm_offset(const thumb2_instruction &ins);
    void THUMB_ins_LDRB_STRB_imm_offset(const thumb2_instruction &ins);
    void THUMB_ins_LDRH_STRH_imm_offset(const thumb2_instruction &ins);
    void THUMB_ins_LDR_STR_SP_relative(const thumb2_instruction &ins);
    void THUMB_ins_ADD_SP_or_PC(const thumb2_instruction &ins);
    void THUMB_ins_ADD_SUB_SP(const thumb2_instruction &ins);
    void THUMB_ins_PUSH_POP(const thumb2_instruction &ins);
    void THUMB_ins_LDM_STM(const thumb2_instruction &ins);
    void THUMB_ins_SWI(const thumb2_instruction &ins);
    void THUMB_ins_UNDEFINED_BCC(const thumb2_instruction &ins);
    void THUMB_ins_BCC(const thumb2_instruction &ins);
    void THUMB_ins_B(const thumb2_instruction &ins);
    void THUMB_ins_BL_BLX_prefix(const thumb2_instruction &ins);
    void THUMB_ins_BL_suffix(const thumb2_instruction &ins);
    void THUMB_ins_BKPT(const thumb2_instruction &ins);
    void THUMB_ins_BLX_suffix(const thumb2_instruction &ins);


    [[nodiscard]] bool addr_in_itcm(u32 addr)
    {
        return ((addr >= cp15.itcm.base_addr) && (addr < cp15.itcm.end_addr));
    }

    [[nodiscard]] u32 read_itcm(u32 addr, u8 sz)
    {
        (*waitstates)++;
        u32 tcm_addr = (addr - cp15.itcm.base_addr) & cp15.itcm.mask;
        return cR[sz](cp15.itcm.data, tcm_addr & (ITCM_SIZE - 1));
    }

    [[nodiscard]] bool addr_in_dtcm(const u32 addr) const
    {
        return ((addr >= cp15.dtcm.base_addr) && ((addr < cp15.dtcm.end_addr)));
    }

    u32 read_dtcm(const u32 addr, const u8 sz)
    {
        (*waitstates)++;
        const u32 tcm_addr = (addr - cp15.dtcm.base_addr) & (DTCM_SIZE - 1);
        return cR[sz](cp15.dtcm.data, tcm_addr & DTCM_MASK);
    }

    void flush_pipeline()
    {
        pipeline.flushed = true;
    }

    void write_dtcm(const u32 addr, const u8 sz, const u32 v)
    {
        (*waitstates)++;
        const u32 tcm_addr = (addr - cp15.dtcm.base_addr) & (cp15.dtcm.size - 1);
        cW[sz](cp15.dtcm.data, tcm_addr & DTCM_MASK, v);
    }

    void write_itcm(u32 addr, u8 sz, u32 v)
    {
        (*waitstates)++;
        u32 tcm_addr = (addr - cp15.itcm.base_addr) & cp15.itcm.mask;
        cW[sz](cp15.itcm.data, tcm_addr & ITCM_MASK, v);
    }

    [[nodiscard]] u32 *getR(const u32 num) const {
        return regmap[num];
    }

    void twrite_reg(u32 *r, const u32 v) {
        *r = v;
    }

    void write_reg(u32 *r, const u32 v) {
        *r = v;
        if (r == &regs.R[15]) {
            flush_pipeline();
        }
    }


    regs regs{};
    scheduler_t *scheduler{};
    u32 sch_irq_sch{};
    struct {
        u32 opcode[2]{};
        u32 addr[2]{};
        u32 access{};
        bool flushed{};
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
                    u32 _res4: 2;               // 30{}, 31
                };
                u32 u{};
            }control{};
            u32 pu_data_cacheable{};
            u32 pu_instruction_cacheable{};
            u32 pu_data_cached_write{};
            u32 pu_data_rw{};
            u32 pu_code_rw{};
            u32 pu_region[8]{};
            u32 dtcm_setting{};
            u32 itcm_setting{};
            u32 trace_process_id{};
        } regs{};

        u32 rng_seed{};
        struct {
             u8 data[DTCM_SIZE]{};
             u32 size{}, base_addr{}, end_addr{}, mask{};
        } dtcm{};

        struct {
            u8 data[ITCM_SIZE]{};
            u32 size{}, base_addr{}, end_addr{}, mask{};
        } itcm{};
    } cp15{};

    u32 *regmap[16]{};
    u32 temp_carry{}; // temp for instructions

    u64 *waitstates{};
    u64 *master_clock{};
    u32 waitstates9{}; // 66MHz waitstates

    arm9_ins *arm_ins{};

    bool halted{};

    struct {
        jsm_debug_read_trace strct{};
        jsm_string str{100};
        jsm_string str2{100};
        bool ok{};
        u64 *cycles{};
        u32 ins_PC{};
        i32 source_id{};
    } trace{};

    bool testing{};

    void *fetch_ptr{}, *read_ptr{}, *write_ptr{};
    u32 (*fetch_ins_func)(void *ptr, u32 addr, u8 sz, u8 access){};
    u32 (*read_func)(void *ptr, u32 addr, u8 sz, u8 access, bool has_effect){};
    void (*write_func)(void *ptr, u32 addr, u8 sz, u8 access, u32 val){};

    arm9_ins opcode_table_arm[4096]{};
    arm9_ins opcode_table_arm_never[4096]{};
    thumb2_instruction opcode_table_thumb[65536]{};

    DBG_START
        DBG_EVENT_VIEW
        DBG_TRACE_VIEW
        DBG_LOG_VIEW
    DBG_END
};
}
