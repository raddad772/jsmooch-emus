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

#define OBIT(x) ((opcode >> (x)) & 1)

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
                case ARM7_system:
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


static inline void write_reg(struct ARM7TDMI *this, u32 *r, u32 v) {
    *r = v;
    if (r == &this->regs.PC) {
        ARM7TDMI_flush_pipeline(this);
        printf("\nNEW PC: %08x", v);
    }
}

static u32 MUL(struct ARM7TDMI *this, u32 product, u32 multiplicand, u32 multiplier, u32 S)
{
    this->cycles_executed++;
    if((multiplier >> 8) && (multiplier >>  8 != 0xFFFFFF)) this->cycles_executed++;
    if((multiplier >> 16) && (multiplier >> 16 != 0xFFFF)) this->cycles_executed++;
    if((multiplier >> 24) && (multiplier >> 24 != 0xFF)) this->cycles_executed++;
    product += multiplicand * multiplier;
    if(this->regs.CPSR.T || S) {
        this->regs.CPSR.Z = product == 0;
        this->regs.CPSR.N = (product >> 31) & 1;
    }
    return product;
}

void ARM7TDMI_ins_MUL_MLA(struct ARM7TDMI *this, u32 opcode)
{
//   if(accumulate) idle();
//  r(d) = MUL(accumulate ? r(n) : 0, r(m), r(s));
    u32 accumulate = OBIT(21);
    if (accumulate) this->cycles_executed++;
    u32 S = OBIT(20);
    u32 Rdd = (opcode >> 16) & 15;
    u32 Rnd = (opcode >> 12) & 15;
    u32 Rsd = (opcode >> 8) & 15;
    u32 Rmd = opcode & 15;
    this->regs.PC += 4;
    u32 Rn = *getR(this, Rnd);
    u32 Rm = *getR(this, Rmd);
    u32 Rs = *getR(this, Rsd);
    u32 *Rd = getR(this, Rdd);
    write_reg(this, Rd, MUL(this, accumulate ? Rn : 0, Rm, Rs, S));
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
    u32 P = OBIT(24); // pre or post. 0=post
    u32 U = OBIT(23); // up/down, 0=down
    u32 I = OBIT(22); //
    u32 W = 1;
    if (P) W = OBIT(21);
    u32 Rnd = (opcode >> 16) & 15;
    u32 Rdd = (opcode >> 12) & 15;
    u32 imm_off = 0;
    imm_off = ((opcode >> 8) & 15) << 4;
    imm_off |= (opcode & 15);
    u32 Rmd = opcode & 15; // Offset register
    u32 *Rn = getR(this, Rnd);
    u32 *Rd = getR(this, Rdd);
    u32 Rm = I ? imm_off : *getR(this, Rmd);
    u32 addr = *Rn;
    if (P) addr = U ? (addr + Rm) : (addr - Rm);
    u32 H = OBIT(5);

    u32 sz = H ? 2 : 1;

    u32 val = this->read(this->read_ptr, addr, sz, ARM7P_nonsequential, 1);
    if (H && !(addr & 1)) { // read of a halfword to a unaligned address produces a byte-extend
        val = SIGNe16to32(val);
    }
    else {
        val = SIGNe8to32(val);
    }
    this->regs.PC += 4;
    if (!P) addr = U ? (addr + Rm) : (addr - Rm);
    if (W) {
        if (Rnd == 15) // writeback fails. technically invalid here
            write_reg(this, Rn, addr + 4);
        else
            write_reg(this, Rn, addr);
    }
    write_reg(this, Rd, val);
}

void ARM7TDMI_ins_MRS(struct ARM7TDMI *this, u32 opcode)
{
    u32 Rdd = (opcode >> 12) & 15;
    u32 *Rd = getR(this, Rdd);
    *Rd = this->regs.CPSR.u;
    this->regs.PC += 4;
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
    u32 Rnd = opcode & 15;
    u32 addr = *getR(this, Rnd);
    this->regs.CPSR.T = addr & 1;
    addr &= 0xFFFFFFFE;
    this->regs.PC = addr;
    ARM7TDMI_flush_pipeline(this);
}


static u32 TEST(struct ARM7TDMI *this, u32 v, u32 S)
{
    if (this->regs.CPSR.T || S) {
        printf("\nTEST FLAGS BY %08x (S:%d)", v, S);
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
        printf("\ndoing ADD..Rnd:%08x Rmd:%08x", Rnd, Rmd);
        u32 overflow = ~(Rnd ^ Rmd) & (Rnd ^ result);
        this->regs.CPSR.V = (overflow >> 31) & 1;
        // cpsr().c = 1 << 31 & (overflow ^ source ^ modify ^ result);
        // printf("\nINTERMEIDATE %08x", overflow ^ Rnd ^ Rmd ^ result);
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
    printf("\nALU OPCODE:%d Rn:%08x Rm:%08x", alu_opcode, Rn, Rm);
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
    printf("\nLSL input %08x : %d", v, amount);
    if (amount == 0) return v;
    this->carry = amount > 32 ? 0 : !!(v & 1 << (32 - amount));
    printf("\nSET CARRY %d", this->carry);
    v = (amount > 31) ? 0 : (v << amount);
    printf("\nLSL output %08x", v);
    return v;
}

// Logical shift right
static u32 LSR(struct ARM7TDMI *this, u32 v, u32 amount)
{
    this->carry = this->regs.CPSR.C;
    if (amount == 0) return v;
    this->carry = (amount > 32) ? 0 : !!(v & 1 << (amount - 1));
    printf("\nSET IT TO %d", this->carry);
    v = (amount > 31) ? 0 : (v >> amount);
    return v;
}

// Arithemtic (sign-extend) shift right
static u32 ASR(struct ARM7TDMI *this, u32 v, u32 amount)
{
 //   carry = cpsr().c;
    this->carry = this->regs.CPSR.C;

    //   if(shift == 0) return source;
    if (amount == 0) return v;

    //   carry = shift > 32 ? source & 1 << 31 : source & 1 << shift - 1;
    this->carry = (amount > 32) ? ((v & 1) << 31) : !!(v & 1 << (amount - 1));

    //   source = shift > 31 ? (i32)source >> 31 : (i32)source >> shift;
    v = (amount > 31) ? (i32)v >> 31 : (i32)v >> amount;

//    return source;
    return v;
}

static u32 ROR(struct ARM7TDMI *this, u32 v, u32 amount)
{
    printf("\nROR INPUT: %08x", v);
    this->carry = this->regs.CPSR.C;
    if (amount == 0) return v;
    amount &= 31;
    if (amount) v = (v << (32 - amount)) | (v >> amount); // ?correct
    this->carry = !!(v & 1 << 31);
    printf("\nROR OUTPUT: %08x", v);
    return v;
}

// Rotate right thru carry
static u32 RRX(struct ARM7TDMI *this, u32 v)
{
    this->carry = v & 1;
    return (v >> 1) | (this->regs.CPSR.C << 31);
}

static u32 get_SPSR_by_mode(struct ARM7TDMI *this){
    switch(this->regs.CPSR.mode) {
        case ARM7_system:
        case ARM7_user:
            printf("\nINVALID!!!!!!");
            return this->regs.CPSR.u;
        case ARM7_fiq:
            return this->regs.SPSR_fiq;
        case ARM7_irq:
            return this->regs.SPSR_irq;
        case ARM7_supervisor:
            return this->regs.SPSR_svc;
        case ARM7_abort:
            return this->regs.SPSR_abt;
        case ARM7_undefined:
            return this->regs.SPSR_und;
        default:
            printf("\nINVALID2!!!");
            return this->regs.CPSR.u;
    }
}

void ARM7TDMI_ins_data_proc_immediate_shift(struct ARM7TDMI *this, u32 opcode)
{
    if (dbg.trace_on) printf("\nexec data_proc_immediate_shift");
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
    this->regs.PC += 4;
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

//        ALU(this, Rn, Rm, alu_opcode, S, Rd);
//    }
//    else {
        ALU(this, Rn, Rm, alu_opcode, S, Rd);
//    }

    if ((S==1) && (Rdd == 15)) {
        this->regs.CPSR.u = get_SPSR_by_mode(this);
        ARM7TDMI_fill_regmap(this);
        //S = 0;
    }

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

void ARM7TDMI_ins_LDR_STR_immediate_offset(struct ARM7TDMI *this, u32 opcode)
{
    u32 P = OBIT(24); // Pre/post. 0 = after-transfer, post
    u32 U = OBIT(23); // 0 = down, 1 = up
    u32 B = OBIT(22); // byte/word, 0 = 32bit, 1 = 8bit
    // when P=0, bit 21 is T, 0= normal, 1= force nonpriviledged, and W=1
    // when P=1, bit 21 is write-back bit, 0 = normal, 1 = write into base
    u32 T = 0; // 0 = normal, 1 = force unprivileged
    u32 W = 1; // writeback. 0 = no, 1 = write into base
    if (P == 0) { T = OBIT(21); }
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
    u32 link = OBIT(24);
    i32 offset = SIGNe24to32(opcode & 0xFFFFFF);
    offset <<= 2;
    if (link) *getR(this, 14) = this->regs.PC - 4;
    this->regs.PC += (u32)offset;
    ARM7TDMI_flush_pipeline(this);
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
    this->regs.R_svc[1] = this->regs.PC - 4;
    this->regs.SPSR_svc = this->regs.CPSR.u;
    this->regs.CPSR.mode = ARM7_supervisor;
    ARM7TDMI_fill_regmap(this);
    this->regs.CPSR.I = 1;
    this->regs.PC = 0x00000008;
    ARM7TDMI_flush_pipeline(this);
}

void ARM7TDMI_ins_INVALID(struct ARM7TDMI *this, u32 opcode)
{
    UNIMPLEMENTED;
}
