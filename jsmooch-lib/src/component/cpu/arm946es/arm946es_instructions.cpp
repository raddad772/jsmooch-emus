//
// Created by . on 1/18/25.
//
#include <cassert>
#include <cstdio>
#include <cstdlib>

#include "helpers/intrinsics.h"

#include "arm946es.h"
#include "arm946es_instructions.h"
#include "nds_cp15.h"

#define UNIMPLEMENTED printf("\nUNIMPLEMENTED INSTRUCTION! %08x PC:%08x cyc:%lld", opcode, regs.PC, *trace.cycles); assert(1==2)
#define AREAD(addr, sz) read((addr), (sz), ARM9P_nonsequential, true)
#define AWRITE(addr, sz, val) write((addr), (sz), ARM9P_nonsequential, (val) & mask)

#define OBIT(x) ((opcode >> (x)) & 1)

#define PC R[15]

namespace ARM946ES {


u32 align_val(const u32 addr, u32 tmp)
{
    if (addr & 3) {
        const u32 misalignment = addr & 3;
        if (misalignment == 1) tmp = ((tmp >> 8) & 0xFFFFFF) | (tmp << 24);
        else if (misalignment == 2) tmp = (tmp >> 16) | (tmp << 16);
        else tmp = ((tmp << 8) & 0xFFFFFF00) | (tmp >> 24);
    }
    return tmp;

}

u32 *core::get_SPSR_by_mode(){
    switch(regs.CPSR.mode) {
        case M_user:
            return &regs.CPSR.u;
        case M_fiq:
            return &regs.SPSR_fiq;
        case M_irq:
            return &regs.SPSR_irq;
        case M_supervisor:
            return &regs.SPSR_svc;
        case M_abort:
            return &regs.SPSR_abt;
        case M_undefined:
            return &regs.SPSR_und;
        default:
        case M_system:
            printf("\nINVALID2!!!");
            return &regs.SPSR_invalid;
    }
}

u32 *core::old_getR(const u32 num) {
    // valid modes are,
    // 16-19, 23, 27, 31
    u32 m = regs.CPSR.mode;
    if ((m < 16) || ((m > 19) && (m != 23) && (m != 27) && (m != 31))) {
        if (num == 15) return &regs.R[15];
        return &regs.R_invalid[num];
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
            return &regs.R[num];
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
            return regs.CPSR.mode == M_fiq ? &regs.R_fiq[num - 8] : &regs.R[num];
        case 13:
        case 14: {
            switch(regs.CPSR.mode) {
                case M_abort:
                    return &regs.R_abt[num - 13];
                case M_fiq:
                    return &regs.R_fiq[num - 8];
                case M_irq:
                    return &regs.R_irq[num - 13];
                case M_supervisor:
                    return &regs.R_svc[num - 13];
                case M_undefined:
                    return &regs.R_und[num - 13];
                case M_user:
                case M_system:
                    return &regs.R[num];
                default:
                    assert(1==2);
                    return nullptr;
            }
            break; }
        case 15:
            return &regs.R[15];
        default:
            assert(1==2);
            return nullptr;
    }
}

void core::fill_regmap() {
    for (u32 i = 8; i < 15; i++) {
        regmap[i] = old_getR(i);
    }
}

u32 core::MUL(u32 product, u32 multiplicand, u32 multiplier, u32 S)
{
    u32 n = 1;

    if((multiplier >> 8) && (multiplier >>  8 != 0xFFFFFF)) n++;
    if((multiplier >> 16) && (multiplier >> 16 != 0xFFFF)) n++;
    if((multiplier >> 24) && (multiplier >> 24 != 0xFF)) n++;
    idle(n);
    product += multiplicand * multiplier;
    if(regs.CPSR.T || S) {
        regs.CPSR.Z = product == 0;
        regs.CPSR.N = (product >> 31) & 1;
    }
    return product;
}

void core::ins_MUL_MLA(const u32 opcode)
{
    const u32 accumulate = OBIT(21);
    if (accumulate) idle(1);
    const u32 S = OBIT(20);
    const u32 Rdd = (opcode >> 16) & 15;
    const u32 Rnd = (opcode >> 12) & 15;
    const u32 Rsd = (opcode >> 8) & 15;
    const u32 Rmd = opcode & 15;
    regs.PC += 4;
    pipeline.access = ARM9P_nonsequential | ARM9P_code;
    const u32 Rn = *getR(Rnd);
    const u32 Rm = *getR(Rmd);
    const u32 Rs = *getR(Rsd);
    u32 *Rd = getR(Rdd);
    write_reg(Rd, MUL(accumulate ? Rn : 0, Rm, Rs, S));
}

void core::ins_MULL_MLAL(const u32 opcode)
{
    u32 S = OBIT(20);
    const u32 Rdd = (opcode >> 16) & 15;
    const u32 Rnd = (opcode >> 12) & 15;
    const u32 Rsd = (opcode >> 8) & 15;
    const u32 Rmd = opcode & 15;
    const u32 sign = OBIT(22); // signed if =1
    const u32 accumulate = OBIT(21); // acumulate if =1
    regs.PC += 4;
    pipeline.access = ARM9P_nonsequential | ARM9P_code;
    const u64 Rm = *getR(Rmd);
    const u64 Rs = *getR(Rsd);

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
    idle(n);

    u32 *Rd = getR(Rdd);
    u32 *Rn = getR(Rnd);
    if (accumulate) result += (static_cast<u64>(*Rd) << 32) | static_cast<u64>(*Rn);
    write_reg(Rn, result & 0xFFFFFFFF);
    write_reg(Rd, result >> 32);
    if (S) {
        regs.CPSR.N = (result >> 63) & 1;
        regs.CPSR.Z = result == 0;
    }
}

void core::ins_SWP(const u32 opcode)
{
    const u32 B = OBIT(22);
    const u32 Rnd = (opcode >> 16) & 15;
    const u32 Rdd = (opcode >> 12) & 15;
    const u32 Rmd = opcode & 15;
    regs.PC += 4;
    pipeline.access = ARM9P_nonsequential | ARM9P_code;
    // Rd=[Rn], [Rn]=Rm
    const u32 *Rn = getR(Rnd);
    u32 *Rd = getR(Rdd);
    const u32 *Rm = getR(Rmd);
    u32 mask = 0xFFFFFFFF;
    u8 sz = 4;
    if (B) {
        mask = 0xFF;
        sz = 1;
    }
    u32 tmp = read(*Rn, sz, ARM9P_nonsequential, true) & mask;
    write(*Rn, sz, ARM9P_nonsequential | ARM9P_lock, (*Rm) & mask); // Rm = [Rn]
    idle(1);
    if (!B) tmp = align_val(*Rn, tmp);
    write_reg(Rd, tmp); // Rd = [Rn]
}

void core::ins_LDRH_STRH(const u32 opcode)
{
    const u32 P = OBIT(24); // pre or post. 0=post
    const u32 U = OBIT(23); // up/down, 0=down
    const u32 I = OBIT(22); // 0 = register, 1 = immediate offset
    const u32 L = OBIT(20);
    u32 W = 1;
    if (P) W = OBIT(21);
    const u32 Rnd = (opcode >> 16) & 15;
    const u32 Rdd = (opcode >> 12) & 15;
    u32 imm_off = 0;
    imm_off = ((opcode >> 8) & 15) << 4;
    imm_off |= (opcode & 15);
    const u32 Rmd = opcode & 15; // Offset register
    u32 *Rn = getR(Rnd);
    u32 *Rd = getR(Rdd);
    const u32 Rm = I ? imm_off : *getR(Rmd);
    u32 addr = *Rn;
    if (P) addr = U ? (addr + Rm) : (addr - Rm);
    // L = 0 is store
    regs.PC += 4;
    pipeline.access = ARM9P_nonsequential | ARM9P_code;
    if (L) {
        u32 val = read(addr, 2, ARM9P_nonsequential, true);
        if (addr &  1) { // read of a halfword to a unaligned address produces a weird ROR
            val = ((val >> 8) & 0xFF) | (val << 24);
        }
        if (!P) addr = U ? (addr + Rm) : (addr - Rm);
        if (W && !((Rnd == 15) && (Rdd == 15))) {
            if (Rnd == 15) // writeback fails. technically invalid here
                write_reg(Rn, addr + 4);
            else
                write_reg(Rn, addr);
        }
        write_reg(Rd, val);
    }
    else {
        u32 val = *Rd;
        /*if (addr & 3) {
            val = (val << 16) | (val >> 16);
        }*/
        write(addr, 2, ARM9P_nonsequential, val & 0xFFFF);
        if (!P) addr = U ? (addr + Rm) : (addr - Rm);
        if (W) {
            if (Rnd == 15) // writeback fails. technically invalid here
                write_reg(Rn, addr + 4);
            else
                write_reg(Rn, addr);
        }
    }

}

void core::ins_LDRSB_LDRSH(const u32 opcode)
{
    const u32 P = OBIT(24); // pre or post. 0=post
    const u32 U = OBIT(23); // up/down, 0=down
    const u32 I = OBIT(22); //
    u32 W = 1;
    if (P) W = OBIT(21);
    const u32 Rnd = (opcode >> 16) & 15;
    const u32 Rdd = (opcode >> 12) & 15;
    u32 imm_off = ((opcode >> 8) & 15) << 4;
    imm_off |= (opcode & 15);
    const u32 Rmd = opcode & 15; // Offset register
    u32 *Rn = getR(Rnd);
    u32 *Rd = getR(Rdd);
    u32 Rm = I ? imm_off : *getR(Rmd);
    u32 addr = *Rn;
    if (P) addr = U ? (addr + Rm) : (addr - Rm);

    u32 H = OBIT(5);

    u8 sz = H ? 2 : 1;

    u32 val = read(addr, sz, ARM9P_nonsequential, true);
    if (H && !(addr & 1)) { // read of a halfword to a unaligned address produces a byte-extend
        val = SIGNe16to32(val);
    }
    else {
        if (H) val = (val >> 8); // what we said above...
        val = SIGNe8to32(val);
    }
    regs.PC += 4;
    pipeline.access = ARM9P_nonsequential | ARM9P_code;
    if (!P) addr = U ? (addr + Rm) : (addr - Rm);
    if (W) {
        if (Rnd == 15) {// writeback fails. technically invalid here
            if (Rdd != 15) write_reg(Rn, addr + 4);
        }
        else
            write_reg(Rn, addr);
    }
    write_reg(Rd, val);
}

void core::ins_MRS(const u32 opcode)
{
   const  u32 PSR = OBIT(22); // 0 = CPSR, 1 = SPSR(current)
    const u32 Rdd = (opcode >> 12) & 15;
    u32 *Rd = getR(Rdd);
    if (PSR) {
        if (regs.CPSR.mode == M_system) *Rd = regs.CPSR.u;
        else *Rd = *get_SPSR_by_mode();
    }
    else {
        *Rd = regs.CPSR.u;
    }

    pipeline.access = ARM9P_sequential | ARM9P_code;
    regs.PC += 4;
}

void core::ins_MSR_reg(const u32 opcode)
{
   const  u32 PSR = OBIT(22); // 0 = CPSR, 1 = SPSR(current)
    u32 f = OBIT(19);
    u32 s = OBIT(18);
    u32 x = OBIT(17);
    u32 c = OBIT(16);
    const u32 Rmd = opcode & 15;
    u32 mask = 0;
    if (f) mask |= 0xFF000000;
    u32 cycles = 0;
    if (s) { cycles = 2; mask |= 0xFF0000; };
    if (x) { cycles = 2; mask |= 0xFF00; };
    if (c) { cycles = 2; mask |= 0xFF; };
    u32 imm = *getR(Rmd);
    if (!PSR) { // CPSR
        if (regs.CPSR.mode == M_user)
            mask &= 0xFF000000;
        if (mask & 0xFF)
            imm |= 0x10; // force this bit always
        //u32 old_mode = regs.CPSR.mode;
        schedule_IRQ_check();
        regs.CPSR.u = (~mask & regs.CPSR.u) | (imm & mask);
        if (mask & 0x0F) {
            fill_regmap();
        }
    }
    else {
        if ((regs.CPSR.mode != M_user) && (regs.CPSR.mode != M_system)) {
            u32 *v = get_SPSR_by_mode();
            *v = (~mask & *v) | (imm & mask);
        }
        cycles = 2;
    }
    if (cycles) idle(cycles);
    pipeline.access = ARM9P_sequential | ARM9P_code;
    regs.PC += 4;
}

void core::ins_MSR_imm(const u32 opcode)
{
    // something -> MSR or CPSR
   const  u32 PSR = OBIT(22); // 0 = CPSR, 1 = SPSR(current)
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
        if (regs.CPSR.mode == M_user)
            mask &= 0xFF000000;
        if (mask & 0xFF)
            imm |= 0x10; // force this bit always
        schedule_IRQ_check();
        regs.CPSR.u = (~mask & regs.CPSR.u) | (imm & mask);
        if (mask & 0x0F) {
            fill_regmap();
        }
    }
    else {
        if ((regs.CPSR.mode != M_user) && (regs.CPSR.mode != M_system)) {
            u32 *v = get_SPSR_by_mode();
            *v = (~mask & *v) | (imm & mask);
        }
        cycles = 2;
    }
    if (cycles) idle(cycles);
    regs.PC += 4;
    pipeline.access = ARM9P_sequential | ARM9P_code;
}

void core::ins_BX(const u32 opcode)
{
    const u32 Rnd = opcode & 15;
    u32 addr = *getR(Rnd);
    regs.CPSR.T = addr & 1;
    addr &= 0xFFFFFFFE;
    regs.PC = addr;
    flush_pipeline();
}


u32 core::TEST(const u32 v, const u32 S)
{
    if (regs.CPSR.T || S) {
        regs.CPSR.N = (v >> 31) & 1;
        regs.CPSR.Z = v == 0;
        regs.CPSR.C = temp_carry;
    }
    return v;
}

u32 core::ADD(const u32 Rnd, const u32 Rmd, const u32 carry, const u32 S)
{
    const u32 result = Rnd + Rmd + carry;
    if (regs.CPSR.T || S) {
        const u32 overflow = ~(Rnd ^ Rmd) & (Rnd ^ result);
        regs.CPSR.V = (overflow >> 31) & 1;
        regs.CPSR.C = ((overflow ^ Rnd ^ Rmd ^ result) >> 31) & 1;
        regs.CPSR.Z = result == 0;
        regs.CPSR.N = (result >> 31) & 1;
    }
    return result;
}


// case 2: v = SUB(Rn, Rm, 1); break;
u32 core::SUB(const u32 Rn, const u32 Rm, const u32 carry, const u32 S)
{
    const u32 iRm = Rm ^ 0xFFFFFFFF;
    return ADD(Rn, iRm, carry, S);
}

u32 core::ALU(const u32 Rn, const u32 Rm, const u32 alu_opcode, const u32 S, u32 *out) {
    switch(alu_opcode) {
        case 0: write_reg(out, TEST(Rn & Rm, S)); break;
        case 1: write_reg(out, TEST(Rn ^ Rm, S)); break;
        case 2: write_reg(out, SUB(Rn, Rm, 1, S)); break;
        case 3: write_reg(out, SUB(Rm, Rn, 1, S)); break;
        case 4: write_reg(out, ADD(Rn, Rm, 0, S)); break;
        case 5: write_reg(out, ADD(Rn, Rm, regs.CPSR.C, S)); break; // TODO: xx HUH?
        case 6: write_reg(out, SUB(Rn, Rm, regs.CPSR.C, S)); break;
        case 7: write_reg(out, SUB(Rm, Rn, regs.CPSR.C, S)); break;
        case 8: TEST(Rn & Rm, S); break;
        case 9: TEST(Rn ^ Rm, S); break;
        case 10: SUB(Rn, Rm, 1, S); break;
        case 11: ADD(Rn, Rm, 0, S); break;
        case 12: write_reg(out, TEST(Rn | Rm, S)); break;
        case 13: write_reg(out, TEST(Rm, S)); break;
        case 14: write_reg(out, TEST(Rn & ~Rm, S)); break;
        case 15: write_reg(out, TEST(Rm ^ 0xFFFFFFFF, S)); break;
        default:
            assert(1==2);
    }
    return *out;
}

// Logical shift left

u32 core::LSL(const u32 v, const u32 amount) {
    temp_carry = regs.CPSR.C;
    if (amount == 0) return v;
    temp_carry = amount > 32 ? 0 : !!(v & 1 << (32 - amount));
    return (amount > 31) ? 0 : (v << amount);
}

// Logical shift right
u32 core::LSR(const u32 v, const u32 amount)
{
    temp_carry = regs.CPSR.C;
    if (amount == 0) return v;
    temp_carry = (amount > 32) ? 0 : !!(v & 1 << (amount - 1));
    return (amount > 31) ? 0 : (v >> amount);
}

// Arithemtic (sign-extend) shift right
u32 core::ASR(const u32 v, const u32 amount)
{
    //   carry = cpsr().c;
    temp_carry = regs.CPSR.C;

    //   if(shift == 0) return source;
    if (amount == 0) return v;

    //   carry = shift > 32 ? source & 1 << 31 : source & 1 << shift - 1;
    temp_carry = (amount >= 32) ? (!!(v & 0x80000000)) : !!(v & (1 << (amount - 1)));

    //   source = shift > 31 ? (i32)source >> 31 : (i32)source >> shift;
    return (amount > 31) ? static_cast<i32>(v) >> 31 : static_cast<i32>(v) >> amount;
}

u32 core::ROR(u32 v, u32 amount)
{
    temp_carry = regs.CPSR.C;
    if (amount == 0) return v;
    amount &= 31;
    if (amount) v = (v << (32 - amount)) | (v >> amount); // ?correct
    temp_carry = !!(v & 1 << 31);
    return v;
}

// Rotate right thru carry
u32 core::RRX(const u32 v)
{
    temp_carry = v & 1;
    return (v >> 1) | (regs.CPSR.C << 31);
}

void core::ins_data_proc_immediate_shift(const u32 opcode)
{
    const u32 alu_opcode = (opcode >> 21) & 15;
    const u32 S = (opcode >> 20) & 1; // set condition codes. 0=no, 1=yes. must be 1 for 8-B
    const u32 Rnd = (opcode >> 16) & 15; // first operand
    const u32 Rdd = (opcode >> 12) & 15; // dest reg.
    const u32 Is = (opcode >> 7) & 31; // shift amount
    const u32 shift_type = (opcode >> 5) & 3; // 0=LSL, 1=LSR, 2=ASR, 3=ROR
    // R(bit4) = 0 for this
    const u32 Rmd = opcode & 15;
    pipeline.access = ARM9P_code | ARM9P_sequential;

    const u32 Rn = *getR(Rnd);
    u32 Rm = *getR(Rmd);
    u32 *Rd = getR(Rdd);
    temp_carry = regs.CPSR.C;
    regs.PC += 4;
    switch(shift_type) {
        case 0: //
            Rm = LSL(Rm, Is);
            break;
        case 1:
            Rm = LSR(Rm, Is ? Is : 32);
            break;
        case 2:
            Rm = ASR(Rm, Is ? Is : 32);
            break;
        case 3:
            Rm = Is ? ROR(Rm, Is) : RRX(Rm);
            break;
        default:
            assert(1==2);
    }

    ALU(Rn, Rm, alu_opcode, S, Rd);
    if ((S==1) && (Rdd == 15)) {
        if (regs.CPSR.mode != M_system) {
            schedule_IRQ_check();
            regs.CPSR.u = *get_SPSR_by_mode();
        }
        fill_regmap();
    }
    if ((Rdd == 15) && ((alu_opcode < 8) || (alu_opcode > 11))) {
       flush_pipeline();
    }
}

void core::ins_data_proc_register_shift(const u32 opcode)
{
    const u32 alu_opcode = (opcode >> 21) & 15;
    const u32 S = (opcode >> 20) & 1; // set condition codes. 0=no, 1=yes. must be 1 for 8-B
    const u32 Rnd = (opcode >> 16) & 15; // first operand
    const u32 Rdd = (opcode >> 12) & 15; // dest reg.
    const u32 Isd = (opcode >> 8) & 15;
    const u32 shift_type = (opcode >> 5) & 3; // 0=LSL, 1=LSR, 2=ASR, 3=ROR
    // R(bit4) = 0 for this
    const u32 Rmd = opcode & 15;

    const u32 Is = (*getR(Isd)) & 0xFF; // shift amount
    idle(1);
    pipeline.access = ARM9P_code | ARM9P_nonsequential; // weird quirk of ARM
    regs.PC += 4;
    const u32 Rn = *getR(Rnd);
    u32 Rm = *getR(Rmd);
    u32 *Rd = getR(Rdd);
    temp_carry = regs.CPSR.C;
    switch(shift_type) {
        case 0: //
            Rm = LSL(Rm, Is);
            break;
        case 1:
            Rm = LSR(Rm, Is);
            break;
        case 2:
            Rm = ASR(Rm, Is);
            break;
        case 3:
            Rm = ROR(Rm, Is);
            break;
    }

    ALU(Rn, Rm, alu_opcode, S, Rd);

    if ((S==1) && (Rdd == 15)) {
        if (regs.CPSR.mode != M_system) {
            schedule_IRQ_check();
            regs.CPSR.u = *get_SPSR_by_mode();
        }
        fill_regmap();
    }
}

void core::ins_undefined_instruction(const u32 opcode)
{
    printf("\nARM9 UNDEFINED INS!");
    assert(1==2);
    regs.R_und[1] = regs.PC - 4;
    regs.SPSR_und = regs.CPSR.u;
    regs.CPSR.mode = M_undefined;
    fill_regmap();
    regs.CPSR.I = 1;
    regs.PC = regs.EBR | 0x00000004;
   flush_pipeline();
}

void core::ins_data_proc_immediate(const u32 opcode)
{
    const u32 alu_opcode = (opcode >> 21) & 15;
    const u32 S = (opcode >> 20) & 1; // set condition codes. 0=no, 1=yes. must be 1 for 8-B
    const u32 Rnd = (opcode >> 16) & 15; // first operand
    const u32 Rdd = (opcode >> 12) & 15; // dest reg.

    const u32 Rn = *getR(Rnd);
    regs.PC += 4;
    u32 *Rd = getR(Rdd);
    pipeline.access = ARM9P_code | ARM9P_sequential;

    u32 Rm = opcode & 0xFF;
    u32 imm_ROR_amount = (opcode >> 7) & 30;
    temp_carry = regs.CPSR.C;
    if (imm_ROR_amount) Rm = ROR(Rm, imm_ROR_amount);

    ALU(Rn, Rm, alu_opcode, S, Rd);
    if ((S==1) && (Rdd == 15)) {
        if (regs.CPSR.mode != M_system) {
            schedule_IRQ_check();
            regs.CPSR.u = *get_SPSR_by_mode();
        }
        fill_regmap();
    }
}

void core::ins_LDR_STR_immediate_offset(const u32 opcode)
{
    const u32 P = OBIT(24); // Pre/post. 0 = after-transfer, post
    const u32 U = OBIT(23); // 0 = down, 1 = up
    const u32 B = OBIT(22); // byte/word, 0 = 32bit, 1 = 8bit
    // when P=0, bit 21 is T, 0= normal, 1= force nonpriviledged, and W=1
    // when P=1, bit 21 is write-back bit, 0 = normal, 1 = write into base
    u32 T = 0; // 0 = normal, 1 = force unprivileged
    u32 W = 1; // writeback. 0 = no, 1 = write into base
    if (P == 0) { T = OBIT(21); }
    else W = OBIT(21);

    const u32 L = OBIT(20); // store = 0, load = 1
    const u32 Rnd = (opcode >> 16) & 15; // base register. PC=+8
    const u32 Rdd = (opcode >> 12) & 15; // source/dest register. PC=+12

    u32 *Rn = getR(Rnd);
    u32 *Rd = getR(Rdd);

    const u32 offset = (opcode & 4095);
    u32 addr = *Rn;
    regs.PC += 4;
    pipeline.access = ARM9P_nonsequential | ARM9P_code;
    //if (Rnd == 15) addr += 4;
    if (P) addr = U ? (addr + offset) : (addr - offset);
    const u8 sz = B ? 1 : 4;
    const u32 mask = B ? 0xFF : 0xFFFFFFFF;
    if (L == 1) {// store to RAM
        u32 v = AREAD(addr, sz);
        if ((addr & 3) && !B) v = align_val(addr, v);
        if (!P) addr = U ? (addr + offset) : (addr - offset);
        if (W) {
            if (Rnd == 15)
                write_reg(Rn, addr+4);
            else
                write_reg(Rn, addr);
        }
        write_reg(Rd, v);
        if (Rdd == 15)
            regs.CPSR.T = regs.PC & 1;
    }
    else {
        AWRITE(addr, sz, *Rd);
        if (!P) addr = U ? (addr + offset) : (addr - offset);
        if (W) {
            if (Rnd == 15)
                write_reg(Rn, addr+4);
            else
                write_reg(Rn, addr);
        }
    }
}

void core::ins_LDR_STR_register_offset(const u32 opcode)
{
    const u32 P = OBIT(24);
    const u32 U = OBIT(23);
    const u32 B = OBIT(22);
    u32 T = 0; // 0 = normal, 1 = force unprivileged
    u32 W = 1; // writeback. 0 = no, 1 = write into base
    if (P == 0) { T = OBIT(21); }
    else W = OBIT(21);
    const u32 L = OBIT(20); // 0 store, 1 load
    const u32 Rnd = (opcode >> 16) & 15; // base reg
    const u32 Rdd = (opcode >> 12) & 15; // source/dest reg
    const u32 Is = (opcode >> 7) & 31;
    const u32 shift_type = (opcode >> 5) & 3;
    const u32 Rmd = opcode & 15;

    u32 *Rn = getR(Rnd);
    u32 Rm = *getR(Rmd);
    u32 *Rd = getR(Rdd);
    temp_carry = regs.CPSR.C;
    switch(shift_type) {
        case 0: //
            Rm = LSL(Rm, Is);
            break;
        case 1:
            Rm = LSR(Rm, Is ? Is : 32);
            break;
        case 2:
            Rm = ASR(Rm, Is ? Is : 32);
            break;
        case 3:
            Rm = Is ? ROR(Rm, Is) : RRX(Rm);
            break;
    }

    u32 addr = *Rn;
    if (P) addr = U ? (addr + Rm) : (addr - Rm);
    const u8 sz = B ? 1 : 4;
    const u32 mask = B ? 0xFF : 0xFFFFFFFF;
    regs.PC += 4;
    pipeline.access = ARM9P_nonsequential | ARM9P_code;
    if (L == 1) { // LDR from RAM
        u32 v = AREAD(addr, sz);
        if ((!B) && (addr & 3)) v = align_val(addr, v);
        if (!P) addr = U ? (addr + Rm) : (addr - Rm);
        if (W)  {
            if (Rnd == 15)
                write_reg(Rn, addr+4);
            else
                write_reg(Rn, addr);
        }
        write_reg(Rd, v);
    }
    else { // STR to RAM
        AWRITE(addr, sz, *Rd);
        if (!P) addr = U ? (addr + Rm) : (addr - Rm);
        if (W) {
            if (Rnd == 15)
                write_reg(Rn, addr+4);
            else
                write_reg(Rn, addr);
        }
    }
}

void core::ins_LDM_STM(const u32 opcode) {
    const u32 P = OBIT(24); // P=0 add offset after. P=1 add offset first
    const u32 U = OBIT(23); // 0=subtract offset, 1 =add
    const u32 S = OBIT(22); // 0=no, 1=load PSR or force user bit
    const u32 W = OBIT(21); // 0=no writeback, 1= writeback
    const u32 L = OBIT(20); // 0=store, 1=load
    const u32 Rnd = (opcode >> 16) & 15;
    const u32 rlist = (opcode & 0xFFFF);

    bool pc_moved = (rlist >> 15) & 1;;

    u32 bytes = 0;
    u32 base_new;
    u32 addr = *getR(Rnd);
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

    regs.PC += 4;
    pipeline.access = ARM9P_code | ARM9P_nonsequential;

    const u32 mode = regs.CPSR.mode;

    if (S && (!L || !pc_moved)) {
        regs.CPSR.mode = M_user;
        fill_regmap();
    }

    u32 i = 0;
    u32 remaining = rlist;
    u32 access = ARM9P_nonsequential;
    while (remaining != 0) {
        while (((remaining >> i) &  1) == 0) i++;

        if (P == U)
            addr += 4;

        if (L) *getR(i) = read(addr, 4, access, true);
        else write(addr, 4, access, *getR(i));

        access = ARM9P_sequential;

        if (P != U)
            addr += 4;

        remaining &= ~(1 << i);
    }

    if (S) {
        if (L && pc_moved) {
            if (regs.CPSR.mode != M_system) {
                schedule_IRQ_check();
                regs.CPSR.u = *get_SPSR_by_mode();
            }
        } else {
            regs.CPSR.mode = mode;
        }
        fill_regmap();
    }

    if (W) {
        if (L) {
            // writeback if base is the only register or *NOT* the "last" register
            if (!Rnd_last || rlist == (1 << Rnd)) {
                *getR(Rnd) = base_new;
            }
        } else {
            *getR(Rnd) = base_new;
        }
    }

    if (L && pc_moved) {
        if ((regs.PC & 1) && !S) {
            regs.CPSR.T = 1;
            regs.PC &= 0xFFFFFFFE;
        }
       flush_pipeline();
    }
}

void core::ins_STC_LDC(const u32 opcode)
{
    printf("\nWARNING STC/LDC");
    regs.PC += 4;
}

void core::ins_CDP(const u32 opcode)
{
    //UNIMPLEMENTED;
}

void core::undefined_exception()
{
    regs.R_und[1] = regs.PC - 4;
    printf("\nWARN: PC MAY BE WRONG");
    regs.SPSR_und = regs.CPSR.u;
    regs.CPSR.mode = M_undefined;
    fill_regmap();

    regs.CPSR.I = 1;
    regs.PC = regs.EBR | 0x00000004;
    flush_pipeline();
}

void core::ins_MCR_MRC(const u32 opcode)
{
    if (regs.CPSR.mode == M_user) {
        undefined_exception();
        return;
    }

    u32 v2 = ((opcode >> 28) & 15) == 15;
    const u32 cp_opc = (opcode >> 21) & 7; // CP Opc - Coprocessor operation code
    const u32 copro_to_arm = !OBIT(20);
    const u32 Cnd = (opcode >> 16) & 15; // Cn     - Coprocessor source/dest. Register  (C0-C15)
    const u32 Rdd = (opcode >> 12) & 15; // Rd     - ARM source/destination Register    (R0-R15)
    const u32 Pnd = (opcode >> 8) & 15; // Coprocessor number                 (P0-P15)
    const u32 CP = (opcode >> 5) & 7; // CP     - Coprocessor information            (0-7)
    const u32 Cmd = opcode & 15; //  Coprocessor operand Register       (C0-C15)
    regs.PC += 4;
    pipeline.access = ARM9P_nonsequential | ARM9P_code;


    if (!copro_to_arm) { // ARM->CoPro
         const u32 val = NDS_CP_read(Pnd, cp_opc, Cnd, Cmd, CP);
         if (Rdd == 15) {
             // When using MRC with R15: Bit 31-28 of data are copied to Bit 31-28 of CPSR (ie. N,Z,C,V flags), other data bits are ignored, CPSR Bit 27-0 are not affected, R15 (PC) is not affected.     */
             schedule_IRQ_check();
             regs.CPSR.u = (regs.CPSR.u & 0x0FFFFFFF) | (val & 0xF0000000);
         }
         else {
             *getR(Rdd) = val;
         }
    }
    else {
        // When using MCR with R15: Coprocessor will receive a data value of PC+12.
        u32 v = *getR(Rdd);
        if (Rdd == 15) v += 4;
        NDS_CP_write(Pnd, cp_opc, Cnd, Cmd, CP, v);
    }
    idle(1);
}

void core::ins_BKPT(const u32 opcode)
{
    if ((opcode >> 28) == 14) { // BKPT
        regs.R_abt[1] = regs.PC - 4;
        regs.SPSR_abt = regs.CPSR.u;
        regs.CPSR.mode = M_abort;
        fill_regmap();
        regs.CPSR.I = 1;
        regs.PC = regs.EBR | 0x0000000C;
        flush_pipeline();
        printf("\nARM9 BKPT!?");
    }
    else {
        ins_undefined_instruction(opcode);
    }
}

void core::ins_SWI(const u32 opcode)
{
    regs.R_svc[1] = regs.PC - 4;
    regs.SPSR_svc = regs.CPSR.u;
    regs.CPSR.mode = M_supervisor;
    fill_regmap();
    regs.CPSR.I = 1;
    regs.PC = regs.EBR | 0x00000008;
    flush_pipeline();
    //printf("\nWARNING SWI %d", opcode & 0xFF);
}

void core::ins_INVALID(const u32 opcode)
{
    //UNIMPLEMENTED;
}

void core::ins_PLD(const u32 opcode)
{
    //UNIMPLEMENTED;
    printf("\nPLD!");
}

void core::ins_SMLAxy(const u32 opcode)
{
    // Passes armwrestler.nds tests!
    const u32 Rdd = (opcode >> 16) & 15;
    const u32 Rnd = (opcode >> 12) & 15;
    const u32 Rsd = (opcode >> 8) & 15;
    const u32 y = OBIT(6); // 1 = RS top, 0 bottom
    const u32 x = OBIT(5); // 1 = Rm top, 0 bottom
    const u32 Rmd = opcode & 15;

    i16 value1, value2;

    if (x) value1 = static_cast<i16>(*getR(Rmd) >> 16);
    else value1 = static_cast<i16>(*getR(Rmd) & 0xFFFF);

    if (y) value2 = static_cast<i16>(*getR(Rsd) >> 16);
    else value2 = static_cast<i16>(*getR(Rsd) & 0xFFFF);

    const u32 first_result = static_cast<u32>(value1 * value2);
    const u32 mop2 = *getR(Rnd);
    const u32 final_result = first_result + mop2;

    if((~(first_result ^ mop2) & (mop2 ^ final_result)) >> 31)
        regs.CPSR.Q = 1;

    *getR(Rdd) = final_result;

    regs.PC += 4;
    pipeline.access = ARM9P_code | ARM9P_sequential;
}

void core::ins_SMLAWy(const u32 opcode)
{
    const u32 Rdd = (opcode >> 16) & 15;
    const u32 Rnd = (opcode >> 12) & 15;
    const u32 Rsd = (opcode >> 8) & 15;
    const u32 y = OBIT(6); // 1 = RS top, 0 bottom
    const u32 Rmd = opcode & 15;

    i32 value1 = static_cast<i32>(*getR(Rmd));
    i16 value2;

    if (y) value2 = static_cast<i16>(*getR(Rsd) >> 16);
    else value2 = static_cast<i16>(*getR(Rsd) & 0xFFFF);

    const u32 first_result = static_cast<u32>((value1 * value2) >> 16);
    const u32 mop2 = *getR(Rnd);
    const u32 final_result = first_result + mop2;

    if((~(first_result ^ mop2) & (mop2 ^ final_result)) >> 31) {
        regs.CPSR.Q = 1;
    }

    *getR(Rdd) = final_result;

    regs.PC += 4;
    pipeline.access = ARM9P_code | ARM9P_sequential;
}

void core::ins_SMULWy(const u32 opcode)
{
    const u32 Rdd = (opcode >> 16) & 15;
    //const u32 Rnd = (opcode >> 12) & 15;
    const u32 Rsd = (opcode >> 8) & 15;
    const u32 y = OBIT(6); // 1 = RS top, 0 bottom
    const u32 Rmd = opcode & 15;

    regs.PC += 4;
    pipeline.access = ARM9P_code | ARM9P_sequential;

    u32 *Rd = getR(Rdd);
    u32 Rs = *getR(Rsd);
    const u32 Rm = *getR(Rmd);

    if (y) Rs >>= 16;
    else Rs &= 0xFFFF;

    const u64 result = (static_cast<i64>(static_cast<i32>(Rm)) * static_cast<i16>(Rs)) >> 16;

    *Rd = result;

    if (Rdd == 15) {
       flush_pipeline();
    }
}

void core::ins_SMLALxy(const u32 opcode)
{
    const u32 Rdd = (opcode >> 16) & 15;
    const u32 Rnd = (opcode >> 12) & 15;
    const u32 Rsd = (opcode >> 8) & 15;
    const u32 y = OBIT(6); // 1 = RS top, 0 bottom
    const u32 x = OBIT(5); // 1 = Rm top, 0 bottom
    const u32 Rmd = opcode & 15;

    regs.PC += 4;
    pipeline.access = ARM9P_code | ARM9P_nonsequential; // this one becomes nonsequential

    u32 *Rd = getR(Rdd);
    u32 Rs = *getR(Rsd);
    u32 Rm = *getR(Rmd);
    u32 *Rn = getR(Rnd);

    if (y) Rs >>= 16;
    else Rs &= 0xFFFF;
    if (x) Rm >>= 16;
    else Rm &= 0xFFFF;

    i64 result = static_cast<i64>(static_cast<i16>(Rm)) * static_cast<i64>(static_cast<i16>(Rs));
    result += static_cast<i64>(static_cast<u64>(*Rn) | (static_cast<u64>(*Rd) << 32));

    *Rn = static_cast<u64>(result) & 0xFFFFFFFF;
    *Rd = static_cast<u64>(result) >> 32;
    if (Rdd == 15) {
       flush_pipeline();
    }
}

void core::ins_SMULxy(const u32 opcode)
{
    const u32 Rdd = (opcode >> 16) & 15;
    const u32 Rsd = (opcode >> 8) & 15;
    const u32 y = OBIT(6); // 1 = RS top, 0 bottom
    const u32 x = OBIT(5); // 1 = Rm top, 0 bottom
    const u32 Rmd = opcode & 15;

    regs.PC += 4;
    pipeline.access = ARM9P_code | ARM9P_nonsequential; // this one becomes nonsequential

    u32 *Rd = getR(Rdd);
    u32 Rs = *getR(Rsd);
    u32 Rm = *getR(Rmd);

    if (y) Rs >>= 16;
    else Rs &= 0xFFFF;
    if (x) Rm >>= 16;
    else Rm &= 0xFFFF;

    *Rd = (static_cast<i16>(Rm) * static_cast<i16>(Rs));
    if (Rdd == 15) {
       flush_pipeline();
    }
}

void core::ins_LDRD_STRD(const u32 opcode)
{
/*
2: LDR{cond}D  Rd,<Address>  ;Load Doubleword  R(d)=[a], R(d+1)=[a+4]
3: STR{cond}D  Rd,<Address>  ;Store Doubleword [a]=R(d), [a+4]=R(d+1)
STRD/LDRD: Rd must be an even numbered register (R0,R2,R4,R6,R8,R10,R12).
STRD/LDRD: Address must be double-word aligned (multiple of eight).
          */
    const u32 P = OBIT(24); // pre or post. 0=post
    const u32 U = OBIT(23); // up/down, 0=down
    const u32 I = OBIT(22); // 0 = register, 1 = immediate offset
    const u32 L = !OBIT(5); // bits 5-6 = opcode. 10 = LDR, 11 = STR. so bit 5 == 1 = STR
    u32 W = 1;
    if (P) W = OBIT(21);

    const u32 Rnd = (opcode >> 16) & 15;
    const u32 Rdd = (opcode >> 12) & 14; // Only use top 3 bits
    u32 imm_off = 0;
    imm_off = ((opcode >> 8) & 15) << 4;
    imm_off |= (opcode & 15);
    const u32 Rmd = opcode & 15; // Offset register
    u32 *Rn = getR(Rnd);
    u32 *Rd = getR(Rdd);
    u32 *Rdp1 = getR(Rdd | 1);
    const u32 Rm = I ? imm_off : *getR(Rmd);
    u32 addr = *Rn;
    if (P) addr = U ? (addr + Rm) : (addr - Rm);
    regs.PC += 4;
    pipeline.access = ARM9P_nonsequential | ARM9P_code;
    if (L) {
        const u32 val = read(addr, 4, ARM9P_nonsequential, true);
        const u32 val_hi = read(addr+4, 4, ARM9P_sequential, true);

        if (!P) addr = U ? (addr + Rm) : (addr - Rm);
        if (W && !((Rnd == 15) && (Rdd == 14))) {
            if (Rnd == 15) // writeback fails. technically invalid here
                write_reg(Rn, addr + 4);
            else
                write_reg(Rn, addr);
        }
        write_reg(Rd, val);
        write_reg(Rdp1, val_hi);
    }
    else {
        write(addr, 4, ARM9P_nonsequential, *Rd);
        write(addr+4, 4, ARM9P_sequential, *Rdp1);
        if (!P) addr = U ? (addr + Rm) : (addr - Rm);
        if (W) {
            if (Rnd == 15) // writeback fails. technically invalid here
                write_reg(Rn, addr + 4);
            else
                write_reg(Rn, addr);
        }
    }
}

void core::ins_CLZ(const u32 opcode)
{
    const u32 Rdd = (opcode >> 12) & 15;
    const u32 Rmd = opcode & 15;

    const u32 v = *getR(Rmd);

    regs.PC += 4;
    pipeline.access = ARM9P_code | ARM9P_sequential;

    *getR(Rdd) = (v == 0) ? 32 : __builtin_clz(v);
    if (Rdd == 15)flush_pipeline();
}

void core::ins_BLX_reg(const u32 opcode)
{
    const u32 link = regs.PC - 4;
    regs.PC = (*getR(opcode & 15));
    *getR(14) = link;
    regs.CPSR.T = regs.PC & 1;
    if (regs.CPSR.T) regs.PC &= 0xFFFFFFFE;
    else regs.PC &= 0xFFFFFFFC;

   flush_pipeline();
}

void core::ins_QADD_QSUB_QDADD_QDSUB(const u32 opcode) {
    const u32 src1 =  opcode & 15;
    const u32 src2 = (opcode >> 16) & 15;
    const u32 dst  = (opcode >> 12) & 15;
    u32 op2  = *getR(src2);

    const u32 subtract = OBIT(21);
    const u32 double_op2 = OBIT(22);

    if(double_op2) {
        u32 result = op2 + op2;

        if((op2 ^ result) >> 31) {
            regs.CPSR.Q = 1;
            result = 0x80000000 - (result >> 31);
        }

        op2 = result;
    }

    if (subtract) {
        const u32 op1 = *getR(src1);
        u32 result = op1 - op2;

        if(((op1 ^ op2) & (op1 ^ result)) >> 31) {
            regs.CPSR.Q = 1;
            result = 0x80000000 - (result >> 31);
        }

        *getR(dst) = result;
    } else {
        const u32 op1 = *getR(src1);
        u32 result = op1 + op2;

        if((~(op1 ^ op2) & (op2 ^ result)) >> 31) {
            regs.CPSR.Q = 1;
            result = 0x80000000 - (result >> 31);
        }
        *getR(dst) = result;
    }

    regs.PC += 4;
    pipeline.access = ARM9P_sequential | ARM9P_code;
}

void core::ins_B_BL(const u32 opcode)
{
    const u32 link = OBIT(24);
    i32 offset = SIGNe24to32(opcode & 0xFFFFFF);
    offset <<= 2;
    if (link) {
        *getR(14) = regs.PC - 4;
    }
    regs.PC += static_cast<u32>(offset);
   flush_pipeline();
}

void core::ins_BLX(const u32 opcode)
{
    i32 offset = SIGNe24to32(opcode & 0xFFFFFF);
    offset <<= 2;
    const i32 H = OBIT(24) << 1;
    offset += H;

    *getR(14) = regs.PC - 4;
    regs.PC += static_cast<u32>(offset);
    regs.CPSR.T = 1;

    flush_pipeline();
}
}