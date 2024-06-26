//
// Created by . on 5/15/24.
//

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "m68000.h"
#include "m68000_instructions.h"
#include "generated/generated_disasm.h"

struct M68k_ins_t M68k_decoded[65536];

static u32 clip32[5] = { 0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF };
static u32 msb32[5] = { 0, 0x80, 0x8000, 0, 0x80000000 };

static i32 sgn32(u32 num, u32 sz) {
    switch(sz) {
        case 1:
            return (i32)(i8)num;
        case 2:
            return (i32)(i16)num;
        case 4:
            return (i32)num;
    }
    assert(1==0);
    return 0;
}

static u32 ADD(struct M68k* this, u32 op1, u32 op2, u32 sz, u32 extend)
{
    // Thanks Ares
    u32 target = clip32[sz] & op1;
    u32 source = clip32[sz] & op2;
    u32 result = target + source + (extend ? this->regs.SR.X : 0);
    u32 carries = target ^ source ^ result;
    u32 overflow = (target ^ result) & (source ^ result);

    this->regs.SR.C = !!((carries ^ overflow) & msb32[sz]);
    this->regs.SR.V = !!(overflow & msb32[sz]);
    this->regs.SR.Z = !!((clip32[sz] & result) ? 0 : (extend ? this->regs.SR.Z : 1));
    this->regs.SR.N = sgn32(result,sz) < 0;
    this->regs.SR.X = this->regs.SR.C;

    return clip32[sz] & result;
}

static u32 SUB(struct M68k* this, u32 op1, u32 op2, u32 sz, u32 extend)
{
    u32 target = clip32[sz] & op2;
    u32 source = clip32[sz] & op1;
    u32 result = target - source - (extend ? this->regs.SR.X : 0);
    u32 carries = target ^ source ^ result;
    u32 overflow = (target ^ result) & (source ^ target);

    this->regs.SR.C = !!((carries ^ overflow) & msb32[sz]);
    this->regs.SR.V = !!(overflow & msb32[sz]);
    this->regs.SR.Z = !!((clip32[sz] & result) ? 0 : (extend ? this->regs.SR.Z : 1));
    this->regs.SR.N = sgn32(result,sz) < 0;
    this->regs.SR.X = this->regs.SR.C;

    return result & clip32[sz];
}

static u32 LSL(struct M68k* this, u32 result, u32 shift, u32 sz)
{
    u32 carry = 0;
    for (u32 i = 0; i < shift; i++) {
        carry = !!(result & msb32[sz]);
        result <<= 1;
    }

    this->regs.SR.C = carry;
    this->regs.SR.V = 0;
    this->regs.SR.Z = (clip32[sz] & result) == 0;
    this->regs.SR.N = !!(msb32[sz] & result);
    if (shift > 0) this->regs.SR.X = this->regs.SR.C;

    return clip32[sz] & result;
}

static u32 LSR(struct M68k* this, u32 result, u32 shift, u32 sz)
{
    u32 carry = 0;
    result &= clip32[sz];
    for (u32 i = 0; i < shift; i++) {
        carry = result & 1;
        result >>= 1;
    }

    this->regs.SR.C = carry;
    this->regs.SR.V = 0;
    this->regs.SR.Z = (clip32[sz] & result) == 0;
    this->regs.SR.N = !!(msb32[sz] & result);
    if (shift > 0) this->regs.SR.X = this->regs.SR.C;

    return clip32[sz] & result;
}

static u32 EOR(struct M68k* this, u32 source, u32 target, u32 sz)
{
    u32 result = target ^ source;
    this->regs.SR.C = this->regs.SR.V = 0;
    result &= clip32[sz];
    this->regs.SR.Z = result == 0;
    this->regs.SR.N = !!(msb32[sz] & result);

    return result;
}

static u32 condition(struct M68k* this, u32 condition) {
    switch(condition) {
        case 0: return 1;
        case 1: return 0;
        case  2: return !this->regs.SR.C && !this->regs.SR.Z;  //HI
        case  3: return  this->regs.SR.C ||  this->regs.SR.Z;  //LS
        case  4: return !this->regs.SR.C;  //CC,HS
        case  5: return  this->regs.SR.C;  //CS,LO
        case  6: return !this->regs.SR.Z;  //NE
        case  7: return  this->regs.SR.Z;  //EQ
        case  8: return !this->regs.SR.V;  //VC
        case  9: return  this->regs.SR.V;  //VS
        case 10: return !this->regs.SR.N;  //PL
        case 11: return  this->regs.SR.N;  //MI
        case 12: return  this->regs.SR.N ==  this->regs.SR.V;  //GE
        case 13: return  this->regs.SR.N !=  this->regs.SR.V;  //LT
        case 14: return  this->regs.SR.N ==  this->regs.SR.V && !this->regs.SR.Z;  //GT
        case 15: return  this->regs.SR.N !=  this->regs.SR.V ||  this->regs.SR.Z;  //LE
        default: assert(1==0);        
    }
}

#define M68KINS(x) static void M68k_ins_##x(struct M68k* this, struct M68k_ins_t *ins) { \
    switch(this->state.instruction.TCU) {
#define M68KINS_NOSWITCH(x) static void M68k_ins_##x(struct M68k* this, struct M68k_ins_t *ins) {
#define INS_END_NOSWITCH }

#define STEP0 case 0: {\
            this->state.instruction.done = 0;

#define STEP(x) break; }\
                case x: {

#define INS_END \
            this->state.instruction.done = 1; \
            break; }\
    }\
    this->state.instruction.TCU++;            \
}

#define INS_END_NOCOMPLETE \
            break; }\
    }\
    this->state.instruction.TCU++;            \
}


void M68k_ins_RESET(struct M68k* this, struct M68k_ins_t *ins) {
    switch(this->state.instruction.TCU) {
        // $0 (32-bit), indirect, -> SSP
        // $4 (32-bit) -> PC
        // SR->interrupt level = 7
        STEP0
            printf("\n\nRESET!");
            this->state.bus_cycle.next_state = M68kS_exec;
            M68k_start_read(this, 0, 2, M68k_FC_supervisor_program, M68kS_exec);
        STEP(1)
            // First word will be
            this->state.instruction.result = this->state.bus_cycle.data << 16;
            M68k_start_read(this, 2, 2, M68k_FC_supervisor_program, M68kS_exec);
        STEP(2)
            this->regs.ASP = this->state.instruction.result | this->state.bus_cycle.data;
            this->state.instruction.result = 0;
            M68k_start_read(this, 4, 2, M68k_FC_supervisor_program, M68kS_exec);
        STEP(3)
            this->state.instruction.result = this->state.bus_cycle.data << 16;
            M68k_start_read(this, 6, 2, M68k_FC_supervisor_program, M68kS_exec);
        STEP(4)
            this->regs.PC = this->state.instruction.result | this->state.bus_cycle.data;
            this->regs.SR.I = 7;
            // Start filling prefetch queue
            M68k_start_read(this, this->regs.PC, 2, M68k_FC_supervisor_program, M68kS_exec);
            this->regs.PC += 2;
        STEP(5)
            this->regs.IRC = this->state.bus_cycle.data;
            this->regs.IR = this->regs.IRC;
            M68k_start_read(this, this->regs.PC, 2, M68k_FC_supervisor_program, M68kS_exec);
            this->regs.PC += 2;
        STEP(6)
            this->regs.IRC = this->state.bus_cycle.data;
            this->regs.IRD = this->regs.IR;
INS_END

// IRD holds currently-executing instruction,
// IR holds currently-decoding instruction,
// IRC holds last pre-fetched word

#define BADINS { printf("\nUNIMPLEMENTED INSTRUCTION! %s", __func__); assert(1==0); }

M68KINS(TAS)
        STEP0
            BADINS;
INS_END


M68KINS(ABCD)
        STEP0
            BADINS;
INS_END

M68KINS(ADD)
        STEP0
            BADINS;
INS_END

M68KINS(ADDA)
        STEP0
            BADINS;
INS_END

M68KINS(ADDI)
        STEP0
            BADINS;
INS_END

M68KINS(ADDQ)
        STEP0
            BADINS;
INS_END

M68KINS(ADDX_sz4_predec)
        STEP0
            M68k_start_read_operands(this, 1, 1, M68kS_exec);
        STEP(1)
            this->state.instruction.result = ADD(this, this->state.op[0].val, this->state.op[1].val, ins->sz, 1);
        STEP(2)
            this->regs.A[ins->ea2.reg] -= 2;
            M68k_start_write(this, this->regs.A[ins->ea2.reg], this->state.instruction.result & 0xFFFF, 2, MAKE_FC(0), M68kS_exec);
        STEP(3)
            this->state.instruction.prefetch++;
            M68k_start_prefetch(this, 1, 1, M68kS_exec); // all prefetches are done before writes
        STEP(4)
            this->regs.A[ins->ea2.reg] -= 2;
            M68k_start_write(this, this->regs.A[ins->ea2.reg], this->state.instruction.result >> 16, 2, MAKE_FC(0), M68kS_exec);
        STEP(5)
INS_END

M68KINS(ADDX_sz4_nopredec)
        STEP0
            M68k_start_read_operands(this, 1, 1, M68kS_exec);
        STEP(1)
            this->state.instruction.result = ADD(this, this->state.op[0].val, this->state.op[1].val, ins->sz, 1);
            M68k_start_prefetch(this, 1, 1, M68kS_exec); // all prefetches are done before writes
        STEP(2)
            M68k_start_write_operand(this, 0, 1, M68kS_exec);
        STEP(3)
            M68k_start_wait(this, 4, M68kS_exec);
        STEP(4)
INS_END


M68KINS(ADDX_sz1_2)
        STEP0
            M68k_start_read_operands(this, 1, 1, M68kS_exec);
        STEP(1)
            this->state.instruction.result = ADD(this, this->state.op[0].val, this->state.op[1].val, ins->sz, 1);
            M68k_start_prefetch(this, 1, 1, M68kS_exec); // all prefetches are done before writes
        STEP(2)
            M68k_start_write_operand(this, 0, 1, M68kS_exec);
        STEP(3)
INS_END

M68KINS_NOSWITCH(ADDX)
    if (ins->sz == 4) {
        u32 u = (ins->operand_mode == M68k_OM_ea_ea) || (ins->operand_mode == M68k_OM_r_ea);
        if (u && (ins->ea2.kind == M68k_AM_address_register_indirect_with_predecrement)) {
            return M68k_ins_ADDX_sz4_predec(this, ins);
        }
        else {
            return M68k_ins_ADDX_sz4_nopredec(this, ins);
        }
    } else {
        return M68k_ins_ADDX_sz1_2(this, ins);
    }
INS_END_NOSWITCH


M68KINS(AND)
        STEP0
            BADINS;
INS_END

M68KINS(ANDI)
        STEP0
            BADINS;
INS_END

M68KINS(ANDI_TO_CCR)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            u32 v = this->regs.SR.u & 0x1F;
            v &= this->regs.IR;
            this->regs.SR.u = (this->regs.SR.u & 0xFFE0) | v;
            M68k_start_wait(this, 8, M68kS_exec);
        STEP(3)
            M68k_start_read(this, this->regs.PC-2, 2, MAKE_FC(1), M68kS_exec);
        STEP(4)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(5)
INS_END

M68KINS(ANDI_TO_SR)
        STEP0
            BADINS;
INS_END

M68KINS(ASL)
        STEP0
            BADINS;
INS_END

M68KINS(ASR)
        STEP0
            BADINS;
INS_END

M68KINS(BCC)
        STEP0
            this->state.instruction.result = condition(this, this->ins->ea1.reg);
            M68k_start_wait(this, this->state.instruction.result ? 2 : 4, M68kS_exec);
        STEP(1)
            if (!this->state.instruction.result) {
                M68k_start_prefetch(this, (this->ins->ea2.reg == 0) ? 2 : 1, 1, M68kS_exec);
            }
            else {
                u32 cnt = SIGNe8to32(this->ins->ea2.reg);
                if (cnt == 0) cnt = SIGNe16to32(this->regs.IRC);
                this->regs.PC -= 2;
                this->regs.PC += cnt;
                M68k_start_prefetch(this, 2, 1, M68kS_exec);
            }
        STEP(2)
INS_END

M68KINS(BCHG)
        STEP0
            BADINS;
INS_END

M68KINS(BCLR)
        STEP0
            BADINS;
INS_END

M68KINS(BRA)
        STEP0
            M68k_start_wait(this, 2, M68kS_exec);
        STEP(1)
            u32 cnt = SIGNe8to32(this->ins->ea1.reg);
            if (cnt == 0) cnt = SIGNe16to32(this->regs.IRC);
            this->regs.PC -= 2;
            this->regs.PC += cnt;
            M68k_start_prefetch(this, 2, 1, M68kS_exec);
        STEP(2)
INS_END

M68KINS(BSET)
        STEP0
            BADINS;
INS_END

/*
 * 0 - currently exec, IRD  -4
 * 0 - IR (next instruction) -4
 * 2 - IRC   -2
 * 4 - next fetch  -0
 */

M68KINS(BSR)
       STEP0
           // at this point, my PC is 6 ahead of actual executing instruction. so we must write -4 to stack
           this->regs.PC -= 2;
           this->regs.A[7] -= 4;
           if (this->ins->ea1.reg != 0) {
               M68k_start_write(this, this->regs.A[7], this->regs.PC, 4, MAKE_FC(0), M68kS_exec);
           }
           else {
               M68k_start_write(this, this->regs.A[7], this->regs.PC+2, 4, MAKE_FC(0), M68kS_exec);
           }
           //this->regs.PC -= 2;
        STEP(1)
            i32 offset;
            if (this->ins->ea1.reg != 0) {
                offset = this->ins->ea1.reg;
                offset = SIGNe8to32(offset);
            }
            else {
                offset = SIGNe16to32(this->regs.IRC);
            }
            printf("\nBSR PC %06x   OFFS %08x", this->regs.PC, offset);

            this->regs.PC += offset;
            printf("\nNEW PC! %06x", this->regs.PC);
            M68k_start_prefetch(this, 2, 0, M68kS_exec);
        STEP(2)
INS_END

M68KINS(BTST)
        STEP0
            BADINS;
INS_END

M68KINS(CHK)
        STEP0
            M68k_start_read_operands(this, 0, 0, M68kS_exec);
        STEP(1)
            u32 target = this->state.op[0].val;
            u32 source = this->state.op[1].val;
            i64 result = (i64)target - (i64)source;
            this->regs.SR.C = (i64)(i16)(result >> 1) < 0;
            this->regs.SR.V = (i32)(i16)((target ^ source) & (target ^ result)) < 0;
            this->regs.SR.N = ((i32)(i16)(result) < 0) ^ this->regs.SR.V;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            if(!this->regs.SR.N && !this->regs.SR.Z) {
                M68k_start_group2_exception(this, M68kIV_chk_instruction, 0, this->regs.PC-4);
                return;
            }
            u32 target = this->state.op[0].val & 0xFFFF;
            this->regs.SR.Z = target == 0;
            this->regs.SR.N = (i32)(i16)target < 0;
            if(this->regs.SR.N) {
                M68k_start_group2_exception(this, M68kIV_chk_instruction, 2, this->regs.PC-4);
            }
        STEP(3)
            M68k_start_wait(this, 6, M68kS_exec);
        STEP(4)
INS_END

M68KINS(CLR)
        STEP0
            BADINS;
INS_END

M68KINS(CMP)
        STEP0
            BADINS;
INS_END

M68KINS(CMPA)
        STEP0
            BADINS;
INS_END

M68KINS(CMPI)
        STEP0
            BADINS;
INS_END

M68KINS(CMPM)
        STEP0
            BADINS;
INS_END

M68KINS(DBCC)
        STEP0
            BADINS;
INS_END

M68KINS(DIVS)
        STEP0
            BADINS;
INS_END

M68KINS(DIVU)
        STEP0
            BADINS;
INS_END

M68KINS(EOR)
        STEP0
            M68k_start_read_operands(this, 0, 0, M68kS_exec);
        STEP(1)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            this->state.instruction.result = EOR(this, this->state.op[0].val, this->state.op[1].val, this->ins->sz);
            M68k_start_write_operand(this, 0, 1, M68kS_exec);
        STEP(3)
            if ((this->ins->sz == 4) && (this->ins->ea2.kind == M68k_AM_data_register_direct)) M68k_start_wait(this, 4, M68kS_exec);
        STEP(4)
INS_END

M68KINS(EORI)
/*
  auto source = extension<Size>();
  auto target = read<Size, Hold>(with);
  auto result = EOR<Size>(source, target);
  prefetch();
  write<Size>(with, result);
  if constexpr(Size == Long) {
    if(with.mode == DataRegisterDirect) idle(4);
  }
 */
        STEP0
            this->state.instruction.result = this->regs.IRC;
            M68k_start_prefetch(this, this->ins->sz >> 1, 1, M68kS_exec);
        STEP(1)
            u32 o = this->state.instruction.result;
            switch(this->ins->sz) {
                case 1:
                    this->state.instruction.result = o & 0xFF;
                    break;
                case 2:
                    this->state.instruction.result = o;
                case 4:
                    this->state.instruction.result = (o << 16) | this->regs.IR;
                    break;
                default:
                    assert(1==0);
            }
            M68k_start_read_operands(this, 0, 0, M68kS_exec);
        STEP(2)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(3)
            this->state.instruction.result = EOR(this, this->state.instruction.result, this->state.op[0].val, this->ins->sz);
            M68k_start_write_operand(this, 0, 0, M68kS_exec);
        STEP(4)
INS_END

M68KINS(EORI_TO_CCR)
        STEP0
            BADINS;
INS_END

M68KINS(EORI_TO_SR)
        STEP0
            BADINS;
INS_END

M68KINS(EXG)
        STEP0
            BADINS;
INS_END

M68KINS(EXT)
        STEP0
            BADINS;
INS_END

M68KINS(ILLEGAL)
        STEP0
            BADINS;
INS_END

M68KINS(JMP)
        STEP0
            BADINS;
INS_END

M68KINS(JSR)
        STEP0
            BADINS;
INS_END

M68KINS(LEA)
        STEP0
            BADINS;
INS_END

M68KINS(LINK)
        STEP0
            BADINS;
INS_END

M68KINS(LSL_qimm_dr)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            M68k_start_wait(this, (this->ins->sz == 4 ? 4 : 2) + this->ins->ea1.reg * 2, M68kS_exec);
        STEP(2)
            this->state.instruction.result = LSL(this, this->regs.D[this->ins->ea2.reg], this->ins->ea1.reg, this->ins->sz);
            M68k_set_dr(this, this->ins->ea2.reg, this->state.instruction.result, this->ins->sz);
INS_END


M68KINS(LSL_dr_dr)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            u32 cnt = this->regs.D[this->ins->ea1.reg] & 63;
            M68k_start_wait(this, (this->ins->sz == 4 ? 4 : 2) + cnt * 2, M68kS_exec);
        STEP(2)
            u32 cnt = this->regs.D[this->ins->ea1.reg] & 63;
            this->state.instruction.result = LSL(this, this->regs.D[this->ins->ea2.reg], cnt, this->ins->sz);
            M68k_set_dr(this, this->ins->ea2.reg, this->state.instruction.result, this->ins->sz);
INS_END

M68KINS(LSL_ea)
        STEP0
            BADINS;
INS_END


M68KINS(LSR_qimm_dr)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            M68k_start_wait(this, (this->ins->sz == 4 ? 4 : 2) + this->ins->ea1.reg * 2, M68kS_exec);
        STEP(2)
            this->state.instruction.result = LSR(this, this->regs.D[this->ins->ea2.reg], this->ins->ea1.reg, this->ins->sz);
            M68k_set_dr(this, this->ins->ea2.reg, this->state.instruction.result, this->ins->sz);
INS_END


M68KINS(LSR_dr_dr)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            u32 cnt = this->regs.D[this->ins->ea1.reg] & 63;
            M68k_start_wait(this, (this->ins->sz == 4 ? 4 : 2) + cnt * 2, M68kS_exec);
        STEP(2)
            u32 cnt = this->regs.D[this->ins->ea1.reg] & 63;
            this->state.instruction.result = LSR(this, this->regs.D[this->ins->ea2.reg], cnt, this->ins->sz);
            M68k_set_dr(this, this->ins->ea2.reg, this->state.instruction.result, this->ins->sz);
INS_END

M68KINS(LSR_ea)
        STEP0
            BADINS;
INS_END

M68KINS(MOVE)
        STEP0
            BADINS;
INS_END

M68KINS(MOVEA)
        STEP0
            M68k_start_read_operands(this, 0, 0, M68kS_exec);
        STEP(1)
            u32 v = this->state.op[0].val;
            if (this->ins->sz == 2) v = SIGNe8to32(v);
            M68k_set_ar(this, this->ins->ea2.reg, v, this->ins->sz);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(3)
INS_END

M68KINS(MOVEM_TO_MEM)
        STEP0
            BADINS;
INS_END

M68KINS(MOVEM_TO_REG)
        STEP0
            BADINS;
INS_END

M68KINS(MOVEP)
        STEP0
            BADINS;
INS_END

M68KINS(MOVEQ)
        STEP0
            BADINS;
INS_END

M68KINS(MOVE_FROM_SR)
        STEP0
            M68k_start_read_operands(this, 0, 0, M68kS_exec);
        STEP(1)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            this->state.instruction.result = M68k_get_SR(this);
            M68k_start_write_operand(this, 0, 0, M68kS_exec);
        STEP(3)
INS_END

M68KINS(MOVE_FROM_USP)
        STEP0
            BADINS;
INS_END

M68KINS(MOVE_TO_CCR)
        STEP0
            BADINS;
INS_END

M68KINS(MOVE_TO_SR)
        STEP0
            BADINS;
INS_END

M68KINS(MOVE_TO_USP)
        STEP0
            BADINS;
INS_END

M68KINS(MULS)
        STEP0
            BADINS;
INS_END

M68KINS(MULU)
        STEP0
            BADINS;
INS_END

M68KINS(NBCD)
        STEP0
            BADINS;
INS_END

M68KINS(NEG)
/*
  auto result = SUB<Size>(read<Size, Hold>(with), 0);
  prefetch();
  write<Size>(with, result);
  if constexpr(Size == Long) {
    if(with.mode == DataRegisterDirect || with.mode == AddressRegisterDirect) idle(2);
  }
 */
        STEP0
            M68k_start_read_operands(this, 0, 0, M68kS_exec);
        STEP(1)
            this->state.instruction.result = SUB(this, this->state.op[0].val, 0, ins->sz, 0);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            if ((this->ins->ea1.kind == M68k_AM_address_register_direct) || (this->ins->ea1.kind == M68k_AM_data_register_direct)) {
                M68k_start_wait(this, 2, M68kS_exec);
            }
        STEP(3)
            M68k_start_write_operand(this, 0, 0, M68kS_exec);
        STEP(4)
INS_END

M68KINS(NEGX)
        STEP0
            BADINS;
INS_END

M68KINS(NOP)
        STEP0
            BADINS;
INS_END

M68KINS(NOT)
        STEP0
            BADINS;
INS_END

M68KINS(OR)
        STEP0
            BADINS;
INS_END

M68KINS(ORI)
        STEP0
            BADINS;
INS_END

M68KINS(ORI_TO_CCR)
        STEP0
            BADINS;
INS_END

M68KINS(ORI_TO_SR)
        STEP0
            BADINS;
INS_END

M68KINS(PEA)
        STEP0
            BADINS;
INS_END

M68KINS(ROL)
        STEP0
            BADINS;
INS_END

M68KINS(ROR)
        STEP0
            BADINS;
INS_END

M68KINS(ROXL)
        STEP0
            BADINS;
INS_END

M68KINS(ROXR)
        STEP0
            BADINS;
INS_END

static void M68k_start_pop(struct M68k* this, u32 sz, u32 FC, enum M68k_states next_state)
{
    M68k_start_read(this, M68k_get_SSP(this), sz, FC, next_state);
    M68k_inc_SSP(this, sz);
}

M68KINS(RTE)
        STEP0
            if (!this->regs.SR.S) {
                M68k_start_group1_exception(this, M68kIV_privilege_violation, 0);
                this->state.instruction.done = 1;
                return;
            }
            this->state.instruction.result = M68k_get_SSP(this);
            M68k_inc_SSP(this, 6);
            M68k_start_read(this, this->state.instruction.result+2, 2, MAKE_FC(0), M68kS_exec);
        STEP(1)
            this->regs.PC = this->state.bus_cycle.data << 16;
            M68k_start_read(this, this->state.instruction.result+0, 2, MAKE_FC(0), M68kS_exec);
        STEP(2)
            M68k_start_read(this, this->state.instruction.result+4, 2, MAKE_FC(0), M68kS_exec);
            M68k_set_SR(this, this->state.bus_cycle.data);
        STEP(3)
            this->regs.PC |= this->state.bus_cycle.data;
            M68k_start_prefetch(this, 2, 1, M68kS_exec);
        STEP(4)
INS_END

M68KINS(RTR)
        STEP0
            BADINS;
INS_END

M68KINS(RTS)
        STEP0
            BADINS;
INS_END

M68KINS(SBCD)
        STEP0
            BADINS;
INS_END

M68KINS(SCC)
        STEP0
            BADINS;
INS_END

M68KINS(STOP)
        STEP0
            BADINS;
INS_END

M68KINS(SUB)
        STEP0
            BADINS;
INS_END

M68KINS(SUBA)
        STEP0
            BADINS;
INS_END

M68KINS(SUBI)
        STEP0
            BADINS;
INS_END

M68KINS(SUBQ)
        STEP0
            BADINS;
INS_END

M68KINS(SUBX_sz4_predec)
        STEP0
            M68k_start_read_operands(this, 1, 1, M68kS_exec);
        STEP(1)
            this->state.instruction.result = SUB(this, this->state.op[1].val, this->state.op[0].val, ins->sz, 1);
            this->regs.A[ins->ea2.reg] += 4;
        STEP(2)
            this->regs.A[ins->ea2.reg] -= 2;
            M68k_start_write(this, this->regs.A[ins->ea2.reg], this->state.instruction.result & 0xFFFF, 2, MAKE_FC(0), M68kS_exec);
        STEP(3)
            this->state.instruction.prefetch++;
            M68k_start_prefetch(this, 1, 1, M68kS_exec); // all prefetches are done before writes
        STEP(4)
            this->regs.A[ins->ea2.reg] -= 2;
            M68k_start_write(this, this->regs.A[ins->ea2.reg], this->state.instruction.result >> 16, 2, MAKE_FC(0), M68kS_exec);
        STEP(5)
INS_END

M68KINS(SUBX_sz4_nopredec)
        STEP0
            M68k_start_read_operands(this, 1, 1, M68kS_exec);
        STEP(1)
            this->state.instruction.result = SUB(this, this->state.op[0].val, this->state.op[1].val, ins->sz, 1);
            M68k_start_prefetch(this, 1, 1, M68kS_exec); // all prefetches are done before writes
        STEP(2)
            M68k_start_write_operand(this, 0, 1, M68kS_exec);
        STEP(3)
            M68k_start_wait(this, 4, M68kS_exec);
        STEP(4)
INS_END

M68KINS(SUBX_sz1_2)
        STEP0
            M68k_start_read_operands(this, 1, 1, M68kS_exec);
        STEP(1)
            this->state.instruction.result = SUB(this, this->state.op[0].val, this->state.op[1].val, ins->sz, 1);
            M68k_start_prefetch(this, 1, 1, M68kS_exec); // all prefetches are done before writes
        STEP(2)
            M68k_start_write_operand(this, 0, 1, M68kS_exec);
        STEP(3)
INS_END

M68KINS_NOSWITCH(SUBX)
    if (ins->sz == 4) {
        u32 u = (ins->operand_mode == M68k_OM_ea_ea) || (ins->operand_mode == M68k_OM_r_ea);
        if (u && (ins->ea2.kind == M68k_AM_address_register_indirect_with_predecrement)) {
            return M68k_ins_SUBX_sz4_predec(this, ins);
        }
        else {
            return M68k_ins_SUBX_sz4_nopredec(this, ins);
        }
    } else {
        return M68k_ins_SUBX_sz1_2(this, ins);
    }
INS_END_NOSWITCH


    M68KINS(SWAP)
        STEP0
            BADINS;
INS_END

M68KINS(TRAP)
        STEP0
            //this->regs.IR = this->regs.IRC;
            //this->regs.IRC = 0;
            //this->regs.PC += 2;
            M68k_start_group2_exception(this, this->ins->ea1.reg + 32, 0, this->regs.PC-2);
        STEP(1)
INS_END

M68KINS(TRAPV)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            if (this->regs.SR.V) M68k_start_group2_exception(this, M68kIV_trapv_instruction, 0, this->regs.PC-4);
        STEP(2)
INS_END

M68KINS(TST)
        STEP0
            M68k_start_read_operands(this, 0, 0, M68kS_exec);
        STEP(1)
            this->regs.SR.C = this->regs.SR.V = 0;
            u32 v = clip32[this->ins->sz] & this->state.op[0].val;
            this->regs.SR.Z = v == 0;
            if (this->ins->sz == 1) v = SIGNe8to32(v);
            else if (this->ins->sz == 2) v = SIGNe16to32(v);
            this->regs.SR.N = v >= 0x80000000;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
INS_END

M68KINS(UNLK)
        STEP0
            BADINS;
INS_END



static u32 M68k_disasm_BAD(struct M68k_ins_t *ins, struct jsm_debug_read_trace *rt, struct jsm_string *out)
{
    printf("\nERROR UNIMPLEMENTED DISASSEMBLY %04x", ins->opcode);
    jsm_string_sprintf(out, "UNIMPLEMENTED DISASSEMBLY %s", __func__);
    return 0;
}


static void M68k_ins_NOINS(struct M68k* this, struct M68k_ins_t *ins)
{
    printf("\nERROR UNIMPLEMENTED M68K INSTRUCTION %04x at PC %06x", ins->opcode, this->regs.PC);
}

#undef M68KINS
#undef INS_END
#undef STEP0
#undef STEP

static int M68k_already_decoded = 0;

#define MF(x) &M68k_ins_##x
#define MD(x) &M68k_disasm_##x

struct m68k_str_ret
{
    u32 t_max, s_max, ea_mode, ea_reg, size_max, rm;

};

static void transform_ea(struct M68k_EA *ea)
{
    if (ea->kind == 7) {
        if (ea->reg == 1) ea->kind = M68k_AM_absolute_long_data;
        else if (ea->reg == 2) ea->kind = M68k_AM_program_counter_with_displacement;
        else if (ea->reg == 3) ea->kind = M68k_AM_program_counter_with_index;
        else if (ea->reg == 4) ea->kind = M68k_AM_immediate;
    }
}

static void bind_opcode(const char* inpt, u32 sz, M68k_ins_func exec_func, M68k_disassemble_t disasm_func, u32 op_or, struct M68k_EA *ea1, struct M68k_EA *ea2, u32 variant, enum M68k_operand_modes operand_mode)
{
    const char *ptr;
    u32 orer = 0x8000;
    u32 out = 0;

    for (ptr = inpt; orer != 0; ptr++) {
        if (*ptr == ' ') continue;
        if (*ptr == '1') out |= orer;
        orer >>= 1;
    }

    out |= op_or;

    assert(out < 65536); assert(out >= 0);

    enum M68k_address_modes k1, k2;
    if (operand_mode == M68k_OM_ea) {
        if ((ea1->kind == M68k_AM_address_register_direct) || (ea1->kind == M68k_AM_data_register_direct)) {
            operand_mode = M68k_OM_r;
        }
    }
    else if (operand_mode == M68k_OM_ea_r) {
        if ((ea1->kind == M68k_AM_address_register_direct) || (ea1->kind == M68k_AM_data_register_direct)) {
            operand_mode = M68k_OM_r_r;
        }
    }
    else if (operand_mode == M68k_OM_r_ea) {
        if ((ea2->kind == M68k_AM_address_register_direct) || (ea2->kind == M68k_AM_data_register_direct)) {
            operand_mode = M68k_OM_r_r;
        }
    }
    else if (operand_mode == M68k_OM_ea_ea) {
        if (((ea1->kind == M68k_AM_address_register_direct) || (ea1->kind == M68k_AM_data_register_direct)) && ((ea2->kind == M68k_AM_address_register_direct) || (ea2->kind == M68k_AM_data_register_direct))) {
            operand_mode = M68k_OM_r_r;
        }
    }

    if (operand_mode == M68k_OM_ea) {
        transform_ea(ea1);
    }
    if (operand_mode == M68k_OM_r_ea) {
        transform_ea(ea2);
    }
    if (operand_mode == M68k_OM_ea_r) {
        transform_ea(ea1);
    }
    if (operand_mode == M68k_OM_ea_ea) {
        transform_ea(ea1);
        transform_ea(ea2);
    }

    struct M68k_ins_t *ins = &M68k_decoded[out];
    //assert(ins->disasm == MD(BAD));
    *ins = (struct M68k_ins_t) {
        .opcode = out,
        .disasm = disasm_func,
        .exec = exec_func,
        .sz = sz,
        .variant = variant,
        .operand_mode = operand_mode
    };
    if (ea1)
        memcpy(&ins->ea1, ea1, sizeof(struct M68k_EA));
    if (ea2)
        memcpy(&ins->ea2, ea2, sizeof(struct M68k_EA));
}

static struct M68k_EA mk_ea(u32 mode, u32 reg) {
    return (struct M68k_EA) {.kind=mode, .reg=reg};
}
#undef BADINS

void do_M68k_decode() {
    if (M68k_already_decoded) return;
    M68k_already_decoded = 1;
    for (u32 i = 0; i < 65536; i++) {
        M68k_decoded[i] = (struct M68k_ins_t) {
                .opcode = i,
                .exec = MF(NOINS),
                .disasm = MD(BADINS),
                .variant = 0
        };
    }

    struct M68k_EA op1, op2;

    // NOLINTNEXTLINE(bugprone-suspicious-include)
    #include "generated/generated_decodes.c"
}
