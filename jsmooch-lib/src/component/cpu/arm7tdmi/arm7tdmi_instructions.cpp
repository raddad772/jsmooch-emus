//
// Created by . on 12/4/24.
//

#include <cassert>
#include <cstdio>
#include <cstdlib>

#include "arm7tdmi.h"
#include "arm7tdmi_instructions.h"

#define UNIMPLEMENTED printf("\nUNIMPLEMENTED INSTRUCTION! %08x PC:%08x cyc:%lld", opcode, th->regs.PC, *th->trace.cycles); assert(1==2)
#define AREAD(addr, sz) th->read((addr), (sz), ARM7P_nonsequential, 1)
#define AWRITE(addr, sz, val) th->write((addr), (sz), ARM7P_nonsequential, (val) & mask)

#define OBIT(x) ((opcode >> (x)) & 1)

#define PC R[15]

namespace ARM7TDMI {

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

static u32 *get_SPSR_by_mode(core *th){
    switch(th->regs.CPSR.mode) {
        case M_user:
            return &th->regs.CPSR.u;
        case M_fiq:
            return &th->regs.SPSR_fiq;
        case M_irq:
            return &th->regs.SPSR_irq;
        case M_supervisor:
            return &th->regs.SPSR_svc;
        case M_abort:
            return &th->regs.SPSR_abt;
        case M_undefined:
            return &th->regs.SPSR_und;
        default:
        case M_system:
            printf("\nINVALID2!!!");
            return &th->regs.SPSR_invalid;
    }
}

static inline u32 *old_getR(core *th, u32 num) {
    // valid modes are,
    // 16-19, 23, 27, 31
    u32 m = th->regs.CPSR.mode;
    if ((m < 16) || ((m > 19) && (m != 23) && (m != 27) && (m != 31))) {
        if (num == 15) return &th->regs.R[15];
        return &th->regs.R_invalid[num];
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
            return &th->regs.R[num];
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
            return th->regs.CPSR.mode == M_fiq ? &th->regs.R_fiq[num - 8] : &th->regs.R[num];
        case 13:
        case 14: {
            switch(th->regs.CPSR.mode) {
                case M_abort:
                    return &th->regs.R_abt[num - 13];
                case M_fiq:
                    return &th->regs.R_fiq[num - 8];
                case M_irq:
                    return &th->regs.R_irq[num - 13];
                case M_supervisor:
                    return &th->regs.R_svc[num - 13];
                case M_undefined:
                    return &th->regs.R_und[num - 13];
                case M_user:
                case M_system:
                    return &th->regs.R[num];
                default:
                    assert(1==2);
                    return nullptr;
            }
            break; }
        case 15:
            return &th->regs.R[15];
        default:
            assert(1==2);
            return nullptr;
    }
}

static inline u32 *getR(core *th, u32 num) {
    return th->regmap[num];
}


void core::fill_regmap() {
    for (u32 i = 8; i < 15; i++) {
        regmap[i] = old_getR(this, i);
    }
}

void core::write_reg(u32 *r, u32 v) {
    *r = v;
    if (r == &regs.PC) {
        flush_pipeline();
    }
}

u32 MUL(core *th, u32 product, u32 multiplicand, u32 multiplier, u32 S)
{
    u32 n = 1;

    if((multiplier >> 8) && (multiplier >>  8 != 0xFFFFFF)) n++;
    if((multiplier >> 16) && (multiplier >> 16 != 0xFFFF)) n++;
    if((multiplier >> 24) && (multiplier >> 24 != 0xFF)) n++;
    th->idle(n);
    product += multiplicand * multiplier;
    if(th->regs.CPSR.T || S) {
        th->regs.CPSR.Z = product == 0;
        th->regs.CPSR.N = (product >> 31) & 1;
    }
    return product;
}

void ins_MUL_MLA(core *th, u32 opcode)
{
    u32 accumulate = OBIT(21);
    if (accumulate) th->idle(1);
    u32 S = OBIT(20);
    u32 Rdd = (opcode >> 16) & 15;
    u32 Rnd = (opcode >> 12) & 15;
    u32 Rsd = (opcode >> 8) & 15;
    u32 Rmd = opcode & 15;
    th->regs.PC += 4;
    th->pipeline.access = ARM7P_nonsequential | ARM7P_code;
    u32 Rn = *getR(th, Rnd);
    u32 Rm = *getR(th, Rmd);
    u32 Rs = *getR(th, Rsd);
    u32 *Rd = getR(th, Rdd);
    th->write_reg(Rd, MUL(th, accumulate ? Rn : 0, Rm, Rs, S));
}

void ins_MULL_MLAL(core *th, u32 opcode)
{
    u32 S = OBIT(20);
    u32 Rdd = (opcode >> 16) & 15;
    u32 Rnd = (opcode >> 12) & 15;
    u32 Rsd = (opcode >> 8) & 15;
    u32 Rmd = opcode & 15;
    u32 sign = OBIT(22); // signed if =1
    u32 accumulate = OBIT(21); // acumulate if =1
    th->regs.PC += 4;
    th->pipeline.access = ARM7P_nonsequential | ARM7P_code;
    u64 Rm = *getR(th, Rmd);
    u64 Rs = *getR(th, Rsd);

    u32 n = 2 + accumulate;
    u64 result = 0;
    if (sign) {
        if ((Rs >> 8) && ((Rs >> 8) != 0xFFFFFF)) n++;
        if ((Rs >> 16) && ((Rs >> 16) != 0xFFFF)) n++;
        if ((Rs >> 24) && ((Rs >> 24) != 0xFF)) n++;
        result = static_cast<u64>(static_cast<i64>(static_cast<i32>(Rm)) * static_cast<i64>(static_cast<i32>(Rs)));
    }
    else {
        if (Rs >> 8) n++;
        if (Rs >> 16) n++;
        if (Rs >> 24) n++;
        result = Rm * Rs;
    }
    th->idle(n);

    u32 *Rd = getR(th, Rdd);
    u32 *Rn = getR(th, Rnd);
    if (accumulate) result += (static_cast<u64>(*Rd) << 32) | static_cast<u64>(*Rn);
    th->write_reg(Rn, result & 0xFFFFFFFF);
    th->write_reg(Rd, result >> 32);
    if (S) {
        th->regs.CPSR.N = (result >> 63) & 1;
        th->regs.CPSR.Z = result == 0;
    }
}

void ins_SWP(core *th, u32 opcode)
{
    u32 B = OBIT(22);
    u32 Rnd = (opcode >> 16) & 15;
    u32 Rdd = (opcode >> 12) & 15;
    u32 Rmd = opcode & 15;
    th->regs.PC += 4;
    th->pipeline.access = ARM7P_nonsequential | ARM7P_code;
    // Rd=[Rn], [Rn]=Rm
    u32 *Rn = getR(th, Rnd);
    u32 *Rd = getR(th, Rdd);
    u32 *Rm = getR(th, Rmd);
    u32 mask = 0xFFFFFFFF;
    u32 sz = 4;
    if (B) {
        mask = 0xFF;
        sz = 1;
    }
    u32 tmp = th->read(*Rn, sz, ARM7P_nonsequential, 1) & mask;
    th->write(*Rn, sz, ARM7P_nonsequential | ARM7P_lock, (*Rm) & mask); // Rm = [Rn]
    th->idle(1);
    if (!B) tmp = align_val(*Rn, tmp);
    th->write_reg(Rd, tmp); // Rd = [Rn]
}

void ins_LDRH_STRH(core *th, u32 opcode)
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
    u32 *Rn = getR(th, Rnd);
    u32 *Rd = getR(th, Rdd);
    u32 Rm = I ? imm_off : *getR(th, Rmd);
    u32 addr = *Rn;
    if (P) addr = U ? (addr + Rm) : (addr - Rm);
    // L = 0 is store
    th->regs.PC += 4;
    th->pipeline.access = ARM7P_nonsequential | ARM7P_code;
    if (L) {
        u32 val = th->read(addr, 2, ARM7P_nonsequential, 1);
        if (addr &  1) { // read of a halfword to a unaligned address produces a weird ROR
            val = ((val >> 8) & 0xFF) | (val << 24);
        }
        if (!P) addr = U ? (addr + Rm) : (addr - Rm);
        if (W && !((Rnd == 15) && (Rdd == 15))) {
            if (Rnd == 15) // writeback fails. technically invalid here
                th->write_reg(Rn, addr + 4);
            else
                th->write_reg(Rn, addr);
        }
        th->idle(1);
        th->write_reg(Rd, val);
    }
    else {
        u32 val = *Rd;
        /*if (addr & 3) {
            val = (val << 16) | (val >> 16);
        }*/
        th->write(addr, 2, ARM7P_nonsequential, val & 0xFFFF);
        if (!P) addr = U ? (addr + Rm) : (addr - Rm);
        if (W) {
            if (Rnd == 15) // writeback fails. technically invalid here
                th->write_reg(Rn, addr + 4);
            else
                th->write_reg(Rn, addr);
        }
    }

}

void ins_LDRSB_LDRSH(core *th, u32 opcode)
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
    u32 *Rn = getR(th, Rnd);
    u32 *Rd = getR(th, Rdd);
    u32 Rm = I ? imm_off : *getR(th, Rmd);
    u32 addr = *Rn;
    if (P) addr = U ? (addr + Rm) : (addr - Rm);

    u32 H = OBIT(5);

    u32 sz = H ? 2 : 1;

    u32 val = th->read(addr, sz, ARM7P_nonsequential, 1);
    if (H && !(addr & 1)) { // read of a halfword to a unaligned address produces a byte-extend
        val = SIGNe16to32(val);
    }
    else {
        if (H) val = (val >> 8); // what we said above...
        val = SIGNe8to32(val);
    }
    th->regs.PC += 4;
    th->pipeline.access = ARM7P_nonsequential | ARM7P_code;
    if (!P) addr = U ? (addr + Rm) : (addr - Rm);
    if (W) {
        if (Rnd == 15) {// writeback fails. technically invalid here
            if (Rdd != 15) th->write_reg(Rn, addr + 4);
        }
        else
            th->write_reg(Rn, addr);
    }
    th->idle(1);
    th->write_reg(Rd, val);
}

void ins_MRS(core *th, u32 opcode)
{
    u32 PSR = OBIT(22); // 0 = CPSR, 1 = SPSR(current)
    u32 Rdd = (opcode >> 12) & 15;
    u32 *Rd = getR(th, Rdd);
    if (PSR) {
        if (th->regs.CPSR.mode == M_system) *Rd = th->regs.CPSR.u;
        else *Rd = *get_SPSR_by_mode(th);
    }
    else {
        *Rd = th->regs.CPSR.u;
    }

    th->pipeline.access = ARM7P_sequential | ARM7P_code;
    th->regs.PC += 4;
}

void ins_MSR_reg(core *th, u32 opcode)
{
    u32 PSR = OBIT(22); // 0 = CPSR, 1 = SPSR(current)
    u32 f = OBIT(19);
    u32 s = OBIT(18);
    u32 x = OBIT(17);
    u32 c = OBIT(16);
    u32 Rmd = opcode & 15;
    u32 mask = 0;
    if (f) mask |= 0xFF000000;
    if (s) mask |= 0xFF0000;
    if (x) mask |= 0xFF00;
    if (c) mask |= 0xFF;
    u32 imm = *getR(th, Rmd);
    if (!PSR) { // CPSR
        if (th->regs.CPSR.mode == M_user)
            mask &= 0xFF000000;
        if (mask & 0xFF)
            imm |= 0x10; // force th bit always
        th->schedule_IRQ_check();
        th->regs.CPSR.u = (~mask & th->regs.CPSR.u) | (imm & mask);
        if (mask & 0x0F) {
            th->fill_regmap();
        }
    }
    else {
        if ((th->regs.CPSR.mode != M_user) && (th->regs.CPSR.mode != M_system)) {
            u32 *v = get_SPSR_by_mode(th);
            *v = (~mask & *v) | (imm & mask);
        }
    }
    th->pipeline.access = ARM7P_sequential | ARM7P_code;
    th->regs.PC += 4;
}

void ins_MSR_imm(core *th, u32 opcode)
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
    if (f) mask |= 0xFF000000;
    if (s) mask |= 0xFF0000;
    if (x) mask |= 0xFF00;
    if (c) mask |= 0xFF;
    if (!PSR) { // CPSR
        if (th->regs.CPSR.mode == M_user)
            mask &= 0xFF000000;
        if (mask & 0xFF)
            imm |= 0x10; // force th bit always
        th->schedule_IRQ_check();
        th->regs.CPSR.u = (~mask & th->regs.CPSR.u) | (imm & mask);
        if (mask & 0x0F) {
            th->fill_regmap();
        }
    }
    else {
        if ((th->regs.CPSR.mode != M_user) && (th->regs.CPSR.mode != M_system)) {
            u32 *v = get_SPSR_by_mode(th);
            *v = (~mask & *v) | (imm & mask);
        }
    }
    th->regs.PC += 4;
    th->pipeline.access = ARM7P_sequential | ARM7P_code;
}

void ins_BX(core *th, u32 opcode)
{
    u32 Rnd = opcode & 15;
    u32 addr = *getR(th, Rnd);
    th->regs.CPSR.T = addr & 1;
    addr &= 0xFFFFFFFE;
    th->regs.PC = addr;
    th->flush_pipeline();
}


static u32 TEST(core *th, u32 v, u32 S)
{
    if (th->regs.CPSR.T || S) {
        th->regs.CPSR.N = (v >> 31) & 1;
        th->regs.CPSR.Z = v == 0;
        th->regs.CPSR.C = th->carry;
    }
    return v;
}

static u32 ADD(core *th, u32 Rnd, u32 Rmd, u32 carry, u32 S)
{
    u32 result = Rnd + Rmd + carry;
    if (th->regs.CPSR.T || S) {
        u32 overflow = ~(Rnd ^ Rmd) & (Rnd ^ result);
        th->regs.CPSR.V = (overflow >> 31) & 1;
        th->regs.CPSR.C = ((overflow ^ Rnd ^ Rmd ^ result) >> 31) & 1;
        th->regs.CPSR.Z = result == 0;
        th->regs.CPSR.N = (result >> 31) & 1;
    }
    return result;
}


static u32 SUB(core *th, u32 Rn, u32 Rm, u32 carry, u32 S)
{
    u32 iRm = Rm ^ 0xFFFFFFFF;
    u32 r = ADD(th, Rn, iRm, carry, S);
    return r;
}

static u32 ALU(core *th, u32 Rn, u32 Rm, u32 alu_opcode, u32 S, u32 *out) {
    switch(alu_opcode) {
        case 0: th->write_reg(out, TEST(th, Rn & Rm, S)); break;
        case 1: th->write_reg(out, TEST(th, Rn ^ Rm, S)); break;
        case 2: th->write_reg(out, SUB(th, Rn, Rm, 1, S)); break;
        case 3: th->write_reg(out, SUB(th, Rm, Rn, 1, S)); break;
        case 4: th->write_reg(out, ADD(th, Rn, Rm, 0, S)); break;
        case 5: th->write_reg(out, ADD(th, Rn, Rm, th->regs.CPSR.C, S)); break; // TODO: xx HUH?
        case 6: th->write_reg(out, SUB(th, Rn, Rm, th->regs.CPSR.C, S)); break;
        case 7: th->write_reg(out, SUB(th, Rm, Rn, th->regs.CPSR.C, S)); break;
        case 8: TEST(th, Rn & Rm, S); break;
        case 9: TEST(th, Rn ^ Rm, S); break;
        case 10: SUB(th, Rn, Rm, 1, S); break;
        case 11: ADD(th, Rn, Rm, 0, S); break;
        case 12: th->write_reg(out, TEST(th, Rn | Rm, S)); break;
        case 13: th->write_reg(out, TEST(th, Rm, S)); break;
        case 14: th->write_reg(out, TEST(th, Rn & ~Rm, S)); break;
        case 15: th->write_reg(out, TEST(th, Rm ^ 0xFFFFFFFF, S)); break;
        default:
            assert(1==2);
    }
    return *out;
}

// Logical shift left

static u32 LSL(core *th, u32 v, u32 amount) {
    th->carry = th->regs.CPSR.C;
    if (amount == 0) return v;
    th->carry = amount > 32 ? 0 : !!(v & 1 << (32 - amount));
    v = (amount > 31) ? 0 : (v << amount);
    return v;
}

// Logical shift right
static u32 LSR(core *th, u32 v, u32 amount)
{
    th->carry = th->regs.CPSR.C;
    if (amount == 0) return v;
    th->carry = (amount > 32) ? 0 : !!(v & 1 << (amount - 1));
    v = (amount > 31) ? 0 : (v >> amount);
    return v;
}

// Arithemtic (sign-extend) shift right
static u32 ASR(core *th, u32 v, u32 amount)
{
 //   carry = cpsr().c;
    th->carry = th->regs.CPSR.C;

    //   if(shift == 0) return source;
    if (amount == 0) return v;

    //   carry = shift > 32 ? source & 1 << 31 : source & 1 << shift - 1;
    th->carry = (amount >= 32) ? (!!(v & 0x80000000)) : !!(v & (1 << (amount - 1)));

    //   source = shift > 31 ? (i32)source >> 31 : (i32)source >> shift;
    v = (amount > 31) ? static_cast<i32>(v) >> 31 : static_cast<i32>(v) >> amount;

//    return source;
    return v;
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

// Rotate right thru carry
static u32 RRX(core *th, u32 v)
{
    th->carry = v & 1;
    return (v >> 1) | (th->regs.CPSR.C << 31);
}

void ins_data_proc_immediate_shift(core *th, u32 opcode)
{
    u32 alu_opcode = (opcode >> 21) & 15;
    u32 S = (opcode >> 20) & 1; // set condition codes. 0=no, 1=yes. must be 1 for 8-B
    u32 Rnd = (opcode >> 16) & 15; // first operand
    u32 Rdd = (opcode >> 12) & 15; // dest reg.
    u32 Is = (opcode >> 7) & 31; // shift amount
    u32 shift_type = (opcode >> 5) & 3; // 0=LSL, 1=LSR, 2=ASR, 3=ROR
    // R(bit4) = 0 for th
    u32 Rmd = opcode & 15;
    th->pipeline.access = ARM7P_code | ARM7P_sequential;

    u32 Rn = *getR(th, Rnd);
    u32 Rm = *getR(th, Rmd);
    u32 *Rd = getR(th, Rdd);
    th->carry = th->regs.CPSR.C;
    th->regs.PC += 4;
    switch(shift_type) {
        case 0: //
            Rm = LSL(th, Rm, Is);
            break;
        case 1:
            Rm = LSR(th, Rm, Is ? Is : 32);
            break;
        case 2:
            Rm = ASR(th, Rm, Is ? Is : 32);
            break;
        case 3:
            Rm = Is ? ROR(th, Rm, Is) : RRX(th, Rm);
            break;
        default:
            assert(1==2);
    }

    ALU(th, Rn, Rm, alu_opcode, S, Rd);
    if ((S==1) && (Rdd == 15)) {
        if (th->regs.CPSR.mode != M_system) {
            u32 v = *get_SPSR_by_mode(th);
            u32 old_i = th->regs.CPSR.I;
            th->regs.CPSR.u = v;
            if (old_i && !th->regs.CPSR.I) th->schedule_IRQ_check();
        }
        th->fill_regmap();
    }
    if ((Rdd == 15) && ((alu_opcode < 8) || (alu_opcode > 11))) {
        th->flush_pipeline();
    }
}

void ins_data_proc_register_shift(core *th, u32 opcode)
{
    u32 alu_opcode = (opcode >> 21) & 15;
    u32 S = (opcode >> 20) & 1; // set condition codes. 0=no, 1=yes. must be 1 for 8-B
    u32 Rnd = (opcode >> 16) & 15; // first operand
    u32 Rdd = (opcode >> 12) & 15; // dest reg.
    u32 Isd = (opcode >> 8) & 15;
    u32 shift_type = (opcode >> 5) & 3; // 0=LSL, 1=LSR, 2=ASR, 3=ROR
    // R(bit4) = 0 for th
    u32 Rmd = opcode & 15;

    u32 Is = (*getR(th, Isd)) & 0xFF; // shift amount
    th->idle(1);
    th->pipeline.access = ARM7P_code | ARM7P_nonsequential; // weird quirk of ARM
    th->regs.PC += 4;
    u32 Rn = *getR(th, Rnd);
    u32 Rm = *getR(th, Rmd);
    u32 *Rd = getR(th, Rdd);
    th->carry = th->regs.CPSR.C;
    switch(shift_type) {
        case 0: //
            Rm = LSL(th, Rm, Is);
            break;
        case 1:
            Rm = LSR(th, Rm, Is);
            break;
        case 2:
            Rm = ASR(th, Rm, Is);
            break;
        case 3:
            Rm = ROR(th, Rm, Is);
            break;
    }

    ALU(th, Rn, Rm, alu_opcode, S, Rd);

    if ((S==1) && (Rdd == 15)) {
        if (th->regs.CPSR.mode != M_system) {
            u32 old_i = th->regs.CPSR.I;
            th->regs.CPSR.u = *get_SPSR_by_mode(th);
            if (old_i && !th->regs.CPSR.I) th->schedule_IRQ_check();
        }
        th->fill_regmap();
    }
}

void ins_undefined_instruction(core *th, u32 opcode)
{
    printf("\nARM UNDEFINED INS!");
    assert(1==2);
    th->regs.R_und[1] = th->regs.PC - 4;
    th->regs.SPSR_und = th->regs.CPSR.u;
    th->regs.CPSR.mode = M_undefined;
    th->fill_regmap();
    th->regs.CPSR.I = 1;
    th->regs.PC = 0x00000004;
    th->flush_pipeline();
}

void ins_data_proc_immediate(core *th, u32 opcode)
{
    u32 alu_opcode = (opcode >> 21) & 15;
    u32 S = (opcode >> 20) & 1; // set condition codes. 0=no, 1=yes. must be 1 for 8-B
    u32 Rnd = (opcode >> 16) & 15; // first operand
    u32 Rdd = (opcode >> 12) & 15; // dest reg.

    u32 Rn = *getR(th, Rnd);
    th->regs.PC += 4;
    u32 *Rd = getR(th, Rdd);
    th->pipeline.access = ARM7P_code | ARM7P_sequential;

    u32 Rm = opcode & 0xFF;
    u32 imm_ROR_amount = (opcode >> 7) & 30;
    th->carry = th->regs.CPSR.C;
    if (imm_ROR_amount) Rm = ROR(th, Rm, imm_ROR_amount);
    ALU(th, Rn, Rm, alu_opcode, S, Rd);
    if ((S==1) && (Rdd == 15)) {
        if (th->regs.CPSR.mode != M_system) {
            u32 old_i = th->regs.CPSR.I;
            th->regs.CPSR.u = *get_SPSR_by_mode(th);
            if (old_i && !th->regs.CPSR.I) th->schedule_IRQ_check();
        }
        th->fill_regmap();
    }
}

void ins_LDR_STR_immediate_offset(core *th, u32 opcode)
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

    u32 *Rn = getR(th, Rnd);
    u32 *Rd = getR(th, Rdd);

    u32 offset = (opcode & 4095);
    u32 addr = *Rn;
    th->regs.PC += 4;
    th->pipeline.access = ARM7P_nonsequential | ARM7P_code;
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
                th->write_reg(Rn, addr+4);
            else
                th->write_reg(Rn, addr);
        }
        th->idle(1);
        th->write_reg(Rd, v);
    }
    else { // load from RAM
        AWRITE(addr, sz, *Rd);
        if (!P) addr = U ? (addr + offset) : (addr - offset);
        if (W) {
            if (Rnd == 15)
                th->write_reg(Rn, addr+4);
            else
                th->write_reg(Rn, addr);
        }
    }
}

void ins_LDR_STR_register_offset(core *th, u32 opcode)
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

    u32 *Rn = getR(th, Rnd);
    u32 Rm = *getR(th, Rmd);
    u32 *Rd = getR(th, Rdd);
    th->carry = th->regs.CPSR.C;
    switch(shift_type) {
        case 0: //
            Rm = LSL(th, Rm, Is);
            break;
        case 1:
            Rm = LSR(th, Rm, Is ? Is : 32);
            break;
        case 2:
            Rm = ASR(th, Rm, Is ? Is : 32);
            break;
        case 3:
            Rm = Is ? ROR(th, Rm, Is) : RRX(th, Rm);
            break;
    }

    u32 addr = *Rn;
    if (P) addr = U ? (addr + Rm) : (addr - Rm);
    u32 sz = B ? 1 : 4;
    u32 mask = B ? 0xFF : 0xFFFFFFFF;
    th->regs.PC += 4;
    th->pipeline.access = ARM7P_nonsequential | ARM7P_code;
    if (L == 1) { // LDR from RAM
        u32 v = AREAD(addr, sz);
        if ((!B) && (addr & 3)) v = align_val(addr, v);
        if (!P) addr = U ? (addr + Rm) : (addr - Rm);
        if (W)  {
            if (Rnd == 15)
                th->write_reg(Rn, addr+4);
            else
                th->write_reg(Rn, addr);
        }
        th->idle(1);
        th->write_reg(Rd, v);
    }
    else { // STR to RAM
        AWRITE(addr, sz, *Rd);
        if (!P) addr = U ? (addr + Rm) : (addr - Rm);
        if (W) {
            if (Rnd == 15)
                th->write_reg(Rn, addr+4);
            else
                th->write_reg(Rn, addr);
        }
    }
}

void ins_LDM_STM(core *th, u32 opcode)
{
    u32 P = OBIT(24); // P=0 add offset after. P=1 add offset first
    u32 U = OBIT(23); // 0=subtract offset, 1 =add
    u32 S = OBIT(22); // 0=no, 1=load PSR or force user bit
    u32 W = OBIT(21); // 0=no writeback, 1= writeback
    u32 L = OBIT(20); // 0=store, 1=load
    u32 Rnd = (opcode >> 16) & 15;

    u32 rlist = (opcode & 0xFFFF);
    u32 rcount = 0;
    //u32 *Rd = getR(th, Rnd);
    int first = -1;
    u32 bit = 0;
    u32 move_pc = 0;
    for (u32 i = 0; i < 16; i++) {
        u32 mbit = (rlist >> (bit++)) & 1;
        rcount += mbit;
        if (mbit && (first==-1)) first = static_cast<int>(i);
    }
    u32 byte_sz = rcount << 2; // 4 byte per register

    if (rlist == 0) {
    /* according to NBA, "If the register list is empty, only r15 will be loaded/stored but
     * the base will be incremented/decremented as if each register was transferred."
     */
        rlist = 0x8000;
        first = 15;
        byte_sz = 64;
    }
    move_pc = (rlist >> 15) & 1;

    u32 cur_addr = *getR(th, Rnd);
    u32 base_addr = cur_addr;

    u32 do_mode_switch = S && (!L || !move_pc);
    u32 old_mode = th->regs.CPSR.mode;
    if (do_mode_switch) {
        th->regs.CPSR.mode = M_user;
        th->fill_regmap();
    }

    if (!U) {
        P = !P;
        cur_addr -= byte_sz;
        base_addr -= byte_sz;
    }
    else {
        base_addr += byte_sz;
    }

    th->pipeline.access = ARM7P_code | ARM7P_nonsequential;
    th->regs.PC += 4;
    int access_type = ARM7P_nonsequential;
    u32 vr = *getR(th, Rnd);
    for (int i = first; i < 16; i++) {
        if (~rlist & (1 << i)) {
            continue;
        }
        if (P) {
            cur_addr += 4;
        }

        if (L) {
            u32 v = th->read(cur_addr, 4, access_type, 1);
            if (W && (i == first)) {
                th->write_reg(getR(th, Rnd), base_addr);
            }
            th->write_reg(getR(th, i), v);
        }
        else {
            th->write(cur_addr, 4, access_type, *getR(th, i));
            if (W && (i == first)) {
                th->write_reg(getR(th, Rnd), base_addr);
            }
        }

        if (!P) cur_addr += 4;
        access_type = ARM7P_sequential;
    }
    if (L) {
        th->idle(1);
        if (do_mode_switch) {
            // According to MBA,
            /*"     During the following two cycles of a usermode LDM,\n"
                   register accesses will go to both the user bank and original bank.
                   */
            // TODO: th
        }

        if (move_pc) {
            if (S) { // If force usermode...
                th->regs.CPSR.u |= 0x10;
                switch(old_mode) {
                    case M_system:
                    case M_user:
                        break;
                    case M_fiq:
                        th->schedule_IRQ_check();
                        th->regs.CPSR.u = th->regs.SPSR_fiq; break;
                    case M_irq:
                        th->schedule_IRQ_check();
                        th->regs.CPSR.u = th->regs.SPSR_irq; break;
                    case M_supervisor:
                        th->schedule_IRQ_check();
                        th->regs.CPSR.u = th->regs.SPSR_svc; break;
                    case M_abort:
                        th->schedule_IRQ_check();
                        th->regs.CPSR.u = th->regs.SPSR_abt; break;
                    case M_undefined:
                        th->schedule_IRQ_check();
                        th->regs.CPSR.u = th->regs.SPSR_und; break;
                    default:
                        break;
                }
                th->pipeline.flushed = 1;
            }
            th->fill_regmap();
        }
    }
    if (do_mode_switch) {
        th->regs.CPSR.mode = old_mode;
        th->fill_regmap();
    }
}

void ins_B_BL(core *th, u32 opcode)
{
    u32 link = OBIT(24);
    i32 offset = SIGNe24to32(opcode & 0xFFFFFF);
    offset <<= 2;
    if (link) *getR(th, 14) = th->regs.PC - 4;
    th->regs.PC += static_cast<u32>(offset);
    /*if (th->regs.PC == 0x08001ec4) {
        dbg_break("BAD MOJO", *th->trace.cycles);
    }*/
    th->flush_pipeline();
}

void ins_STC_LDC(core *th, u32 opcode)
{
    printf("\nWARNING STC/LDC");
    th->regs.PC += 4;
}

void ins_CDP(core *th, u32 opcode)
{
    UNIMPLEMENTED;
}

void ins_MCR_MRC(core *th, u32 opcode)
{
    dbg_break("BAD ARM OP", *th->trace.cycles);
    //UNIMPLEMENTED;
}

void ins_SWI(core *th, u32 opcode)
{
    th->regs.R_svc[1] = th->regs.PC - 4;
    th->regs.SPSR_svc = th->regs.CPSR.u;
    th->regs.CPSR.mode = M_supervisor;
    th->fill_regmap();
    th->regs.CPSR.I = 1;
    th->regs.PC = 0x00000008;
    th->flush_pipeline();
    //printf("\nWARNING SWI %d", opcode & 0xFF);
}

void ins_INVALID(core *th, u32 opcode)
{
    UNIMPLEMENTED;
}
}