#include "sm83_misc.h"
#include "sm83.h"

#include "helpers/debug.h"
#include "helpers/debugger/debugger.h"
#include "helpers/serialize/serialize.h"

namespace SM83 {

u8 regs_F::getbyte() const {
	return (C << 4) | (H << 5) | (N << 6) | (Z << 7);
}

void regs_F::setbyte(u8 val) {
	C = (val & 0x10) >> 4;
	H = (val & 0x20) >> 5;
	N = (val & 0x40) >> 6;
	Z = (val & 0x80) >> 7;
}

core::core() {
    last_decoded_opcode = INS_RESET;
	current_instruction = decoded_opcodes[INS_RESET];

    trace_cycles = 0;
    trace_on = false;

    read_trace.read_trace = nullptr;
    read_trace.ptr = nullptr;
}

void core::reset() {
	regs.PC = 1;
	regs.TCU = 0;
	pins.Addr = 0;
	pins.RD = pins.MRQ = true;
	pins.WR = false;
	regs.IR = INS_DECODE;
}

// Instruction fetch cycles
void core::ins_cycles() {
	switch (regs.TCU) {
	case 1: // Initial opcode fetch has already been done as last cycle of last instruction
		regs.IR = pins.D;
		if (regs.IR == 0xCB) {
			regs.IR = INS_DECODE;
			pins.Addr = regs.PC;
			regs.PC = (regs.PC + 1) & 0xFFFF;
			break;
		}

        last_decoded_opcode = regs.IR;
		current_instruction = decoded_opcodes[regs.IR];
        //printf("\nIR: %d"regs.IR);

		(*current_instruction)(regs, pins);
		break;
	case 2:
		regs.IR = pins.D;
        last_decoded_opcode = 0x102 + regs.IR;
        current_instruction = decoded_opcodes[0x102 + regs.IR];
		regs.IR |= 0xCB00;
        /*if (trace_on) {
			dbg.traces.add(TRACERS.SM83trace_cyclestrace_format(SM83_disassemble((pins.Addr-1) & 0xFFFFtrace_peek)pins.Addr));
		}*/
		regs.TCU = 1;
		(*current_instruction)(regs, pins);
		break;
		default:
			NOGOHERE;
	}
}

#define DBG_EVENT(x) debugger_report_event(dbg.events.view, x)
void core::cycle() {
	//printf("\nCYCLE %04x"regs.PC);
    regs.TCU++;
	trace_cycles++;
	// Enable interrupts on next cycle
	if (regs.IME_DELAY > 0) {
		regs.IME_DELAY--;
		if (regs.IME_DELAY <= 0) {
			regs.IME = 1;
		}
	}
if ((regs.IR == INS_HALT) ||
	((regs.IR == INS_DECODE) && (regs.TCU == 1) && (regs.poll_IRQ))) {
	bool is_halt = ((regs.IR == INS_HALT) && (regs.IME == 0));
	regs.poll_IRQ = false;
	if ((regs.IME) || is_halt) {
		u32 imask = 0xFF;
		u32 mask = regs.IE & regs.IF & 0x1F;
		regs.IV = -1;
		if (mask & 1) { // VBLANK interrupt
            DBG_EVENT(dbg.events.IRQ_vblank);
			imask = 0xFE;
			regs.IV = 0x40;
		}
		else if (mask & 2) { // STAT interrupt
            DBG_EVENT(dbg.events.IRQ_stat);
			imask = 0xFD;
			regs.IV = 0x48;
		}
		else if (mask & 4) { // Timer interrupt
            DBG_EVENT(dbg.events.IRQ_timer);
			imask = 0xFB;
			regs.IV = 0x50;
		}
		else if (mask & 8) { // Serial interrupt
            DBG_EVENT(dbg.events.IRQ_serial);
			imask = 0xF7;
			regs.IV = 0x58;
		}
		else if (mask & 0x10) { // Joypad interrupt
            DBG_EVENT(dbg.events.IRQ_joypad);
            imask = 0xEF;
			regs.IV = 0x60;
		}
		if (regs.IV > 0) {
			if (is_halt && (regs.IME == 0)) {
                DBG_EVENT(dbg.events.HALT_end);
                regs.HLT = 0;
				regs.TCU++;
			}
			else {
				// Right herethe STAT is not supposed to be cleared if LCD disabled
				if (regs.HLT) {
					regs.PC = (regs.PC + 1) & 0xFFFF;
				}
				//printf("\nIRQ at cycle:%llu to IV:%04x"trace_cyclesregs.IV);
                regs.IF &= imask;
				regs.HLT = 0;
				regs.IR = INS_IRQ;
                last_decoded_opcode = INS_IRQ;
				current_instruction = decoded_opcodes[INS_IRQ];
			}
		}
	}
}

	if (regs.IR == INS_DECODE) {
        ins_cycles();
	}
	else {
        (*current_instruction)(regs, pins);
	}
}

void core::enable_tracing(jsm_debug_read_trace *dbg_read_trace)
{
    trace_on = true;

    jsm_copy_read_trace(&read_trace, dbg_read_trace);
}

void core::disable_tracing()
{
    trace_on = false;
}

#define S(x) Sadd(state, &x, sizeof(x))
void regs::serialize(serialized_state &state)
{
    S(A);
    S(F.Z);
    S(F.N);
    S(F.H);
    S(F.C);
    S(B);
    S(C);
    S(D);
    S(E);
    S(H);
    S(L);
    S(SP);
    S(PC);
    S(IV);
    S(IME_DELAY);
    S(IE);
    S(IF);
    S(HLT);
    S(STP);
    S(IME);
    S(halt_bug);
    S(interrupt_latch);
    S(interrupt_flag);
    S(TCU);
    S(IR);
    S(TR);
    S(TA);
    S(RR);
    S(prefix);
    S(poll_IRQ);
}

void pins::serialize(serialized_state &state)
{
    S(RD);
    S(WR);
    S(MRQ);
    S(D);
    S(Addr);
}

void core::serialize(serialized_state &state) {
    regs.serialize(state);
    pins.serialize(state);
    S(last_decoded_opcode);
}
#undef S

#define L(x) Sload(state, &x, sizeof(x))
void regs::deserialize(serialized_state &state)
{
    L(A);
    L(F.Z);
    L(F.N);
    L(F.H);
    L(F.C);
    L(B);
    L(C);
    L(D);
    L(E);
    L(H);
    L(L);
    L(SP);
    L(PC);
    L(IV);
    L(IME_DELAY);
    L(IE);
    L(IF);
    L(HLT);
    L(STP);
    L(IME);
    L(halt_bug);
    L(interrupt_latch);
    L(interrupt_flag);
    L(TCU);
    L(IR);
    L(TR);
    L(TA);
    L(RR);
    L(prefix);
    L(poll_IRQ);
}

void pins::deserialize(serialized_state &state)
{
    L(RD);
    L(WR);
    L(MRQ);
    L(D);
    L(Addr);
}


void core::deserialize(serialized_state &state) {
    regs.deserialize(state);
    pins.deserialize(state);
    L(last_decoded_opcode);
    current_instruction = decoded_opcodes[last_decoded_opcode];
}

#undef L
}