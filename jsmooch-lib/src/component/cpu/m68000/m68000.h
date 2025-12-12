//
// Created by RadDad772 on 4/14/24.
//

#pragma once

#include "helpers/debugger/debuggerdefs.h"
#include "helpers/int.h"
#include "helpers/cvec.h"
#include "m68000_instructions.h"

#define M68K_E_CLOCK
#define M68K_TESTING
struct disassembly_entry;
struct serialized_state;
namespace M68k {

enum OS {
    OS_pause1 = 0,
    OS_prefetch1 = 1,
    OS_pause2 = 2,
    OS_prefetch2 = 3,
    OS_read1 = 4,
    OS_read2 = 5,
};

enum interrupt_vectors {
    IV_reset = 0,
    IV_bus_error = 2,
    IV_address_error = 3,
    IV_illegal_instruction = 4,
    IV_zero_divide = 5,
    IV_chk_instruction = 6,
    IV_trapv_instruction = 7,
    IV_privilege_violation = 8,
    IV_trace = 9,
    IV_line1010 = 10,
    IV_line1111 = 11,
    //IV_format_error = 14, // MC68010 and later only
    IV_uninitialized_irq = 15,
    IV_spurious_interrupt = 24, // bus error during IRQ
    IV_interrupt_l1_autovector = 25,
    IV_interrupt_l2_autovector = 26,
    IV_interrupt_l3_autovector = 27,
    IV_interrupt_l4_autovector = 28,
    IV_interrupt_l5_autovector = 29,
    IV_interrupt_l6_autovector = 30,
    IV_interrupt_l7_autovector = 31,
    IV_trap_base = 32,
    IV_user_base = 64
};


enum function_codes {
    FC_user_data = 1,
    FC_user_program = 2,
    FC_supervisor_data = 5,
    FC_supervisor_program = 6,
    FC_cpu_space = 7
};

struct regs {
    void serialize(serialized_state &state);
    void deserialize(serialized_state &state);
    /*
     * When a data register is used as either a source or a destination operand,
     * only the appropriate low-order portion is changed{}; the remaining high-order
     * portion is neither used nor changed.
     */
    u32 D[8]{};
    u32 A[8]{};
    u32 IPC{};
    u32 PC{};

    u32 ASP{}; // Alternate stack pointer, holds alternate stack pointer.
    union {
        struct {
            u16 C: 1; // carry    1
            u16 V: 1; // overflow 2
            u16 Z: 1; // zero     4
            u16 N: 1; // negative 8
            u16 X: 1; // extend   10
            u16 _: 3;

            u16 I: 3;  // interrupt priority mask
            u16 _2: 2;
            u16 S: 1; // supervisor state
            u16 _3: 1;
            u16 T: 1; // trace mode
        };
        u16 u{};
    } SR{};

    // Prefetch registers
    u32 IRC{}; // holds last word prefetched from external memory
    u32 IR{}; // instruction currently decoding
    u32 IRD{}; // instruction currently executing

    u32 next_SR_T{}; // temporary register
};

struct pins {
    void serialize(serialized_state &state);
    void deserialize(serialized_state &state);
    u8 FC{}; // Function codes
    u32 Addr{};
    u16 D{};
    u8 DTACK{}; // Data ack. set externally
    u8 VPA{}; // VPA valid peripheral address, for autovectored interrupts and old peripherals for 6800 processors
    u8 VMA{}; // VMA is a signalling thing for VPA meaning valid memory address
    u8 AS{}; // Address Strobe. set interally
    u8 RW{}; // Read-write
    u8 IPL{}; // Interrupt request level
    u8 UDS{};
    u8 LDS{};

    u8 RESET{};
};

enum states {
    S_read8 = 0, // 0
    S_read16, // 1
    S_read32, // 2
    S_write8, // 3
    S_write16, // 4
    S_write32, // 5
    S_decode, // decode 6
    S_read_operands, // operands 7
    S_write_operand, // operands 8
    S_exec, // execute 9
    S_prefetch,
    S_wait_cycles, // wait cycles to outside world
    S_exc_group0,
    S_exc_group12,
    S_stopped,
    S_bus_cycle_iaq,
    S_exc_interrupt,
};

struct ins_t;

struct iack_handler {
    void (*handler)(void*);
    void *ptr;
};

struct core {
    explicit core(bool megadrive_bug_in);
    void setup_tracing(const jsm_debug_read_trace *strct, u64 *trace_cycle_pointer);
    regs regs{};
    pins pins{};

    std::vector<iack_handler> iack_handlers{};

    u32 ins_decoded{};
    u32 testing{};
    u32 opc{};

    bool megadrive_bug{};

    struct {
        u32 ins_PC{};
    } debug{};


    struct {
        states current{};
        bool nmi{};
        u32 internal_interrupt_level{};

        struct {
            u32 TCU{}; // Subcycle of like rmw8 etc.
            u32 reversed{};
            u32 addr{};
            u32 original_addr{};
            u32 data{};
            u32 done{};
            i32 e_phase{};
            void (core::*func)(){};
            states next_state{};
            u32 FC{};

            u32 addr_error{};
        } bus_cycle{};

        struct {
            i32 num{};
            u32 TCU{};
            states next_state{};
            u32 FC{};
        } prefetch{};

        struct {
            u32 state[6]{}; // which of the 4 from enum M68kOS are left

            u32 TCU{};

            u32 cur_op_num{};
            // Op1 and 2 after fetch
            u32 temp{};
            u32 fast{};
            u32 sz{};
            u32 allow_reverse{};
            u32 prefetch[2]{};
            u32 read_fc{};

            EA *ea{};
            states next_state{};
        } operands{};


        struct {
            i32 cycles_left{};
            states next_state{};
        } wait_cycles{};

        struct {
            u32 TCU{};
            u32 done{};
            u32 result{}; // used for results
            i32 prefetch{};
            u32 temp{};
            u32 temp2{};
            // Op1 and 2 addresses
        } instruction{};

        struct {
            u32 addr{};
            u32 ext_words{};
            u32 val{};
            u32 reversed{};
            u32 t, t2{};

            u32 held, hold{};
            u32 new_val{};
        } op[2]{};

        struct {
            u32 group{};
            struct {
                u32 vector{};
                u32 TCU{};

                u32 normal_state{};
                u32 addr{};
                u32 IR{};
                u32 SR{};
                u32 PC{};
                u32 first_word{};
                u32 base_addr{};
            } group0{};

            struct {
                u32 vector{};
                u32 TCU{};
                i32 wait_cycles{};

                u32 base_addr{};
                u32 SR{};
                u32 PC{};
                u32 group{};
            } group12{};

            struct {
                u32 TCU{};

                // Indicates we have an interrupt to process
                bool on_next_instruction{};
                u32 new_I{};

                // For saving
                u32 base_addr{};
                u32 SR{};
                u32 PC{};
            } interrupt{};

            u32 group1_pending{};
        } exception{};

        struct {
            states next_state{};
        } stopped{};

        struct {
            u32 ilevel{};
            u32 TCU{};
            u32 autovectored{};
            u32 vector_number{};
        } bus_cycle_iaq{};

        //u64 e_phase{};
        u32 e_clock_count{};
    } state{};

    struct {
        jsm_debug_read_trace strct{};
        jsm_string str{1000};
        u32 ok{};
        u64 *cycles{};
        u64 my_cycles{};
    } trace{};

    u32 last_decode{};
    ins_t *ins{};
    ins_t SPEC_RESET{};

private:
    void decode();
    void trace_format();
    u32 process_interrupts();
    void lycoder_pprint1();
    void lycoder_pprint2() const;
    void prefetch();
    void sample_interrupts();
    void read_operands();
    void swap_ASP();
    void bus_cycle_iaq();
    void exc_interrupt();
    void exc_group0();
    void exc_group12();
    u32 serialize_func() const;
    void deserialize_func(u32 fn);
    u32 get_dr(u32 num, u32 sz) const;
    u32 get_ar(u32 num, u32 sz) const;
    void adjust_IPC(u32 opnum, u32 sz);
    bool am_in_group0_or_1() const;
    void bus_cycle_read();
    void start_group0_exception(u32 vector_number, i32 wait_cycles, bool was_in_group0_or_1, u32 addr);
    void start_group12_exception(u32 vector_number, i32 wait_cycles, u32 PC, u32 groupnum);
    void start_wait(u32 num, states state_after);
    void start_group2_exception(u32 vector_number, i32 wait_cycles, u32 PC);
    void start_group1_exception(u32 vector_number, i32 wait_cycles, u32 PC);
    void bus_cycle_write();
    void start_prefetch(u32 num, u32 is_program, states next_state);
    void start_read(u32 addr, u32 sz, u32 FC, u32 reversed, states next_state);
    void start_write(u32 addr, u32 val, u32 sz, u32 FC, u32 reversed, states next_state);
    void set_dr(u32 num, u32 result, u32 sz);
    void set_ar(u32 num, u32 result, u32 sz);
    u32 read_ea_addr(uint32 opnum, u32 sz, bool hold, u32 prefetch);
    u32 write_ea_addr(const EA *ea, bool commit, u32 opnum);
    void start_write_operand(u32 commit, u32 op_num, states next_state, bool allow_reverse, bool force_reverse);
    void eval_ea_wait(u32 num_ea, u32 opnum, u32 sz);
    void start_read_operand_for_ea(u32 fast, u32 sz, states next_state, u32 wait_states, bool hold, bool allow_reverse);
    u32 get_r(const EA *ea, u32 sz) const;
    void start_read_operands(u32 fast, u32 sz, states next_state, u32 wait_states, bool hold, bool allow_reverse, bool read_fc);
    u32 inc_SSP(u32 num);
    u32 dec_SSP(u32 num);
    u32 get_SSP() const;
    void set_SSP(u32 to);
    void finalize_ea(u32 opnum);
    void read_operands_prefetch(u32 opnum);
    void read_operands_read(u32 opnum, bool commit);
    void set_r(const EA *ea, u32 val, u32 sz);
    void start_push(u32 val, u32 sz, states next_state, u32 reverse);
    void start_pop(u32 sz, u32 FC, states next_state);
    u32 ADD(u32 op1, u32 op2, u32 sz, u32 extend);
    u32 AND(u32 source, u32 target, u32 sz);
    u32 ASL(u32 result, u32 shift, u32 sz);
    u32 ASR(u32 result, u32 shift, u32 sz);
    u32 CMP(u32 source, u32 target, u32 sz);
    u32 ROL(u32 result, u32 shift, u32 sz);
    u32 ROR(u32 result, u32 shift, u32 sz);
    u32 ROXL(u32 result, u32 shift, u32 sz);
    u32 ROXR(u32 result, u32 shift, u32 sz);
    u32 SUB(u32 op1, u32 op2, u32 sz, u32 extend, u32 change_x);
    u32 LSL(u32 result, u32 shift, u32 sz);
    u32 LSR(u32 result, u32 shift, u32 sz);
    u32 EOR(u32 source, u32 target, u32 sz);
    u32 OR(u32 source, u32 target, u32 sz);
    [[nodiscard]] u32 condition(u32 condition) const;
    void set_IPC();
    void correct_IPC_MOVE_l_pf0(u32 opnum);

public:
#define INS(x) void ins_##x()
    INS(RESET_POWER);
    INS(ALINE);
    INS(FLINE);
    INS(TAS);
    INS(ABCD);
    INS(ADD);
    INS(ADDA);
    INS(ADDI);
    INS(ADDQ_ar);
    INS(ADDQ);
    INS(ADDX_sz4_predec);
    INS(ADDX_sz4_nopredec);
    INS(ADDX_sz1_2);
    INS(ADDX);
    INS(AND);
    INS(ANDI);
    INS(ANDI_TO_CCR);
    INS(ANDI_TO_SR);
    INS(ASL_qimm_dr);
    INS(ASL_dr_dr);
    INS(ASL_ea);
    INS(ASR_qimm_dr);
    INS(ASR_dr_dr);
    INS(ASR_ea);
    INS(BCC);
    INS(BCHG_dr_ea);
    INS(BCHG_ea);
    INS(BCLR_dr_ea);
    INS(BCLR_ea);
    INS(BRA);
    INS(BSET_dr_ea);
    INS(BSET_ea);
    INS(BSR);
    INS(BTST_dr_ea);
    INS(BTST_ea);
    INS(CHK);
    INS(CLR);
    INS(CMP);
    INS(CMPA);
    INS(CMPI);
    INS(CMPM);
    INS(DBCC);
    INS(DIVS);
    INS(DIVU);
    INS(EOR);
    INS(EORI);
    INS(EORI_TO_CCR);
    INS(EORI_TO_SR);
    INS(EXG);
    INS(EXT);
    INS(ILLEGAL);
    INS(JMP);
    INS(JSR);
    INS(LEA);
    INS(LINK);
    INS(LSL_qimm_dr);
    INS(LSL_dr_dr);
    INS(LSL_ea);
    INS(LSR_qimm_dr);
    INS(LSR_dr_dr);
    INS(LSR_ea);
    INS(MOVE);
    INS(MOVEA);
    INS(MOVEM_TO_MEM);
    INS(MOVEM_TO_REG);
    INS(MOVEP_dr_ea);
    INS(MOVEP_ea_dr);
    INS(MOVEP);
    INS(MOVEQ);
    INS(MOVE_FROM_SR);
    INS(MOVE_FROM_USP);
    INS(MOVE_TO_CCR);
    INS(MOVE_TO_SR);
    INS(MOVE_TO_USP);
    INS(MULS);
    INS(MULU);
    INS(NBCD);
    INS(NEG);
    INS(NEGX);
    INS(NOP);
    INS(NOT);
    INS(OR);
    INS(ORI);
    INS(ORI_TO_CCR);
    INS(ORI_TO_SR);
    INS(PEA);
    INS(RESET);
    INS(ROL_qimm_dr);
    INS(ROL_dr_dr);
    INS(ROL_ea);
    INS(ROR_qimm_dr);
    INS(ROR_dr_dr);
    INS(ROR_ea);
    INS(ROXL_qimm_dr);
    INS(ROXL_dr_dr);
    INS(ROXL_ea);
    INS(ROXR_qimm_dr);
    INS(ROXR_dr_dr);
    INS(ROXR_ea);
    INS(RTE);
    INS(RTR);
    INS(RTS);
    INS(SBCD);
    INS(SCC);
    INS(STOP);
    INS(SUB);
    INS(SUBA);
    INS(SUBI_21);
    INS(SUBI);
    INS(SUBQ);
    INS(SUBQ_ar);
    INS(SUBX_sz4_predec);
    INS(SUBX_sz4_nopredec);
    INS(SUBX_sz1_2);
    INS(SUBX);
    INS(SWAP);
    INS(TRAP);
    INS(TRAPV);
    INS(TST);
    INS(UNLK);
    INS(NOINS);
#undef INS

    void serialize(serialized_state &m_state);
    void deserialize(serialized_state &m_state);

    void unstop();
    void reset();
    void cycle();
    void set_SR(u32 val, u32 immediate_t);
    [[nodiscard]] u32 get_SR() const;
    void pprint_ea(ins_t &ins, u32 opnum, jsm_string *outstr) const;
    void register_iack_handler(void *ptr, void (*handler)(void*));
    void set_interrupt_level(u32 val);
    void disassemble_entry(disassembly_entry& entry);
    DBG_EVENT_VIEW_ONLY_START
    IRQ
    DBG_EVENT_VIEW_ONLY_END

};

}