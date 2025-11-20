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

struct M6502_P {
    u8 C{};
    u8 Z{};
    u8 I{};
    u8 D{};
    u8 B{};
    u8 V{};
    u8 N{};
    void setbyte(u8 val);
    u8 getbyte() const;
};

struct M6502_regs {
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

struct M6502_pins {
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

struct M6502 {

    explicit M6502(M6502_ins_func *opcode_table);

    void power_on();
    void reset();
    void cycle();
    void setup_tracing(jsm_debug_read_trace *strct, u64 *trace_cycle_pointer);
    void serialize(serialized_state &state);
    void deserialize(serialized_state &state);
    void disassemble_entry(disassembly_entry* entry);


    M6502_regs regs{};
    M6502_pins pins{};
    u32 PCO{};

    DBG_EVENT_VIEW_ONLY_START
    IRQ, NMI
    DBG_EVENT_VIEW_ONLY_END

    struct {
        u32 ok{};
        u64 *cycles{};
        u64 my_cycles{};
        jsm_debug_read_trace strct{};
    } trace{};

    u32 first_reset=1;

    M6502_ins_func current_instruction{};
    M6502_ins_func *opcode_table{};
};

void M6502_poll_IRQs(M6502_regs *regs, M6502_pins *pins);
void M6502_poll_NMI_only(M6502_regs *regs, M6502_pins *pins);

struct serialized_state;
