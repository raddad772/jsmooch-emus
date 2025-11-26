//
// Created by Dave on 2/4/2024.
//

#pragma once

#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/debugger/debuggerdefs.h"
#include "helpers/debugger/debugger.h"
#include "helpers/serialize/serialize.h"
#include "m6502_misc.h"

namespace M6502 {
union M6502_P {
    struct {
        u8 C : 1;
        u8 Z : 1;
        u8 I : 1;
        u8 D : 1;
        u8 B : 1;
        u8 _ : 1;
        u8 V : 1;
        u8 N : 1;
    };
    u8 u{};
    void setbyte(u8 val);

    u8 getbyte();

    [[nodiscard]] u8 getbyte() const;
};

struct regs {
    u32 A{}, X{}, Y{};
    u32 PC{}, S{};
    M6502_P P{};

    u32 TCU{}, IR{};
    i32 TA{}, TR{};

    u32 HLT{};
    u32 do_IRQ{};
    u32 WAI{};
    u32 STP{};

    u32 NMI_old{}, NMI_level_detected{}, do_NMI{};

    void serialize(serialized_state &state);
    void deserialize(serialized_state &state);
};

struct pins {
    u32 Addr{};
    u32 D{};
    u32 RW{};

    u8 IRQ{};
    u8 NMI{};
    u8 RST{};

    u8 RDY{};

    void serialize(serialized_state &state);
    void deserialize(serialized_state &state);
};

struct core {
    explicit core(ins_func *opcode_table);

    void power_on();
    void reset();
    void cycle();
    void setup_tracing(jsm_debug_read_trace *strct, u64 *trace_cycle_pointer);
    void serialize(serialized_state &state);
    void deserialize(serialized_state &state);

    regs regs{};
    pins pins{};

    DBG_EVENT_VIEW_ONLY_START
    IRQ, NMI
    DBG_EVENT_VIEW_ONLY_END

    struct {
        u32 ok{};
        u64 *cycles{};
        u64 my_cycles{};
        jsm_debug_read_trace strct{};
        u32 ins_PC{};
        jsm_string str{1000}, str2{200};
        struct {
            dbglog_view *view{};
            u32 id{};
        } dbglog{};

    } trace{};

    u32 first_reset=1;

    ins_func current_instruction{};
    ins_func *opcode_table{};
};

void poll_IRQs(regs &regs, pins &pins);
void poll_NMI_only(regs &regs, pins &pins);
}