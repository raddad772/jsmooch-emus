//
// Created by . on 12/4/24.
//

#include <stdio.h>
#include <stdlib.h>

#include "arm7tdmi.h"
#include "arm7tdmi_instructions.h"

#define UNIMPLEMENTED printf("\nUNIMPLEMENTED INSTRUCTION! %08x", opcode); assert(1==2)
#define AREAD(addr, sz) this->read(this->read_ptr, (addr), (sz), this->pipeline.access, 1)
#define AWRITE(addr, sz, val) this->write(this->write_ptr, (addr), (sz), this->pipeline.access, (val) & mask)

static inline u32 *old_getR(struct ARM7TDMI *this, u32 num) {
    switch(num) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
            return &this->regs.R[num];
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
            return this->regs.CPSR.mode == ARM7_fiq ? &this->regs.R_fiq[num - 8] : &this->regs.R[num];
        case 13:
        case 14: {
            switch(this->regs.CPSR.mode) {
                case ARM7_abort:
                    return &this->regs.R_abt[num - 13];
                case ARM7_fiq:
                    return &this->regs.R_fiq[num - 8];
                case ARM7_irq:
                    return &this->regs.R_irq[num - 13];
                case ARM7_supervisor:
                    return &this->regs.R_svc[num - 13];
                case ARM7_undefined:
                    return &this->regs.R_und[num - 13];
                case ARM7_user:
                    return &this->regs.R[num];
                default:
                    assert(1==2);
                    return NULL;
            }
            break; }
        case 15:
            return &this->regs.R[15];
        default:
            assert(1==2);
            return NULL;
    }
}

static inline u32 *getR(struct ARM7TDMI *this, u32 num) {
    return this->regmap[num];
}

void ARM7TDMI_fill_regmap(struct ARM7TDMI *this) {
    for (u32 i = 8; i < 15; i++) {
        this->regmap[i] = old_getR(this, i);
    }
}

static inline void flush_pipeline(struct ARM7TDMI *this)
{
    //assert(1==2);
    printf("\nFLUSH THE PIPE!@");
}

static inline void write_reg(struct ARM7TDMI *this, u32 *r, u32 v) {
    *r = v;
    if (r == &this->regs.PC) {
        flush_pipeline(this);
    }
}

void ARM7TDMI_ins_MUL_MLA(struct ARM7TDMI *this, u32 opcode)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_ins_MULL_MLAL(struct ARM7TDMI *this, u32 opcode)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_ins_SWP(struct ARM7TDMI *this, u32 opcode)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_ins_LDRH_STRH(struct ARM7TDMI *this, u32 opcode)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_ins_LDRSB_LDRSH(struct ARM7TDMI *this, u32 opcode)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_ins_MRS(struct ARM7TDMI *this, u32 opcode)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_ins_MSR_reg(struct ARM7TDMI *this, u32 opcode)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_ins_MSR_imm(struct ARM7TDMI *this, u32 opcode)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_ins_BX(struct ARM7TDMI *this, u32 opcode)
{
    UNIMPLEMENTED;
}


static u32 TEST(struct ARM7TDMI *this, u32 v, u32 S)
{
    if (this->regs.CPSR.T || S) {
        this->regs.CPSR.N = (v >> 31) & 1;
        this->regs.CPSR.Z = v == 0;
        this->regs.CPSR.C = this->carry;
    }
    return v;
}

static u32 ADD(struct ARM7TDMI *this, u32 Rnd, u32 Rmd, u32 carry, u32 S)
{
    u32 result = Rnd + Rmd + carry;
    if (this->regs.CPSR.T || S) {
        u32 overflow = ~(Rnd ^ Rmd) & (Rnd ^ Rmd);
        this->regs.CPSR.V = (overflow >> 31) & 1;
        this->regs.CPSR.C = ((overflow ^ Rnd ^ Rmd ^ result) >> 31) & 1;
        this->regs.CPSR.Z = result == 0;
        this->regs.CPSR.N = (result >> 31) & 1;
    }
    return result;
}


// case 2: v = SUB(this, Rn, Rm, 1); break;
static u32 SUB(struct ARM7TDMI *this, u32 Rn, u32 Rm, u32 carry, u32 S)
{
    return ADD(this, Rn, Rm ^ 0xFFFFFFFF, carry, S);
}

static u32 ALU(struct ARM7TDMI *this, u32 Rn, u32 Rm, u32 alu_opcode, u32 S, u32 *out) {
    switch(alu_opcode) {
        case 0: write_reg(this, out, TEST(this, Rn & Rm, S)); break;
        case 1: write_reg(this, out, TEST(this, Rn ^ Rm, S)); break;
        case 2: write_reg(this, out, SUB(this, Rn, Rm, 1, S)); break;
        case 3: write_reg(this, out, SUB(this, Rm, Rn, 1, S)); break;
        case 4: write_reg(this, out, ADD(this, Rn, Rm, 0, S)); break;
        case 5: write_reg(this, out, ADD(this, Rn, Rm, this->regs.CPSR.C, S)); break; // ADDC
        case 6: write_reg(this, out, SUB(this, Rn, Rm, this->regs.CPSR.C, S)); break;
        case 7: write_reg(this, out, SUB(this, Rm, Rn, this->regs.CPSR.C, S)); break;
        case 8: TEST(this, Rn & Rm, S); break;
        case 9: TEST(this, Rn ^ Rm, S); break;
        case 10: SUB(this, Rn, Rm, 1, S); break;
        case 11: ADD(this, Rn, Rm, 0, S); break;
        case 12: write_reg(this, out, TEST(this, Rn | Rm, S)); break;
        case 13: write_reg(this, out, TEST(this, Rm, S)); break;
        case 14: write_reg(this, out, TEST(this, Rn & ~Rm, S)); break;
        case 15: write_reg(this, out, TEST(this, Rm ^ 0xFFFFFFFF, S)); break;
    }
    return *out;
}

// Logical shift left

static u32 LSL(struct ARM7TDMI *this, u32 v, u32 amount) {
    this->carry = this->regs.CPSR.C;
    if (amount == 0) return v;
    this->carry = amount > 32 ? 0 : (v & 1 << (32 - amount));
    v = (amount > 31) ? 0 : (v << amount);
    return v;
}

// Logical shift right
static u32 LSR(struct ARM7TDMI *this, u32 v, u32 amount)
{
    this->carry = this->regs.CPSR.C;
    if (amount == 0) return v;
    this->carry = (amount > 32) ? 0 : (v & 1 << (amount - 1));
    v = (amount > 31) ? 0 : (v >> amount);
    return v;
}

// Arithemtic (sign-extend) shift right
static u32 ASR(struct ARM7TDMI *this, u32 v, u32 amount)
{
    this->carry = this->regs.CPSR.C;
    if (amount == 0) return v;
    this->carry = (amount > 32) ? ((v & 1) << 31) : (v & 1 << (amount - 1));
    v = (amount > 31) ? (i32)v >> 31 : (i32)v >> amount;
    return v;
}

static u32 ROR(struct ARM7TDMI *this, u32 v, u32 amount)
{
    this->carry = this->regs.CPSR.C;
    if (amount == 0) return v;
    amount &= 31;
    if (amount) v = v << (32 - amount) | v >> amount; // ?correct
    this->carry = v & 1 << 31;
    return v;
}

// Rotate right thru carry
static u32 RRX(struct ARM7TDMI *this, u32 v)
{
    this->carry = v & 1;
    return (v >> 1) | (this->regs.CPSR.C << 31);
}

void ARM7TDMI_ins_data_proc_immediate_shift(struct ARM7TDMI *this, u32 opcode)
{
    u32 alu_opcode = (opcode >> 21) & 15;
    u32 S = (opcode >> 20) & 1; // set condition codes. 0=no, 1=yes. must be 1 for 8-B
    u32 Rnd = (opcode >> 16) & 15; // first operand
    u32 Rdd = (opcode >> 12) & 15; // dest reg.
    u32 Is = (opcode >> 7) & 31; // shift amount
    u32 shift_type = (opcode >> 5) & 3; // 0=LSL, 1=LSR, 2=ASR, 3=ROR
    // R(bit4) = 0 for this
    u32 Rmd = opcode & 15;

    u32 Rn = *getR(this, Rnd);
    u32 Rm = *getR(this, Rmd);
    u32 *Rd = getR(this, Rdd);
    this->carry = this->regs.CPSR.C;
    switch(shift_type) {
        case 0: //
            Rm = LSL(this, Rm, Is);
            break;
        case 1:
            Rm = LSR(this, Rm, Is ? Is : 32);
            break;
        case 2:
            Rm = ASR(this, Rm, Is ? Is : 32);
            break;
        case 3:
            Rm = Is ? ROR(this, Rm, Is) : RRX(this, Rm);
            break;
    }

    ALU(this, Rn, Rm, alu_opcode, S, Rd);
}

void ARM7TDMI_ins_data_proc_register_shift(struct ARM7TDMI *this, u32 opcode)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_ins_undefined_instruction(struct ARM7TDMI *this, u32 opcode)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_ins_data_processing_immediate(struct ARM7TDMI *this, u32 opcode)
{
    UNIMPLEMENTED;
}

#define OBIT(x) ((opcode >> (x)) & 1)
void ARM7TDMI_ins_LDR_STR_immediate_offset(struct ARM7TDMI *this, u32 opcode)
{
    printf("\nYAY TIS INSTRUTION!");
    u32 P = OBIT(24); // Pre/post. 0 = after-transfer, post
    u32 U = OBIT(23); // 0 = down, 1 = up
    u32 B = OBIT(22); // byte/word, 0 = 32bit, 1 = 8bit
    // when P=0, bit 21 is T, 0= normal, 1= force nonpriviledged, and W=1
    // when P=1, bit 21 is write-back bit, 0 = normal, 1 = write into base
    u32 T = 0; // 0 = normal, 1 = force unprivileged
    u32 W = 0; // writeback. 0 = no, 1 = write into base
    if (P == 0) { T = OBIT(21); W = 1; }
    else W = OBIT(21);

    u32 L = OBIT(20); // store = 0, load = 1
    u32 Rnd = (opcode >> 16) & 15; // base register. PC=+8
    u32 Rdd = (opcode >> 12) & 15; // source/dest register. PC=+12

    u32 *Rn = getR(this, Rnd);
    u32 *Rd = getR(this, Rdd);

    u32 offset = (opcode & 4095);
    u32 addr = *Rn;
    if (Rnd == 15) addr += 4;
    if (P) addr = U ? (addr + offset) : (addr - offset);
    //   [Rn], <#{+/-}expression>
    // when read byte, upper 24 bits 0-extend
    // T unchanged on ARM4. on 5, it can change it
    // When reading a word from a halfword-aligned address (which is located in the middle between two word-aligned addresses), the lower 16bit of Rd will contain [address] ie. the addressed halfword, and the upper 16bit of Rd will contain [Rd-2] ie. more or less unwanted garbage. However, by isolating lower bits this may be used to read a halfword from memory. (Above applies to little endian mode, as used in GBA.)
    // CPSR not affected
    // Execution Time: For normal LDR: 1S+1N+1I. For LDR PC: 2S+2N+1I. For STR: 2N.
    u32 sz = B ? 1 : 4;
    u32 mask = B ? 0xFF : 0xFFFFFFFF;
    if (L == 0) {// store to RAM
        u32 v = AREAD(addr, sz);
        if ((addr & 2) && (B == 0)) {
            v = (v & 0xFFFF) | ((*Rd - 2) << 16);
        }
        write_reg(this, Rd, v);
    }
    else { // load from RAM
        AWRITE(addr, sz, *Rd);
    }
    if (!P) addr = U ? (addr + offset) : (addr - offset);
    if (W) write_reg(this, Rn, addr);
}

void ARM7TDMI_ins_LDR_STR_register_offset(struct ARM7TDMI *this, u32 opcode)
{
    u32 P = OBIT(24);
    u32 U = OBIT(23);
    u32 B = OBIT(22);
    u32 T = 0; // 0 = normal, 1 = force unprivileged
    u32 W = 0; // writeback. 0 = no, 1 = write into base
    if (P == 0) { T = OBIT(21); W = 1; }
    else W = OBIT(21);
    u32 L = OBIT(20); // 0 store, 1 load
    u32 Rnd = (opcode >> 16) & 15; // base reg
    u32 Rdd = (opcode >> 12) & 15; // source/dest reg
    u32 Is = (opcode >> 7) & 31;
    u32 shift_type = (opcode >> 5) & 3;
    u32 Rmd = opcode & 15;

    u32 *Rn = getR(this, Rnd);
    u32 Rm = *getR(this, Rmd);
    u32 *Rd = getR(this, Rdd);
    this->carry = this->regs.CPSR.C;
    switch(shift_type) {
        case 0: //
            Rm = LSL(this, Rm, Is);
            break;
        case 1:
            Rm = LSR(this, Rm, Is ? Is : 32);
            break;
        case 2:
            Rm = ASR(this, Rm, Is ? Is : 32);
            break;
        case 3:
            Rm = Is ? ROR(this, Rm, Is) : RRX(this, Rm);
            break;
    }

    u32 addr = *Rn;
    if (P) addr = U ? (addr + Rm) : (addr - Rm);
    u32 sz = B ? 1 : 4;
    u32 mask = B ? 0xFF : 0xFFFFFFFF;
    if (L == 0) { // store to RAM
        u32 v = AREAD(addr, sz);
        write_reg(this, Rd, v);
    }
    else {
        AWRITE(addr, sz, *Rd);
    }
    if (!P) addr = U ? (addr + Rm) : (addr - Rm);
    if (W) write_reg(this, Rn, addr);
}

void ARM7TDMI_ins_LDM_STM(struct ARM7TDMI *this, u32 opcode)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_ins_B_BL(struct ARM7TDMI *this, u32 opcode)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_ins_STC_LDC(struct ARM7TDMI *this, u32 opcode)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_ins_CDP(struct ARM7TDMI *this, u32 opcode)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_ins_MCR_MRC(struct ARM7TDMI *this, u32 opcode)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_ins_SWI(struct ARM7TDMI *this, u32 opcode)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_ins_INVALID(struct ARM7TDMI *this, u32 opcode)
{
    UNIMPLEMENTED;
}
