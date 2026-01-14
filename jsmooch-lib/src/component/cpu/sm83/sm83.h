#pragma once

#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/cvec.h"
#include "helpers/debugger/debuggerdefs.h"
#include "helpers/serialize/serialize.h"

#include "sm83_misc.h"

namespace SM83 {

struct regs_F {
	u8 Z{};
	u8 N{};
	u8 H{};
	u8 C{};

	u8 getbyte() const;
	void setbyte(u8 val);
};

struct regs {
	void serialize(serialized_state &state);
	void deserialize(serialized_state &state);
	u32 A{};
	regs_F F{};
	u32 B{};
	u32 C{};
	u32 D{};
	u32 E{};
	u32 H{};
	u32 L{};
	u32 SP{};
	u32 PC{};

	i32 IV{};  // Interrupt vector to execute
	i32 IME_DELAY{};

	u32 IE{}; // Enable interrupt?
	u32 IF{}; // Interrupt flag
	u32 HLT{};
	u32 STP{};
	u32 IME{}; // Global enable interrupt

	u32 halt_bug{};

	u32 interrupt_latch{};
	u32 interrupt_flag{};

    // internal/speculative
	u32 TCU{}; // "Timing Control Unit" basically kind cycle of an op we're on
	u32 IR{}; // "Instruction Register" currently-executing register

	u32 TR{}; // Temporary Register
	u32 TA{}; // Temporary Address
	u32 RR{}; // Remporary Register

	u32 prefix{};
	bool poll_IRQ{};
};

struct pins {
	void serialize(serialized_state &state);
	void deserialize(serialized_state &state);
	bool RD{}; // External read request
	bool WR{}; // External write request
	bool MRQ{}; // External memory request

	u8 D{}; // 8 Data pins
	u16 Addr{}; // 16 Address pins
};

struct core {
	core();
	regs regs{};
	pins pins{};

	void cycle();
	void reset();
	void ins_cycles();
	void enable_tracing(jsm_debug_read_trace *dbg_read_trace);
	void disable_tracing();
    bool trace_on{};
    u64 trace_cycles{};
	void serialize(serialized_state &state);
	void deserialize(serialized_state &state);

    u32 last_decoded_opcode{INS_RESET};
    ins_func current_instruction{decoded_opcodes[INS_RESET]};

    jsm_debug_read_trace read_trace{};

    DBG_EVENT_VIEW_ONLY_START
    IRQ_vblank{}, IRQ_stat{},
    IRQ_joypad{}, IRQ_timer{},
    IRQ_serial{}, HALT_end{}
    DBG_EVENT_VIEW_ONLY_END
};


};