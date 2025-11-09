#include "sm83_misc.h"
#include "sm83.h"
#include "sm83_opcodes.h"
#include "stdio.h"
#include "helpers/debug.h"
#include "fail"
#include "helpers/serialize/serialize.h"

u32 SM83_regs_F_getbyte(struct SM83_regs_F* this) {
	return (this->C << 4) | (this->H << 5) | (this->N << 6) | (this->Z << 7);
}

void SM83_regs_F_setbyte(struct SM83_regs_F* this, u32 val) {
	this->C = (val & 0x10) >> 4;
	this->H = (val & 0x20) >> 5;
	this->N = (val & 0x40) >> 6;
	this->Z = (val & 0x80) >> 7;
}

void SM83_regs_F_init(struct SM83_regs_F* this) {
	this->Z = this->N = this->H = this->C = 0;
}

void SM83_pins_init(struct SM83_pins* this) {
	this->RD = this->WR = this->MRQ = 0;
	this->D = 0;
	this->Addr = 0;
}

void SM83_regs_init(struct SM83_regs* this) {
	SM83_regs_F_init(&(this->F));
	this->A = this->B = this->C = this->D = this->E = this->H = this->L = this->SP = this->PC = 0;

	this->IV = this->IME_DELAY = 0;

	this->IE = this->IF = this->HLT = this->STP = this->IME = this->halt_bug = this->interrupt_latch = this->interrupt_flag = 0;

	this->TCU = this->IR = this->TR = this->TA = this->RR = this->prefix = 0;
	this->poll_IRQ = 0;
}


void SM83_init(struct SM83* this) {
    dbg_init();
    this->last_decoded_opcode = SM83_S_RESET;
	this->current_instruction = SM83_decoded_opcodes[SM83_S_RESET];
	SM83_regs_init(&(this->regs));
	SM83_pins_init(&(this->pins));
    DBG_EVENT_VIEW_INIT;
    this->dbg.events.IRQ_joypad = -1;
    this->dbg.events.IRQ_stat = -1;
    this->dbg.events.IRQ_vblank = -1;
    this->dbg.events.IRQ_serial = -1;
    this->dbg.events.IRQ_timer = -1;
    this->dbg.events.HALT_end = -1;

    this->trace_cycles = 0;
    this->trace_on = 0;

    this->read_trace.read_trace = NULL;
    this->read_trace.ptr = NULL;
}

void SM83_reset(struct SM83* this) {
	this->regs.PC = 1;
	this->regs.TCU = 0;
	this->pins.Addr = 0;
	this->pins.RD = this->pins.MRQ = 1;
	this->pins.WR = 0;
	this->regs.IR = SM83_S_DECODE;
}

// Instruction fetch cycles
void SM83_ins_cycles(struct SM83* this) {
	switch (this->regs.TCU) {
	case 1: // Initial opcode fetch has already been done as last cycle of last instruction
		this->regs.IR = this->pins.D;
		if (this->regs.IR == 0xCB) {
			this->regs.IR = SM83_S_DECODE;
			this->pins.Addr = this->regs.PC;
			this->regs.PC = (this->regs.PC + 1) & 0xFFFF;
			break;
		}

        this->last_decoded_opcode = this->regs.IR;
		this->current_instruction = SM83_decoded_opcodes[this->regs.IR];
        //printf("\nIR: %d", this->regs.IR);

		(*this->current_instruction)(&(this->regs), &(this->pins));
		break;
	case 2:
		this->regs.IR = this->pins.D;
        this->last_decoded_opcode = 0x102 + this->regs.IR;
        this->current_instruction = SM83_decoded_opcodes[0x102 + this->regs.IR];
		this->regs.IR |= 0xCB00;
        /*if (this->trace_on) {
			dbg.traces.add(TRACERS.SM83, this->trace_cycles, this->trace_format(SM83_disassemble((this->pins.Addr-1) & 0xFFFF, this->trace_peek), this->pins.Addr));
		}*/
		this->regs.TCU = 1;
		(*this->current_instruction)(&(this->regs), &(this->pins));
		break;
	}
}

void SM83_cycle(struct SM83* this) {
	//printf("\nCYCLE %04x", this->regs.PC);
    this->regs.TCU++;
	this->trace_cycles++;
	i32 is_halt;
	u32 imask;
	u32 mask;
	// Enable interrupts on next cycle
	if (this->regs.IME_DELAY > 0) {
		this->regs.IME_DELAY--;
		if (this->regs.IME_DELAY <= 0) {
			this->regs.IME = 1;
		}
	}
	if ((this->regs.IR == SM83_HALT) ||
		((this->regs.IR == SM83_S_DECODE) && (this->regs.TCU == 1) && (this->regs.poll_IRQ))) {
		is_halt = ((this->regs.IR == SM83_HALT) && (this->regs.IME == 0));
		this->regs.poll_IRQ = 0;
		imask = 0xFF;
		if ((this->regs.IME) || is_halt) {
			mask = this->regs.IE & this->regs.IF & 0x1F;
			this->regs.IV = -1;
			imask = 0xFF;
			if (mask & 1) { // VBLANK interrupt
                DBG_EVENT(this->dbg.events.IRQ_vblank);
				imask = 0xFE;
				this->regs.IV = 0x40;
			}
			else if (mask & 2) { // STAT interrupt
                DBG_EVENT(this->dbg.events.IRQ_stat);
				imask = 0xFD;
				this->regs.IV = 0x48;
			}
			else if (mask & 4) { // Timer interrupt
                DBG_EVENT(this->dbg.events.IRQ_timer);
				imask = 0xFB;
				this->regs.IV = 0x50;
			}
			else if (mask & 8) { // Serial interrupt
                DBG_EVENT(this->dbg.events.IRQ_serial);
				imask = 0xF7;
				this->regs.IV = 0x58;
			}
			else if (mask & 0x10) { // Joypad interrupt
                DBG_EVENT(this->dbg.events.IRQ_joypad);
                imask = 0xEF;
				this->regs.IV = 0x60;
			}
			if (this->regs.IV > 0) {
				if (is_halt && (this->regs.IME == 0)) {
                    DBG_EVENT(this->dbg.events.HALT_end);
                    this->regs.HLT = 0;
					this->regs.TCU++;
				}
				else {
					// Right here, the STAT is not supposed to be cleared if LCD disabled
					if (this->regs.HLT) {
						this->regs.PC = (this->regs.PC + 1) & 0xFFFF;
					}
					//printf("\nIRQ at cycle:%llu to IV:%04x", this->trace_cycles, this->regs.IV);
                    this->regs.IF &= imask;
					this->regs.HLT = 0;
					this->regs.IR = SM83_S_IRQ;
                    this->last_decoded_opcode = SM83_S_IRQ;
					this->current_instruction = SM83_decoded_opcodes[SM83_S_IRQ];
				}
			}
		}
	}

	if (this->regs.IR == SM83_S_DECODE) {
		// operand()
		// if CB, operand() again
		//printf("\nINS_CYCLES");
        SM83_ins_cycles(this);
	}
	else {
        (*this->current_instruction)(&(this->regs), &(this->pins));
	}
}

void SM83_enable_tracing(struct SM83* this, struct jsm_debug_read_trace *dbg_read_trace)
{
    this->trace_on = 1;

    jsm_copy_read_trace(&this->read_trace, dbg_read_trace);
}

void SM83_disable_tracing(struct SM83* this)
{
    this->trace_on = 0;
}

#define S(x) Sadd(state, &this-> x, sizeof(this-> x))
static void serialize_regs(struct SM83_regs *this, struct serialized_state *state)
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

static void serialize_pins(struct SM83_pins* this, struct serialized_state *state)
{
    S(RD);
    S(WR);
    S(MRQ);
    S(D);
    S(Addr);
}

void SM83_serialize(struct SM83 *this, struct serialized_state *state) {
    serialize_regs(&this->regs, state);
    serialize_pins(&this->pins, state);
    S(last_decoded_opcode);
}
#undef S

#define L(x) Sload(state, &this-> x, sizeof(this-> x))
static void deserialize_regs(struct SM83_regs *this, struct serialized_state *state)
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

static void deserialize_pins(struct SM83_pins *this, struct serialized_state *state)
{
    L(RD);
    L(WR);
    L(MRQ);
    L(D);
    L(Addr);
}


void SM83_deserialize(struct SM83 *this, struct serialized_state *state) {
    deserialize_regs(&this->regs, state);
    deserialize_pins(&this->pins, state);
    L(last_decoded_opcode);
    this->current_instruction = SM83_decoded_opcodes[this->last_decoded_opcode];
}

#undef L