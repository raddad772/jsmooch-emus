//
// Created by . on 2/11/25.
//

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include "helpers/intrinsics.h"
#include "r3000.h"
#include "r3000_instructions.h"
#include "gte.h"

#define BADINS printf("\nUNIMPLEMENTED INSTRUCTION!"); assert(1==2)

// ranch_delay and cop0 default to 0
namespace R3000 {
static constexpr char reg_alias_arr[33][12] = {
        "r0", "at", "v0", "v1",
        "a0", "a1", "a2", "a3",
        "t0", "t1", "t2", "t3",
        "t4", "t5", "t6", "t7",
        "s0", "s1", "s2", "s3",
        "s4", "s5", "s6", "s7",
        "t8", "t9", "k0", "k1",
        "gp", "sp", "fp", "ra",
        "unknown reg"
};

void core::COP_write_reg(u32 COP, u32 num, u32 val)
{
    switch(COP) {
        case 0:
            // TODO: add 1-cycle delay
            regs.COP0[num] = val;
            if (num == 12) {
                if (update_sr) update_sr(update_sr_ptr, this, val);
            }
            return;
        case 2:
            gte.write_reg(num, val);
            return;
        default:
            printf("\nwrite to unimplemented COP %d", COP);
    }
}

u32 core::COP_read_reg(u32 COP, u32 num)
{
    switch(COP) {
        case 0:
            return regs.COP0[num];
        case 2:
            return gte.read_reg(num);
        default:
            printf("\nread from unimplemented COP %d", COP);
            return 0xFF;
    }
}


// default link reg to 31

void core::branch(i64 rel, u32 doit, u32 link, u32 link_reg)
{
    if (doit) {
        if (pipe.current.new_PC != 0xFFFFFFFF)
            pipe.item0.new_PC = pipe.current.new_PC + rel;
        else
            pipe.item0.new_PC = regs.PC + rel;
    }

    if (link)
        fs_reg_write(link_reg, regs.PC+4);
}

void core::jump(u32 new_addr, u32 doit, u32 link, u32 link_reg)
{
    pipe.item0.new_PC = new_addr;

    if (link)
        fs_reg_write(link_reg, regs.PC+4);
}

u32 core::fs_reg_delay_read(i32 target) {
    auto *p = &pipe.current;
    if (p->target == target) {
        p->target = -1;
        return p->value;
    }
    else {
        return regs.R[target];
    }
}
    
void core::fNA(u32 opcode, OPCODE *op)
{
    printf("\nBAD INSTRUCTION %08x AT PC(+4) %08x cycle:%lld", opcode, regs.PC, current_clock());
    dbg_break("BAD INSTRUCTIONN", 0);
}

void core::fSLL(u32 opcode, OPCODE *op)
{
    if (opcode == 0) {
        return;
    }
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;
    u32 imm5 = (opcode >> 6) & 0x1F;

    fs_reg_write(rd, regs.R[rt] << imm5);
}

void core::fSRL(u32 opcode, OPCODE *op)
{
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;
    u32 imm5 = (opcode >> 6) & 0x1F;

    fs_reg_write(rd, regs.R[rt] >> imm5);
}

void core::fSRA(u32 opcode, OPCODE *op)
{
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;
    u32 imm5 = (opcode >> 6) & 0x1F;

    fs_reg_write(rd, static_cast<u32>(static_cast<i32>(regs.R[rt]) >> imm5));
}

void core::fSLLV(u32 opcode, OPCODE *op)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;

    fs_reg_write(rd, regs.R[rt] << (regs.R[rs] & 0x1F));
}

void core::fSRLV(u32 opcode, OPCODE *op)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;

    fs_reg_write(rd, regs.R[rt] >> (regs.R[rs] & 0x1F));
}

void core::fSRAV(u32 opcode, OPCODE *op)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;

    fs_reg_write(rd, static_cast<u32>(static_cast<i32>(regs.R[rt]) >> (regs.R[rs] & 0x1F)));
}

#define DEFAULT_LINKREG 31

void core::fJR(u32 opcode, OPCODE *op)
{
    u32 rs = (opcode >> 21) & 0x1F;
    jump(regs.R[rs], 1, 0, DEFAULT_LINKREG);
}

void core::fJALR(u32 opcode, OPCODE *op)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;
    jump(regs.R[rs], 1, 1, rd);
}

void core::fSYSCALL(u32 opcode, OPCODE *op)
{
    regs.PC -= 4;
    exception(8, 0, 0);
}

void core::fBREAK(u32 opcode, OPCODE *op)
{
    regs.PC -= 4;
    exception(9, 0, 0);
}

void core::wait_for(u64 timecode)
{
    u64 current = *clock + *waitstates;
    if (current < timecode) {
        *waitstates += (timecode - current) - 1;
    }
}

void core::fMFHI(u32 opcode, OPCODE *op)
{
    u32 rd = (opcode >> 11) & 0x1F;

    // TODO: add delay here until multiplier.clock_end
    wait_for(multiplier.clock_end);
    multiplier.finish();
    fs_reg_write(rd, multiplier.hi);
}

void core::fMFLO(u32 opcode, OPCODE *op)
{
    u32 rd = (opcode >> 11) & 0x1F;

    wait_for(multiplier.clock_end);
    multiplier.finish();
    fs_reg_write(rd, multiplier.lo);
}

void core::fMTHI(u32 opcode, OPCODE *op)
{
    u32 rs = (opcode >> 21) & 0x1F;

    // TODO: interrupt multiplier?
    multiplier.hi = regs.R[rs];
}

void core::fMTLO(u32 opcode, OPCODE *op)
{
    u32 rs = (opcode >> 21) & 0x1F;

    // TODO: interrupt multiplier?
    multiplier.lo = regs.R[rs];
}

void core::fMULT(u32 opcode, OPCODE *op)
{
    // SIGNED multiply
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 spd = 0;

    i32 o1 = static_cast<i32>(regs.R[rs]);

    // TODO: make this a little more correct
    if (abs(o1) < 0x800)
        spd = 5;
    else if (abs(o1) < 0x100000)
        spd = 8;
    else
        spd = 12;

    multiplier.set(0, 0, static_cast<u32>(o1), regs.R[rt], 0, spd, current_clock());
}

void core::fMULTU(u32 opcode, OPCODE *op)
{
    // UNSIGNED multiply
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 spd = 0;
    u32 o1 = regs.R[rs];

    // TODO: make this a little more correct
    if (o1 < 0x800)
        spd = 5;
    else if (o1 < 0x100000)
        spd = 8;
    else
        spd = 12;

    multiplier.set(0, 0, o1, regs.R[rt], 1, spd, current_clock());
}

void core::fDIV(u32 opcode, OPCODE *op)
{
    // SIGNED divide
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;

    multiplier.set(0, 0, regs.R[rs], regs.R[rt], 2, 35, current_clock());
}

void core::fDIVU(u32 opcode, OPCODE *op)
{
    // UNSIGNED divide
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;

    multiplier.set(0, 0, regs.R[rs], regs.R[rt], 3, 35, current_clock());
}

void core::fADD(u32 opcode, OPCODE *op)
{
    //   001xxx | rs   | rt   | <--immediate16bit--> | alu-imm
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;
    int r;
    if (sadd_overflow(regs.R[rs], regs.R[rt], &r)) {
        regs.PC -= 4;
        exception(0xC, 0, 0);
        return;
    }
    fs_reg_write(rd, r);

}

void core::fADDU(u32 opcode, OPCODE *op)
{
    //   001xxx | rs   | rt   | <--immediate16bit--> | alu-imm
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;

    fs_reg_write(rd, regs.R[rs] + regs.R[rt]);
}

void core::fSUB(u32 opcode, OPCODE *op)
{
    //   001xxx | rs   | rt   | <--immediate16bit--> | alu-imm
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;
    int r;
    if (ssub_overflow(regs.R[rs], regs.R[rt], &r)) {
        regs.PC -= 4;
        exception(0xC, 0, 0);
        return;
    }

    fs_reg_write(rd, r);
}

void core::fSUBU(u32 opcode, OPCODE *op)
{
    //   001xxx | rs   | rt   | <--immediate16bit--> | alu-imm
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;

    fs_reg_write(rd, regs.R[rs] - static_cast<i32>(regs.R[rt]));
}

void core::fAND(u32 opcode, OPCODE *op)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;

    fs_reg_write(rd, regs.R[rs] & regs.R[rt]);
}

void core::fOR(u32 opcode, OPCODE *op)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;

    fs_reg_write(rd, regs.R[rs] | regs.R[rt]);
}

void core::fXOR(u32 opcode, OPCODE *op)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;

    fs_reg_write(rd, regs.R[rs] ^ regs.R[rt]);
}

void core::fNOR(u32 opcode, OPCODE *op) {
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;

    fs_reg_write(rd, (regs.R[rs] | regs.R[rt]) ^ 0xFFFFFFFF);
}

void core::fSLT(u32 opcode, OPCODE *op)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;

    fs_reg_write(rd, +(static_cast<i32>(regs.R[rs]) < static_cast<i32>(regs.R[rt])));
}

void core::fSLTU(u32 opcode, OPCODE *op)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;

    fs_reg_write(rd, +(regs.R[rs] < regs.R[rt]));
}

void core::fBcondZ(u32 opcode, OPCODE *op) {
    u32 rs = (opcode >> 21) & 0x1F;
    u32 w = (opcode >> 16) & 0x1F;
    if ((w < 0x10) || (w > 0x11)) w &= 1;
    i32 imm = opcode & 0xFFFF;
    imm = SIGNe16to32(imm);
    u32 take = regs.R[rs] >> 31;
    switch (w) {
        case 0: // BLTZ
            break;
        case 1: // BGEZ
            take ^= 1;
            break;
        case 0x10: // BLTZAL
            fs_reg_write(31, regs.PC + 4);
            break;
        case 0x11: // BGEZAL
            take ^= 1;
            fs_reg_write(31, regs.PC + 4);
            break;
        default:
            printf("\nBad B..Z instruction! %08x", opcode);
            return;
    }
    branch(imm * 4, take, 0, DEFAULT_LINKREG);
}

void core::fJ(u32 opcode, OPCODE *op)
{
//  00001x | <---------immediate26bit---------> | j/jal
    jump((regs.PC & 0xF0000000) + ((opcode & 0x3FFFFFF) << 2), 1, 0, DEFAULT_LINKREG);
}

void core::fJAL(u32 opcode, OPCODE *op)
{

//  00001x | <---------immediate26bit---------> | j/jal
    jump((regs.PC & 0xF0000000) + ((opcode & 0x3FFFFFF) << 2), 1, 1, DEFAULT_LINKREG);
}

void core::fBEQ(u32 opcode, OPCODE *op)
{
    branch(
                 (static_cast<u32>(static_cast<i16>(opcode & 0xFFFF))*4),
                 regs.R[(opcode >> 21) & 0x1F] == regs.R[(opcode >> 16) & 0x1F],
                 0, DEFAULT_LINKREG);
}

void core::fBNE(u32 opcode, OPCODE *op)
{
    // 00010x | rs   | rt   | <--immediate16bit--> |
    branch(
                 (static_cast<u32>(static_cast<i16>(opcode & 0xFFFF))*4),
                 regs.R[(opcode >> 21) & 0x1F] != regs.R[(opcode >> 16) & 0x1F],
                 0, DEFAULT_LINKREG);
}

void core::fBLEZ(u32 opcode, OPCODE *op)
{
    // 00010x | rs   | rt   | <--immediate16bit--> |
    branch(
                 (static_cast<u32>(static_cast<i16>(opcode & 0xFFFF))*4),
                 static_cast<i32>(regs.R[(opcode >> 21) & 0x1F]) <= 0,
                 0, DEFAULT_LINKREG);
}

void core::fBGTZ(u32 opcode, OPCODE *op)
{
    // 00010x | rs   | rt   | <--immediate16bit--> |
    branch(
                 (static_cast<u32>(static_cast<i16>(opcode & 0xFFFF))*4),
                 static_cast<i32>(regs.R[(opcode >> 21) & 0x1F])  > 0,
                 0, DEFAULT_LINKREG);
}

void core::fADDI(u32 opcode, OPCODE *op)
{
    //   001xxx | rs   | rt   | <--immediate16bit--> | alu-imm
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm = opcode & 0xFFFF;
    imm = SIGNe16to32(imm);
    int r;
    if (sadd_overflow(regs.R[rs], imm, &r)) {
        regs.PC -= 4;
        exception(0xC, 0, 0);
        return;
    }

    // TODO: add overflow trap
    fs_reg_write(rt, regs.R[rs] + imm);
}

void core::fADDIU(u32 opcode, OPCODE *op) {
    //   001xxx | rs   | rt   | <--immediate16bit--> | alu-imm
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm = static_cast<u32>(static_cast<i16>(opcode & 0xFFFF));

    fs_reg_write(rt, regs.R[rs] + (u32)imm);
}

void core::fSLTI(u32 opcode, OPCODE *op)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    i32 imm16 = static_cast<i16>(opcode & 0xFFFF); // sign-extend

    fs_reg_write(rt, +(static_cast<i32>(regs.R[rs]) < imm16));
}

void core::fSLTIU(u32 opcode, OPCODE *op)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = static_cast<u32>(static_cast<i16>(opcode & 0xFFFF)); // sign-extend

    // unary operator converts to 0/1
    fs_reg_write(rt, +(regs.R[rs] < imm16));
}

void core::fANDI(u32 opcode, OPCODE *op)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = opcode & 0xFFFF;

    fs_reg_write(rt, imm16 & regs.R[rs]);
}

void core::fORI(u32 opcode, OPCODE *op)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = opcode & 0xFFFF;

    fs_reg_write(rt, imm16 | regs.R[rs]);
}

void core::fXORI(u32 opcode, OPCODE *op)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = opcode & 0xFFFF;

    fs_reg_write(rt, imm16 ^ regs.R[rs]);
}

void core::fLUI(u32 opcode, OPCODE *op)
{
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = opcode & 0xFFFF;

    fs_reg_write(rt, imm16 << 16);
}

void core::fMFC(u32 opcode, OPCODE *op, u32 copnum)
{
    // move FROM co
    // rt = cop[rd]
    *clock += 6;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;
    fs_reg_delay(rt, COP_read_reg(copnum, rd));
}

void core::fMTC(u32 opcode, OPCODE *op, u32 copnum)
{
    // move TO co
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;
    // cop[rd] = reg[rt]
    COP_write_reg(copnum, rd, regs.R[rt]);
}

void core::fCOP0_RFE(u32 opcode, OPCODE *op)
{
    // move FROM co
    // rt = cop[rd]
    // The RFE opcode moves some bits in cop0r12 (SR): bit2-3 are copied to bit0-1, all other bits (including bit4-5) are left unchanged.
    u32 r12 = COP_read_reg(0, RCR_SR);
    // bit4-5 are copied to bit2-3
    u32 b23 = (r12 >> 2) & 0x0C; // Move from 4-5 to 2-3
    u32 b01 = (r12 >> 2) & 3; // Move from 2-3 to 0-1
    COP_write_reg(0, 12, (r12 & 0xFFFFFFF0) | b01 | b23);
    if (update_sr) update_sr(update_sr_ptr, this, regs.COP0[RCR_SR]);
    if (dbg.dvptr) {
        if (dbg.dvptr->id_break[trace.rfe_id]) {
            dbg_break("RFE!", *clock);
        }
        if (dbg.dvptr->ids_enabled[trace.rfe_id]) {
            trace.str.quickempty();
            trace.str.sprintf("RFE");
            dbg.dvptr->add_printf(trace.rfe_id, *clock, DBGLS_TRACE, "%s", trace.str.ptr);
        }
    }
}

void core::fCOP(u32 opcode, OPCODE *op)
{
    u32 copnum = (opcode >> 26) & 3;

    // Opcode 0x10 is cop0, then just take the value of rs. 0 is mfc0, 4 is mtc0, and 0x10 is rfe
    u32 opc = (opcode >> 26) & 0x1F;
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;
    // Opcode 0x10 is cop0, then just take the value of rs. 0 is mfc0, 4 is mtc0, and 0x10 is rfe

    if (copnum == 0) {
        // Cop0 decoding easy
        if (opc == 0x10) {
            switch (rs) {
                case 0:
                    fMFC(opcode, op, 0);
                    return;
                case 4:
                    fMTC(opcode, op, 0);
                    return;
                case 0x10:
                    fCOP0_RFE(opcode, op);
                    return;
            }
        }
        printf("\nBAD COP0 INSTRUCTION! %08x", opcode);
    }
    else {
        if (copnum != 2) {
            printf("\nBAD COP INS? %08x", opcode);
            exception(8, 0, 0);
            return;
        }
        if (opcode & 0x2000000) {
            gte.command(opcode, current_clock());
            return;
        }
        u32 bits5 = (opcode >> 21) & 0x1F;
        u32 low6 = (opcode & 0x3F);
        if (low6 != 0) { // TLBxxx, RFE (which is only COP0?)
            printf("\nBAD COP INSTRUCTION3 %08x", opcode);
            return;
        }
        switch(bits5) {
            case 0: // MFCn rt = dat
                if (rt != 0) regs.R[rt] = gte.read_reg(rd);
                return;
            case 2: // CFCn rt = cnt
                if (rt != 0) regs.R[rt] = gte.read_reg(rd+32);
                return;
            case 4: // MTCn  dat = rt
                gte.write_reg(rd, regs.R[rt]);
                return;
            case 6: // CTCn  cnt = rt
                gte.write_reg(rd+32, regs.R[rt]);
                return;
            default:
                printf("\nUNKNOWN COP INSTRUCTION %08x", opcode);
        }
    }
}

void core::fLB(u32 opcode, OPCODE *op)
{
    //lb  rt,imm(rs)    rt=[imm+rs]  ;byte sign-extended
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = static_cast<u32>(static_cast<i16>(opcode & 0xFFFF));
    u32 addr = regs.R[rs] + imm16;

    u32 rd = read(read_ptr, addr, 1, 1) & 0xFF;
    rd = SIGNe8to32(rd);
    fs_reg_delay(rt, rd);
}

void core::fLH(u32 opcode, OPCODE *op)
{
    //lb  rt,imm(rs)    rt=[imm+rs]  ;byte sign-extended
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = static_cast<u32>(static_cast<i16>(opcode & 0xFFFF));
    u32 addr = regs.R[rs] + imm16;

    if (addr & 1) {
        regs.PC -= 4;
        exception(4, 0, 0);
        return;
    }

    u32 rd = read(read_ptr, addr, 2, 1) & 0xFFFF;
    rd = SIGNe16to32(rd);

    //rd = (u32)((rd << 16) >> 16);
    fs_reg_delay(rt, rd);
}

void core::fLWL(u32 opcode, OPCODE *op)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = static_cast<u32>(static_cast<i16>(opcode & 0xFFFF));
    u32 addr = regs.R[rs] + imm16;

    // Fetch register from delay if it's there, and also clobber it
    u32 cur_v = fs_reg_delay_read(rt);

    u32 aligned_addr = addr & 0xFFFFFFFC;
    u32 aligned_word = read(read_ptr, aligned_addr, 4, 1);
    u32 fv = 0;
    switch(addr & 3) {
        case 0:
            fv = ((cur_v & 0x00FFFFFF) | (aligned_word << 24));
            break;
        case 1:
            fv = ((cur_v & 0x0000FFFF) | (aligned_word << 16));
            break;
        case 2:
            fv = ((cur_v & 0x000000FF) | (aligned_word << 8));
            break;
        case 3:
            fv = aligned_word;
            break;
    }
    fs_reg_delay(rt, fv);
}

void core::fLW(u32 opcode, OPCODE *op)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = static_cast<u32>(static_cast<i16>(opcode & 0xFFFF));
    u32 addr = regs.R[rs] + imm16;

    if (addr & 3) {
        regs.PC -= 4;
        exception(4, 0, 0);
        return;
    }

    //printf("\nLW imm16:%04x addr:%08x", imm16, addr);
    fs_reg_delay(rt, read(read_ptr, addr, 4, 1));
}

void core::fLBU(u32 opcode, OPCODE *op)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = static_cast<u32>(static_cast<i16>(opcode & 0xFFFF));
    u32 addr = regs.R[rs] + imm16;

    u32 rd = read(read_ptr, addr, 1, 1);
    fs_reg_delay(rt, rd&0xFF);
}

void core::fLHU(u32 opcode, OPCODE *op)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = static_cast<u32>(static_cast<i16>(opcode & 0xFFFF));
    u32 addr = regs.R[rs] + imm16;
    if (addr & 1) {
        regs.PC -= 4;
        exception(4, 0, 0);
        return;
    }

    u32 rd = read(read_ptr, addr, 2, 1);
    fs_reg_delay(rt, rd&0xFFFF);
}

void core::fLWR(u32 opcode, OPCODE *op)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = static_cast<u32>(static_cast<i16>(opcode & 0xFFFF));
    u32 addr = regs.R[rs] + imm16;

    // Fetch register from delay if it's there, and also clobber it
    u32 cur_v = fs_reg_delay_read(rt);

    u32 aligned_addr = addr & 0xFFFFFFFC;
    u32 aligned_word = read(read_ptr, aligned_addr, 4, 1);
    u32 fv = 0;
    switch(addr & 3) {
        case 0:
            fv = aligned_word;
            break;
        case 1:
            fv = ((cur_v & 0xFF000000) | (aligned_word >> 8));
            break;
        case 2:
            fv = ((cur_v & 0xFFFF0000) | (aligned_word >> 16));
            break;
        case 3:
            fv = ((cur_v & 0xFFFFFF00) | (aligned_word >> 24));
            break;
    }
    fs_reg_delay(rt, fv);
}

void core::fSB(u32 opcode, OPCODE *op)
{
    //lb  rt,imm(rs)    rt=[imm+rs]  ;byte sign-extended
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = static_cast<u32>(static_cast<i16>(opcode & 0xFFFF));
    u32 addr = regs.R[rs] + imm16;

    write(write_ptr, addr, 1, regs.R[rt]);
}

void core::fSH(u32 opcode, OPCODE *op)
{
    //lb  rt,imm(rs)    rt=[imm+rs]  ;byte sign-extended
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = static_cast<u32>(static_cast<i16>(opcode & 0xFFFF));
    u32 addr = regs.R[rs] + imm16;
    if (addr & 1) {
        regs.PC -= 4;
        exception(5, 0, 0);
        return;
    }

    write(write_ptr, addr, 2, regs.R[rt]);
}

void core::fSWL(u32 opcode, OPCODE *op)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = static_cast<u32>(static_cast<i16>(opcode & 0xFFFF));
    u32 addr = regs.R[rs] + imm16;

    u32 v = regs.R[rt];

    u32 aligned_addr = (addr & 0xFFFFFFFC)>>0;
    u32 cur_mem = read(read_ptr, aligned_addr, 4, 1);

    switch(addr & 3) {
        case 0: // upper 8
            cur_mem = ((cur_mem & 0xFFFFFF00) | (v >> 24));
            break;
        case 1:
            cur_mem = ((cur_mem & 0xFFFF0000) | (v >> 16));
            break;
        case 2:
            cur_mem = ((cur_mem & 0xFF000000) | (v >> 8));
            break;
        case 3:
            cur_mem = v;
            break;
    }
    write(write_ptr, aligned_addr, 4, cur_mem);
}

void core::fSW(u32 opcode, OPCODE *op)
{
    //lb  rt,imm(rs)    rt=[imm+rs]  ;byte sign-extended
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = static_cast<u32>(static_cast<i16>(opcode & 0xFFFF));
    u32 addr = regs.R[rs] + imm16;

    if (addr & 3) {
        regs.PC -= 4;
        exception(5, 0, 0);
        return;
    }

    write(write_ptr, addr, 4, regs.R[rt]);
}

void core::fSWR(u32 opcode, OPCODE *op)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = static_cast<u32>(static_cast<i16>(opcode & 0xFFFF));
    u32 addr = regs.R[rs] + imm16;
    u32 v = regs.R[rt];

    u32 aligned_addr = (addr & 0xFFFFFFFC)>>0;
    u32 cur_mem = read(read_ptr, aligned_addr, 4, 1);

    switch(addr & 3) {
        case 0: // upper 8
            cur_mem = v;
            break;
        case 1:
            cur_mem = ((cur_mem & 0x000000FF) | (v << 8));
            break;
        case 2:
            cur_mem = ((cur_mem & 0x0000FFFF) | (v << 16));
            break;
        case 3:
            cur_mem = ((cur_mem & 0x00FFFFFF) | (v << 24));
            break;
    }
    write(write_ptr, aligned_addr, 4, cur_mem);
}

void core::fLWC(u32 opcode, OPCODE *op)
{
    // ;cop#dat_rt = [rs+imm]  ;word
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = static_cast<u32>(static_cast<i16>(opcode & 0xFFFF));
    u32 addr = regs.R[rs] + imm16;

    u32 rd = read(read_ptr, addr, 4, 1);
    // TODO: add the 1-cycle delay to this
    COP_write_reg(op->arg, rt, rd);
    idle(1);
}

void core::fSWC(u32 opcode, OPCODE *op)
{
    // ;cop#dat_rt = [rs+imm]  ;word
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = static_cast<u32>(static_cast<i16>(opcode & 0xFFFF));
    u32 addr = regs.R[rs] + imm16;
    u32 rd;
    switch(op->arg) {
        case 0:
            rd = regs.COP0[rt];
            break;
        case 2:
            rd = gte.read_reg(rt);
            break;
        default:
            exception(0x0B, 0, 0);
            return;
    }

    // TODO: add the 1-cycle delay to this
    write(write_ptr, addr, 4, rd);
}
}