//
// Created by RadDad772 on 4/14/24.
//

#ifndef JSMOOCH_EMUS_M68000_H
#define JSMOOCH_EMUS_M68000_H

#include "helpers/int.h"
#include "m68000_instructions.h"

#define M68K_E_CLOCK
//#define M68K_TESTING


enum M68kOS {
    M68kOS_pause1 = 0,
    M68kOS_prefetch1 = 1,
    M68kOS_pause2 = 2,
    M68kOS_prefetch2 = 3,
    M68kOS_read1 = 4,
    M68kOS_read2 = 5,
};

enum M68k_interrupt_vectors {
    M68kIV_reset = 0,
    M68kIV_bus_error = 2,
    M68kIV_address_error = 3,
    M68kIV_illegal_instruction = 4,
    M68kIV_zero_divide = 5,
    M68kIV_chk_instruction = 6,
    M68kIV_trapv_instruction = 7,
    M68kIV_privilege_violation = 8,
    M68kIV_trace = 9,
    M68kIV_line1010 = 10,
    M68kIV_line1111 = 11,
    //M68kIV_format_error = 14, // MC68010 and later only
    M68kIV_uninitialized_irq = 15,
    M68kIV_spurious_interrupt = 24, // bus error during IRQ
    M68kIV_interrupt_l1_autovector = 25,
    M68kIV_interrupt_l2_autovector = 26,
    M68kIV_interrupt_l3_autovector = 27,
    M68kIV_interrupt_l4_autovector = 28,
    M68kIV_interrupt_l5_autovector = 29,
    M68kIV_interrupt_l6_autovector = 30,
    M68kIV_interrupt_l7_autovector = 31,
    M68kIV_trap_base = 32,
    M68kIV_user_base = 64
};


enum M68k_function_codes {
    M68k_FC_user_data = 1,
    M68k_FC_user_program = 2,
    M68k_FC_supervisor_data = 5,
    M68k_FC_supervisor_program = 6,
    M68k_FC_cpu_space = 7
};

struct M68k_regs {
    /*
     * When a data register is used as either a source or a destination operand,
     * only the appropriate low-order portion is changed; the remaining high-order
     * portion is neither used nor changed.
     */
    u32 D[8];
    u32 A[8];
    u32 IPC;
    u32 PC;

    u32 ASP; // Alternate stack pointer, holds alternate stack pointer.
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
        u16 u;
    } SR;

    // Prefetch registers
    u32 IRC; // holds last word prefetched from external memory
    u32 IR; // instruction currently decoding
    u32 IRD; // instruction currently executing

    u32 next_SR_T; // temporary register
};

struct M68k_pins {
    u32 FC; // Function codes
    u32 Addr;
    u32 D;
    u32 DTACK; // Data ack. set externally
    u32 VPA; // VPA valid peripheral address, for autovectored interrupts and old peripherals for 6800 processors
    u32 VMA; // VMA is a signalling thing for VPA meaning valid memory address
    u32 AS; // Address Strobe. set interally
    u32 RW; // Read-write
    u32 IPL; // Interrupt request level
    u32 UDS;
    u32 LDS;

    u32 RESET;
};

enum M68k_states {
    M68kS_read8 = 0, // 0
    M68kS_read16, // 1
    M68kS_read32, // 2
    M68kS_write8, // 3
    M68kS_write16, // 4
    M68kS_write32, // 5
    M68kS_decode, // decode 6
    M68kS_read_operands, // operands 7
    M68kS_write_operand, // operands 8
    M68kS_exec, // execute 9
    M68kS_prefetch,
    M68kS_wait_cycles, // wait cycles to outside world
    M68kS_exc_group0,
    M68kS_exc_group12,
    M68kS_stopped,
    M68kS_bus_cycle_iaq,
    M68kS_exc_interrupt,
};

struct M68k_ins_t;

struct M68k {
    struct M68k_regs regs;
    struct M68k_pins pins;

    u32 ins_decoded;
    u32 testing;
    u32 opc;

    u32 megadrive_bug;

    struct {
        enum M68k_states current;
        u32 nmi;
        u32 internal_interrupt_level;

        struct {
            u32 TCU; // Subcycle of like rmw8 etc.
            u32 reversed;
            u32 addr;
            u32 original_addr;
            u32 data;
            u32 done;
            i32 e_phase;
            void (*func)(struct M68k*);
            enum M68k_states next_state;
            u32 FC;

            u32 addr_error;
        } bus_cycle;

        struct {
            i32 num;
            u32 TCU;
            enum M68k_states next_state;
            u32 FC;
        } prefetch;

        struct {
            u32 state[6]; // which of the 4 from enum M68kOS are left

            u32 TCU;

            u32 cur_op_num;
            // Op1 and 2 after fetch
            u32 temp;
            u32 fast;
            u32 sz;
            u32 allow_reverse;
            u32 prefetch[2];
            u32 read_fc;

            struct M68k_EA *ea;
            enum M68k_states next_state;
        } operands;


        struct {
            i32 cycles_left;
            enum M68k_states next_state;
        } wait_cycles;

        struct {
            u32 TCU;
            u32 done;
            u32 result; // used for results
            i32 prefetch;
            u32 temp;
            u32 temp2;
            // Op1 and 2 addresses
        } instruction;

        struct {
            u32 addr;
            u32 ext_words;
            u32 val;
            u32 reversed;
            u32 t, t2;

            u32 held, hold;
            u32 new_val;
        } op[2];

        struct {
            u32 group;
            struct {
                u32 vector;
                u32 TCU;

                u32 normal_state;
                u32 addr;
                u32 IR;
                u32 SR;
                u32 PC;
                u32 first_word;
                u32 base_addr;
            } group0;

            struct {
                u32 vector;
                u32 TCU;
                i32 wait_cycles;

                u32 base_addr;
                u32 SR;
                u32 PC;
                u32 group;
            } group12;

            struct {
                u32 TCU;

                // Indicates we have an interrupt to process
                u32 on_next_instruction;
                u32 new_I;

                // For saving
                u32 base_addr;
                u32 SR;
                u32 PC;
            } interrupt;

            u32 group1_pending;
        } exception;

        struct {
            enum M68k_states next_state;
        } stopped;

        struct {
            u32 ilevel;
            u32 TCU;
            u32 autovectored;
            u32 vector_number;
        } bus_cycle_iaq;

        //u64 e_phase;
        u32 e_clock_count;
    } state;

    struct {
        struct jsm_debug_read_trace strct;
        struct jsm_string str;
        u32 ok;
        u64 *cycles;
        u32 *hpos;
    } trace;

    struct M68k_ins_t *ins;
    struct M68k_ins_t SPEC_RESET;
};

void M68k_cycle(struct M68k* this);
void M68k_init(struct M68k* this, u32 megadrive_bug);
void M68k_delete(struct M68k* this);
void M68k_reset(struct M68k* this);
void M68k_setup_tracing(struct M68k* this, struct jsm_debug_read_trace *strct, u64 *trace_cycle_pointer);
void M68k_unstop(struct M68k* this);
void M68k_set_interrupt_level(struct M68k* this, u32 val);

void M68k_set_SR(struct M68k* this, u32 val, u32 immediate_t);
u32 M68k_get_SR(struct M68k* this);

#endif //JSMOOCH_EMUS_M68000_H
