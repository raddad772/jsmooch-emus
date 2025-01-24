//
// Created by . on 1/18/25.
//
#include <stdio.h>
#include <stdlib.h>

#include "arm946es.h"
#include "arm946es_instructions.h"
#include "nds_cp15.h"

#define UNIMPLEMENTED printf("\nUNIMPLEMENTED INSTRUCTION! %08x PC:%08x cyc:%lld", opcode, this->regs.PC, *this->trace.cycles); assert(1==2)
#define AREAD(addr, sz) ARM946ES_read(this, (addr), (sz), ARM9P_nonsequential, 1)
#define AWRITE(addr, sz, val) ARM946ES_write(this, (addr), (sz), ARM9P_nonsequential, (val) & mask)

#define OBIT(x) ((opcode >> (x)) & 1)

#define PC R[15]

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

static u32 *get_SPSR_by_mode(struct ARM946ES *this){
    switch(this->regs.CPSR.mode) {
        case ARM9_user:
            return &this->regs.CPSR.u;
        case ARM9_fiq:
            return &this->regs.SPSR_fiq;
        case ARM9_irq:
            return &this->regs.SPSR_irq;
        case ARM9_supervisor:
            return &this->regs.SPSR_svc;
        case ARM9_abort:
            return &this->regs.SPSR_abt;
        case ARM9_undefined:
            return &this->regs.SPSR_und;
        default:
        case ARM9_system:
            printf("\nINVALID2!!!");
            return &this->regs.SPSR_invalid;
    }
}

static inline u32 *old_getR(struct ARM946ES *this, u32 num) {
    // valid modes are,
    // 16-19, 23, 27, 31
    u32 m = this->regs.CPSR.mode;
    if ((m < 16) || ((m > 19) && (m != 23) && (m != 27) && (m != 31))) {
        if (num == 15) return &this->regs.R[15];
        return &this->regs.R_invalid[num];
    }
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
            return this->regs.CPSR.mode == ARM9_fiq ? &this->regs.R_fiq[num - 8] : &this->regs.R[num];
        case 13:
        case 14: {
            switch(this->regs.CPSR.mode) {
                case ARM9_abort:
                    return &this->regs.R_abt[num - 13];
                case ARM9_fiq:
                    return &this->regs.R_fiq[num - 8];
                case ARM9_irq:
                    return &this->regs.R_irq[num - 13];
                case ARM9_supervisor:
                    return &this->regs.R_svc[num - 13];
                case ARM9_undefined:
                    return &this->regs.R_und[num - 13];
                case ARM9_user:
                case ARM9_system:
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

static inline u32 *getR(struct ARM946ES *this, u32 num) {
    return this->regmap[num];
}

/*static void pprint_mode(u32 w)
{
    switch(w) {
        case ARM9_user:
            printf("user");
            break;
        case ARM9_undefined:
            printf("undefined");
            break;
        case ARM9_supervisor:
            printf("supervisor");
            break;
        case ARM9_fiq:
            printf("fiq");
            break;
        case ARM9_irq:
            printf("irq");
            break;
        case ARM9_system:
            printf("system");
            break;
        default:
            printf("unknown:%d", w);
            break;
    }
}*/


void ARM946ES_fill_regmap(struct ARM946ES *this) {
    for (u32 i = 8; i < 15; i++) {
        this->regmap[i] = old_getR(this, i);
    }
}

static inline void write_reg(struct ARM946ES *this, u32 *r, u32 v) {
    *r = v;
    if (r == &this->regs.PC) {
        ARM946ES_flush_pipeline(this);
    }
}

static u32 MUL(struct ARM946ES *this, u32 product, u32 multiplicand, u32 multiplier, u32 S)
{
    u32 n = 1;

    if((multiplier >> 8) && (multiplier >>  8 != 0xFFFFFF)) n++;
    if((multiplier >> 16) && (multiplier >> 16 != 0xFFFF)) n++;
    if((multiplier >> 24) && (multiplier >> 24 != 0xFF)) n++;
    ARM946ES_idle(this, n);
    product += multiplicand * multiplier;
    if(this->regs.CPSR.T || S) {
        this->regs.CPSR.Z = product == 0;
        this->regs.CPSR.N = (product >> 31) & 1;
    }
    return product;
}

void ARM946ES_ins_MUL_MLA(struct ARM946ES *this, u32 opcode)
{
    u32 accumulate = OBIT(21);
    if (accumulate) ARM946ES_idle(this, 1);
    u32 S = OBIT(20);
    u32 Rdd = (opcode >> 16) & 15;
    u32 Rnd = (opcode >> 12) & 15;
    u32 Rsd = (opcode >> 8) & 15;
    u32 Rmd = opcode & 15;
    this->regs.PC += 4;
    this->pipeline.access = ARM9P_nonsequential | ARM9P_code;
    u32 Rn = *getR(this, Rnd);
    u32 Rm = *getR(this, Rmd);
    u32 Rs = *getR(this, Rsd);
    u32 *Rd = getR(this, Rdd);
    write_reg(this, Rd, MUL(this, accumulate ? Rn : 0, Rm, Rs, S));
}

void ARM946ES_ins_MULL_MLAL(struct ARM946ES *this, u32 opcode)
{
    u32 S = OBIT(20);
    u32 Rdd = (opcode >> 16) & 15;
    u32 Rnd = (opcode >> 12) & 15;
    u32 Rsd = (opcode >> 8) & 15;
    u32 Rmd = opcode & 15;
    u32 sign = OBIT(22); // signed if =1
    u32 accumulate = OBIT(21); // acumulate if =1
    this->regs.PC += 4;
    this->pipeline.access = ARM9P_nonsequential | ARM9P_code;
    u64 Rm = *getR(this, Rmd);
    u64 Rs = *getR(this, Rsd);

    u32 n = 2 + accumulate;
    u64 result = 0;
    if (sign) {
        if ((Rs >> 8) && ((Rs >> 8) != 0xFFFFFF)) n++;
        if ((Rs >> 16) && ((Rs >> 16) != 0xFFFF)) n++;
        if ((Rs >> 24) && ((Rs >> 24) != 0xFF)) n++;
        result = (u64)((i64)(i32)Rm * (i64)(i32)Rs);
    }
    else {
        if (Rs >> 8) n++;
        if (Rs >> 16) n++;
        if (Rs >> 24) n++;
        result = Rm * Rs;
    }
    ARM946ES_idle(this, n);

    u32 *Rd = getR(this, Rdd);
    u32 *Rn = getR(this, Rnd);
    if (accumulate) result += (((u64)(*Rd)) << 32) | ((u64)(*Rn));
    write_reg(this, Rn, result & 0xFFFFFFFF);
    write_reg(this, Rd, result >> 32);
    if (S) {
        this->regs.CPSR.N = (result >> 63) & 1;
        this->regs.CPSR.Z = result == 0;
    }
}

void ARM946ES_ins_SWP(struct ARM946ES *this, u32 opcode)
{
    u32 B = OBIT(22);
    u32 Rnd = (opcode >> 16) & 15;
    u32 Rdd = (opcode >> 12) & 15;
    u32 Rmd = opcode & 15;
    this->regs.PC += 4;
    this->pipeline.access = ARM9P_nonsequential | ARM9P_code;
    // Rd=[Rn], [Rn]=Rm
    u32 *Rn = getR(this, Rnd);
    u32 *Rd = getR(this, Rdd);
    u32 *Rm = getR(this, Rmd);
    u32 mask = 0xFFFFFFFF;
    u32 sz = 4;
    if (B) {
        mask = 0xFF;
        sz = 1;
    }
    u32 tmp = ARM946ES_read(this, *Rn, sz, ARM9P_nonsequential, 1) & mask;
    ARM946ES_write(this, *Rn, sz, ARM9P_nonsequential | ARM9P_lock, (*Rm) & mask); // Rm = [Rn]
    ARM946ES_idle(this, 1);
    if (!B) tmp = align_val(*Rn, tmp);
    write_reg(this, Rd, tmp); // Rd = [Rn]
}

void ARM946ES_ins_LDRH_STRH(struct ARM946ES *this, u32 opcode)
{
    u32 P = OBIT(24); // pre or post. 0=post
    u32 U = OBIT(23); // up/down, 0=down
    u32 I = OBIT(22); // 0 = register, 1 = immediate offset
    u32 L = OBIT(20);
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
    // L = 0 is store
    this->regs.PC += 4;
    this->pipeline.access = ARM9P_nonsequential | ARM9P_code;
    if (L) {
        u32 val = ARM946ES_read(this, addr, 2, ARM9P_nonsequential, 1);
        if (addr &  1) { // read of a halfword to a unaligned address produces a weird ROR
            val = ((val >> 8) & 0xFF) | (val << 24);
        }
        if (!P) addr = U ? (addr + Rm) : (addr - Rm);
        if (W && !((Rnd == 15) && (Rdd == 15))) {
            if (Rnd == 15) // writeback fails. technically invalid here
                write_reg(this, Rn, addr + 4);
            else
                write_reg(this, Rn, addr);
        }
        write_reg(this, Rd, val);
    }
    else {
        u32 val = *Rd;
        /*if (addr & 3) {
            val = (val << 16) | (val >> 16);
        }*/
        ARM946ES_write(this, addr, 2, ARM9P_nonsequential, val & 0xFFFF);
        if (!P) addr = U ? (addr + Rm) : (addr - Rm);
        if (W) {
            if (Rnd == 15) // writeback fails. technically invalid here
                write_reg(this, Rn, addr + 4);
            else
                write_reg(this, Rn, addr);
        }
    }

}

void ARM946ES_ins_LDRSB_LDRSH(struct ARM946ES *this, u32 opcode)
{
    u32 P = OBIT(24); // pre or post. 0=post
    u32 U = OBIT(23); // up/down, 0=down
    u32 I = OBIT(22); //
    u32 W = 1;
    if (P) W = OBIT(21);
    u32 Rnd = (opcode >> 16) & 15;
    u32 Rdd = (opcode >> 12) & 15;
    u32 imm_off = ((opcode >> 8) & 15) << 4;
    imm_off |= (opcode & 15);
    u32 Rmd = opcode & 15; // Offset register
    u32 *Rn = getR(this, Rnd);
    u32 *Rd = getR(this, Rdd);
    u32 Rm = I ? imm_off : *getR(this, Rmd);
    u32 addr = *Rn;
    if (P) addr = U ? (addr + Rm) : (addr - Rm);

    u32 H = OBIT(5);

    u32 sz = H ? 2 : 1;

    u32 val = ARM946ES_read(this, addr, sz, ARM9P_nonsequential, 1);
    if (H && !(addr & 1)) { // read of a halfword to a unaligned address produces a byte-extend
        val = SIGNe16to32(val);
    }
    else {
        if (H) val = (val >> 8); // what we said above...
        val = SIGNe8to32(val);
    }
    this->regs.PC += 4;
    this->pipeline.access = ARM9P_nonsequential | ARM9P_code;
    if (!P) addr = U ? (addr + Rm) : (addr - Rm);
    if (W) {
        if (Rnd == 15) {// writeback fails. technically invalid here
            if (Rdd != 15) write_reg(this, Rn, addr + 4);
        }
        else
            write_reg(this, Rn, addr);
    }
    write_reg(this, Rd, val);
}

void ARM946ES_ins_MRS(struct ARM946ES *this, u32 opcode)
{
    u32 PSR = OBIT(22); // 0 = CPSR, 1 = SPSR(current)
    u32 Rdd = (opcode >> 12) & 15;
    u32 *Rd = getR(this, Rdd);
    if (PSR) {
        if (this->regs.CPSR.mode == ARM9_system) *Rd = this->regs.CPSR.u;
        else *Rd = *get_SPSR_by_mode(this);
    }
    else {
        *Rd = this->regs.CPSR.u;
    }

    this->pipeline.access = ARM9P_sequential | ARM9P_code;
    this->regs.PC += 4;
}

void ARM946ES_ins_MSR_reg(struct ARM946ES *this, u32 opcode)
{
    u32 PSR = OBIT(22); // 0 = CPSR, 1 = SPSR(current)
    u32 f = OBIT(19);
    u32 s = OBIT(18);
    u32 x = OBIT(17);
    u32 c = OBIT(16);
    u32 Rmd = opcode & 15;
    u32 mask = 0;
    if (f) mask |= 0xFF000000;
    u32 cycles = 0;
    if (s) { cycles = 2; mask |= 0xFF0000; };
    if (x) { cycles = 2; mask |= 0xFF00; };
    if (c) { cycles = 2; mask |= 0xFF; };
    u32 imm = *getR(this, Rmd);
    if (!PSR) { // CPSR
        if (this->regs.CPSR.mode == ARM9_user)
            mask &= 0xFF000000;
        if (mask & 0xFF)
            imm |= 0x10; // force this bit always
        u32 old_mode = this->regs.CPSR.mode;
        this->regs.CPSR.u = (~mask & this->regs.CPSR.u) | (imm & mask);
        if (mask & 0x0F) {
            ARM946ES_fill_regmap(this);
        }
    }
    else {
        if ((this->regs.CPSR.mode != ARM9_user) && (this->regs.CPSR.mode != ARM9_system)) {
            u32 *v = get_SPSR_by_mode(this);
            *v = (~mask & *v) | (imm & mask);
        }
        cycles = 2;
    }
    if (cycles) ARM946ES_idle(this, cycles);
    this->pipeline.access = ARM9P_sequential | ARM9P_code;
    this->regs.PC += 4;
}

void ARM946ES_ins_MSR_imm(struct ARM946ES *this, u32 opcode)
{
    // something -> MSR or CPSR
    u32 PSR = OBIT(22); // 0 = CPSR, 1 = SPSR(current)
    u32 f = OBIT(19);
    u32 s = OBIT(18);
    u32 x = OBIT(17);
    u32 c = OBIT(16);
    u32 Is = ((opcode >> 8) & 15) << 1;
    u32 imm = opcode & 255;
    if (Is) imm = (imm << (32 - Is)) | (imm >> Is);
    u32 mask = 0;
    u32 cycles = 0;
    if (f) mask |= 0xFF000000;
    if (s) { mask |= 0xFF0000; cycles = 2; }
    if (x) { mask |= 0xFF00; cycles = 2; }
    if (c) { mask |= 0xFF; cycles = 2; }
    if (!PSR) { // CPSR
        if (this->regs.CPSR.mode == ARM9_user)
            mask &= 0xFF000000;
        if (mask & 0xFF)
            imm |= 0x10; // force this bit always
        this->regs.CPSR.u = (~mask & this->regs.CPSR.u) | (imm & mask);
        if (mask & 0x0F) {
            ARM946ES_fill_regmap(this);
        }
    }
    else {
        if ((this->regs.CPSR.mode != ARM9_user) && (this->regs.CPSR.mode != ARM9_system)) {
            u32 *v = get_SPSR_by_mode(this);
            *v = (~mask & *v) | (imm & mask);
        }
        cycles = 2;
    }
    if (cycles) ARM946ES_idle(this, cycles);
    this->regs.PC += 4;
    this->pipeline.access = ARM9P_sequential | ARM9P_code;
}

void ARM946ES_ins_BX(struct ARM946ES *this, u32 opcode)
{
    u32 Rnd = opcode & 15;
    u32 addr = *getR(this, Rnd);
    this->regs.CPSR.T = addr & 1;
    addr &= 0xFFFFFFFE;
    this->regs.PC = addr;
    ARM946ES_flush_pipeline(this);
}


static u32 TEST(struct ARM946ES *this, u32 v, u32 S)
{
    if (this->regs.CPSR.T || S) {
        this->regs.CPSR.N = (v >> 31) & 1;
        this->regs.CPSR.Z = v == 0;
        this->regs.CPSR.C = this->carry;
    }
    return v;
}

static u32 ADD(struct ARM946ES *this, u32 Rnd, u32 Rmd, u32 carry, u32 S)
{
    u32 result = Rnd + Rmd + carry;
    if (this->regs.CPSR.T || S) {
        u32 overflow = ~(Rnd ^ Rmd) & (Rnd ^ result);
        this->regs.CPSR.V = (overflow >> 31) & 1;
        this->regs.CPSR.C = ((overflow ^ Rnd ^ Rmd ^ result) >> 31) & 1;
        this->regs.CPSR.Z = result == 0;
        this->regs.CPSR.N = (result >> 31) & 1;
    }
    return result;
}


// case 2: v = SUB(this, Rn, Rm, 1); break;
static u32 SUB(struct ARM946ES *this, u32 Rn, u32 Rm, u32 carry, u32 S)
{
    u32 iRm = Rm ^ 0xFFFFFFFF;
    u32 r = ADD(this, Rn, iRm, carry, S);
    return r;
}

static u32 ALU(struct ARM946ES *this, u32 Rn, u32 Rm, u32 alu_opcode, u32 S, u32 *out) {
    switch(alu_opcode) {
        case 0: write_reg(this, out, TEST(this, Rn & Rm, S)); break;
        case 1: write_reg(this, out, TEST(this, Rn ^ Rm, S)); break;
        case 2: write_reg(this, out, SUB(this, Rn, Rm, 1, S)); break;
        case 3: write_reg(this, out, SUB(this, Rm, Rn, 1, S)); break;
        case 4: write_reg(this, out, ADD(this, Rn, Rm, 0, S)); break;
        case 5: write_reg(this, out, ADD(this, Rn, Rm, this->regs.CPSR.C, S)); break; // TODO: xx HUH?
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
        default:
            assert(1==2);
    }
    return *out;
}

// Logical shift left

static u32 LSL(struct ARM946ES *this, u32 v, u32 amount) {
    this->carry = this->regs.CPSR.C;
    if (amount == 0) return v;
    this->carry = amount > 32 ? 0 : !!(v & 1 << (32 - amount));
    v = (amount > 31) ? 0 : (v << amount);
    return v;
}

// Logical shift right
static u32 LSR(struct ARM946ES *this, u32 v, u32 amount)
{
    this->carry = this->regs.CPSR.C;
    if (amount == 0) return v;
    this->carry = (amount > 32) ? 0 : !!(v & 1 << (amount - 1));
    v = (amount > 31) ? 0 : (v >> amount);
    return v;
}

// Arithemtic (sign-extend) shift right
static u32 ASR(struct ARM946ES *this, u32 v, u32 amount)
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

static u32 ROR(struct ARM946ES *this, u32 v, u32 amount)
{
    this->carry = this->regs.CPSR.C;
    if (amount == 0) return v;
    amount &= 31;
    if (amount) v = (v << (32 - amount)) | (v >> amount); // ?correct
    this->carry = !!(v & 1 << 31);
    return v;
}

// Rotate right thru carry
static u32 RRX(struct ARM946ES *this, u32 v)
{
    this->carry = v & 1;
    return (v >> 1) | (this->regs.CPSR.C << 31);
}

void ARM946ES_ins_data_proc_immediate_shift(struct ARM946ES *this, u32 opcode)
{
    u32 alu_opcode = (opcode >> 21) & 15;
    u32 S = (opcode >> 20) & 1; // set condition codes. 0=no, 1=yes. must be 1 for 8-B
    u32 Rnd = (opcode >> 16) & 15; // first operand
    u32 Rdd = (opcode >> 12) & 15; // dest reg.
    u32 Is = (opcode >> 7) & 31; // shift amount
    u32 shift_type = (opcode >> 5) & 3; // 0=LSL, 1=LSR, 2=ASR, 3=ROR
    // R(bit4) = 0 for this
    u32 Rmd = opcode & 15;
    this->pipeline.access = ARM9P_code | ARM9P_sequential;

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
        default:
            assert(1==2);
    }

    ALU(this, Rn, Rm, alu_opcode, S, Rd);
    if ((S==1) && (Rdd == 15)) {
        if (this->regs.CPSR.mode != ARM9_system) {
            u32 v = *get_SPSR_by_mode(this);
            this->regs.CPSR.u = v;
        }
        ARM946ES_fill_regmap(this);
    }
    if ((Rdd == 15) && ((alu_opcode < 8) || (alu_opcode > 11))) {
        ARM946ES_flush_pipeline(this);
    }
}

void ARM946ES_ins_data_proc_register_shift(struct ARM946ES *this, u32 opcode)
{
    u32 alu_opcode = (opcode >> 21) & 15;
    u32 S = (opcode >> 20) & 1; // set condition codes. 0=no, 1=yes. must be 1 for 8-B
    u32 Rnd = (opcode >> 16) & 15; // first operand
    u32 Rdd = (opcode >> 12) & 15; // dest reg.
    u32 Isd = (opcode >> 8) & 15;
    u32 shift_type = (opcode >> 5) & 3; // 0=LSL, 1=LSR, 2=ASR, 3=ROR
    // R(bit4) = 0 for this
    u32 Rmd = opcode & 15;

    u32 Is = (*getR(this, Isd)) & 0xFF; // shift amount
    ARM946ES_idle(this, 1);
    this->pipeline.access = ARM9P_code | ARM9P_nonsequential; // weird quirk of ARM
    this->regs.PC += 4;
    u32 Rn = *getR(this, Rnd);
    u32 Rm = *getR(this, Rmd);
    u32 *Rd = getR(this, Rdd);
    this->carry = this->regs.CPSR.C;
    switch(shift_type) {
        case 0: //
            Rm = LSL(this, Rm, Is);
            break;
        case 1:
            Rm = LSR(this, Rm, Is);
            break;
        case 2:
            Rm = ASR(this, Rm, Is);
            break;
        case 3:
            Rm = ROR(this, Rm, Is);
            break;
    }

    ALU(this, Rn, Rm, alu_opcode, S, Rd);

    if ((S==1) && (Rdd == 15)) {
        if (this->regs.CPSR.mode != ARM9_system)
            this->regs.CPSR.u = *get_SPSR_by_mode(this);
        ARM946ES_fill_regmap(this);
    }
}

void ARM946ES_ins_undefined_instruction(struct ARM946ES *this, u32 opcode)
{
    printf("\nARM UNDEFINED INS!");
    assert(1==2);
    this->regs.R_und[1] = this->regs.PC - 4;
    this->regs.SPSR_und = this->regs.CPSR.u;
    this->regs.CPSR.mode = ARM9_undefined;
    ARM946ES_fill_regmap(this);
    this->regs.CPSR.I = 1;
    this->regs.PC = this->regs.EBR | 0x00000004;
    ARM946ES_flush_pipeline(this);
}

void ARM946ES_ins_data_proc_immediate(struct ARM946ES *this, u32 opcode)
{
    u32 alu_opcode = (opcode >> 21) & 15;
    u32 S = (opcode >> 20) & 1; // set condition codes. 0=no, 1=yes. must be 1 for 8-B
    u32 Rnd = (opcode >> 16) & 15; // first operand
    u32 Rdd = (opcode >> 12) & 15; // dest reg.

    u32 Rn = *getR(this, Rnd);
    this->regs.PC += 4;
    u32 *Rd = getR(this, Rdd);
    this->pipeline.access = ARM9P_code | ARM9P_sequential;

    u32 Rm = opcode & 0xFF;
    u32 imm_ROR_amount = (opcode >> 7) & 30;
    this->carry = this->regs.CPSR.C;
    if (imm_ROR_amount) Rm = ROR(this, Rm, imm_ROR_amount);

    if (((opcode >> 28) == 1) && (alu_opcode == 12) && (Rnd == 1)){
        printf("\nFAIL HAPPENED HERE3! Rm:%x", Rm);
        //dbg_break("THE PLACE hAPPENED", *this->trace.cycles);
    }
    ALU(this, Rn, Rm, alu_opcode, S, Rd);
    if ((S==1) && (Rdd == 15)) {
        if (this->regs.CPSR.mode != ARM9_system)
            this->regs.CPSR.u = *get_SPSR_by_mode(this);
        ARM946ES_fill_regmap(this);
    }
}

void ARM946ES_ins_LDR_STR_immediate_offset(struct ARM946ES *this, u32 opcode)
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
    this->regs.PC += 4;
    this->pipeline.access = ARM9P_nonsequential | ARM9P_code;
    //if (Rnd == 15) addr += 4;
    if (P) addr = U ? (addr + offset) : (addr - offset);
    u32 sz = B ? 1 : 4;
    u32 mask = B ? 0xFF : 0xFFFFFFFF;
    if (L == 1) {// store to RAM
        u32 v = AREAD(addr, sz);
        if ((addr & 3) && !B) v = align_val(addr, v);
        if (!P) addr = U ? (addr + offset) : (addr - offset);
        if (W) {
            if (Rnd == 15)
                write_reg(this, Rn, addr+4);
            else
                write_reg(this, Rn, addr);
        }
        write_reg(this, Rd, v);
        if (Rdd == 15)
            this->regs.CPSR.T = this->regs.PC & 1;
    }
    else {
        AWRITE(addr, sz, *Rd);
        if (!P) addr = U ? (addr + offset) : (addr - offset);
        if (W) {
            if (Rnd == 15)
                write_reg(this, Rn, addr+4);
            else
                write_reg(this, Rn, addr);
        }
    }
}

void ARM946ES_ins_LDR_STR_register_offset(struct ARM946ES *this, u32 opcode)
{
    u32 P = OBIT(24);
    u32 U = OBIT(23);
    u32 B = OBIT(22);
    u32 T = 0; // 0 = normal, 1 = force unprivileged
    u32 W = 1; // writeback. 0 = no, 1 = write into base
    if (P == 0) { T = OBIT(21); }
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
    this->regs.PC += 4;
    this->pipeline.access = ARM9P_nonsequential | ARM9P_code;
    if (L == 1) { // LDR from RAM
        u32 v = AREAD(addr, sz);
        if ((!B) && (addr & 3)) v = align_val(addr, v);
        if (!P) addr = U ? (addr + Rm) : (addr - Rm);
        if (W)  {
            if (Rnd == 15)
                write_reg(this, Rn, addr+4);
            else
                write_reg(this, Rn, addr);
        }
        write_reg(this, Rd, v);
    }
    else { // STR to RAM
        AWRITE(addr, sz, *Rd);
        if (!P) addr = U ? (addr + Rm) : (addr - Rm);
        if (W) {
            if (Rnd == 15)
                write_reg(this, Rn, addr+4);
            else
                write_reg(this, Rn, addr);
        }
    }
}

void ARM946ES_ins_LDM_STM(struct ARM946ES *this, u32 opcode) {
    u32 P = OBIT(24); // P=0 add offset after. P=1 add offset first
    u32 U = OBIT(23); // 0=subtract offset, 1 =add
    u32 S = OBIT(22); // 0=no, 1=load PSR or force user bit
    u32 W = OBIT(21); // 0=no writeback, 1= writeback
    u32 L = OBIT(20); // 0=store, 1=load
    u32 Rnd = (opcode >> 16) & 15;
    u32 rlist = (opcode & 0xFFFF);

    u32 pc_moved = rlist & (1 << 15);

    u32 bytes = 0;
    u32 base_new;
    u32 addr = *getR(this, Rnd);
    u32 Rnd_last = 0;

    if (rlist != 0) {
        for (u32 i = 0; i <= 15; i++) {
            if ((rlist >> i) & 1)
                bytes += sizeof(u32);
        }

        Rnd_last = (rlist >> Rnd) == 1;
    } else {
        bytes = 64;
    }

    if (!U) {
        addr -= bytes;
        base_new = addr;
    } else {
        base_new = addr + bytes;
    }

    this->regs.PC += 4;
    this->pipeline.access = ARM9P_code | ARM9P_nonsequential;

    u32 mode = this->regs.CPSR.mode;

    if (S && (!L || !pc_moved)) {
        this->regs.CPSR.mode = ARM9_user;
        ARM946ES_fill_regmap(this);
    }

    u32 i = 0;
    u32 remaining = rlist;
    u32 access = ARM9P_nonsequential;
    while (remaining != 0) {
        while (((remaining >> i) &  1) == 0) i++;

        if (P == U)
            addr += 4;

        if (L) *getR(this, i) = ARM946ES_read(this, addr, 4, access, 1);
        else ARM946ES_write(this, addr, 4, access, *getR(this, i));

        access = ARM9P_sequential;

        if (P != U)
            addr += 4;

        remaining &= ~(1 << i);
    }

    if (S) {
        if (L && pc_moved) {
            if (this->regs.CPSR.mode != ARM9_system)
                this->regs.CPSR.u = *get_SPSR_by_mode(this);
        } else {
            this->regs.CPSR.mode = mode;
        }
        ARM946ES_fill_regmap(this);
    }

    if (W) {
        if (L) {
            // writeback if base is the only register or *NOT* the "last" register
            if (!Rnd_last || rlist == (1 << Rnd)) {
                *getR(this, Rnd) = base_new;
            }
        } else {
            *getR(this, Rnd) = base_new;
        }
    }

    if (L && pc_moved) {
        if ((this->regs.PC & 1) && !S) {
            this->regs.CPSR.T = 1;
            this->regs.PC &= 0xFFFFFFFE;
        }
        ARM946ES_flush_pipeline(this);
    }
}

void ARM946ES_ins_STC_LDC(struct ARM946ES *this, u32 opcode)
{
    printf("\nWARNING STC/LDC");
    this->regs.PC += 4;
}

void ARM946ES_ins_CDP(struct ARM946ES *this, u32 opcode)
{
    UNIMPLEMENTED;
}

static void undefined_exception(struct ARM946ES *this)
{
    this->regs.R_und[1] = this->regs.PC - 4;
    printf("\nWARN: PC MAY BE WRONG");
    this->regs.SPSR_und = this->regs.CPSR.u;
    this->regs.CPSR.mode = ARM9_undefined;
    ARM946ES_fill_regmap(this);

    this->regs.CPSR.I = 1;
    this->regs.PC = this->regs.EBR | 0x00000004;
    ARM946ES_flush_pipeline(this);
}

void ARM946ES_ins_MCR_MRC(struct ARM946ES *this, u32 opcode)
{
    if (this->regs.CPSR.mode == ARM9_user) {
        undefined_exception(this);
        return;
    }

    u32 v2 = ((opcode >> 28) & 15) == 15;
    u32 cp_opc = (opcode >> 21) & 7; // CP Opc - Coprocessor operation code
    u32 copro_to_arm = !OBIT(20);
    u32 Cnd = (opcode >> 16) & 15; // Cn     - Coprocessor source/dest. Register  (C0-C15)
    u32 Rdd = (opcode >> 12) & 15; // Rd     - ARM source/destination Register    (R0-R15)
    u32 Pnd = (opcode >> 8) & 15; // Coprocessor number                 (P0-P15)
    u32 CP = (opcode >> 5) & 7; // CP     - Coprocessor information            (0-7)
    u32 Cmd = opcode & 15; //  Coprocessor operand Register       (C0-C15)
    this->regs.PC += 4;
    this->pipeline.access = ARM9P_nonsequential | ARM9P_code;


    if (!copro_to_arm) { // ARM->CoPro
         u32 val = NDS_CP_read(this, Pnd, cp_opc, Cnd, Cmd, CP);
         if (Rdd == 15) {
             // When using MRC with R15: Bit 31-28 of data are copied to Bit 31-28 of CPSR (ie. N,Z,C,V flags), other data bits are ignored, CPSR Bit 27-0 are not affected, R15 (PC) is not affected.     */
             this->regs.CPSR.u = (this->regs.CPSR.u & 0x0FFFFFFF) | (val & 0xF0000000);
         }
         else {
             *getR(this, Rdd) = val;
         }
    }
    else {
        // When using MCR with R15: Coprocessor will receive a data value of PC+12.
        u32 v = *getR(this, Rdd);
        if (Rdd == 15) v += 4;
        NDS_CP_write(this, Pnd, cp_opc, Cnd, Cmd, CP, v);
    }
    ARM946ES_idle(this, 1);
}

void ARM946ES_ins_SWI(struct ARM946ES *this, u32 opcode)
{
    if ((opcode >> 28) == 14) { // BKPT
        this->regs.R_abt[1] = this->regs.PC - 4;
        this->regs.SPSR_abt = this->regs.CPSR.u;
        this->regs.CPSR.mode = ARM9_abort;
        ARM946ES_fill_regmap(this);
        this->regs.CPSR.I = 1;
        this->regs.PC = this->regs.EBR | 0x0000000C;
        ARM946ES_flush_pipeline(this);
        return;
    }
    this->regs.R_svc[1] = this->regs.PC - 4;
    this->regs.SPSR_svc = this->regs.CPSR.u;
    this->regs.CPSR.mode = ARM9_supervisor;
    ARM946ES_fill_regmap(this);
    this->regs.CPSR.I = 1;
    this->regs.PC = this->regs.EBR | 0x00000008;
    ARM946ES_flush_pipeline(this);
    //printf("\nWARNING SWI %d", opcode & 0xFF);
}

void ARM946ES_ins_INVALID(struct ARM946ES *this, u32 opcode)
{
    UNIMPLEMENTED;
}

void ARM946ES_ins_PLD(struct ARM946ES *this, u32 opcode)
{
    //UNIMPLEMENTED;
    printf("\nPLD!");
}

void ARM946ES_ins_SMLAxy(struct ARM946ES *this, u32 opcode)
{
    u32 Rdd = (opcode >> 16) & 15;
    u32 Rnd = (opcode >> 12) & 15;
    u32 Rsd = (opcode >> 8) & 15;
    u32 y = OBIT(6); // 1 = RS top, 0 bottom
    u32 x = OBIT(5); // 1 = Rm top, 0 bottom
    u32 Rmd = opcode & 15;

    this->regs.PC += 4;
    this->pipeline.access = ARM9P_code | ARM9P_sequential;

    u32 *Rd = getR(this, Rdd);
    u32 Rn = *getR(this, Rnd);
    u32 Rs = *getR(this, Rsd);
    u32 Rm = *getR(this, Rmd);
    if (x) Rm >>= 16;
    else Rm &= 0xFFFF;
    if (y) Rs >>= 16;
    else Rs &= 0xFFFF;

    u64 mul_result = ((i16)Rm * (i16)Rs);
    u64 result = mul_result + Rn;

    *Rd = result;
    if (Rdd == 15) {
        ARM946ES_flush_pipeline(this);
    }

    this->regs.CPSR.Q |= result > 0xFFFFFFFF;

}

void ARM946ES_ins_SMLAWy(struct ARM946ES *this, u32 opcode)
{
    u32 Rdd = (opcode >> 16) & 15;
    u32 Rnd = (opcode >> 12) & 15;
    u32 Rsd = (opcode >> 8) & 15;
    u32 y = OBIT(6); // 1 = RS top, 0 bottom
    u32 Rmd = opcode & 15;

    this->regs.PC += 4;
    this->pipeline.access = ARM9P_code | ARM9P_sequential;

    u32 *Rd = getR(this, Rdd);
    u32 Rn = *getR(this, Rnd);
    u32 Rs = *getR(this, Rsd);
    u32 Rm = *getR(this, Rmd);

    if (y) Rs >>= 16;
    else Rs &= 0xFFFF;

    u64 multiply_result = ((i64)(i32)Rm * (i64)(i16)Rs) >> 16;
    u64 result = multiply_result + Rn;

    *Rd = result;
    if (Rdd == 15) {
        ARM946ES_flush_pipeline(this);
    }

    this->regs.CPSR.Q |= result > 0xFFFFFFFF;

}

void ARM946ES_ins_SMULWy(struct ARM946ES *this, u32 opcode)
{
    u32 Rdd = (opcode >> 16) & 15;
    //u32 Rnd = (opcode >> 12) & 15;
    u32 Rsd = (opcode >> 8) & 15;
    u32 y = OBIT(6); // 1 = RS top, 0 bottom
    u32 Rmd = opcode & 15;

    this->regs.PC += 4;
    this->pipeline.access = ARM9P_code | ARM9P_sequential;

    u32 *Rd = getR(this, Rdd);
    u32 Rs = *getR(this, Rsd);
    u32 Rm = *getR(this, Rmd);

    if (y) Rs >>= 16;
    else Rs &= 0xFFFF;

    u64 result = ((i64)(i32)Rm * (i16)Rs) >> 16;

    *Rd = result;

    if (Rdd == 15) {
        ARM946ES_flush_pipeline(this);
    }
}

void ARM946ES_ins_SMLALxy(struct ARM946ES *this, u32 opcode)
{
    u32 Rdd = (opcode >> 16) & 15;
    u32 Rnd = (opcode >> 12) & 15;
    u32 Rsd = (opcode >> 8) & 15;
    u32 y = OBIT(6); // 1 = RS top, 0 bottom
    u32 x = OBIT(5); // 1 = Rm top, 0 bottom
    u32 Rmd = opcode & 15;

    this->regs.PC += 4;
    this->pipeline.access = ARM9P_code | ARM9P_nonsequential; // this one becomes nonsequential

    u32 *Rd = getR(this, Rdd);
    u32 Rs = *getR(this, Rsd);
    u32 Rm = *getR(this, Rmd);
    u32 *Rn = getR(this, Rnd);

    if (y) Rs >>= 16;
    else Rs &= 0xFFFF;
    if (x) Rm >>= 16;
    else Rm &= 0xFFFF;

    i64 result = (i64)(i16)Rm * (i64)(i16)Rs;
    result += (i64)(((u64)*Rn) | (((u64)*Rd) << 32));

    *Rn = ((u64)result) & 0xFFFFFFFF;
    *Rd = ((u64)result) >> 32;
    if (Rdd == 15) {
        ARM946ES_flush_pipeline(this);
    }
}

void ARM946ES_ins_SMULxy(struct ARM946ES *this, u32 opcode)
{
    u32 Rdd = (opcode >> 16) & 15;
    u32 Rsd = (opcode >> 8) & 15;
    u32 y = OBIT(6); // 1 = RS top, 0 bottom
    u32 x = OBIT(5); // 1 = Rm top, 0 bottom
    u32 Rmd = opcode & 15;

    this->regs.PC += 4;
    this->pipeline.access = ARM9P_code | ARM9P_nonsequential; // this one becomes nonsequential

    u32 *Rd = getR(this, Rdd);
    u32 Rs = *getR(this, Rsd);
    u32 Rm = *getR(this, Rmd);

    if (y) Rs >>= 16;
    else Rs &= 0xFFFF;
    if (x) Rm >>= 16;
    else Rm &= 0xFFFF;

    *Rd = ((i16)Rm * (i16)Rs);
    if (Rdd == 15) {
        ARM946ES_flush_pipeline(this);
    }
}

void ARM946ES_ins_LDRD_STRD(struct ARM946ES *this, u32 opcode)
{
/*
2: LDR{cond}D  Rd,<Address>  ;Load Doubleword  R(d)=[a], R(d+1)=[a+4]
3: STR{cond}D  Rd,<Address>  ;Store Doubleword [a]=R(d), [a+4]=R(d+1)
STRD/LDRD: Rd must be an even numbered register (R0,R2,R4,R6,R8,R10,R12).
STRD/LDRD: Address must be double-word aligned (multiple of eight).
          */
    u32 P = OBIT(24); // pre or post. 0=post
    u32 U = OBIT(23); // up/down, 0=down
    u32 I = OBIT(22); // 0 = register, 1 = immediate offset
    u32 L = !OBIT(5); // bits 5-6 = opcode. 10 = LDR, 11 = STR. so bit 5 == 1 = STR
    u32 W = 1;
    if (P) W = OBIT(21);

    u32 Rnd = (opcode >> 16) & 15;
    u32 Rdd = (opcode >> 12) & 14; // Only use top 3 bits
    u32 imm_off = 0;
    imm_off = ((opcode >> 8) & 15) << 4;
    imm_off |= (opcode & 15);
    u32 Rmd = opcode & 15; // Offset register
    u32 *Rn = getR(this, Rnd);
    u32 *Rd = getR(this, Rdd);
    u32 *Rdp1 = getR(this, Rdd | 1);
    u32 Rm = I ? imm_off : *getR(this, Rmd);
    u32 addr = *Rn;
    if (P) addr = U ? (addr + Rm) : (addr - Rm);
    this->regs.PC += 4;
    this->pipeline.access = ARM9P_nonsequential | ARM9P_code;
    if (L) {
        u32 val = ARM946ES_read(this, addr, 4, ARM9P_nonsequential, 1);
        u32 val_hi = ARM946ES_read(this, addr+4, 4, ARM9P_sequential, 1);

        if (!P) addr = U ? (addr + Rm) : (addr - Rm);
        if (W && !((Rnd == 15) && (Rdd == 14))) {
            if (Rnd == 15) // writeback fails. technically invalid here
                write_reg(this, Rn, addr + 4);
            else
                write_reg(this, Rn, addr);
        }
        write_reg(this, Rd, val);
        write_reg(this, Rdp1, val_hi);
    }
    else {
        ARM946ES_write(this, addr, 4, ARM9P_nonsequential, *Rd);
        ARM946ES_write(this, addr+4, 4, ARM9P_sequential, *Rdp1);
        if (!P) addr = U ? (addr + Rm) : (addr - Rm);
        if (W) {
            if (Rnd == 15) // writeback fails. technically invalid here
                write_reg(this, Rn, addr + 4);
            else
                write_reg(this, Rn, addr);
        }
    }
}

void ARM946ES_ins_CLZ(struct ARM946ES *this, u32 opcode)
{
    u32 Rdd = (opcode >> 12) & 15;
    u32 Rmd = opcode & 15;

    u32 v = *getR(this, Rmd);

    this->regs.PC += 4;
    this->pipeline.access = ARM9P_code | ARM9P_sequential;

    *getR(this, Rdd) = (v == 0) ? 32 : __builtin_clz(v);
    if (Rdd == 15) ARM946ES_flush_pipeline(this);
}

void ARM946ES_ins_BLX_reg(struct ARM946ES *this, u32 opcode)
{
    u32 link = this->regs.PC - 4;
    this->regs.PC = (*getR(this, opcode & 15));
    *getR(this, 14) = link;
    this->regs.CPSR.T = this->regs.PC & 1;
    if (this->regs.CPSR.T) this->regs.PC &= 0xFFFFFFFE;
    else this->regs.PC &= 0xFFFFFFFC;

    ARM946ES_flush_pipeline(this);
}

void ARM946ES_ins_QADD_QSUB_QDADD_QDSUB(struct ARM946ES *this, u32 opcode) {
    u32 src1 =  opcode & 15;
    u32 src2 = (opcode >> 16) & 15;
    u32 dst  = (opcode >> 12) & 15;
    u32 op2  = *getR(this, src2);

    u32 subtract = OBIT(21);
    u32 double_op2 = OBIT(22);

    if(double_op2) {
        u32 result = op2 + op2;

        if((op2 ^ result) >> 31) {
            this->regs.CPSR.Q = 1;
            result = 0x80000000 - (result >> 31);
        }

        op2 = result;
    }

    if (subtract) {
        u32 op1 = *getR(this, src1);
        u32 result = op1 - op2;

        if(((op1 ^ op2) & (op1 ^ result)) >> 31) {
            this->regs.CPSR.Q = 1;
            result = 0x80000000 - (result >> 31);
        }

        *getR(this, dst) = result;
    } else {
        u32 op1 = *getR(this, src1);
        u32 result = op1 + op2;

        if((~(op1 ^ op2) & (op2 ^ result)) >> 31) {
            this->regs.CPSR.Q = 1;
            result = 0x80000000 - (result >> 31);
        }
        *getR(this, dst) = result;
    }

    this->regs.PC += 4;
    this->pipeline.access = ARM9P_sequential | ARM9P_code;
}

void ARM946ES_ins_B_BL(struct ARM946ES *this, u32 opcode)
{
    u32 link = OBIT(24);
    i32 offset = SIGNe24to32(opcode & 0xFFFFFF);
    offset <<= 2;
    if (link) *getR(this, 14) = this->regs.PC - 4;
    this->regs.PC += (u32)offset;
    ARM946ES_flush_pipeline(this);
}

void ARM946ES_ins_BLX(struct ARM946ES *this, u32 opcode)
{
    i32 offset = SIGNe24to32(opcode & 0xFFFFFF);
    offset <<= 2;
    u32 H = OBIT(24) << 1;
    offset += H;

    *getR(this, 14) = this->regs.PC - 4;
    this->regs.PC += (u32)offset;
    this->regs.CPSR.T = 1;

    ARM946ES_flush_pipeline(this);
}