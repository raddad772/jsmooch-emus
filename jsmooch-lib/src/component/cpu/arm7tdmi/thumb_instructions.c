//
// Created by . on 12/18/24.
//
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "arm7tdmi.h"
#include "thumb_instructions.h"

#define UNIMPLEMENTED printf("\nUNIMPLEMENTED INSTRUCTION! %04x", ins->opcode); assert(1==2)

#define PC R[15]
#define rSP R[13]

static u32 LSL(struct ARM7TDMI *this, u32 v, u32 amount, u32 set_flags) {
    this->carry = this->regs.CPSR.C;
    if (amount == 0) return v;
    this->carry = amount > 32 ? 0 : !!(v & 1 << (32 - amount));
    v = (amount > 31) ? 0 : (v << amount);
    return v;
}

// Logical shift right
static u32 LSR(struct ARM7TDMI *this, u32 v, u32 amount, u32 set_flags)
{
    if (set_flags) this->carry = this->regs.CPSR.C;
    if (amount == 0) return v;
    if (set_flags) this->carry = (amount > 32) ? 0 : !!(v & 1 << (amount - 1));
    v = (amount > 31) ? 0 : (v >> amount);
    return v;
}

// Arithemtic (sign-extend) shift right
static u32 ASR(struct ARM7TDMI *this, u32 v, u32 amount, u32 set_flags)
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


static inline void write_reg(struct ARM7TDMI *this, u32 *r, u32 v) {
    *r = v;
}

static inline u32 *getR(struct ARM7TDMI *this, u32 num) {
    return this->regmap[num];
}
/*
THUMB_ADC_R1_R2: 00001852
THUMB_ADC_R2_R3: 0000189B
THUMB_ADC_R3_R4: 000018E4
THUMB_ADC_R6_R7: 000019BF
THUMB_NOP: 000046C0
 */

static int condition_passes(struct ARM7TDMI_regs *this, int which) {
#define flag(x) (this->CPSR. x)
    switch(which) {
        case ARM7CC_AL:    return 1;
        case ARM7CC_NV:    return 0;
        case ARM7CC_EQ:    return flag(Z) == 1;
        case ARM7CC_NE:    return flag(Z) != 1;
        case ARM7CC_CS_HS: return flag(C) == 1;
        case ARM7CC_CC_LO: return flag(C) == 0;
        case ARM7CC_MI:    return flag(N) == 1;
        case ARM7CC_PL:    return flag(N) == 0;
        case ARM7CC_VS:    return flag(V) == 1;
        case ARM7CC_VC:    return flag(V) == 0;
        case ARM7CC_HI:    return (flag(C) == 1) && (flag(Z) == 0);
        case ARM7CC_LS:    return (flag(C) == 0) || (flag(Z) == 1);
        case ARM7CC_GE:    return flag(N) == flag(V);
        case ARM7CC_LT:    return flag(N) != flag(V);
        case ARM7CC_GT:    return (flag(Z) == 0) && (flag(N) == flag(V));
        case ARM7CC_LE:    return (flag(Z) == 1) || (flag(N) != flag(V));
        default:
        NOGOHERE;
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


void ARM7TDMI_THUMB_ins_INVALID(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

static u32 ADD(struct ARM7TDMI *this, u32 op1, u32 op2)
{
    u32 result = op1 + op2;
    this->regs.CPSR.N = (result >> 31) & 1;
    this->regs.CPSR.Z = result == 0;
    this->regs.CPSR.C = result < op1;
    this->carry = this->regs.CPSR.C;
    this->regs.CPSR.V = (~(op1 ^ op2) & (op2 ^ result)) >> 31;
    return result;
}

static u32 SUB(struct ARM7TDMI *this, u32 op1, u32 op2, u32 set_flags)
{
    u32 result = op1 - op2;
    this->regs.CPSR.N = (result >> 31) & 1;
    this->regs.CPSR.Z = result == 0;
    this->carry = op1 >= op2;
    this->regs.CPSR.C = this->carry;
    this->regs.CPSR.V = ((op1 ^ op2) & (op1 ^ result)) >> 31;
    return result;
}

void ARM7TDMI_THUMB_ins_ADD_SUB(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    u64 val;
    if (ins->I) {
        val = ins->imm;
    }
    else {
        val = *getR(this, ins->Rn);
    }
    this->regs.PC += 2;
    u32 *Rd = getR(this, ins->Rd);
    u32 op1 = *getR(this, ins->Rs);
    if (ins->sub_opcode) *Rd = SUB(this, op1, val, 1);
    else *Rd = ADD(this, op1, val);
}

void ARM7TDMI_THUMB_ins_LSL_LSR_ASR(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    u32 *Rd = getR(this, ins->Rd);
    u32 Rs = *getR(this, ins->Rs);
    this->regs.PC += 2;
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

void ARM7TDMI_THUMB_ins_MOV_CMP_ADD_SUB(struct ARM7TDMI *this, struct thumb_instruction *ins)
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
    this->regs.PC += 2;
}

static u32 ROR(struct ARM7TDMI *this, u32 v, u32 amount)
{
    this->carry = this->regs.CPSR.C;
    if (amount == 0) return v;
    amount &= 31;
    if (amount) v = (v << (32 - amount)) | (v >> amount); // ?correct
    this->carry = !!(v & 1 << 31);
    return v;
}

void ARM7TDMI_THUMB_ins_data_proc(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
#define setnz(x) this->regs.CPSR.N = ((x) >> 31) & 1; \
              this->regs.CPSR.Z = (x) == 0;
 u32 Rs = *getR(this, ins->Rs);
    u32 *Rd = getR(this, ins->Rd);
    this->regs.PC += 2;
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
            this->cycles_executed++;
            *Rd = LSL(this, *Rd, Rs & 0xFF, 1);
            setnz(*Rd);
            break;
        case 3: {// LSR
            this->cycles_executed++;
            *Rd = LSR(this, *Rd, Rs & 0xFF, 1);
            setnz(*Rd);
            break; }
        case 4: // ASR
            //
            //*Rd = Rs < 32 ? (((i32)(*Rd)) >> Rs) : ((Rs & 0x80000000) ? 0xFFFFFFFF : 0);
            *Rd = ASR(this, *Rd, Rs & 0xFF, 1);
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
            *Rd *= Rs;
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

void ARM7TDMI_THUMB_ins_BX(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    u32 addr = *getR(this, ins->Rs);
    this->regs.CPSR.T = addr & 1;
    this->regs.PC = addr & 0xFFFFFFFE;
    ARM7TDMI_flush_pipeline(this);
}

void ARM7TDMI_THUMB_ins_ADD_CMP_MOV_hi(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    assert(ins->sub_opcode != 3);
    u32 op1 = *getR(this, ins->Rs);
    u32 op2 = *getR(this, ins->Rd);
    this->regs.PC += 2;

    switch(ins->sub_opcode) {
        case 0: // ADD
            *getR(this, ins->Rd) = op1 + op2;
            if (ins->Rd == 15) {
                this->regs.PC &= 0xFFFFFFFE;
                ARM7TDMI_flush_pipeline(this);
            }
            break;
        case 1: // CMP
            SUB(this, op2, op1, 1);
            break;
        case 2: // MOV
            *getR(this, ins->Rd) = op1;
            if (ins->Rd == 15) {
                this->regs.PC &= 0xFFFFFFFE;
                ARM7TDMI_flush_pipeline(this);
            }
            break;
    }
}

void ARM7TDMI_THUMB_ins_LDR_PC_relative(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    u32 addr = (this->regs.PC & (~3)) + ins->imm;
    this->regs.PC += 2;
    u32 *Rd = getR(this, ins->Rd);
    u32 v = ARM7TDMI_read(this, addr, 4, ARM7P_nonsequential, 1);
    *Rd = v;
}

void ARM7TDMI_THUMB_ins_LDRH_STRH_reg_offset(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    u32 addr = *getR(this, ins->Rb) + *getR(this, ins->Ro);
    u32 *Rd = getR(this, ins->Rd);
    this->regs.PC += 2;
    if (ins->L) { // load
        u32 v = ARM7TDMI_read(this, addr, 2, ARM7P_nonsequential, 1);
        if (addr & 1) v = (v >> 8) | (v << 24);
        write_reg(this, Rd, v);
    }
    else { // store
        ARM7TDMI_write(this, addr, 2, ARM7P_nonsequential, (*Rd) & 0xFFFF);
    }
}

void ARM7TDMI_THUMB_ins_LDRSH_LDRSB_reg_offset(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    u32 addr = *getR(this, ins->Rb) + *getR(this, ins->Ro);
    this->regs.PC += 2;
    u32 *Rd = getR(this, ins->Rd);
    u32 sz = ins->B ? 1 : 2;
    u32 mask = ins->B ? 0xFF : 0xFFFF;

    u32 v = ARM7TDMI_read(this, addr, sz, ARM7P_nonsequential, 1);
    if ((ins->B)) {
        v = SIGNe8to32(v);
    }
    else {
        if (addr & 1) { v = (v >> 8); v = SIGNe8to32(v); }
        else v = SIGNe16to32(v);
    }
    *Rd = v;
}

void ARM7TDMI_THUMB_ins_LDR_STR_reg_offset(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    u32 addr = *getR(this, ins->Rb) + *getR(this, ins->Ro);
    this->regs.PC += 2;
    u32 *Rd = getR(this, ins->Rd);
/*
	@ LDR Rd,[Rb,#imm]
	@ LDR Rd,[Rb,Ro]
 */
    if (ins->L) { // Load
        u32 v = ARM7TDMI_read(this, addr, 4, ARM7P_nonsequential, 1);
        if (addr & 3) v = align_val(addr, v);
        *Rd = v;
    }
    else { // Store
        ARM7TDMI_write(this, addr, 4, ARM7P_nonsequential, *Rd);
    }
}

void ARM7TDMI_THUMB_ins_LDRB_STRB_reg_offset(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    u32 addr = *getR(this, ins->Rb) + *getR(this, ins->Ro);
    this->regs.PC += 2;
    u32 *Rd = getR(this, ins->Rd);
    if (ins->L) { // Load
        u32 v = ARM7TDMI_read(this, addr, 1, ARM7P_nonsequential, 1);
        *Rd = v;
    }
    else { // Store
        ARM7TDMI_write(this, addr, 1, ARM7P_nonsequential, (*Rd) & 0xFF);
    }
}

void ARM7TDMI_THUMB_ins_LDR_STR_imm_offset(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    u32 addr = *getR(this, ins->Rb) + ins->imm;
    this->regs.PC += 2;
    u32 *Rd = getR(this, ins->Rd);
    if (ins->L) { // Load
        u32 v = ARM7TDMI_read(this, addr, 4, ARM7P_nonsequential, 1);
        if (addr & 3) v = align_val(addr, v);
        /*
	ldr 	r1,=0x44332211
	ldr 	r2,=0x88776655
	ldr 	r3,=_tvar64
         */
        *Rd = v;
    }
    else { // Store
        ARM7TDMI_write(this, addr, 4, ARM7P_nonsequential, *Rd);
    }
}

void ARM7TDMI_THUMB_ins_LDRB_STRB_imm_offset(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    u32 addr = *getR(this, ins->Rb) + ins->imm;
    u32 *Rd = getR(this, ins->Rd);
    this->regs.PC += 2;
    if (ins->L) { // load
        u32 v = ARM7TDMI_read(this, addr, 1, ARM7P_nonsequential, 1);
        //if (addr & 1) v = (v >> 8) | (v << 24);
        write_reg(this, Rd, v);
    }
    else { // store
        ARM7TDMI_write(this, addr, 1, ARM7P_nonsequential, (*Rd) & 0xFF);
    }
}

void ARM7TDMI_THUMB_ins_LDRH_STRH_imm_offset(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    u32 addr = *getR(this, ins->Rb) + ins->imm;
    u32 *Rd = getR(this, ins->Rd);
    this->regs.PC += 2;
    if (ins->L) { // load
        u32 v = ARM7TDMI_read(this, addr, 2, ARM7P_nonsequential, 1);
        if (addr & 1) v = (v >> 8) | (v << 24);
        write_reg(this, Rd, v);
    }
    else { // store
        ARM7TDMI_write(this, addr, 2, ARM7P_nonsequential, (*Rd) & 0xFFFF);
    }
}

void ARM7TDMI_THUMB_ins_LDR_STR_SP_relative(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    u32 addr = *getR(this, 13) + ins->imm;
    this->regs.PC += 2;
    if (ins->L) { // if Load
        u32 v = ARM7TDMI_read(this, addr, 4, ARM7P_nonsequential, 1);
        if (addr & 3) v = align_val(addr, v);
        write_reg(this, getR(this, ins->Rd), v);
    }
    else { // Store
        ARM7TDMI_write(this, addr, 4, ARM7P_nonsequential, *getR(this, ins->Rd));
    }
}

void ARM7TDMI_THUMB_ins_ADD_SP_or_PC(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    u32 *Rd = getR(this, ins->Rd);
    if (ins->SP) *Rd = *getR(this, 13) + ins->imm;
    else *Rd = (this->regs.PC & 0xFFFFFFFD) + ins->imm;
    this->regs.PC += 2;
    this->pipeline.access = ARM7P_sequential | ARM7P_code;
}

void ARM7TDMI_THUMB_ins_ADD_SUB_SP(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    u32 *sp = getR(this, 13);
    if (ins->S) *sp -= ins->imm;
    else *sp += ins->imm;
    this->regs.PC += 2;
    this->pipeline.access = ARM7P_sequential | ARM7P_code;
}

void ARM7TDMI_THUMB_ins_PUSH_POP(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    u32 *r13 = getR(this, 13);
    u32 pop = ins->sub_opcode;
    this->regs.PC += 2;
    if ((ins->rlist == 0) && (!ins->PC_LR)) {
        if (pop) {
            this->regs.PC = ARM7TDMI_read(this, *r13, 4, ARM7P_nonsequential, 1);
            ARM7TDMI_flush_pipeline(this);
            *r13 += 64;
        }
        else {
            *r13 -= 64;
            ARM7TDMI_write(this, *r13, 4, ARM7P_nonsequential, this->regs.PC);
        }
        return;
    }
    u32 addr = *r13;
    int atype = ARM7P_nonsequential;
    if (pop) {
        for (u32 i = 0; i < 8; i++) {
            if (ins->rlist & (1 << i)) {
                *getR(this, i) = ARM7TDMI_read(this, addr, 4, atype, 1);
                addr += 4;
                atype = ARM7P_sequential;
            }
        }
        if (ins->PC_LR) {
            this->regs.PC = ARM7TDMI_read(this, addr, 4, atype, 1) & 0xFFFFFFFE;
            *r13 = addr + 4;
            this->cycles_executed++;
            ARM7TDMI_flush_pipeline(this);
            return;
        }
        this->cycles_executed++;
        *r13 = addr;
    }
    else { // push!
        for (u32 i = 0; i < 8; i++) {
            if (ins->rlist & (1 << i))
                addr -= 4;
        }
        if (ins->PC_LR) addr -= 4;
        *r13 = addr;
        for (u32 i = 0; i < 8; i++) {
            if (ins->rlist & (1 << i)) {
                ARM7TDMI_write(this, addr, 4, atype, *getR(this, i));
                atype = ARM7P_sequential;
                addr += 4;
            }
        }
        if (ins->PC_LR) {
            ARM7TDMI_write(this, addr, 4, atype, *getR(this, 14));
        }
    }
}

void ARM7TDMI_THUMB_ins_LDM_STM(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    u32 *r13 = getR(this, ins->Rb);
    u32 load = ins->sub_opcode;
    this->regs.PC += 2;
    // 	stmia 	r3!,{r1,r2}
    if (ins->rlist == 0)  {
        if (load) {
            this->regs.PC = ARM7TDMI_read(this, *r13, 4, ARM7P_nonsequential, 1);
            ARM7TDMI_flush_pipeline(this);
        }
        else {
            ARM7TDMI_write(this, *r13, 4, ARM7P_nonsequential, this->regs.PC);
        }
        *r13 += 0x40;
        return;
    }
    if (load) {
        u32 addr = *r13;
        u32 atype = ARM7P_nonsequential;
        for (u32 i = 0; i < 8; i++) {
            if (ins->rlist & (1 << i)) {
                write_reg(this, getR(this, i), ARM7TDMI_read(this, addr, 4, atype, 1));
                addr += 4;
            }
        }
        this->cycles_executed++;
        if (~ins->rlist & (1 << ins->Rb))
            *r13 = addr;

    }
    else { // store
        u32 first = 0;
        u32 count = 0;
        for (int i = 7; i >= 0; i--) {
            if (ins->rlist & (1 << i)) {
                count++;
                first = i;
            }
        }
        u32 addr = *r13;
        u32 addr_new = addr + (count << 2);
        u32 atype = ARM7P_nonsequential;
        for (int i = first; i < 8; i++) {
            if (ins->rlist & (1 << i)) {
                ARM7TDMI_write(this, addr, 4, atype, *getR(this, i));
                if (i == first) *r13 = addr_new;
                addr += 4;
                atype = ARM7P_sequential;
            }
        }
    }
}

void ARM7TDMI_THUMB_ins_SWI(struct ARM7TDMI *this, struct thumb_instruction *ins)
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
    this->regs.CPSR.mode = ARM7_supervisor;
    this->regs.CPSR.T = 0; // exit THUMB
    this->regs.CPSR.I = 1;
    ARM7TDMI_fill_regmap(this);
    this->regs.PC = 0x00000008;
    ARM7TDMI_flush_pipeline(this);
}

void ARM7TDMI_THUMB_ins_UNDEFINED_BCC(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    ARM7TDMI_THUMB_ins_BCC(this, ins);
}

void ARM7TDMI_THUMB_ins_BCC(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    this->regs.PC += 2;
    if (condition_passes(&this->regs, ins->sub_opcode)) {
        this->regs.PC += ins->imm - 2;
        ARM7TDMI_flush_pipeline(this);
    }
}

void ARM7TDMI_THUMB_ins_B(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    this->regs.PC += ins->imm;
    ARM7TDMI_flush_pipeline(this);
}

void ARM7TDMI_THUMB_ins_BL_BLX_prefix(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    u32 *lr = getR(this, 14);
    this->regs.PC += 2;
    *lr = this->regs.PC + ins->imm - 2;
}

void ARM7TDMI_THUMB_ins_BL_suffix(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    u32 v = (this->regs.PC - 2) | 1;
    u32 *LR = getR(this, 14);
    this->regs.PC = ((*LR) + ins->imm) & 0xFFFFFFFE;
    *LR = v;
    ARM7TDMI_flush_pipeline(this);
}

