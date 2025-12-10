//
// Created by . on 12/18/24.
//
#include <cassert>
#include <cstdio>
#include <cstdlib>

#include "arm946es.h"
#include "thumb2_instructions.h"

#define UNIMPLEMENTED printf("\nUNIMPLEMENTED INSTRUCTION! %04x", ins.opcode); assert(1==2)

#define PC R[15]
#define rSP R[13]

namespace ARM946ES {

u32 core::tLSL(u32 v, u32 amount, bool set_flags) {
    temp_carry = regs.CPSR.C;
    if (amount == 0) return v;
    temp_carry = amount > 32 ? 0 : !!(v & 1 << (32 - amount));
    v = (amount > 31) ? 0 : (v << amount);
    return v;
}

// Logical shift right
u32 core::tLSR(u32 v, u32 amount, bool set_flags)
{
    if (set_flags) temp_carry = regs.CPSR.C;
    if (amount == 0) return v;
    if (set_flags) temp_carry = (amount > 32) ? 0 : !!(v & 1 << (amount - 1));
    v = (amount > 31) ? 0 : (v >> amount);
    return v;
}

// Arithemtic (sign-extend) shift right
u32 core::tASR(u32 v, u32 amount, bool set_flags)
{
    //   carry = cpsr().c;
    temp_carry = regs.CPSR.C;

    //   if(shift == 0) return source;
    if (amount == 0) return v;

    //   carry = shift > 32 ? source & 1 << 31 : source & 1 << shift - 1;
    temp_carry = (amount >= 32) ? (!!(v & 0x80000000)) : !!(v & (1 << (amount - 1)));

    //   source = shift > 31 ? (i32)source >> 31 : (i32)source >> shift;
    v = (amount > 31) ? static_cast<i32>(v) >> 31 : static_cast<i32>(v) >> amount;

//    return source;
    return v;
}

static u32 align_val(const u32 addr, u32 tmp)
{
    if (addr & 3) {
        const u32 misalignment = addr & 3;
        if (misalignment == 1) tmp = ((tmp >> 8) & 0xFFFFFF) | (tmp << 24);
        else if (misalignment == 2) tmp = (tmp >> 16) | (tmp << 16);
        else tmp = ((tmp << 8) & 0xFFFFFF00) | (tmp >> 24);
    }
    return tmp;

}


void core::THUMB_ins_INVALID(const thumb2_instruction &ins)
{
    UNIMPLEMENTED;
}

u32 core::tADD(const u32 op1, const u32 op2)
{
    const u32 result = op1 + op2;
    regs.CPSR.N = (result >> 31) & 1;
    regs.CPSR.Z = result == 0;
    regs.CPSR.C = result < op1;
    temp_carry = regs.CPSR.C;
    regs.CPSR.V = (~(op1 ^ op2) & (op2 ^ result)) >> 31;
    return result;
}

u32 core::tSUB(const u32 op1, const u32 op2, bool set_flags)
{
    const u32 result = op1 - op2;
    regs.CPSR.N = (result >> 31) & 1;
    regs.CPSR.Z = result == 0;
    temp_carry = op1 >= op2;
    regs.CPSR.C = temp_carry;
    regs.CPSR.V = ((op1 ^ op2) & (op1 ^ result)) >> 31;
    return result;
}

void core::THUMB_ins_ADD_SUB(const thumb2_instruction &ins)
{
    u64 val;
    if (ins.I) {
        val = ins.imm;
    }
    else {
        val = *getR(ins.Rn);
    }
    regs.PC += 2;
    pipeline.access = ARM9P_sequential | ARM9P_code;
    u32 *Rd = getR(ins.Rd);
    u32 op1 = *getR(ins.Rs);
    if (ins.sub_opcode) *Rd = tSUB(op1, val, true);
    else *Rd = tADD(op1, val);
}

void core::THUMB_ins_LSL_LSR_ASR(const thumb2_instruction &ins)
{
    //UPDATE!
    u32 *Rd = getR(ins.Rd);
    const u32 Rs = *getR(ins.Rs);
    regs.PC += 2;
    pipeline.access = ARM9P_sequential | ARM9P_code;
    temp_carry = regs.CPSR.C;
    switch(ins.sub_opcode) {
        case 0: // LSL
            *Rd = tLSL(Rs, ins.imm, true);
            break;
        case 1: // LSR
            *Rd = tLSR(Rs, ins.imm ? ins.imm : 32, true);
            break;
        case 2: // ASR
            *Rd = tASR(Rs, ins.imm ? ins.imm : 32, true);
            break;
    }
    regs.CPSR.C = temp_carry;
    regs.CPSR.Z = *Rd == 0;
    regs.CPSR.N = ((*Rd) >> 31) & 1;
}

void core::THUMB_ins_MOV_CMP_ADD_SUB(const thumb2_instruction &ins)
{
    u32 *Rd = getR(ins.Rd);
    switch(ins.sub_opcode) {
        case 0: // MOV
            *Rd = ins.imm;
            regs.CPSR.N = (*Rd >> 31) & 1;
            regs.CPSR.Z = (*Rd) == 0;
            break;
        case 1: // CMP
            tSUB(*Rd, ins.imm, true);
            break;
        case 2: // ADD
            *Rd = tADD(*Rd, ins.imm);
            break;
        case 3: // SUB
            *Rd = tSUB(*Rd, ins.imm, true);
            break;
    }
    pipeline.access = ARM9P_sequential | ARM9P_code;
    regs.PC += 2;
}

u32 core::tROR(u32 v, u32 amount)
{
    temp_carry = regs.CPSR.C;
    if (amount == 0) return v;
    amount &= 31;
    if (amount) v = (v << (32 - amount)) | (v >> amount); // ?correct
    temp_carry = !!(v & 1 << 31);
    return v;
}


// Thanks Ares for this trick
static u32 thumb_mul_ticks(u32 multiplier, u32 is_signed)
{
    u32 n = 1;
    if(multiplier >>  8 && multiplier >>  8 != 0xffffff) n++;
    if(multiplier >> 16 && multiplier >> 16 !=   0xffff) n++;
    if(multiplier >> 24 && multiplier >> 24 !=     0xff) n++;
    return n;
}

void core::THUMB_ins_data_proc(const thumb2_instruction &ins)
{
#define setnz(x) regs.CPSR.N = ((x) >> 31) & 1; \
              regs.CPSR.Z = (x) == 0;
    u32 Rs = *getR(ins.Rs);
    u32 *Rd = getR(ins.Rd);
    regs.PC += 2;
    pipeline.access = ARM9P_sequential | ARM9P_code;
    temp_carry = regs.CPSR.C;
    switch(ins.sub_opcode) {
        case 0: // AND (N,Z)
            *Rd &= Rs;
            setnz(*Rd);
            break;
        case 1: // XOR (N,Z)
            *Rd ^= Rs;
            setnz(*Rd);
            break;
        case 2: // LSL (N,Z,C)
            idle(1);
            pipeline.access = ARM9P_nonsequential | ARM9P_code;
            *Rd = tLSL(*Rd, Rs & 0xFF, true);
            setnz(*Rd);
            break;
        case 3: {// LSR
            idle(1);
            pipeline.access = ARM9P_nonsequential | ARM9P_code;
            *Rd = tLSR(*Rd, Rs & 0xFF, true);
            setnz(*Rd);
            break; }
        case 4: // ASR
            idle(1);
            *Rd = tASR(*Rd, Rs & 0xFF, true);
            pipeline.access = ARM9P_nonsequential | ARM9P_code;
            setnz(*Rd);
            break;
        case 5: // ADC
            *Rd = tADD(*Rd, Rs + temp_carry);
            break;
        case 6: // SBC
            *Rd = tSUB((*Rd) - (temp_carry ^ 1), Rs, true);
            break;
        case 7: // tROR
            *Rd = tROR(*Rd, Rs & 0xFF);
            idle(1);
            pipeline.access = ARM9P_nonsequential | ARM9P_code;
            setnz(*Rd);
            break;
        case 8: { // TST (N, Z)
            const u32 v = (*Rd) & Rs;
            setnz(v);
            break; }
        case 9: // NEG
            *Rd = tSUB(0, Rs, true);
            break;
        case 10: // CMP Rd, Rs
            tSUB(*Rd, Rs, true);
            break;
        case 11: // CMN
            tADD(*Rd, Rs);
            break;
        case 12: // ORR
            *Rd |= Rs;
            setnz(*Rd);
            break;
        case 13: // MUL
            idle(thumb_mul_ticks(*Rd, 0));
            *Rd *= Rs;
            pipeline.access = ARM9P_nonsequential | ARM9P_code;
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
    regs.CPSR.C = temp_carry;
}

void core::THUMB_ins_BX_BLX(const thumb2_instruction &ins)
{
    // Update the BLX!
    // for BX, MSBd must be zero
    // for BLX, MSBd must be one
    // MSBd = bit 7
    const u32 addr = *getR(ins.Rs);
    regs.CPSR.T = addr & 1;
    pipeline.access = ARM9P_nonsequential | ARM9P_code;
    const u32 lnk = regs.PC - 1;
    regs.PC = addr & 0xFFFFFFFE;
    if (ins.sub_opcode) { // BLX
        *getR(14) = lnk;
    }
    flush_pipeline();
}

void core::THUMB_ins_ADD_CMP_MOV_hi(const thumb2_instruction &ins)
{
    assert(ins.sub_opcode != 3);
    const u32 op1 = *getR(ins.Rs);
    const u32 op2 = *getR(ins.Rd);
    regs.PC += 2;
    pipeline.access = ARM9P_sequential | ARM9P_code;

    switch(ins.sub_opcode) {
        case 0: // ADD
            *getR(ins.Rd) = op1 + op2;
            if (ins.Rd == 15) {
                regs.PC &= 0xFFFFFFFE;
                flush_pipeline();
            }
            break;
        case 1: // CMP
            tSUB(op2, op1, true);
            break;
        case 2: // MOV
            *getR(ins.Rd) = op1;
            if (ins.Rd == 15) {
                regs.PC &= 0xFFFFFFFE;
                flush_pipeline();
            }
            break;
    }
}

void core::THUMB_ins_LDR_PC_relative(const thumb2_instruction &ins)
{
    const u32 addr = (regs.PC & (~3)) + ins.imm;
    regs.PC += 2;
    pipeline.access = ARM9P_nonsequential | ARM9P_code;
    u32 *Rd = getR(ins.Rd);
    *Rd = read(addr, 4, ARM9P_nonsequential, true);
}

void core::THUMB_ins_LDRH_STRH_reg_offset(const thumb2_instruction &ins)
{
    u32 addr = *getR(ins.Rb) + *getR(ins.Ro);
    u32 *Rd = getR(ins.Rd);
    regs.PC += 2;
    pipeline.access = ARM9P_nonsequential | ARM9P_code;
    if (ins.L) { // load
        u32 v = read(addr, 2, ARM9P_nonsequential, true);
        if (addr & 1) v = (v >> 8) | (v << 24);
        *Rd = v;
    }
    else { // store
        write(addr, 2, ARM9P_nonsequential, (*Rd) & 0xFFFF);
    }
}

void core::THUMB_ins_LDRSH_LDRSB_reg_offset(const thumb2_instruction &ins)
{
    const u32 addr = *getR(ins.Rb) + *getR(ins.Ro);
    regs.PC += 2;
    pipeline.access = ARM9P_nonsequential | ARM9P_code;
    u32 *Rd = getR(ins.Rd);
    const u8 sz = ins.B ? 1 : 2;
    u32 mask = ins.B ? 0xFF : 0xFFFF;

    u32 v = read(addr, sz, ARM9P_nonsequential, true);
    if ((ins.B)) {
        v = SIGNe8to32(v);
    }
    else {
        v = SIGNe16to32(v);
    }
    *Rd = v;
}

void core::THUMB_ins_LDR_STR_reg_offset(const thumb2_instruction &ins)
{
    const u32 addr = *getR(ins.Rb) + *getR(ins.Ro);
    regs.PC += 2;
    pipeline.access = ARM9P_nonsequential | ARM9P_code;
    u32 *Rd = getR(ins.Rd);
    if (ins.L) { // Load
        u32 v = read(addr, 4, ARM9P_nonsequential, true);
        if (addr & 3) v = align_val(addr, v);
        *Rd = v;
    }
    else { // Store
        write(addr, 4, ARM9P_nonsequential, *Rd);
    }
}

void core::THUMB_ins_LDRB_STRB_reg_offset(const thumb2_instruction &ins)
{
    const u32 addr = *getR(ins.Rb) + *getR(ins.Ro);
    regs.PC += 2;
    u32 *Rd = getR(ins.Rd);
    pipeline.access = ARM9P_nonsequential | ARM9P_code;
    if (ins.L) { // Load
        *Rd = read(addr, 1, ARM9P_nonsequential, true);
    }
    else { // Store
        write(addr, 1, ARM9P_nonsequential, (*Rd) & 0xFF);
    }
}

void core::THUMB_ins_LDR_STR_imm_offset(const thumb2_instruction &ins)
{
    const u32 addr = *getR(ins.Rb) + ins.imm;
    regs.PC += 2;
    pipeline.access = ARM9P_nonsequential | ARM9P_code;
    u32 *Rd = getR(ins.Rd);
    if (ins.L) { // Load
        u32 v = read(addr, 4, ARM9P_nonsequential, true);
        if (addr & 3) v = align_val(addr, v);
        *Rd = v;
    }
    else { // Store
        write(addr, 4, ARM9P_nonsequential, *Rd);
    }
}

void core::THUMB_ins_LDRB_STRB_imm_offset(const thumb2_instruction &ins)
{
    const u32 addr = *getR(ins.Rb) + ins.imm;
    u32 *Rd = getR(ins.Rd);
    regs.PC += 2;
    pipeline.access = ARM9P_nonsequential | ARM9P_code;
    if (ins.L) { // load
        const u32 v = read(addr, 1, ARM9P_nonsequential, true);
        //if (addr & 1) v = (v >> 8) | (v << 24);
        twrite_reg(Rd, v);
    }
    else { // store
        write(addr, 1, ARM9P_nonsequential, (*Rd) & 0xFF);
    }
}

void core::THUMB_ins_LDRH_STRH_imm_offset(const thumb2_instruction &ins)
{
    const u32 addr = *getR(ins.Rb) + ins.imm;
    u32 *Rd = getR(ins.Rd);
    regs.PC += 2;
    pipeline.access = ARM9P_nonsequential | ARM9P_code;
    if (ins.L) { // load
        u32 v = read(addr, 2, ARM9P_nonsequential, true);
        if (addr & 1) v = (v >> 8) | (v << 24);
        twrite_reg(Rd, v);
    }
    else { // store
        write(addr, 2, ARM9P_nonsequential, (*Rd) & 0xFFFF);
    }
}

void core::THUMB_ins_LDR_STR_SP_relative(const thumb2_instruction &ins)
{
    const u32 addr = *getR(13) + ins.imm;
    regs.PC += 2;
    pipeline.access = ARM9P_nonsequential | ARM9P_code;
    if (ins.L) { // if Load
        u32 v = read(addr, 4, ARM9P_nonsequential, true);
        if (addr & 3) v = align_val(addr, v);
        twrite_reg(getR(ins.Rd), v);
    }
    else { // Store
        write(addr, 4, ARM9P_nonsequential, *getR(ins.Rd));
    }
}

void core::THUMB_ins_ADD_SP_or_PC(const thumb2_instruction &ins)
{
    u32 *Rd = getR(ins.Rd);
    if (ins.SP) *Rd = *getR(13) + ins.imm;
    else *Rd = (regs.PC & 0xFFFFFFFD) + ins.imm;
    regs.PC += 2;
    pipeline.access = ARM9P_sequential | ARM9P_code;
}

void core::THUMB_ins_ADD_SUB_SP(const thumb2_instruction &ins)
{
    u32 *sp = getR(13);
    if (ins.S) *sp -= ins.imm;
    else *sp += ins.imm;
    regs.PC += 2;
    pipeline.access = ARM9P_sequential | ARM9P_code;
}

void core::THUMB_ins_PUSH_POP(const thumb2_instruction &ins)
{
    u32 *r13 = getR(13);
    const u32 pop = ins.sub_opcode;

    const u32 rlist = ins.rlist;
    u32 address = *getR(13);

    u32 access = ARM9P_nonsequential;
    if (pop) {
        for (u32 r=0; r<8; r++) {
            if ((rlist  >> r) & 1) {
                *getR(r) = read(address, 4, access, true);
                address += 4;
                access = ARM9P_sequential;
            }
        }

        if(ins.PC_LR) {
            regs.PC = read(address, 4, access, true);
            *getR(13) = address + 4;
            regs.CPSR.T = regs.PC & 1;
            flush_pipeline();
            return;
        }

        *getR(13) = address;
    } else {
        for (u32 r = 0; r < 8; r++) {
            if ((rlist >> r) & 1)
                address -= 4;
        }

        if(ins.PC_LR) {
            address -= 4;
        }

        *getR(13) = address;

        for (u32 r = 0; r < 8; r++) {
            if ((rlist >> r) & 1) {
                write(address, 4, access, *getR(r));
                address += 4;
                access = ARM9P_sequential;
            }
        }

        if(ins.PC_LR) {
            write(address, 4, access, *getR(14));
        }
    }

    regs.PC += 2;
    pipeline.access = ARM9P_code | ARM9P_nonsequential;
}

void core::THUMB_ins_LDM_STM(const thumb2_instruction &ins) {
    const u32 rlist = ins.rlist;
    u32 address = *getR(ins.Rb);
    const u32 L = ins.sub_opcode;
    u32 access = ARM9P_nonsequential;
    if (L) {
        for (u32 i = 0; i <= 7; i++) {
            if ((rlist >> i) & 1) {
                *getR(i) = read(address, 4, access, true);
                address += 4;
            }
            access = ARM9P_sequential;
        }

        if (~rlist & (1 << ins.Rb)) {
            *getR(ins.Rb) = address;
        }
    } else {
        for (u32 reg = 0; reg <= 7; reg++) {
            if ((rlist >> reg) & 1) {
                write(address, 4, access, *getR(reg));
                address += 4;
            }
            access = ARM9P_sequential;
        }

        *getR(ins.Rb) = address;
    }

    regs.PC += 2;
}

void core::THUMB_ins_SWI(const thumb2_instruction &ins)
{
    /*
Execution SWI/BKPT:
  R14_svc=PC+2     R14_abt=PC+4   ;save return address
  SPSR_svc=CPSR    SPSR_abt=CPSR  ;save CPSR flags
  CPSR=<changed>   CPSR=<changed> ;Enter svc/abt, ARM state, IRQs disabled
  PC=VVVV0008h     PC=VVVV000Ch   ;jump to SWI/PrefetchAbort vector address
     */
    regs.R_svc[1] = regs.PC - 2;
    regs.SPSR_svc = regs.CPSR.u;
    regs.CPSR.mode = M_supervisor;
    regs.CPSR.T = 0; // exit THUMB
    regs.CPSR.I = 1; // mask IRQ
    pipeline.access = ARM9P_nonsequential | ARM9P_code;
    fill_regmap();
    regs.PC = regs.EBR | 0x00000008;
    flush_pipeline();
}

void core::THUMB_ins_BKPT(const thumb2_instruction &ins)
{
    regs.R_abt[1] = regs.PC - 2;
    regs.SPSR_abt = regs.CPSR.u;
    regs.CPSR.mode = M_abort;
    regs.CPSR.T = 0; // exit THUMB
    regs.CPSR.I = 1; // mask IRQ
    pipeline.access = ARM9P_nonsequential | ARM9P_code;
    fill_regmap();
    regs.PC = regs.EBR | 0x0000000C;
    flush_pipeline();
}

void core::THUMB_ins_UNDEFINED_BCC(const thumb2_instruction &ins)
{
    THUMB_ins_BCC(ins);
}

void core::THUMB_ins_BCC(const thumb2_instruction &ins)
{
    regs.PC += 2;
    if (condition_passes(static_cast<condition_codes>(ins.sub_opcode))) {
        pipeline.access = ARM9P_nonsequential | ARM9P_code;
        regs.PC += ins.imm - 2;
        flush_pipeline();
    }
    else {
        pipeline.access = ARM9P_sequential | ARM9P_code;
    }
}

void core::THUMB_ins_B(const thumb2_instruction &ins)
{
    regs.PC += ins.imm;
    flush_pipeline();
}

void core::THUMB_ins_BL_BLX_prefix(const thumb2_instruction &ins)
{
    u32 *lr = getR(14);
    regs.PC += 2;
    *lr = regs.PC + ins.imm - 2;
    pipeline.access = ARM9P_sequential | ARM9P_code;
}

void core::THUMB_ins_BL_suffix(const thumb2_instruction &ins)
{
    const u32 v = (regs.PC - 2) | 1;
    u32 *LR = getR(14);
    regs.PC = ((*LR) + ins.imm) & 0xFFFFFFFE;
    *LR = v;
    flush_pipeline();
}

void core::THUMB_ins_BLX_suffix(const thumb2_instruction &ins)
{

    const u32 v = (regs.PC - 2) | 1;
    u32 *LR = getR(14);
    const u32 addr = (*LR) + ins.imm;
    *LR = v;
    regs.PC = addr & 0xFFFFFFFC;
    regs.CPSR.T = 0;
    flush_pipeline();
}
}