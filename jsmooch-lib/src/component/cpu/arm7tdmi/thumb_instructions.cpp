//
// Created by . on 12/18/24.
//
#include <cassert>
#include <cstdio>
#include <cstdlib>

#include "arm7tdmi.h"
#include "thumb_instructions.h"

#define UNIMPLEMENTED printf("\nUNIMPLEMENTED INSTRUCTION! %04x", ins->opcode); assert(1==2)

#define PC R[15]
#define rSP R[13]

namespace ARM7TDMI {

static u32 LSL(core *th, u32 v, u32 amount, u32 set_flags) {
    th->carry = th->regs.CPSR.C;
    if (amount == 0) return v;
    th->carry = amount > 32 ? 0 : !!(v & 1 << (32 - amount));
    v = (amount > 31) ? 0 : (v << amount);
    return v;
}

// Logical shift right
static u32 LSR(core *th, u32 v, u32 amount, u32 set_flags)
{
    if (set_flags) th->carry = th->regs.CPSR.C;
    if (amount == 0) return v;
    if (set_flags) th->carry = (amount > 32) ? 0 : !!(v & 1 << (amount - 1));
    v = (amount > 31) ? 0 : (v >> amount);
    return v;
}

// Arithemtic (sign-extend) shift right
static u32 ASR(core *th, u32 v, u32 amount, u32 set_flags)
{
    //   carry = cpsr().c;
    th->carry = th->regs.CPSR.C;

    //   if(shift == 0) return source;
    if (amount == 0) return v;

    //   carry = shift > 32 ? source & 1 << 31 : source & 1 << shift - 1;
    th->carry = (amount >= 32) ? (!!(v & 0x80000000)) : !!(v & (1 << (amount - 1)));

    //   source = shift > 31 ? (i32)source >> 31 : (i32)source >> shift;
    v = (amount > 31) ? (i32)v >> 31 : (i32)v >> amount;

//    return source;
    return v;
}


static inline void write_reg(core *th, u32 *r, u32 v) {
    *r = v;
}

static inline u32 *getR(core *th, u32 num) {
    return th->regmap[num];
}
/*
THUMB_ADC_R1_R2: 00001852
THUMB_ADC_R2_R3: 0000189B
THUMB_ADC_R3_R4: 000018E4
THUMB_ADC_R6_R7: 000019BF
THUMB_NOP: 000046C0
 */

static int condition_passes(regs *th, int which) {
#define flag(x) (th->CPSR. x)
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


void THUMB_ins_INVALID(core *th, thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

static u32 ADD(core *th, u32 op1, u32 op2)
{
    u32 result = op1 + op2;
    th->regs.CPSR.N = (result >> 31) & 1;
    th->regs.CPSR.Z = result == 0;
    th->regs.CPSR.C = result < op1;
    th->carry = th->regs.CPSR.C;
    th->regs.CPSR.V = (~(op1 ^ op2) & (op2 ^ result)) >> 31;
    return result;
}

static u32 SUB(core *th, u32 op1, u32 op2, u32 set_flags)
{
    u32 result = op1 - op2;
    th->regs.CPSR.N = (result >> 31) & 1;
    th->regs.CPSR.Z = result == 0;
    th->carry = op1 >= op2;
    th->regs.CPSR.C = th->carry;
    th->regs.CPSR.V = ((op1 ^ op2) & (op1 ^ result)) >> 31;
    return result;
}

void THUMB_ins_ADD_SUB(core *th, thumb_instruction *ins)
{
    u64 val;
    if (ins->I) {
        val = ins->imm;
    }
    else {
        val = *getR(th, ins->Rn);
    }
    th->regs.PC += 2;
    th->pipeline.access = ARM7P_sequential | ARM7P_code;
    u32 *Rd = getR(th, ins->Rd);
    u32 op1 = *getR(th, ins->Rs);
    if (ins->sub_opcode) *Rd = SUB(th, op1, val, 1);
    else *Rd = ADD(th, op1, val);
}

void THUMB_ins_LSL_LSR_ASR(core *th, thumb_instruction *ins)
{
    u32 *Rd = getR(th, ins->Rd);
    u32 Rs = *getR(th, ins->Rs);
    th->regs.PC += 2;
    th->pipeline.access = ARM7P_sequential | ARM7P_code;
    th->carry = th->regs.CPSR.C;
    switch(ins->sub_opcode) {
        case 0: // LSL
            *Rd = LSL(th, Rs, ins->imm, 1);
            break;
        case 1: // LSR
            *Rd = LSR(th, Rs, ins->imm ? ins->imm : 32, 1);
            break;
        case 2: // ASR
            *Rd = ASR(th, Rs, ins->imm ? ins->imm : 32, 1);
            break;
    }
    th->regs.CPSR.C = th->carry;
    th->regs.CPSR.Z = *Rd == 0;
    th->regs.CPSR.N = ((*Rd) >> 31) & 1;
}

void THUMB_ins_MOV_CMP_ADD_SUB(core *th, thumb_instruction *ins)
{
    u32 *Rd = getR(th, ins->Rd);
    switch(ins->sub_opcode) {
        case 0: // MOV
            *Rd = ins->imm;
            th->regs.CPSR.N = (*Rd >> 31) & 1;
            th->regs.CPSR.Z = (*Rd) == 0;
            break;
        case 1: // CMP
            SUB(th, *Rd, ins->imm, 1);
            break;
        case 2: // ADD
            *Rd = ADD(th, *Rd, ins->imm);
            break;
        case 3: // SUB
            *Rd = SUB(th, *Rd, ins->imm, 1);
            break;
    }
    th->pipeline.access = ARM7P_sequential | ARM7P_code;
    th->regs.PC += 2;
}

static u32 ROR(core *th, u32 v, u32 amount)
{
    th->carry = th->regs.CPSR.C;
    if (amount == 0) return v;
    amount &= 31;
    if (amount) v = (v << (32 - amount)) | (v >> amount); // ?correct
    th->carry = !!(v & 1 << 31);
    return v;
}


// Thanks Ares for th trick
static inline u32 thumb_mul_ticks(u32 multiplier, u32 is_signed)
{
    u32 n = 1;
    if(multiplier >>  8 && multiplier >>  8 != 0xffffff) n++;
    if(multiplier >> 16 && multiplier >> 16 !=   0xffff) n++;
    if(multiplier >> 24 && multiplier >> 24 !=     0xff) n++;
    return n;
}

void THUMB_ins_data_proc(core *th, thumb_instruction *ins)
{
#define setnz(x) th->regs.CPSR.N = ((x) >> 31) & 1; \
              th->regs.CPSR.Z = (x) == 0;
    u32 Rs = *getR(th, ins->Rs);
    u32 *Rd = getR(th, ins->Rd);
    th->regs.PC += 2;
    th->pipeline.access = ARM7P_sequential | ARM7P_code;
    th->carry = th->regs.CPSR.C;
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
            th->idle(1);
            th->pipeline.access = ARM7P_nonsequential | ARM7P_code;
            *Rd = LSL(th, *Rd, Rs & 0xFF, 1);
            setnz(*Rd);
            break;
        case 3: {// LSR
            th->idle(1);
            th->pipeline.access = ARM7P_nonsequential | ARM7P_code;
            *Rd = LSR(th, *Rd, Rs & 0xFF, 1);
            setnz(*Rd);
            break; }
        case 4: // ASR
            th->idle(1);
            *Rd = ASR(th, *Rd, Rs & 0xFF, 1);
            th->pipeline.access = ARM7P_nonsequential | ARM7P_code;
            setnz(*Rd);
            break;
        case 5: // ADC
            *Rd = ADD(th, *Rd, Rs + th->carry);
            break;
        case 6: // SBC
            *Rd = SUB(th, (*Rd) - (th->carry ^ 1), Rs, 1);
            break;
        case 7: // ROR
            *Rd = ROR(th, *Rd, Rs & 0xFF);
            th->idle(1);
            th->pipeline.access = ARM7P_nonsequential | ARM7P_code;
            setnz(*Rd);
            break;
        case 8: { // TST (N, Z)
            u32 v = (*Rd) & Rs;
            setnz(v);
            break; }
        case 9: // NEG
            *Rd = SUB(th, 0, Rs, 1);
            break;
        case 10: // CMP Rd, Rs
            SUB(th, *Rd, Rs, 1);
            break;
        case 11: // CMN
            ADD(th, *Rd, Rs);
            break;
        case 12: // ORR
            *Rd |= Rs;
            setnz(*Rd);
            break;
        case 13: // MUL
            th->idle(thumb_mul_ticks(*Rd, 0));
            *Rd *= Rs;
            th->pipeline.access = ARM7P_nonsequential | ARM7P_code;
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
    th->regs.CPSR.C = th->carry;
}

void THUMB_ins_BX(core *th, thumb_instruction *ins)
{
    u32 addr = *getR(th, ins->Rs);
    th->regs.CPSR.T = addr & 1;
    th->pipeline.access = ARM7P_nonsequential | ARM7P_code;
    th->regs.PC = addr & 0xFFFFFFFE;
    th->flush_pipeline();
}

void THUMB_ins_ADD_CMP_MOV_hi(core *th, thumb_instruction *ins)
{
    assert(ins->sub_opcode != 3);
    u32 op1 = *getR(th, ins->Rs);
    u32 op2 = *getR(th, ins->Rd);
    th->regs.PC += 2;
    th->pipeline.access = ARM7P_sequential | ARM7P_code;

    switch(ins->sub_opcode) {
        case 0: // ADD
            *getR(th, ins->Rd) = op1 + op2;
            if (ins->Rd == 15) {
                th->regs.PC &= 0xFFFFFFFE;
                th->flush_pipeline();
            }
            break;
        case 1: // CMP
            SUB(th, op2, op1, 1);
            break;
        case 2: // MOV
            *getR(th, ins->Rd) = op1;
            if (ins->Rd == 15) {
                th->regs.PC &= 0xFFFFFFFE;
                th->flush_pipeline();
            }
            break;
    }
}

void THUMB_ins_LDR_PC_relative(core *th, thumb_instruction *ins)
{
    u32 addr = (th->regs.PC & (~3)) + ins->imm;
    th->regs.PC += 2;
    th->pipeline.access = ARM7P_nonsequential | ARM7P_code;
    u32 *Rd = getR(th, ins->Rd);
    u32 v = th->read(addr, 4, ARM7P_nonsequential, 1);
    *Rd = v;
    th->idle(1);
}

void THUMB_ins_LDRH_STRH_reg_offset(core *th, thumb_instruction *ins)
{
    u32 addr = *getR(th, ins->Rb) + *getR(th, ins->Ro);
    u32 *Rd = getR(th, ins->Rd);
    th->regs.PC += 2;
    th->pipeline.access = ARM7P_nonsequential | ARM7P_code;
    if (ins->L) { // load
        u32 v = th->read(addr, 2, ARM7P_nonsequential, 1);
        if (addr & 1) v = (v >> 8) | (v << 24);
        write_reg(th, Rd, v);
    }
    else { // store
        th->write(addr, 2, ARM7P_nonsequential, (*Rd) & 0xFFFF);
    }
}

void THUMB_ins_LDRSH_LDRSB_reg_offset(core *th, thumb_instruction *ins)
{
    u32 addr = *getR(th, ins->Rb) + *getR(th, ins->Ro);
    th->regs.PC += 2;
    th->pipeline.access = ARM7P_nonsequential | ARM7P_code;
    u32 *Rd = getR(th, ins->Rd);
    u32 sz = ins->B ? 1 : 2;
    u32 mask = ins->B ? 0xFF : 0xFFFF;

    u32 v = th->read(addr, sz, ARM7P_nonsequential, 1);
    th->idle(1);
    if ((ins->B)) {
        v = SIGNe8to32(v);
    }
    else {
        if (addr & 1) { v = (v >> 8); v = SIGNe8to32(v); }
        else v = SIGNe16to32(v);
    }
    *Rd = v;
}

void THUMB_ins_LDR_STR_reg_offset(core *th, thumb_instruction *ins)
{
    u32 addr = *getR(th, ins->Rb) + *getR(th, ins->Ro);
    th->regs.PC += 2;
    th->pipeline.access = ARM7P_nonsequential | ARM7P_code;
    u32 *Rd = getR(th, ins->Rd);
/*
	@ LDR Rd,[Rb,#imm]
	@ LDR Rd,[Rb,Ro]
 */
    if (ins->L) { // Load
        u32 v = th->read(addr, 4, ARM7P_nonsequential, 1);
        th->idle(1);
        if (addr & 3) v = align_val(addr, v);
        *Rd = v;
    }
    else { // Store
        th->write(addr, 4, ARM7P_nonsequential, *Rd);
    }
}

void THUMB_ins_LDRB_STRB_reg_offset(core *th, thumb_instruction *ins)
{
    u32 addr = *getR(th, ins->Rb) + *getR(th, ins->Ro);
    th->regs.PC += 2;
    u32 *Rd = getR(th, ins->Rd);
    th->pipeline.access = ARM7P_nonsequential | ARM7P_code;
    if (ins->L) { // Load
        u32 v = th->read(addr, 1, ARM7P_nonsequential, 1);
        th->idle(1);
        *Rd = v;
    }
    else { // Store
        th->write(addr, 1, ARM7P_nonsequential, (*Rd) & 0xFF);
    }
}

void THUMB_ins_LDR_STR_imm_offset(core *th, thumb_instruction *ins)
{
    u32 addr = *getR(th, ins->Rb) + ins->imm;
    th->regs.PC += 2;
    th->pipeline.access = ARM7P_nonsequential | ARM7P_code;
    u32 *Rd = getR(th, ins->Rd);
    if (ins->L) { // Load
        u32 v = th->read(addr, 4, ARM7P_nonsequential, 1);
        th->idle(1);
        if (addr & 3) v = align_val(addr, v);
        *Rd = v;
    }
    else { // Store
        th->write(addr, 4, ARM7P_nonsequential, *Rd);
    }
}

void THUMB_ins_LDRB_STRB_imm_offset(core *th, thumb_instruction *ins)
{
    u32 addr = *getR(th, ins->Rb) + ins->imm;
    u32 *Rd = getR(th, ins->Rd);
    th->regs.PC += 2;
    th->pipeline.access = ARM7P_nonsequential | ARM7P_code;
    if (ins->L) { // load
        u32 v = th->read(addr, 1, ARM7P_nonsequential, 1);
        //if (addr & 1) v = (v >> 8) | (v << 24);
        th->idle(1);
        write_reg(th, Rd, v);
    }
    else { // store
        th->write(addr, 1, ARM7P_nonsequential, (*Rd) & 0xFF);
    }
}

void THUMB_ins_LDRH_STRH_imm_offset(core *th, thumb_instruction *ins)
{
    u32 addr = *getR(th, ins->Rb) + ins->imm;
    u32 *Rd = getR(th, ins->Rd);
    th->regs.PC += 2;
    th->pipeline.access = ARM7P_nonsequential | ARM7P_code;
    if (ins->L) { // load
        u32 v = th->read(addr, 2, ARM7P_nonsequential, 1);
        th->idle(1);
        if (addr & 1) v = (v >> 8) | (v << 24);
        write_reg(th, Rd, v);
    }
    else { // store
        th->write(addr, 2, ARM7P_nonsequential, (*Rd) & 0xFFFF);
    }
}

void THUMB_ins_LDR_STR_SP_relative(core *th, thumb_instruction *ins)
{
    u32 addr = *getR(th, 13) + ins->imm;
    th->regs.PC += 2;
    th->pipeline.access = ARM7P_nonsequential | ARM7P_code;
    if (ins->L) { // if Load
        u32 v = th->read(addr, 4, ARM7P_nonsequential, 1);
        th->idle(1);
        if (addr & 3) v = align_val(addr, v);
        write_reg(th, getR(th, ins->Rd), v);
    }
    else { // Store
        th->write(addr, 4, ARM7P_nonsequential, *getR(th, ins->Rd));
    }
}

void THUMB_ins_ADD_SP_or_PC(core *th, thumb_instruction *ins)
{
    u32 *Rd = getR(th, ins->Rd);
    if (ins->SP) *Rd = *getR(th, 13) + ins->imm;
    else *Rd = (th->regs.PC & 0xFFFFFFFD) + ins->imm;
    th->regs.PC += 2;
    th->pipeline.access = ARM7P_sequential | ARM7P_code;
}

void THUMB_ins_ADD_SUB_SP(core *th, thumb_instruction *ins)
{
    u32 *sp = getR(th, 13);
    if (ins->S) *sp -= ins->imm;
    else *sp += ins->imm;
    th->regs.PC += 2;
    th->pipeline.access = ARM7P_sequential | ARM7P_code;
}

void THUMB_ins_PUSH_POP(core *th, thumb_instruction *ins)
{
    u32 *r13 = getR(th, 13);
    u32 pop = ins->sub_opcode;
    th->regs.PC += 2;
    th->pipeline.access = ARM7P_nonsequential | ARM7P_code;
    if ((ins->rlist == 0) && (!ins->PC_LR)) {
        if (pop) {
            th->regs.PC = th->read(*r13, 4, ARM7P_nonsequential, 1);
            th->flush_pipeline();
            *r13 += 64;
        }
        else {
            *r13 -= 64;
            th->write(*r13, 4, ARM7P_nonsequential, th->regs.PC);
        }
        return;
    }
    u32 addr = *r13;
    int atype = ARM7P_nonsequential;
    if (pop) {
        for (u32 i = 0; i < 8; i++) {
            if (ins->rlist & (1 << i)) {
                *getR(th, i) = th->read(addr, 4, atype, 1);
                addr += 4;
                atype = ARM7P_sequential;
            }
        }
        if (ins->PC_LR) {
            th->regs.PC = th->read(addr, 4, atype, 1) & 0xFFFFFFFE;
            *r13 = addr + 4;
            th->idle(1);
            th->flush_pipeline();
            return;
        }
        th->idle(1);
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
                th->write(addr, 4, atype, *getR(th, i));
                atype = ARM7P_sequential;
                addr += 4;
            }
        }
        if (ins->PC_LR) {
            th->write(addr, 4, atype, *getR(th, 14));
        }
    }
}

void THUMB_ins_LDM_STM(core *th, thumb_instruction *ins)
{
    u32 *r13 = getR(th, ins->Rb);
    u32 load = ins->sub_opcode;
    th->regs.PC += 2;
    // 	stmia 	r3!,{r1,r2}
    th->pipeline.access = ARM7P_nonsequential | ARM7P_code;
    if (ins->rlist == 0)  {
        if (load) {
            th->regs.PC = th->read(*r13, 4, ARM7P_nonsequential, 1);
            th->flush_pipeline();
        }
        else {
            th->write(*r13, 4, ARM7P_nonsequential, th->regs.PC);
        }
        *r13 += 0x40;
        return;
    }
    if (load) {
        u32 addr = *r13;
        u32 atype = ARM7P_nonsequential;
        for (u32 i = 0; i < 8; i++) {
            if (ins->rlist & (1 << i)) {
                write_reg(th, getR(th, i), th->read(addr, 4, atype, 1));
                atype = ARM7P_sequential;
                addr += 4;
            }
        }
        th->idle(1);
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
                th->write(addr, 4, atype, *getR(th, i));
                if (i == first) *r13 = addr_new;
                addr += 4;
                atype = ARM7P_sequential;
            }
        }
    }
}

void THUMB_ins_SWI(core *th, thumb_instruction *ins)
{
    /*
Execution SWI/BKPT:
  R14_svc=PC+2     R14_abt=PC+4   ;save return address
  SPSR_svc=CPSR    SPSR_abt=CPSR  ;save CPSR flags
  CPSR=<changed>   CPSR=<changed> ;Enter svc/abt, ARM state, IRQs disabled
  PC=VVVV0008h     PC=VVVV000Ch   ;jump to SWI/PrefetchAbort vector address
     */
    th->regs.R_svc[1] = th->regs.PC - 2;
    th->regs.SPSR_svc = th->regs.CPSR.u;
    th->regs.CPSR.mode = ARM7_supervisor;
    th->regs.CPSR.T = 0; // exit THUMB
    th->regs.CPSR.I = 1; // mask IRQ
    th->pipeline.access = ARM7P_nonsequential | ARM7P_code;
    th->fill_regmap();
    th->regs.PC = 0x00000008;
    th->flush_pipeline();
}

void THUMB_ins_UNDEFINED_BCC(core *th, thumb_instruction *ins)
{
    THUMB_ins_BCC(th, ins);
}

void THUMB_ins_BCC(core *th, thumb_instruction *ins)
{
    th->regs.PC += 2;
    if (condition_passes(&th->regs, ins->sub_opcode)) {
        th->pipeline.access = ARM7P_nonsequential | ARM7P_code;
        th->regs.PC += ins->imm - 2;
        th->flush_pipeline();
    }
    else {
        th->pipeline.access = ARM7P_sequential | ARM7P_code;
    }
}

void THUMB_ins_B(core *th, thumb_instruction *ins)
{
    th->regs.PC += ins->imm;
    th->flush_pipeline();
}

void THUMB_ins_BL_BLX_prefix(core *th, thumb_instruction *ins)
{
    u32 *lr = getR(th, 14);
    th->regs.PC += 2;
    *lr = th->regs.PC + ins->imm - 2;
    th->pipeline.access = ARM7P_sequential | ARM7P_code;
}

void THUMB_ins_BL_suffix(core *th, thumb_instruction *ins)
{
    u32 v = (th->regs.PC - 2) | 1;
    u32 *LR = getR(th, 14);
    th->regs.PC = ((*LR) + ins->imm) & 0xFFFFFFFE;
    *LR = v;
    th->flush_pipeline();
}

}