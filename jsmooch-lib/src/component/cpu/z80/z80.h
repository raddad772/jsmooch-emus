//
// Created by Dave on 2/7/2024.
//
#pragma once

#include "helpers/cvec.h"
#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/debugger/debuggerdefs.h"
#include "helpers/serialize/serialize.h"

namespace Z80 {
constexpr u32 S_IRQ = 0x100;
constexpr u32 S_RESET = 0x101;
constexpr u32 S_DECODE = 0x102;
constexpr u32 MAX_OPCODE = 0x101;

struct regs_F {
    union {
        struct {
            u8 C : 1{};
            u8 N : 1{};
            union {
                u8 P: 1{};
                u8 V: 1;
            };
            u8 Y : 1{};
            u8 H : 1{};
            u8 X : 1{};
            u8 Z : 1{};
            u8 S : 1{};
        };
        u8 u;
    };
};

struct regs {
    void serialize(serialized_state &state);
    void deserialize(serialized_state &state);
    u16 reset_vector{};
    u32 IR{}; // Instruction Register
    u8 TCU{}; // Internal instruction cycle timer register (not on real Z80 under this name)

    u16 A{};
    u16 B{};
    u16 C{};
    u16 D{};
    u16 E{};
    u16 H{};
    u16 L{};
    regs_F F;
    u32 I{}; // Iforget
    u32 R{}; // Refresh counter

    // Shadow registers
    u16 AF_{};
    u16 BC_{};
    u16 DE_{};
    u16 HL_{};

    // Junk calculations
    u32 TR{}, TA{};

    // Temps for register swapping
    u32 AFt{};
    u32 BCt{};
    u32 DEt{};
    u32 HLt{};
    u32 Ht{};
    u32 Lt{};

    // 16-bit registers
    u16 PC{};
    u16 SP{};
    u16 IX{};
    u16 IY{};

    u32 t[10]{};
    u16 WZ{};
    u16 EI{};
    u8 P{};
    u8 Q{};
    u8 IFF1{};
    u8 IFF2{};
    u8 IM{};
    u8 HALT{};

    u32 data{};
    enum prefix {
        P_HL,
        P_IX,
        P_IY
      };

    i32 IRQ_vec{};
    prefix rprefix=P_HL;
    u32 prefix{};

    u32 poll_IRQ{};
    void exchange_shadow_af();
    void exchange_de_hl();
    void exchange_shadow();
    void inc_R();
};

struct pins {
    void serialize(serialized_state &state);
    void deserialize(serialized_state &state);
    u16 Addr{};
    u8 D{};

    u8 IRQ_maskable{1};
    u8 RD{}, WR{}, IO{}, MRQ{};

    u8 M1{}, WAIT{}; // M1 pin
};

typedef void (*ins_func)(regs&, pins&);

struct core {
    void serialize(serialized_state &state);
    void deserialize(serialized_state &state);
    void notify_IRQ(bool level);
    void notify_NMI(bool level);
    explicit core(bool CMOS);
    static ins_func fetch_decoded(u32 opcode, u32 prefix);
    void setup_tracing(jsm_debug_read_trace* dbg_read_trace, u64 *trace_cycle_pointer);
    void reset();
    void set_pins_opcode();
    void set_pins_nothing();
    void set_instruction(u32 to);
    void ins_cycles();
    void cycle();
    void lycoder_print();
    void printf_trace();
    void trace_format();

    regs regs{};
    pins pins{};
    bool CMOS{};
    bool IRQ_pending{};
    bool NMI_pending{};
    bool NMI_ack{};

    struct {
        u32 ok;
        u64 *cycles;
        u64 last_cycle;
        u64 my_cycles;
    } trace{};

    ins_func current_instruction{};

    jsm_debug_read_trace read_trace;

    DBG_EVENT_VIEW_ONLY_START
    IRQ, NMI
    DBG_EVENT_VIEW_ONLY_END

    u32 PCO{};
};

u32 parity(u32 val);

}