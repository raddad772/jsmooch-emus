//
// Created by Dave on 2/7/2024.
//

#include "stdio.h"
#include "assert.h"

#include "z80.h"
#include "z80_opcodes.h"
#include "z80_disassembler.h"

void Z80_regs_F_init(struct Z80_regs_F* this)
{
    this->S = this->Z = this->Y = this->H = this->X = this->PV = this->N = this->C = 0;
}

void Z80_regs_F_setbyte(struct Z80_regs_F* this, u32 val)
{
    this->C = val & 1;
    this->N = (val & 2) >> 1;
    this->PV = (val & 4) >> 2;
    this->X = (val & 8) >> 3;
    this->H = (val & 0x10) >> 4;
    this->Y = (val & 0x20) >> 5;
    this->Z = (val & 0x40) >> 6;
    this->S = (val & 0x80) >> 7;
}

u32 Z80_regs_F_getbyte(struct Z80_regs_F* this)
{
    return this->C | (this->N << 1) | (this->PV << 2) | (this->X << 3) | (this->H << 4) | (this->Y << 5) | (this->Z << 6) | (this->S << 7);
}

void Z80_regs_init(struct Z80_regs* this)
{
    this->IR = this->TCU = 0;
    this->A = this->B = this->C = this->D = 0;
    this->E = this->H = this->L = 0;
    Z80_regs_F_init(&this->F);
    this->I = this->R = 0;

    this->AF_ = this->BC_ = this->DE_ = this->HL_ = 0;

    this->junkvar = 0;

    this->AFt = this->BCt = this->DEt = this->HLt = this->Ht = this->Lt = 0;

    this->PC = this->SP = this->IX = this->IY = 0;

    this->t[0] = this->t[1] = this->t[2] = this->t[3] = this->t[4] = 0;
    this->t[5] = this->t[6] = this->t[7] = this->t[8] = this->t[9] = 0;
    
    this->WZ = this->EI = this->P = this->Q = 0;
    this->IFF1 = this->IFF2 = this->IM = this->HALT = 0;
    
    this->data = 0;
    
    this->IRQ_vec = -1;
    this->rprefix = Z80P_HL;
    this->prefix = 0;
    
    this->poll_IRQ = 0;
}

void Z80_regs_inc_R(struct Z80_regs* this)
{
    this->R = (this->R & 0x80) | ((this->R + 1) & 0x7F);
}

void Z80_regs_exchange_shadow(struct Z80_regs* this)
{
    this->BCt = (this->B << 8) | this->C;
    this->DEt = (this->D << 8) | this->E;
    this->HLt = (this->H << 8) | this->L;

    this->B = (this->BC_ & 0xFF00) >> 8;
    this->C = this->BC_ & 0xFF;
    this->D = (this->DE_ & 0xFF00) >> 8;
    this->E = this->DE_ & 0xFF;
    this->H = (this->HL_ & 0xFF00) >> 8;
    this->L = this->HL_ & 0xFF;

    this->BC_ = this->BCt;
    this->DE_ = this->DEt;
    this->HL_ = this->HLt;

}

void Z80_regs_exchange_de_hl(struct Z80_regs* this)
{
    this->Ht = this->H;
    this->Lt = this->L;
    this->H = this->D;
    this->L = this->E;
    this->D = this->Ht;
    this->E = this->Lt;

}

void Z80_regs_exchange_shadow_af(struct Z80_regs* this)
{
    this->AFt = (this->A << 8) | Z80_regs_F_getbyte(&this->F);

    this->A = (this->AF_ & 0xFF00) >> 8;
    Z80_regs_F_setbyte(&this->F, this->AF_ & 0xFF);

    this->AF_ = this->AFt;
}

void Z80_pins_init(struct Z80_pins* this)
{
    this->RES = this->NMI = this->IRQ = 0;
    this->Addr = 0;
    this->D = 0;
    this->IRQ_maskable = 1;

    this->RD = this->WR = this->IO = this->MRQ = 0;
}

void Z80_init(struct Z80* this, u32 CMOS)
{
    Z80_regs_init(&this->regs);
    Z80_pins_init(&this->pins);

    this->CMOS = CMOS;
    this->prefix_was = 0;

    this->IRQ_pending = this->NMI_pending = this->NMI_ack = 0;

    this->trace_on = 0;
    this->trace_cycles = this->last_trace_cycle = 0;

    this->current_instruction = NULL;

    this->PCO = 0;
}

#define THIS struct Z80* this

void Z80_setup_tracing(struct Z80* this, struct jsm_debug_read_trace* dbg_read_trace)
{
    jsm_copy_read_trace(&this->read_trace, dbg_read_trace);
}

void Z80_enable_tracing(struct Z80* this)
{
    this->trace_on = 1;
}

void Z80_disable_tracing(struct Z80* this)
{
    this->trace_on = 0;
}

u32 Z80_prefix_to_codemap(u32 prefix) {
    switch (prefix) {
        case 0:
            return 0;
        case 0xCB:
            return Z80_MAX_OPCODE + 1;
        case 0xDD:
            return (Z80_MAX_OPCODE + 1) * 2;
        case 0xED:
            return (Z80_MAX_OPCODE + 1) * 3;
        case 0xFD:
            return (Z80_MAX_OPCODE + 1) * 4;
        case 0xDDCB:
            return (Z80_MAX_OPCODE + 1) * 5;
        case 0xFDCB:
            return (Z80_MAX_OPCODE + 1) * 6;
        default:
            printf("BAD PREFIX");
            return -1;
    }
}

Z80_ins_func Z80_fetch_decoded(u32 opcode, u32 prefix)
{
    return Z80_decoded_opcodes[Z80_prefix_to_codemap(prefix) + opcode];
}

void Z80_reset(struct Z80* this)
{
    this->regs.rprefix = Z80P_HL;
    this->regs.prefix = 0;
    this->regs.A = 0xFF;
    Z80_regs_F_setbyte(&this->regs.F, 0xFF);
    this->regs.B = this->regs.C = this->regs.D = this->regs.E = 0;
    this->regs.H = this->regs.L = this->regs.IX = this->regs.IY = 0;
    this->regs.WZ = this->regs.PC = 0;
    this->regs.SP = 0xFFFF;
    this->regs.EI = this->regs.P = this->regs.Q = 0;
    this->regs.HALT = this->regs.IFF1 = this->regs.IFF2 = 0;
    this->regs.IM = 1;

    this->IRQ_pending = this->NMI_pending = this->NMI_ack = 0;
    this->regs.IRQ_vec = -1;

    this->regs.IR = Z80_S_RESET;
    this->current_instruction = Z80_fetch_decoded(this->regs.IR, 0);
    this->regs.TCU = 0;
}

void Z80_notify_IRQ(struct Z80* this, u32 level) {
    this->IRQ_pending = level != 0;
}

void Z80_notify_NMI(struct Z80* this, u32 level) {
    if ((level == 0) && this->NMI_ack) { this->NMI_ack = false; }
    this->NMI_pending = this->NMI_pending || (level > 0);
}

void Z80_set_pins_opcode(struct Z80* this)
{
    this->pins.RD = this->pins.MRQ = this->pins.WR = this->pins.IO = 0;
    this->pins.Addr = this->regs.PC;
    this->regs.PC = (this->regs.PC + 1) & 0xFFFF;
}

void Z80_set_pins_nothing(struct Z80* this)
{
    this->pins.RD = this->pins.MRQ = 0;
    this->pins.WR = this->pins.IO = 0;
}

void Z80_set_instruction(struct Z80* this, u32 to)
{
    this->regs.IR = to;
    this->current_instruction = Z80_fetch_decoded(this->regs.IR, this->regs.prefix);
    this->prefix_was = this->regs.prefix;
    this->regs.TCU = 0;
    this->regs.prefix = 0;
    this->regs.rprefix = Z80P_HL;
}

void Z80_ins_cycles(struct Z80* this)
{
    switch(this->regs.TCU) {
        // 1-4 is fetch next thing and interpret
        // addr T0-1
        // REFRESH on addr T2-3
        // MREQ on T1
        // RD on T1
        // data latch T2
        case 0: // already handled by fetch of next instruction starting
            Z80_set_pins_opcode(this);
            //this->regs.rprefix = Z80P_HL;
            //this->regs.prefix = 0;
            break;
        case 1: // T1 MREQ, RD
            if (this->regs.HALT) { this->regs.TCU = 0; break; }
            if (this->regs.poll_IRQ) {
                // Make sure we only do this at start of an instruction
                this->regs.poll_IRQ = false;
                if (this->NMI_pending && !this->NMI_ack) {
                    this->NMI_pending = false;
                    this->NMI_ack = true;
                    this->regs.IRQ_vec = 0x66;
                    this->pins.IRQ_maskable = false;
                    Z80_set_instruction(this, Z80_S_IRQ);
                    if (dbg.brk_on_NMIRQ) {
                        dbg_break();
                        //console.log('NMI', this->trace_cycles);
                        //dbg.break(D_RESOURCE_TYPES.Z80);
                    }
                } else if (this->IRQ_pending && this->regs.IFF1 && (!(this->regs.EI))) {
                    this->pins.D = 0xFF;
                    this->regs.PC = (this->regs.PC - 1) & 0xFFFF;
                    this->pins.IRQ_maskable = true;
                    this->regs.IRQ_vec = 0x38;
                    this->pins.D = 0xFF;
                    Z80_set_instruction(this, Z80_S_IRQ);
                    if (dbg.brk_on_NMIRQ) {
                        //console.log(this->trace_cycles);
                        dbg_break();
                    }
                }
            }
            this->pins.RD = 1;
            this->pins.MRQ = 1;
            break;
        case 2: // T2, RD to 0 and data latch, REFRESH and MRQ=1 for REFRESH
            this->pins.RD = 0;
            this->pins.MRQ = 0;
            this->regs.t[0] = this->pins.D;
            this->pins.Addr = (this->regs.I << 8) | this->regs.R;
            break;
        case 3: // T3 not much here
            //this->pins.MRQ = 0;
            // If we need to fetch another, start that and set TCU back to 1
            Z80_regs_inc_R(&this->regs);
            if (this->regs.t[0] == 0xDD) { this->regs.prefix = 0xDD; this->regs.rprefix = Z80P_IX; this->regs.TCU = -1; break; }
            if (this->regs.t[0] == 0xfD) { this->regs.prefix = 0xFD; this->regs.rprefix = Z80P_IY; this->regs.TCU = -1; break; }
            // elsewise figure out what to do next
            // this gets a little tricky
            // 4, 5, 6, 7, 8, 9, 10, 11, 12 = rprefix != HL and is CB, execute CBd
            if ((this->regs.t[0] == 0xCB) && (this->regs.rprefix != Z80P_HL)) {
                this->regs.prefix = ((this->regs.prefix << 8) | 0xCB) & 0xFFFF;
                break;
            }
                // . so 13, 14, 15, 16. opcode, then immediate execution CB
            else if (this->regs.t[0] == 0xCB) {
                this->regs.prefix = 0xCB;
                this->regs.TCU = 12;
                break;
            }
                // reuse 13-16
            else if (this->regs.t[0] == 0xED) {
                this->regs.prefix = 0xED;
                this->regs.TCU = 12;
                break;
            }
            else {
                //this->regs.prefix = 0x00;
                Z80_set_instruction(this, this->regs.t[0]);
                break;
            }
        case 4: // CBd begins here, as does operand()
            //
            switch(this->regs.rprefix) {
                case Z80P_HL:
                    this->regs.WZ = (this->regs.H << 8) | this->regs.L;
                    break;
                case Z80P_IX:
                    this->regs.WZ = this->regs.IX;
                    break;
                case Z80P_IY:
                    this->regs.WZ = this->regs.IY;
                    break;
            }
            Z80_set_pins_opcode(this);
            break;
        case 5: // operand() middle
            this->pins.RD = 1;
            this->pins.MRQ = 1;
            break;
        case 6: // operand() end
            this->regs.WZ = (u32)(((i32)this->regs.WZ + ((i32)(i8)this->pins.D)) & 0xFFFF);
            Z80_set_pins_nothing(this);
            this->regs.TCU += 2;
            break;
        case 7: // wait a cycle
            break;
        case 8: // wait one more cycle
            break;
        case 9: // start opcode fetch
            Z80_set_pins_opcode(this);
            break;
        case 10:
            this->pins.RD = 1;
            this->pins.MRQ = 1;
            break;
        case 11: // cycle 3 of opcode tech
            Z80_set_pins_nothing(this);
            this->regs.t[0] = this->pins.D;
            Z80_set_instruction(this, this->regs.t[0]);
            break;
        case 12: // cycle 4 of opcode fetch. execute instruction!
            //this->set_instruction(this->regs.t[0]);
            break;
        case 13: // CB regular and ED regular starts here
            Z80_set_pins_opcode(this);
            break;
        case 14:
            this->pins.MRQ = 1;
            this->pins.RD = 1;
            break;
        case 15:
            this->pins.Addr = (this->regs.I << 8) | this->regs.R;
            Z80_regs_inc_R(&this->regs);
            this->regs.t[0] = this->pins.D;
            Z80_set_pins_nothing(this);
            break;
        case 16:
            // execute from CB or ED now
            Z80_set_instruction(this, this->regs.t[0]);
            break;
        default:
            assert(1!=0);
            break;
    }
}

void Z80_trace_format(struct Z80* this)
{
    char t[250];
    if (this->regs.IR == 0x101) {
        dbg_printf("\nZ80(%06llu)          RESET", this->trace_cycles);
        return;
    }
    //Z80(   931)    008C  LDIR          TCU:1 PC:008E  A:00 B:1F C:F5 D:C0 E:0B H:C0 L:0A SP:DFF0 IX:0000 IY:0000 I:00 R:5E WZ:008D F:sZyhxPnc
    u32 b = this->read_trace.read_trace(this->read_trace.ptr, this->PCO);
    Z80_disassemble(this->PCO, b, &this->read_trace, t);
    dbg_printf("\nZ80(%06llu)    %04x  %s", this->trace_cycles, this->PCO, t);
    dbg_seek_in_line(35);
    dbg_printf("PC:%04X A:%02X B:%02X C:%02X D:%02X E:%02X H:%02X L:%02X SP:%04X IX:%04X IY:%04X I:%02X R:%02X WZ:%04X F:%02X TCU:%d",
               this->PCO, this->regs.A, this->regs.B, this->regs.C, this->regs.D, this->regs.E,
               this->regs.H, this->regs.L, this->regs.SP, this->regs.IX, this->regs.IY, this->regs.I, this->regs.R,
               this->regs.WZ, Z80_regs_F_getbyte(&this->regs.F), this->regs.TCU);
}

void Z80_lycoder_print(struct Z80* this)
{
    dbg_printf("\n%08d %04X %02X%02X %02X%02X %02X%02X %02X%02X %04X %04X", this->trace_cycles, this->regs.PC, this->regs.A,
               Z80_regs_F_getbyte(&this->regs.F), this->regs.B, this->regs.C, this->regs.D, this->regs.E,
               this->regs.H, this->regs.L, this->regs.IX, this->regs.IY);
}

void Z80_cycle(struct Z80* this)
{
    this->regs.TCU++;
    this->trace_cycles++;
#ifdef Z80_TRACE_BRK
    if (this->trace_cycles == Z80_TRACE_BRK) {
        printf("\nTRACE BREAK!");
        dbg.break();
    }
#endif
    if (this->regs.IR == Z80_S_DECODE) {
        // Long logic to decode opcodes and decide what to do
        if ((this->regs.TCU == 1) && (this->regs.prefix == 0)) this->PCO = this->pins.Addr;
        Z80_ins_cycles(this);
    } else {
#ifdef Z80_DBG_SUPPORT
#ifdef LYCODER
        if (dbg.trace_on && this->regs.TCU == 1 && this->trace_cycles > 5000000) {
            this->last_trace_cycle = this->PCO;
            Z80_lycoder_print(this);
        }
#else
        if (dbg.trace_on && this->regs.TCU == 1) {
            this->last_trace_cycle = this->PCO;
            Z80_trace_format(this);
        }
#endif // LYBODER
#endif // Z80_DBG_SUPPORT
        // Execute an actual opcode
        this->current_instruction(&this->regs, &this->pins);
    }
}

u32 Z80_parity(u32 val) {
    val ^= val >> 4;
    val ^= val >> 2;
    val ^= val >> 1;
    return (val & 1) == 0;
}