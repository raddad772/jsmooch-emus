//
// Created by Dave on 2/10/2024.
//

#pragma once

#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/scheduler.h"
#include "helpers/debugger/debuggerdefs.h"

#include "tmu.h"

// One of these for external mem system to write/read registers here,
// One for this to read/write main memory
namespace SH4 {
struct memaccess_t {
    void *ptr{};
    u64 (*read)(void*,u32,u8,bool*){};
    void (*write)(void*,u32,u64,u8,bool*){};
};


struct FV {
    float a{}, b{}, c{}, d{};
};

struct mtx {
    float xf0{}, xf1{}, xf2{}, xf3{};
    float xf4{}, xf5{}, xf6{}, xf7{};
    float xf8{}, xf9{}, xf10{}, xf11{};
    float xf12{}, xf13{}, xf14{}, xf15{};
};

union regs_SR {
    struct {
        u32 T : 1; // 0. True/false or carry/borrow flag
        u32 S : 1; // 1. Saturation for MAC instruction
        u32 _ : 2; // 2-3
        u32 IMASK : 4; // 4-7 interrupt mask bits
        u32 Q : 1; // 8 Used in DIV instructions
        u32 M : 1; // 9 Used in DIV instructions
        u32 _2 : 5; // 10-14
        u32 FD : 1; // 15. FPU disab le
        u32 _3 : 12; // 16-27
        u32 BL : 1; // 28. Exception/interrupt
        u32 RB : 1; // 29. Register bank select!
        u32 MD : 1; // 30. 1= privileged mode
        u32 _4: 1;
    };
    u32 u;
};
union regs_FPSCR {
    struct {
        u32 RM : 2; // 0-1. Rounding mode. 0=Nearest{}, 1=Zero{}, 2=Reserved{}, 3=Reserved
        u32 Flag : 5; // 2-6. exception flag              None{}, Invalid Op(V){}, /0(Z){}, Overflow(O){}, Underflow(U){}, Inexact(I)
        u32 Enable : 5; // 7-11. exception enable
        u32 Cause : 6; // 12-17. exception cause
        u32 DN : 1; // 18. Denormalization mode. 0 = do it{}, 1 = treat it as 0
        u32 PR : 1; // 19. Precision mode. 0 = single precision{}, 1 = double
        u32 SZ : 1; // 20. Size-transfer mode. 0=FMOV is 32-bit{}, 1=FMOV is 64-bit
        u32 FR : 1; // 21. Register bank select
        u32 _ : 10;
    };
    u32 u{};
};

struct REGS {
    void SR_set(u32 val);
    u32 SR_get();
    void FPSCR_set(u32 val);
    void FPSCR_bankswitch();
    void FPSCR_update(u32 old_RM, u32 old_DN);
    [[nodiscard]] u32 FPSCR_get() const;
    u32 R[16]{}; // registers
    u32 R_[8]{}; // shadow registers

    // -- accessed any time
    u32 GBR{}; // Global Bank Register
    regs_SR SR{};

    // -- accessed in privileged
    u32 SSR{}; // Saved Status Register
    u32 SPC{}; // Saved PC
    u32 VBR{}; // Vector Base Register
    u32 SGR{}; // Saved reg15
    u32 DBR{}; // Debug base register

    // System registers{}, any mode
    u32 MACL{}; // MAC-lo
    u32 MACH{}; // MAC-hi
    u32 PR{}; // Procedure register
    u32 PC{}; // Program Counter
    regs_FPSCR FPSCR{}; // Floating Point Status Control Register
    union {
        float f;
        u32 u{};
    } FPUL{};

    union {
        u32 U32[16];
        u64 U64[8]{};
        float FP32[16];
        double FP64[8];
        FV FV[4];
        mtx MTX;
    } fb[3]{};

    u32 QACR[2]{};  // 0xFF000038 + 3C
#include "generated/regs_decls.h"
private:
    void swap_register_banks();
};


// Number of SH4 interrupt sources
#define SH4I_NUM 27

struct clock {
    i64 timer_cycles{};
    i64 trace_cycles_blocks{};
};

enum IRQ_SOURCES {
    IRQ_nmi = 0,
    IRQ_irl,
    IRQ_hitachi_udi,
    IRQ_gpio_gpioi,
    IRQ_dmac_dmte0,
    IRQ_dmac_dmte1,
    IRQ_dmac_dmte2,
    IRQ_dmac_dmte3,
    IRQ_dmac_dmae,
    IRQ_tmu0_tuni0,
    IRQ_tmu1_tuni1,
    IRQ_tmu2_tuni2,
    IRQ_tmu2_ticpi2,
    IRQ_rtc_ati,
    IRQ_rtc_pri,
    IRQ_rtc_cui,
    IRQ_sci1_eri,
    IRQ_sci1_rxi,
    IRQ_sci1_txi,
    IRQ_sci1_tei,
    IRQ_scif_eri,
    IRQ_scif_rxi,
    IRQ_scif_bri,
    IRQ_scif_txi,
    IRQ_wdt_iti,
    IRQ_ref_rcmi,
    IRQ_ref_rovi
};

struct IRQ_SOURCE {
    IRQ_SOURCES source{};
    u32 priority{};
    u32 sub_priority{};
    u32 raised{};
    u32 intevt{};
};

struct ins_t;

struct core {
    explicit core(scheduler_t *scheduler_in);
    void reset();
    void setup_tracing(jsm_debug_read_trace *rt, u64 *trace_cycles_in);
    REGS regs{};
    clock clock{};

    i32 cycles{};

    i32 pp_last_m{};
    i32 pp_last_n{};
    u8 SQ[2][32]{}; // store queues!
    u8 OC[8 * 1024]{}; // Operand Cache!

    struct {
        jsm_string str{100};
        jsm_string str2{100};
        bool ok{};
        u64 *cycles{};
        u64 my_cycles{};

        u32 exception_id{};
        u32 ins_id{};
    } trace{};

    jsm_debug_read_trace read_trace{};

    IRQ_SOURCE interrupt_sources[SH4I_NUM]{};
    IRQ_SOURCE* interrupt_map[SH4I_NUM]{};
    u32 interrupt_highest_priority{}; // used to compare to IMASK
    TMU tmu;
    void trace_format(ins_t *ins);

    void *mptr{};
    u32 (*fetch_ins)(void* ptr,u32 addr){};
    u64 (*read)(void* ptr,u32 addr,u8 sz){};
    void (*write)(void*,u32 addr,u64 val,u8 sz){};

    jsm_string console{5000};

    scheduler_t* scheduler{};
    void run_cycles(u32 howmany);
    void set_IRL(u32 level);
    void interrupt_pend(IRQ_SOURCES source, u32 onoff);
    u64 ma_read(u32 addr, u8 sz, bool* success);
    void ma_write(u32 addr, u64 val, u8 sz, bool* success);
    void give_memaccess(memaccess_t* to);

    DBG_START
        DBG_LOG_VIEW
    DBG_END


private:
    void console_add(u32 val, u8 sz);
    void set_QACR(u32 num, u32 val);
    void lycoder_print(u32 opcode);
    void pprint(ins_t *ins, bool last_traces);
    void exec_interrupt();
    void IRQ_set_highest_priority();
    void fetch_and_exec(bool is_delay_slot);
    void interrupt();
    void sort_interrupts();
    void IPR_update();
    void do_sh4_decode();
    void ins_EMPTY(ins_t *ins);
    void ins_MOV(ins_t *ins);
    void ins_MOVI(ins_t *ins);
    void ins_MOVA(ins_t *ins);
    void ins_MOVWI(ins_t *ins);
    void ins_MOVLI(ins_t *ins);
    void ins_MOVBL(ins_t *ins);
    void ins_MOVWL(ins_t *ins);
    void ins_MOVLL(ins_t *ins);
    void ins_MOVBS(ins_t *ins);
    void ins_MOVWS(ins_t *ins);
    void ins_MOVLS(ins_t *ins);
    void ins_MOVBP(ins_t *ins);
    void ins_MOVWP(ins_t *ins);
    void ins_MOVLP(ins_t *ins);
    void ins_MOVBM(ins_t *ins);
    void ins_MOVWM(ins_t *ins);
    void ins_MOVLM(ins_t *ins);
    void ins_MOVBL4(ins_t *ins);
    void ins_MOVWL4(ins_t *ins);
    void ins_MOVLL4(ins_t *ins);
    void ins_MOVBS4(ins_t *ins);
    void ins_MOVWS4(ins_t *ins);
    void ins_MOVLS4(ins_t *ins);
    void ins_MOVBL0(ins_t *ins);
    void ins_MOVWL0(ins_t *ins);
    void ins_MOVLL0(ins_t *ins);
    void ins_MOVBS0(ins_t *ins);
    void ins_MOVWS0(ins_t *ins);
    void ins_MOVLS0(ins_t *ins);
    void ins_MOVBLG(ins_t *ins);
    void ins_MOVWLG(ins_t *ins);
    void ins_MOVLLG(ins_t *ins);
    void ins_MOVBSG(ins_t *ins);
    void ins_MOVWSG(ins_t *ins);
    void ins_MOVLSG(ins_t *ins);
    void ins_MOVT(ins_t *ins);
    void ins_SWAPB(ins_t *ins);
    void ins_SWAPW(ins_t *ins);
    void ins_XTRCT(ins_t *ins);
    void ins_ADD(ins_t *ins);
    void ins_ADDI(ins_t *ins);
    void ins_ADDC(ins_t *ins);
    void ins_ADDV(ins_t *ins);
    void ins_CMPIM(ins_t *ins);
    void ins_CMPEQ(ins_t *ins);
    void ins_CMPHS(ins_t *ins);
    void ins_CMPGE(ins_t *ins);
    void ins_CMPHI(ins_t *ins);
    void ins_CMPGT(ins_t *ins);
    void ins_CMPPL(ins_t *ins);
    void ins_CMPPZ(ins_t *ins);
    void ins_CMPSTR(ins_t *ins);
    void ins_DIV0S(ins_t *ins);
    void ins_DIV0U(ins_t *ins);
    void ins_DIV1(ins_t *ins);
    void ins_DMULS(ins_t *ins);
    void ins_DMULU(ins_t *ins);
    void ins_DT(ins_t *ins);
    void ins_EXTSB(ins_t *ins);
    void ins_EXTSW(ins_t *ins);
    void ins_EXTUB(ins_t *ins);
    void ins_EXTUW(ins_t *ins);
    void ins_MACL(ins_t *ins);
    void ins_MACW(ins_t *ins);
    void ins_MULL(ins_t *ins);
    void ins_MULS(ins_t *ins);
    void ins_MULU(ins_t *ins);
    void ins_NEG(ins_t *ins);
    void ins_NEGC(ins_t *ins);
    void ins_SUB(ins_t *ins);
    void ins_SUBC(ins_t *ins);
    void ins_SUBV(ins_t *ins);
    void ins_AND(ins_t *ins);
    void ins_ANDI(ins_t *ins);
    void ins_ANDM(ins_t *ins);
    void ins_NOT(ins_t *ins);
    void ins_OR(ins_t *ins);
    void ins_ORI(ins_t *ins);
    void ins_ORM(ins_t *ins);
    void ins_TAS(ins_t *ins);
    void ins_TST(ins_t *ins);
    void ins_TSTI(ins_t *ins);
    void ins_TSTM(ins_t *ins);
    void ins_XOR(ins_t *ins);
    void ins_XORI(ins_t *ins);
    void ins_XORM(ins_t *ins);
    void ins_ROTCL(ins_t *ins);
    void ins_ROTCR(ins_t *ins);
    void ins_ROTL(ins_t *ins);
    void ins_ROTR(ins_t *ins);
    void ins_SHAD(ins_t *ins);
    void ins_SHAL(ins_t *ins);
    void ins_SHAR(ins_t *ins);
    void ins_SHLD(ins_t *ins);
    void ins_SHLL(ins_t *ins);
    void ins_SHLL2(ins_t *ins);
    void ins_SHLL8(ins_t *ins);
    void ins_SHLL16(ins_t *ins);
    void ins_SHLR(ins_t *ins);
    void ins_SHLR2(ins_t *ins);
    void ins_SHLR8(ins_t *ins);
    void ins_SHLR16(ins_t *ins);
    void ins_BF(ins_t *ins);
    void ins_BFS(ins_t *ins);
    void ins_BT(ins_t *ins);
    void ins_BTS(ins_t *ins);
    void ins_BRA(ins_t *ins);
    void ins_BRAF(ins_t *ins);
    void ins_BSR(ins_t *ins);
    void ins_BSRF(ins_t *ins);
    void ins_JMP(ins_t *ins);
    void ins_JSR(ins_t *ins);
    void ins_RTS(ins_t *ins);
    void ins_CLRMAC(ins_t *ins);
    void ins_CLRS(ins_t *ins);
    void ins_CLRT(ins_t *ins);
    void ins_LDCSR(ins_t *ins);
    void ins_LDCMSR(ins_t *ins);
    void ins_LDCGBR(ins_t *ins);
    void ins_LDCMGBR(ins_t *ins);
    void ins_LDCVBR(ins_t *ins);
    void ins_LDCMVBR(ins_t *ins);
    void ins_LDCSSR(ins_t *ins);
    void ins_LDCMSSR(ins_t *ins);
    void ins_LDCSPC(ins_t *ins);
    void ins_LDCMSPC(ins_t *ins);
    void ins_LDCDBR(ins_t *ins);
    void ins_LDCMDBR(ins_t *ins);
    void ins_LDCRn_BANK(ins_t *ins);
    void ins_LDCMRn_BANK(ins_t *ins);
    void ins_LDSMACH(ins_t *ins);
    void ins_LDSMMACH(ins_t *ins);
    void ins_LDSMACL(ins_t *ins);
    void ins_LDSMMACL(ins_t *ins);
    void ins_LDSPR(ins_t *ins);
    void ins_LDSMPR(ins_t *ins);
    void ins_LDTLB(ins_t *ins);
    void ins_MOVCAL(ins_t *ins);
    void ins_NOP(ins_t *ins);
    void ins_OCBI(ins_t *ins);
    void ins_OCBP(ins_t *ins);
    void ins_OCBWB(ins_t *ins);
    void ins_PREF(ins_t *ins);
    void ins_RTE(ins_t *ins);
    void ins_SETS(ins_t *ins);
    void ins_SETT(ins_t *ins);
    void ins_SLEEP(ins_t *ins);
    void ins_STCSR(ins_t *ins);
    void ins_STCMSR(ins_t *ins);
    void ins_STCGBR(ins_t *ins);
    void ins_STCMGBR(ins_t *ins);
    void ins_STCVBR(ins_t *ins);
    void ins_STCMVBR(ins_t *ins);
    void ins_STCSGR(ins_t *ins);
    void ins_STCMSGR(ins_t *ins);
    void ins_STCSSR(ins_t *ins);
    void ins_STCMSSR(ins_t *ins);
    void ins_STCSPC(ins_t *ins);
    void ins_STCMSPC(ins_t *ins);
    void ins_STCDBR(ins_t *ins);
    void ins_STCMDBR(ins_t *ins);
    void ins_STCRm_BANK(ins_t *ins);
    void ins_STCMRm_BANK(ins_t *ins);
    void ins_STSMACH(ins_t *ins);
    void ins_STSMMACH(ins_t *ins);
    void ins_STSMACL(ins_t *ins);
    void ins_STSMMACL(ins_t *ins);
    void ins_STSPR(ins_t *ins);
    void ins_STSMPR(ins_t *ins);
    void ins_TRAPA(ins_t *ins);
    void ins_FMOV(ins_t *ins);
    void ins_FMOV_LOAD(ins_t *ins);
    void ins_FMOV_STORE(ins_t *ins);
    void ins_FMOV_RESTORE(ins_t *ins);
    void ins_FMOV_SAVE(ins_t *ins);
    void ins_FMOV_INDEX_LOAD(ins_t *ins);
    void ins_FMOV_INDEX_STORE(ins_t *ins);
    void ins_FMOV_DR(ins_t *ins);
    void ins_FMOV_DRXD(ins_t *ins);
    void ins_FMOV_XDDR(ins_t *ins);
    void ins_FMOV_XDXD(ins_t *ins);
    void ins_FMOV_LOAD_DR(ins_t *ins);
    void ins_FMOV_LOAD_XD(ins_t *ins);
    void ins_FMOV_STORE_DR(ins_t *ins);
    void ins_FMOV_STORE_XD(ins_t *ins);
    void ins_FMOV_RESTORE_DR(ins_t *ins);
    void ins_FMOV_RESTORE_XD(ins_t *ins);
    void ins_FMOV_SAVE_DR(ins_t *ins);
    void ins_FMOV_SAVE_XD(ins_t *ins);
    void ins_FMOV_INDEX_LOAD_DR(ins_t *ins);
    void ins_FMOV_INDEX_LOAD_XD(ins_t *ins);
    void ins_FMOV_INDEX_STORE_DR(ins_t *ins);
    void ins_FMOV_INDEX_STORE_XD(ins_t *ins);
    void ins_FLDI0(ins_t *ins);
    void ins_FLDI1(ins_t *ins);
    void ins_FLDS(ins_t *ins);
    void ins_FSTS(ins_t *ins);
    void ins_FABS(ins_t *ins);
    void ins_FNEG(ins_t *ins);
    void ins_FADD(ins_t *ins);
    void ins_FSUB(ins_t *ins);
    void ins_FMUL(ins_t *ins);
    void ins_FMAC(ins_t *ins);
    void ins_FDIV(ins_t *ins);
    void ins_FSQRT(ins_t *ins);
    void ins_FCMP_EQ(ins_t *ins);
    void ins_FCMP_GT(ins_t *ins);
    void ins_FLOAT_single(ins_t *ins);
    void ins_FTRC_single(ins_t *ins);
    void ins_FIPR(ins_t *ins);
    void ins_FTRV(ins_t *ins);
    void ins_FABSDR(ins_t *ins);
    void ins_FNEGDR(ins_t *ins);
    void ins_FADDDR(ins_t *ins);
    void ins_FSUBDR(ins_t *ins);
    void ins_FMULDR(ins_t *ins);
    void ins_FDIVDR(ins_t *ins);
    void ins_FSQRTDR(ins_t *ins);
    void ins_FCMP_EQDR(ins_t *ins);
    void ins_FCMP_GTDR(ins_t *ins);
    void ins_FLOAT_double(ins_t *ins);
    void ins_FTRC_double(ins_t *ins);
    void ins_FCNVDS(ins_t *ins);
    void ins_FCNVSD(ins_t *ins);
    void ins_LDSFPSCR(ins_t *ins);
    void ins_STSFPSCR(ins_t *ins);
    void ins_LDSMFPSCR(ins_t *ins);
    void ins_STSMFPSCR(ins_t *ins);
    void ins_LDSFPUL(ins_t *ins);
    void ins_STSFPUL(ins_t *ins);
    void ins_LDSMFPUL(ins_t *ins);
    void ins_STSMFPUL(ins_t *ins);
    void ins_FRCHG(ins_t *ins);
    void ins_FSCHG(ins_t *ins);
    void ins_FSRRA(ins_t *ins);
    void ins_FSCA(ins_t *ins);

};
}