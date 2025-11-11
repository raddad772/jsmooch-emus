//
// Created by . on 12/18/24.
//
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "arm946es.h"
#include "thumb2_instructions.h"

#define UNIMPLEMENTED printf("\nUNIMPLEMENTED INSTRUCTION! %04x", ins->opcode); assert(1==2)

#define PC R[15]
#define rSP R[13]

static u32 LSL(ARM946ES *this, u32 v, u32 amount, u32 set_flags) {
    this->carry = this->regs.CPSR.C;
    if (amount == 0) return v;
    this->carry = amount > 32 ? 0 : !!(v & 1 << (32 - amount));
    v = (amount > 31) ? 0 : (v << amount);
    return v;
}

// Logical shift right
static u32 LSR(ARM946ES *this, u32 v, u32 amount, u32 set_flags)
{
    if (set_flags) this->carry = this->regs.CPSR.C;
    if (amount == 0) return v;
    if (set_flags) this->carry = (amount > 32) ? 0 : !!(v & 1 << (amount - 1));
    v = (amount > 31) ? 0 : (v >> amount);
    return v;
}

// Arithemtic (sign-extend) shift right
static u32 ASR(ARM946ES *this, u32 v, u32 amount, u32 set_flags)
{
    //   carry = cpsr().c;
    this->carry = this->regs.CPSR.C;

    //   if(shift == 0) return source;
    if (amount == 0) return v;

    //   carry = shift > 32 ? source & 1 << 31 : source & 1 << shift - 1;
    this->carry = (amount >= 32) ? (!!(v & 0x80000000)) : !!(v & (1 << (amount - 1)));

    //   source = shift > 31 ? (i32)source >> 31 : (i32)source >> shift;
    v = (amount > 31) ? (i32)v >> 31 : (i32)v >> amount;

//    return source;
    return v;
}


static inline void write_reg(ARM946ES *this, u32 *r, u32 v) {
    *r = v;
}

static inline u32 *getR(ARM946ES *this, u32 num) {
    return this->regmap[num];
}
/*
THUMB_ADC_R1_R2: 00001852
THUMB_ADC_R2_R3: 0000189B
THUMB_ADC_R3_R4: 000018E4
THUMB_ADC_R6_R7: 000019BF
THUMB_NOP: 000046C0
 */

static int condition_passes(ARM946ES_regs *this, int which) {
#define flag(x) (this->CPSR. x)
    switch(which) {
        case ARM9CC_AL:    return 1;
        case ARM9CC_NV:    return 0;
        case ARM9CC_EQ:    return flag(Z) == 1;
        case ARM9CC_NE:    return flag(Z) != 1;
        case ARM9CC_CS_HS: return flag(C) == 1;
        case ARM9CC_CC_LO: return flag(C) == 0;
        case ARM9CC_MI:    return flag(N) == 1;
        case ARM9CC_PL:    return flag(N) == 0;
        case ARM9CC_VS:    return flag(V) == 1;
        case ARM9CC_VC:    return flag(V) == 0;
        case ARM9CC_HI:    return (flag(C) == 1) && (flag(Z) == 0);
        case ARM9CC_LS:    return (flag(C) == 0) || (flag(Z) == 1);
        case ARM9CC_GE:    return flag(N) == flag(V);
        case ARM9CC_LT:    return flag(N) != flag(V);
        case ARM9CC_GT:    return (flag(Z) == 0) && (flag(N) == flag(V));
        case ARM9CC_LE:    return (flag(Z) == 1) || (flag(N) != flag(V));
        default:
            NOGOHERE;
            return 0;
    }
#undef flag
}

static u32 align_val(u32 addr, u32 tmp)
{
    if (addr & 3) {
        u32 misalignment = addr & 3;
        if (misalignment == 1) tmp = ((tmp >> 8) & 0xFFFFFF) | (tmp << 24);
        else if (misalignment == 2) tmp = (tmp >> 16) | (tmp << 16);
        else tmp = ((tmp << 8) & 0xFFFFFF00) | (tmp >> 24);
    }
    return tmp;

}


void ARM946ES_THUMB_ins_INVALID(ARM946ES *this, thumb2_instruction *ins)
{
    UNIMPLEMENTED;
}

static u32 ADD(ARM946ES *this, u32 op1, u32 op2)
{
    u32 result = op1 + op2;
    this->regs.CPSR.N = (result >> 31) & 1;
    this->regs.CPSR.Z = result == 0;
    this->regs.CPSR.C = result < op1;
    this->carry = this->regs.CPSR.C;
    this->regs.CPSR.V = (~(op1 ^ op2) & (op2 ^ result)) >> 31;
    return result;
}

static u32 SUB(ARM946ES *this, u32 op1, u32 op2, u32 set_flags)
{
    u32 result = op1 - op2;
    this->regs.CPSR.N = (result >> 31) & 1;
    this->regs.CPSR.Z = result == 0;
    this->carry = op1 >= op2;
    this->regs.CPSR.C = this->carry;
    this->regs.CPSR.V = ((op1 ^ op2) & (op1 ^ result)) >> 31;
    return result;
}

void ARM946ES_THUMB_ins_ADD_SUB(ARM946ES *this, thumb2_instruction *ins)
{
    u64 val;
    if (ins->I) {
        val = ins->imm;
    }
    else {
        val = *getR(this, ins->Rn);
    }
    this->regs.PC += 2;
    this->pipeline.access = ARM9P_sequential | ARM9P_code;
    u32 *Rd = getR(this, ins->Rd);
    u32 op1 = *getR(this, ins->Rs);
    if (ins->sub_opcode) *Rd = SUB(this, op1, val, 1);
    else *Rd = ADD(this, op1, val);
}

void ARM946ES_THUMB_ins_LSL_LSR_ASR(ARM946ES *this, thumb2_instruction *ins)
{
    //UPDATE!
    u32 *Rd = getR(this, ins->Rd);
    u32 Rs = *getR(this, ins->Rs);
    this->regs.PC += 2;
    this->pipeline.access = ARM9P_sequential | ARM9P_code;
    this->carry = this->regs.CPSR.C;
    switch(ins->sub_opcode) {
        case 0: // LSL
            *Rd = LSL(this, Rs, ins->imm, 1);
            break;
        case 1: // LSR
            *Rd = LSR(this, Rs, ins->imm ? ins->imm : 32, 1);
            break;
        case 2: // ASR
            *Rd = ASR(this, Rs, ins->imm ? ins->imm : 32, 1);
            break;
    }
    this->regs.CPSR.C = this->carry;
    this->regs.CPSR.Z = *Rd == 0;
    this->regs.CPSR.N = ((*Rd) >> 31) & 1;
}

void ARM946ES_THUMB_ins_MOV_CMP_ADD_SUB(ARM946ES *this, thumb2_instruction *ins)
{
    u32 *Rd = getR(this, ins->Rd);
    switch(ins->sub_opcode) {
        case 0: // MOV
            *Rd = ins->imm;
            this->regs.CPSR.N = (*Rd >> 31) & 1;
            this->regs.CPSR.Z = (*Rd) == 0;
            break;
        case 1: // CMP
            SUB(this, *Rd, ins->imm, 1);
            break;
        case 2: // ADD
            *Rd = ADD(this, *Rd, ins->imm);
            break;
        case 3: // SUB
            *Rd = SUB(this, *Rd, ins->imm, 1);
            break;
    }
    this->pipeline.access = ARM9P_sequential | ARM9P_code;
    this->regs.PC += 2;
}

static u32 ROR(ARM946ES *this, u32 v, u32 amount)
{
    this->carry = this->regs.CPSR.C;
    if (amount == 0) return v;
    amount &= 31;
    if (amount) v = (v << (32 - amount)) | (v >> amount); // ?correct
    this->carry = !!(v & 1 << 31);
    return v;
}


// Thanks Ares for this trick
static inline u32 thumb_mul_ticks(u32 multiplier, u32 is_signed)
{
    u32 n = 1;
    if(multiplier >>  8 && multiplier >>  8 != 0xffffff) n++;
    if(multiplier >> 16 && multiplier >> 16 !=   0xffff) n++;
    if(multiplier >> 24 && multiplier >> 24 !=     0xff) n++;
    return n;
}

void ARM946ES_THUMB_ins_data_proc(ARM946ES *this, thumb2_instruction *ins)
{
#define setnz(x) this->regs.CPSR.N = ((x) >> 31) & 1; \
              this->regs.CPSR.Z = (x) == 0;
    u32 Rs = *getR(this, ins->Rs);
    u32 *Rd = getR(this, ins->Rd);
    this->regs.PC += 2;
    this->pipeline.access = ARM9P_sequential | ARM9P_code;
    this->carry = this->regs.CPSR.C;
    switch(ins->sub_opcode) {
        case 0: // AND (N,Z)
            *Rd &= Rs;
            setnz(*Rd);
            break;
        case 1: // XOR (N,Z)
            *Rd ^= Rs;
            setnz(*Rd);
            break;
        case 2: // LSL (N,Z,C)
            ARM946ES_idle(this, 1);
            this->pipeline.access = ARM9P_nonsequential | ARM9P_code;
            *Rd = LSL(this, *Rd, Rs & 0xFF, 1);
            setnz(*Rd);
            break;
        case 3: {// LSR
            ARM946ES_idle(this, 1);
            this->pipeline.access = ARM9P_nonsequential | ARM9P_code;
            *Rd = LSR(this, *Rd, Rs & 0xFF, 1);
            setnz(*Rd);
            break; }
        case 4: // ASR
            ARM946ES_idle(this, 1);
            *Rd = ASR(this, *Rd, Rs & 0xFF, 1);
            this->pipeline.access = ARM9P_nonsequential | ARM9P_code;
            setnz(*Rd);
            break;
        case 5: // ADC
            *Rd = ADD(this, *Rd, Rs + this->carry);
            break;
        case 6: // SBC
            *Rd = SUB(this, (*Rd) - (this->carry ^ 1), Rs, 1);
            break;
        case 7: // ROR
            *Rd = ROR(this, *Rd, Rs & 0xFF);
            ARM946ES_idle(this, 1);
            this->pipeline.access = ARM9P_nonsequential | ARM9P_code;
            setnz(*Rd);
            break;
        case 8: { // TST (N, Z)
            u32 v = (*Rd) & Rs;
            setnz(v);
            break; }
        case 9: // NEG
            *Rd = SUB(this, 0, Rs, 1);
            break;
        case 10: // CMP Rd, Rs
            SUB(this, *Rd, Rs, 1);
            break;
        case 11: // CMN
            ADD(this, *Rd, Rs);
            break;
        case 12: // ORR
            *Rd |= Rs;
            setnz(*Rd);
            break;
        case 13: // MUL
            ARM946ES_idle(this, thumb_mul_ticks(*Rd, 0));
            *Rd *= Rs;
            this->pipeline.access = ARM9P_nonsequential | ARM9P_code;
            setnz(*Rd);
            break;
        case 14: // BIC
            *Rd &= (Rs ^ 0xFFFFFFFF);
            setnz(*Rd);
            break;
        case 15: // MVN
            *Rd = Rs ^ 0xFFFFFFFF;
            setnz(*Rd);
            break;
    }
    this->regs.CPSR.C = this->carry;
}

void ARM946ES_THUMB_ins_BX_BLX(ARM946ES *this, thumb2_instruction *ins)
{
    // Update the BLX!
    // for BX, MSBd must be zero
    // for BLX, MSBd must be one
    // MSBd = bit 7
    u32 addr = *getR(this, ins->Rs);
    this->regs.CPSR.T = addr & 1;
    this->pipeline.access = ARM9P_nonsequential | ARM9P_code;
    u32 lnk = this->regs.PC - 1;
    this->regs.PC = addr & 0xFFFFFFFE;
    if (ins->sub_opcode) { // BLX
        *getR(this, 14) = lnk;
    }
    ARM946ES_flush_pipeline(this);
}

void ARM946ES_THUMB_ins_ADD_CMP_MOV_hi(ARM946ES *this, thumb2_instruction *ins)
{
    assert(ins->sub_opcode != 3);
    u32 op1 = *getR(this, ins->Rs);
    u32 op2 = *getR(this, ins->Rd);
    this->regs.PC += 2;
    this->pipeline.access = ARM9P_sequential | ARM9P_code;

    switch(ins->sub_opcode) {
        case 0: // ADD
            *getR(this, ins->Rd) = op1 + op2;
            if (ins->Rd == 15) {
                this->regs.PC &= 0xFFFFFFFE;
                ARM946ES_flush_pipeline(this);
            }
            break;
        case 1: // CMP
            SUB(this, op2, op1, 1);
            break;
        case 2: // MOV
            *getR(this, ins->Rd) = op1;
            if (ins->Rd == 15) {
                this->regs.PC &= 0xFFFFFFFE;
                ARM946ES_flush_pipeline(this);
            }
            break;
    }
}

void ARM946ES_THUMB_ins_LDR_PC_relative(ARM946ES *this, thumb2_instruction *ins)
{
    u32 addr = (this->regs.PC & (~3)) + ins->imm;
    this->regs.PC += 2;
    this->pipeline.access = ARM9P_nonsequential | ARM9P_code;
    u32 *Rd = getR(this, ins->Rd);
    u32 v = ARM946ES_read(this, addr, 4, ARM9P_nonsequential, 1);
    *Rd = v;
}

void ARM946ES_THUMB_ins_LDRH_STRH_reg_offset(ARM946ES *this, thumb2_instruction *ins)
{
    u32 addr = *getR(this, ins->Rb) + *getR(this, ins->Ro);
    u32 *Rd = getR(this, ins->Rd);
    this->regs.PC += 2;
    this->pipeline.access = ARM9P_nonsequential | ARM9P_code;
    if (ins->L) { // load
        u32 v = ARM946ES_read(this, addr, 2, ARM9P_nonsequential, 1);
        if (addr & 1) v = (v >> 8) | (v << 24);
        write_reg(this, Rd, v);
    }
    else { // store
        ARM946ES_write(this, addr, 2, ARM9P_nonsequential, (*Rd) & 0xFFFF);
    }
}

void ARM946ES_THUMB_ins_LDRSH_LDRSB_reg_offset(ARM946ES *this, thumb2_instruction *ins)
{
    u32 addr = *getR(this, ins->Rb) + *getR(this, ins->Ro);
    this->regs.PC += 2;
    this->pipeline.access = ARM9P_nonsequential | ARM9P_code;
    u32 *Rd = getR(this, ins->Rd);
    u32 sz = ins->B ? 1 : 2;
    u32 mask = ins->B ? 0xFF : 0xFFFF;

    u32 v = ARM946ES_read(this, addr, sz, ARM9P_nonsequential, 1);
    if ((ins->B)) {
        v = SIGNe8to32(v);
    }
    else {
        v = SIGNe16to32(v);
    }
    *Rd = v;
}

void ARM946ES_THUMB_ins_LDR_STR_reg_offset(ARM946ES *this, thumb2_instruction *ins)
{
    u32 addr = *getR(this, ins->Rb) + *getR(this, ins->Ro);
    this->regs.PC += 2;
    this->pipeline.access = ARM9P_nonsequential | ARM9P_code;
    u32 *Rd = getR(this, ins->Rd);
    if (ins->L) { // Load
        u32 v = ARM946ES_read(this, addr, 4, ARM9P_nonsequential, 1);
        if (addr & 3) v = align_val(addr, v);
        *Rd = v;
    }
    else { // Store
        ARM946ES_write(this, addr, 4, ARM9P_nonsequential, *Rd);
    }
}

void ARM946ES_THUMB_ins_LDRB_STRB_reg_offset(ARM946ES *this, thumb2_instruction *ins)
{
    u32 addr = *getR(this, ins->Rb) + *getR(this, ins->Ro);
    this->regs.PC += 2;
    u32 *Rd = getR(this, ins->Rd);
    this->pipeline.access = ARM9P_nonsequential | ARM9P_code;
    if (ins->L) { // Load
        u32 v = ARM946ES_read(this, addr, 1, ARM9P_nonsequential, 1);
        *Rd = v;
    }
    else { // Store
        ARM946ES_write(this, addr, 1, ARM9P_nonsequential, (*Rd) & 0xFF);
    }
}

void ARM946ES_THUMB_ins_LDR_STR_imm_offset(ARM946ES *this, thumb2_instruction *ins)
{
    u32 addr = *getR(this, ins->Rb) + ins->imm;
    this->regs.PC += 2;
    this->pipeline.access = ARM9P_nonsequential | ARM9P_code;
    u32 *Rd = getR(this, ins->Rd);
    if (ins->L) { // Load
        u32 v = ARM946ES_read(this, addr, 4, ARM9P_nonsequential, 1);
        if (addr & 3) v = align_val(addr, v);
        *Rd = v;
    }
    else { // Store
        ARM946ES_write(this, addr, 4, ARM9P_nonsequential, *Rd);
    }
}

void ARM946ES_THUMB_ins_LDRB_STRB_imm_offset(ARM946ES *this, thumb2_instruction *ins)
{
    u32 addr = *getR(this, ins->Rb) + ins->imm;
    u32 *Rd = getR(this, ins->Rd);
    this->regs.PC += 2;
    this->pipeline.access = ARM9P_nonsequential | ARM9P_code;
    if (ins->L) { // load
        u32 v = ARM946ES_read(this, addr, 1, ARM9P_nonsequential, 1);
        //if (addr & 1) v = (v >> 8) | (v << 24);
        write_reg(this, Rd, v);
    }
    else { // store
        ARM946ES_write(this, addr, 1, ARM9P_nonsequential, (*Rd) & 0xFF);
    }
}

void ARM946ES_THUMB_ins_LDRH_STRH_imm_offset(ARM946ES *this, thumb2_instruction *ins)
{
    u32 addr = *getR(this, ins->Rb) + ins->imm;
    u32 *Rd = getR(this, ins->Rd);
    this->regs.PC += 2;
    this->pipeline.access = ARM9P_nonsequential | ARM9P_code;
    if (ins->L) { // load
        u32 v = ARM946ES_read(this, addr, 2, ARM9P_nonsequential, 1);
        if (addr & 1) v = (v >> 8) | (v << 24);
        write_reg(this, Rd, v);
    }
    else { // store
        ARM946ES_write(this, addr, 2, ARM9P_nonsequential, (*Rd) & 0xFFFF);
    }
}

void ARM946ES_THUMB_ins_LDR_STR_SP_relative(ARM946ES *this, thumb2_instruction *ins)
{
    u32 addr = *getR(this, 13) + ins->imm;
    this->regs.PC += 2;
    this->pipeline.access = ARM9P_nonsequential | ARM9P_code;
    if (ins->L) { // if Load
        u32 v = ARM946ES_read(this, addr, 4, ARM9P_nonsequential, 1);
        if (addr & 3) v = align_val(addr, v);
        write_reg(this, getR(this, ins->Rd), v);
    }
    else { // Store
        ARM946ES_write(this, addr, 4, ARM9P_nonsequential, *getR(this, ins->Rd));
    }
}

void ARM946ES_THUMB_ins_ADD_SP_or_PC(ARM946ES *this, thumb2_instruction *ins)
{
    u32 *Rd = getR(this, ins->Rd);
    if (ins->SP) *Rd = *getR(this, 13) + ins->imm;
    else *Rd = (this->regs.PC & 0xFFFFFFFD) + ins->imm;
    this->regs.PC += 2;
    this->pipeline.access = ARM9P_sequential | ARM9P_code;
}

void ARM946ES_THUMB_ins_ADD_SUB_SP(ARM946ES *this, thumb2_instruction *ins)
{
    u32 *sp = getR(this, 13);
    if (ins->S) *sp -= ins->imm;
    else *sp += ins->imm;
    this->regs.PC += 2;
    this->pipeline.access = ARM9P_sequential | ARM9P_code;
}

void ARM946ES_THUMB_ins_PUSH_POP(ARM946ES *this, thumb2_instruction *ins)
{
    u32 *r13 = getR(this, 13);
    u32 pop = ins->sub_opcode;

    u32 rlist = ins->rlist;
    u32 address = *getR(this, 13);

    u32 access = ARM9P_nonsequential;
    if (pop) {
        for (u32 r=0; r<8; r++) {
            if ((rlist  >> r) & 1) {
                *getR(this, r) = ARM946ES_read(this, address, 4, access, 1);
                address += 4;
                access = ARM9P_sequential;
            }
        }

        if(ins->PC_LR) {
            this->regs.PC = ARM946ES_read(this, address, 4, access, 1);
            *getR(this, 13) = address + 4;
            this->regs.CPSR.T = this->regs.PC & 1;
            ARM946ES_flush_pipeline(this);
            return;
        }

        *getR(this, 13) = address;
    } else {
        for (u32 r = 0; r < 8; r++) {
            if ((rlist >> r) & 1)
                address -= 4;
        }

        if(ins->PC_LR) {
            address -= 4;
        }

        *getR(this, 13) = address;

        for (u32 r = 0; r < 8; r++) {
            if ((rlist >> r) & 1) {
                ARM946ES_write(this, address, 4, access, *getR(this, r));
                address += 4;
                access = ARM9P_sequential;
            }
        }

        if(ins->PC_LR) {
            ARM946ES_write(this, address, 4, access, *getR(this, 14));
        }
    }

    this->regs.PC += 2;
    this->pipeline.access = ARM9P_code | ARM9P_nonsequential;
}

void ARM946ES_THUMB_ins_LDM_STM(ARM946ES *this, thumb2_instruction *ins) {
    u32 rlist = ins->rlist;
    u32 address = *getR(this, ins->Rb);
    u32 L = ins->sub_opcode;
    u32 access = ARM9P_nonsequential;
    if (L) {
        for (u32 i = 0; i <= 7; i++) {
            if ((rlist >> i) & 1) {
                *getR(this, i) = ARM946ES_read(this, address, 4, access, 1);
                address += 4;
            }
            access = ARM9P_sequential;
        }

        if (~rlist & (1 << ins->Rb)) {
            *getR(this, ins->Rb) = address;
        }
    } else {
        for (u32 reg = 0; reg <= 7; reg++) {
            if ((rlist >> reg) & 1) {
                ARM946ES_write(this, address, 4, access, *getR(this, reg));
                address += 4;
            }
            access = ARM9P_sequential;
        }

        *getR(this, ins->Rb) = address;
    }

    this->regs.PC += 2;
}

void ARM946ES_THUMB_ins_SWI(ARM946ES *this, thumb2_instruction *ins)
{
    /*
Execution SWI/BKPT:
  R14_svc=PC+2     R14_abt=PC+4   ;save return address
  SPSR_svc=CPSR    SPSR_abt=CPSR  ;save CPSR flags
  CPSR=<changed>   CPSR=<changed> ;Enter svc/abt, ARM state, IRQs disabled
  PC=VVVV0008h     PC=VVVV000Ch   ;jump to SWI/PrefetchAbort vector address
     */
    this->regs.R_svc[1] = this->regs.PC - 2;
    this->regs.SPSR_svc = this->regs.CPSR.u;
    this->regs.CPSR.mode = ARM9_supervisor;
    this->regs.CPSR.T = 0; // exit THUMB
    this->regs.CPSR.I = 1; // mask IRQ
    this->pipeline.access = ARM9P_nonsequential | ARM9P_code;
    ARM946ES_fill_regmap(this);
    this->regs.PC = this->regs.EBR | 0x00000008;
    ARM946ES_flush_pipeline(this);
}

void ARM946ES_THUMB_ins_BKPT(ARM946ES *this, thumb2_instruction *ins)
{
    this->regs.R_abt[1] = this->regs.PC - 2;
    this->regs.SPSR_abt = this->regs.CPSR.u;
    this->regs.CPSR.mode = ARM9_abort;
    this->regs.CPSR.T = 0; // exit THUMB
    this->regs.CPSR.I = 1; // mask IRQ
    this->pipeline.access = ARM9P_nonsequential | ARM9P_code;
    ARM946ES_fill_regmap(this);
    this->regs.PC = this->regs.EBR | 0x0000000C;
    ARM946ES_flush_pipeline(this);
}

void ARM946ES_THUMB_ins_UNDEFINED_BCC(ARM946ES *this, thumb2_instruction *ins)
{
    ARM946ES_THUMB_ins_BCC(this, ins);
}

void ARM946ES_THUMB_ins_BCC(ARM946ES *this, thumb2_instruction *ins)
{
    this->regs.PC += 2;
    if (condition_passes(&this->regs, ins->sub_opcode)) {
        this->pipeline.access = ARM9P_nonsequential | ARM9P_code;
        this->regs.PC += ins->imm - 2;
        ARM946ES_flush_pipeline(this);
    }
    else {
        this->pipeline.access = ARM9P_sequential | ARM9P_code;
    }
}

void ARM946ES_THUMB_ins_B(ARM946ES *this, thumb2_instruction *ins)
{
    this->regs.PC += ins->imm;
    ARM946ES_flush_pipeline(this);
}

void ARM946ES_THUMB_ins_BL_BLX_prefix(ARM946ES *this, thumb2_instruction *ins)
{
    u32 *lr = getR(this, 14);
    this->regs.PC += 2;
    *lr = this->regs.PC + ins->imm - 2;
    this->pipeline.access = ARM9P_sequential | ARM9P_code;
}

void ARM946ES_THUMB_ins_BL_suffix(ARM946ES *this, thumb2_instruction *ins)
{
    u32 v = (this->regs.PC - 2) | 1;
    u32 *LR = getR(this, 14);
    this->regs.PC = ((*LR) + ins->imm) & 0xFFFFFFFE;
    *LR = v;
    ARM946ES_flush_pipeline(this);
}

void ARM946ES_THUMB_ins_BLX_suffix(ARM946ES *this, thumb2_instruction *ins)
{

    u32 v = (this->regs.PC - 2) | 1;
    u32 *LR = getR(this, 14);
    u32 addr = (*LR) + ins->imm;
    *LR = v;
    this->regs.PC = addr & 0xFFFFFFFC;
    this->regs.CPSR.T = 0;
    ARM946ES_flush_pipeline(this);
}
