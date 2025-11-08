//
// Created by . on 6/27/25.
//

#ifndef JSMOOCH_EMUS_M680X0_H
#define JSMOOCH_EMUS_M680X0_H

#include "helpers/debugger/debuggerdefs.h"
#include "helpers/debugger/debugger.h"
#include "helpers/debug.h"
#include "helpers/int.h"
#include "helpers/cvec.h"

enum M68x_kinds {
    M68K_M68000,
    M68K_M68008,
    M68K_M68010,
    M68K_M68020,
    M68K_M68030
};

enum M68x_interrupt_vectors {
    M68xIV_reset = 0,
    M68xIV_bus_error = 2,
    M68xIV_address_error = 3,
    M68xIV_illegal_instruction = 4,
    M68xIV_zero_divide = 5,
    M68xIV_chk_instruction = 6,
    M68xIV_trapv_instruction = 7,
    M68xIV_privilege_violation = 8,
    M68xIV_trace = 9,
    M68xIV_line1010 = 10,
    M68xIV_line1111 = 11,
    //M68xIV_format_error = 14, // MC68010 and later only
    M68xIV_uninitialized_irq = 15,
    M68xIV_spurious_interrupt = 24, // bus error during IRQ
    M68xIV_interrupt_l1_autovector = 25,
    M68xIV_interrupt_l2_autovector = 26,
    M68xIV_interrupt_l3_autovector = 27,
    M68xIV_interrupt_l4_autovector = 28,
    M68xIV_interrupt_l5_autovector = 29,
    M68xIV_interrupt_l6_autovector = 30,
    M68xIV_interrupt_l7_autovector = 31,
    M68xIV_trap_base = 32,
    M68xIV_user_base = 64
};

struct M68x_regs {
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

struct M68x_pins {
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

struct M68x_iack_handler {
    void (*handler)(void*);
    void *ptr;
};

struct M68x {
    struct M68x_regs regs;
    struct M68x_pins pins;

    struct cvec iack_handlers;

    u32 ins_decoded;
    u32 testing;
    u32 opc;

    u32 megadrive_bug;

    struct {
        u32 ins_PC;
    } debug;

    DBG_EVENT_VIEW_ONLY_START
            IRQ
    DBG_EVENT_VIEW_ONLY_END

    struct {
        //u64 e_phase;
        u32 e_clock_count;
    } state;

    struct {
        struct jsm_debug_read_trace strct;
        struct jsm_string str;
        u32 ok;
        u64 *cycles;
        u64 my_cycles;
    } trace;

    u32 last_decode;
};


#endif //JSMOOCH_EMUS_M680X0_H
