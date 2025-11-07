//
// Created by . on 4/16/25.
//

#ifndef JSMOOCH_EMUS_WDC65816_H
#define JSMOOCH_EMUS_WDC65816_H

#include "helpers_c/int.h"

#include "helpers_c/debugger/debuggerdefs.h"
#include "helpers_c/debugger/debugger.h"
#include "helpers_c/debug.h"


#define WDC65816_OP_RESET 0x100
#define WDC65816_OP_NMI 0x103
#define WDC65816_OP_IRQ 0x102
#define WDC65816_OP_ABORT 0x101

struct WDC65816_ctxt {
    u32 regs; // bits 0-15: regs 0-15. bit 16: CPSR, etc...
};

struct WDC65816_regs;
struct WDC65816_pins;
typedef void (*WDC65816_ins_func)(struct WDC65816_regs*, struct WDC65816_pins*);

union WDC65816_P {
    struct {
        u8 C : 1;
        u8 Z : 1;
        u8 I: 1;
        u8 D : 1;
        u8 X : 1;
        u8 M : 1;
        u8 V : 1;
        u8 N : 1;
    };
    u8 v;
};

struct WDC65816_regs {
    // Hidden registers used internally to track state
    u32 RES_pending;
    u16 IR; // Instruction register. Holds opcode.
    u8 TCU; // Timing Control Unit, counts up during execution. Set to 0 at instruction fetch, incremented every cycle thereafter
    u32 MD; // Memory Data Register, holds last known "good" RAM value from a read. Mostly this is not used correctly inside the emukator
    i32 TR; // Temp Register, for operations
    u32 TA; // Temporary register, for operations, usually used for addresses
    u8 skipped_cycle; // For when we...skip a cycle! For timing of jumps and such
    u8 STP; // Are we in STOP?
    u8 WAI; // Are we in WAIT?

    // Registers exposed to programs
    u16 C; // B (high 8 bits) and A (low 8 bits) = C.
    u32 D; // Direct Page register
    u32 X; // X index
    u32 Y; // Y index
    union WDC65816_P P;
    u16 PBR; // Program Bank Register
    u32 PC; // Program Counter
    u32 S; // Stack pointer
    u32 DBR; // Data Bank Register
    u32 E; // Hidden "Emulation" bit


    u8 old_I; // old I flag, for use timing IRQs
    u8 NMI_pending;
    u8 IRQ_pending;
    u32 interrupt_pending;
    i32 NMI_old;
};

struct WDC65816_pins {
    u8 VPB; // Output. Vector Pull, gets folded into PDV
    u8 ABORT; // Input. Abort. Not sure if emulated?
    u8 IRQ; // in. IRQ
    u8 ML; // out. Not emulated
    u8 NMI; // in. NMIs
    u8 VPA; // out. Valid Program Address, gets folded into PDV
    u8 VDA; // out. Valid Data Address, gets folded into PDV
    u32 Addr; // out. Address pins 0-15
    u8 BA; // out. Bank Address, 8 bits
    u32 D; // in/out Data in/out, 8 bits
    u8 RW; // 0 = reading, 1 = writing, *IF* PDV asserted
    u8 E; // state of Emulation bit
    u8 MX; // M and X flags set. Not emulated
    u8 RES; // RESET signal. Not emulated

    u8 PDV; // combined program, data, vector pin, to simplify.
};

struct WDC65816 {
    struct WDC65816_pins pins;
    struct WDC65816_regs regs;

    u64 *master_clock;
    u64 int_clock;

    struct {
        struct jsm_debug_read_trace strct;
        struct jsm_string str, str2;
        u32 ok;
        u32 ins_PC;
        u32 PCO;
        i32 source_id;

        struct {
            struct dbglog_view *view;
            u32 id;
        } dbglog;

    } trace;

    WDC65816_ins_func ins;

    DBG_START
        DBG_EVENT_VIEW_START
            IRQ, NMI
        DBG_EVENT_VIEW_END
        DBG_TRACE_VIEW
    DBG_END

};

void WDC65816_cycle(struct WDC65816*);
void WDC65816_init(struct WDC65816*, u64 *master_clock);
void WDC65816_delete(struct WDC65816*);
void WDC65816_reset(struct WDC65816*);
void WDC65816_setup_tracing(struct WDC65816*, struct jsm_debug_read_trace *strct);
void WDC65816_set_IRQ_level(struct WDC65816 *, u32 level);
void WDC65816_set_NMI_level(struct WDC65816 *, u32 level);

#endif //JSMOOCH_EMUS_WDC65816_H
