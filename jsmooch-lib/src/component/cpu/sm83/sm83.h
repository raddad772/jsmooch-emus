#ifndef _SM83_H
#define _SM83_H

#include "helpers/int.h"
#include "helpers/debug.h"
#include "sm83_misc.h"

struct SM83_regs_F {
	u32 Z;
	u32 N;
	u32 H;
	u32 C;
};

struct SM83_regs {
	u32 A;
	struct SM83_regs_F F;
	u32 B;
	u32 C;
	u32 D;
	u32 E;
	u32 H;
	u32 L;
	u32 SP;
	u32 PC;

	i32 IV;  // Interrupt vector to execute
	i32 IME_DELAY;

	u32 IE; // Enable interrupt?
	u32 IF; // Interrupt flag
	u32 HLT;
	u32 STP;
	u32 IME; // Global enable interrupt

	u32 halt_bug;

	u32 interrupt_latch;
	u32 interrupt_flag;

    // internal/speculative
	u32 TCU; // "Timing Control Unit" basically kind cycle of an op we're on
	u32 IR; // "Instruction Register" currently-executing register

	u32 TR; // Temporary Register
	u32 TA; // Temporary Address
	u32 RR; // Remporary Register

	u32 prefix;
	_Bool poll_IRQ;
};

struct SM83_pins {
	u32 RD; // External read request
	u32 WR; // External write request
	u32 MRQ; // External memory request

	u32 D; // 8 Data pins
	u32 Addr; // 16 Address pins
};

struct SM83 {
	struct SM83_regs regs;
	struct SM83_pins pins;

    u32 trace_on;
    u64 trace_cycles;

    SM83_ins_func current_instruction;

    struct jsm_debug_read_trace read_trace;
};

void SM83_regs_F_setbyte(struct SM83_regs_F* this, u32 val);
u32 SM83_regs_F_getbyte(struct SM83_regs_F* this);
void SM83_regs_F_init(struct SM83_regs_F* this);
void SM83_regs_init(struct SM83_regs* this);
void SM83_pins_init(struct SM83_pins* this);

void SM83_init(struct SM83* this); // Initializes SM83 processor struct. Use before using the struct
void SM83_cycle(struct SM83* this); // Runs SM83 cycle
void SM83_reset(struct SM83* this); // Resets SM83 processor
void SM83_enable_tracing(struct SM83* this, struct jsm_debug_read_trace *dbg_read_trace);
void SM83_disable_tracing(struct SM83* this);


#endif