//
// Created by . on 2/11/25.
//

#include <assert.h>
#include <printf.h>
#include <stdlib.h>
#include "r3000.h"
#include "r3000_instructions.h"
#include "gte.h"

#define BADINS printf("\nUNIMPLEMENTED INSTRUCTION!"); assert(1==2)

// ranch_delay and cop0 default to 0

static const char reg_alias_arr[33][12] = {
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

static inline u64 R3000_current_clock(struct R3000 *core)
{
    return *core->clock + *core->waitstates;
}

static void COP_write_reg(struct R3000 *core, u32 COP, u32 num, u32 val)
{
    switch(COP) {
        case 0:
            // TODO: add 1-cycle delay
            core->regs.COP0[num] = val;
            if (num == 12) {
                if (core->update_sr) core->update_sr(core->update_sr_ptr, core, val);
            }
            return;
        case 2:
            GTE_write_reg(&core->gte, num, val);
            return;
        default:
            printf("\nwrite to unimplemented COP %d", COP);
    }
}

static u32 COP_read_reg(struct R3000 *core, u32 COP, u32 num)
{
    switch(COP) {
        case 0:
            return core->regs.COP0[num];
        case 2:
            return GTE_read_reg(&core->gte, num);
        default:
            printf("\nread from unimplemented COP %d", COP);
            return 0xFF;
    }
}


// default link reg to 31
static inline void R3000_fs_reg_write(struct R3000 *core, u32 target, u32 value)
{
    if (target != 0) core->regs.R[target] = value;

    //if (core->trace_on) core->debug_reg_list.push(target);

    // cancel in-pipeline write to register
    struct R3000_pipeline_item *p = &core->pipe.current;
    if (p->target == target) p->target = -1;
}

static inline void R3000_branch(struct R3000 *core, u32 new_addr, u32 doit, u32 link, u32 link_reg)
{
    if (doit)
        core->pipe.item0.new_PC = new_addr;

    if (link)
        R3000_fs_reg_write(core, link_reg, core->regs.PC+4);
}

static inline u32 R3000_fs_reg_delay_read(struct R3000 *core, i32 target) {
    struct R3000_pipeline_item *p = &core->pipe.current;
    if (p->target == target) {
        printf("\nLoad shortcut %s %08x", reg_alias_arr[p->target], p->value);
        p->target = -1;
        return p->value;
    }
    else {
        return core->regs.R[target];
    }
}

static inline void R3000_fs_reg_delay(struct R3000 *core, i32 target, u32 value)
{
    struct R3000_pipeline_item *p = &core->pipe.item0;
    p->target = target;
    p->value = value;
}


void R3000_fNA(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    printf("\nBAD INSTRUCTION %08x AT PC(+4) %08x cycle:%lld", opcode, core->regs.PC, R3000_current_clock(core));
}

void R3000_fSLL(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    if (opcode == 0) {
        return;
    }
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;
    u32 imm5 = (opcode >> 6) & 0x1F;

    R3000_fs_reg_write(core, rd, core->regs.R[rt] << imm5);
}

void R3000_fSRL(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;
    u32 imm5 = (opcode >> 6) & 0x1F;

    R3000_fs_reg_write(core, rd, core->regs.R[rt] >> imm5);
}

void R3000_fSRA(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;
    u32 imm5 = (opcode >> 6) & 0x1F;

    R3000_fs_reg_write(core, rd, (u32)(((i32)core->regs.R[rt]) >> imm5));
}

void R3000_fSLLV(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;

    R3000_fs_reg_write(core, rd, core->regs.R[rt] << (core->regs.R[rs] & 0x1F));
}

void R3000_fSRLV(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;

    R3000_fs_reg_write(core, rd, core->regs.R[rt] >> (core->regs.R[rs] & 0x1F));
}

void R3000_fSRAV(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;

    R3000_fs_reg_write(core, rd, (u32)(((i32)core->regs.R[rt]) >> (core->regs.R[rs] & 0x1F)));
}

#define DEFAULT_LINKREG 31

void R3000_fJR(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 a = core->regs.R[rs];
    R3000_branch(core, a, 1, 0, DEFAULT_LINKREG);
}

void R3000_fJALR(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;
    u32 a = core->regs.R[rs];
    if ((a & 3) != 0) {
        printf("\nADDRESS EXCEPTION2, HANDLE?");
    }
    R3000_branch(core, a, 1, 1, rd);
}

void R3000_fSYSCALL(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    R3000_exception(core, 8, 0, 0);
}

void R3000_fBREAK(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    printf("\nWARNING FIX BREAK!");
    R3000_exception(core, 8, 0, 0);
}

static void wait_for(struct R3000 *core, u64 timecode)
{
    u64 current = *core->clock + *core->waitstates;
    if (current < timecode) {
        *core->waitstates += (timecode - current) - 1;
    }
}

void R3000_fMFHI(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rd = (opcode >> 11) & 0x1F;

    // TODO: add delay here until core->multiplier.clock_end
    wait_for(core, core->multiplier.clock_end);
    R3000_multiplier_finish(&core->multiplier);
    R3000_fs_reg_write(core, rd, core->multiplier.hi);
}

void R3000_fMFLO(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rd = (opcode >> 11) & 0x1F;

    wait_for(core, core->multiplier.clock_end);
    R3000_multiplier_finish(&core->multiplier);
    R3000_fs_reg_write(core, rd, core->multiplier.lo);
}

void R3000_fMTHI(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rs = (opcode >> 21) & 0x1F;

    // TODO: interrupt multiplier?
    core->multiplier.hi = core->regs.R[rs];
}

void R3000_fMTLO(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rs = (opcode >> 21) & 0x1F;

    // TODO: interrupt multiplier?
    core->multiplier.lo = core->regs.R[rs];
}

void R3000_fMULT(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    // SIGNED multiply
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 spd = 0;

    i32 o1 = (i32)core->regs.R[rs];

    // TODO: make this a little more correct
    if (abs(o1) < 0x800)
        spd = 5;
    else if (abs(o1) < 0x100000)
        spd = 8;
    else
        spd = 12;

    R3000_multiplier_set(&core->multiplier, 0, 0, (u32)o1, core->regs.R[rt], 0, spd, R3000_current_clock(core));
}

void R3000_fMULTU(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    // UNSIGNED multiply
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 spd = 0;
    u32 o1 = core->regs.R[rs];

    // TODO: make this a little more correct
    if (o1 < 0x800)
        spd = 5;
    else if (o1 < 0x100000)
        spd = 8;
    else
        spd = 12;

    R3000_multiplier_set(&core->multiplier, 0, 0, o1, core->regs.R[rt], 1, spd, R3000_current_clock(core));
}

void R3000_fDIV(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    // SIGNED divide
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;

    R3000_multiplier_set(&core->multiplier, 0, 0, core->regs.R[rs], core->regs.R[rt], 2, 35, R3000_current_clock(core));
}

void R3000_fDIVU(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    // UNSIGNED divide
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;

    R3000_multiplier_set(&core->multiplier, 0, 0, core->regs.R[rs], core->regs.R[rt], 3, 35, R3000_current_clock(core));
}

void R3000_fADD(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    //   001xxx | rs   | rt   | <--immediate16bit--> | alu-imm
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;

    // TODO: add overflow trap
    R3000_fs_reg_write(core, rd, core->regs.R[rs] + core->regs.R[rt]);

}

void R3000_fADDU(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    //   001xxx | rs   | rt   | <--immediate16bit--> | alu-imm
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;

    R3000_fs_reg_write(core, rd, core->regs.R[rs] + core->regs.R[rt]);
}

void R3000_fSUB(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    //   001xxx | rs   | rt   | <--immediate16bit--> | alu-imm
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;

    // TODO: add overflow trap
    R3000_fs_reg_write(core, rd, core->regs.R[rs] - core->regs.R[rt]);
}

void R3000_fSUBU(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    //   001xxx | rs   | rt   | <--immediate16bit--> | alu-imm
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;

    R3000_fs_reg_write(core, rd, core->regs.R[rs] - (i32)core->regs.R[rt]);
}

void R3000_fAND(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;

    R3000_fs_reg_write(core, rd, core->regs.R[rs] & core->regs.R[rt]);
}

void R3000_fOR(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;

    R3000_fs_reg_write(core, rd, core->regs.R[rs] | core->regs.R[rt]);
}

void R3000_fXOR(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;

    R3000_fs_reg_write(core, rd, core->regs.R[rs] ^ core->regs.R[rt]);
}

void R3000_fNOR(u32 opcode, struct R3000_opcode *op, struct R3000 *core) {
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;

    R3000_fs_reg_write(core, rd, (core->regs.R[rs] | core->regs.R[rt]) ^ 0xFFFFFFFF);
}

void R3000_fSLT(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;

    R3000_fs_reg_write(core, rd, +(((i32)core->regs.R[rs]) < ((i32)core->regs.R[rt])));
}

void R3000_fSLTU(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;

    R3000_fs_reg_write(core, rd, +(core->regs.R[rs] < core->regs.R[rt]));
}

void R3000_fBcondZ(u32 opcode, struct R3000_opcode *op, struct R3000 *core) {
    /*
     000001 | rs   | 00000| <--immediate16bit--> | bltz
     000001 | rs   | 00001| <--immediate16bit--> | bgez
     000001 | rs   | 10000| <--immediate16bit--> | bltzal
     000001 | rs   | 10001| <--immediate16bit--> | bgezal
    */
    u32 rs = (opcode >> 21) & 0x1F;
    u32 w = (opcode >> 16) & 0x1F;
    i32 imm = opcode & 0xFFFF;
    imm = SIGNe16to32(imm);
    u32 take = false;
    switch (w) {
        case 0: // BLTZ
            take = ((i32) core->regs.R[rs]) < 0;
            break;
        case 1: // BGEZ
            take = ((i32) core->regs.R[rs]) >= 0;
            break;
        case 0x10: // BLTZAL
            take = ((i32) core->regs.R[rs]) < 0;
            R3000_fs_reg_write(core, 31, core->regs.PC + 4);
            break;
        case 0x11: // BGEZAL
            take = ((i32) core->regs.R[rs]) >= 0;
            R3000_fs_reg_write(core, 31, core->regs.PC + 4);
            break;
        default:
            printf("Bad B..Z instruction! %08x", opcode);
            return;
    }
    R3000_branch(core, core->regs.PC + (imm * 4), take, 0, DEFAULT_LINKREG);
}

void R3000_fJ(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
//  00001x | <---------immediate26bit---------> | j/jal
    R3000_branch(core, (core->regs.PC & 0xF0000000) + ((opcode & 0x3FFFFFF) << 2), 1, 0, DEFAULT_LINKREG);
}

void R3000_fJAL(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{

//  00001x | <---------immediate26bit---------> | j/jal
    R3000_branch(core, (core->regs.PC & 0xF0000000) + ((opcode & 0x3FFFFFF) << 2), 1, 1, DEFAULT_LINKREG);
}

void R3000_fBEQ(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    R3000_branch(core,
                 core->regs.PC + ((u32)((i16)(opcode & 0xFFFF))*4),
                 core->regs.R[(opcode >> 21) & 0x1F] == core->regs.R[(opcode >> 16) & 0x1F],
                 0, DEFAULT_LINKREG);
}

void R3000_fBNE(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    // 00010x | rs   | rt   | <--immediate16bit--> |
    R3000_branch(core,
                 core->regs.PC + ((u32)((i16)(opcode & 0xFFFF))*4),
                 core->regs.R[(opcode >> 21) & 0x1F] != core->regs.R[(opcode >> 16) & 0x1F],
                 0, DEFAULT_LINKREG);
}

void R3000_fBLEZ(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    // 00010x | rs   | rt   | <--immediate16bit--> |
    R3000_branch(core,
                 core->regs.PC + ((u32)((i16)(opcode & 0xFFFF))*4),
                 ((i32)core->regs.R[(opcode >> 21) & 0x1F]) <= 0,
                 0, DEFAULT_LINKREG);
}

void R3000_fBGTZ(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    // 00010x | rs   | rt   | <--immediate16bit--> |
    R3000_branch(core,
                 core->regs.PC + ((u32)((i16)(opcode & 0xFFFF))*4),
                 ((i32)core->regs.R[(opcode >> 21) & 0x1F])  > 0,
                 0, DEFAULT_LINKREG);
}

void R3000_fADDI(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    //   001xxx | rs   | rt   | <--immediate16bit--> | alu-imm
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm = opcode & 0xFFFF;
    imm = SIGNe16to32(imm);

    // TODO: add overflow trap
    R3000_fs_reg_write(core, rt, core->regs.R[rs] + imm);
}

void R3000_fADDIU(u32 opcode, struct R3000_opcode *op, struct R3000 *core) {
    //   001xxx | rs   | rt   | <--immediate16bit--> | alu-imm
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm = (u32)((i16)(opcode & 0xFFFF));

    R3000_fs_reg_write(core, rt, core->regs.R[rs] + (u32)imm);
}

void R3000_fSLTI(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    i32 imm16 = ((i16)(opcode & 0xFFFF)); // sign-extend

    R3000_fs_reg_write(core, rt, +((i32)core->regs.R[rs] < imm16));
}

void R3000_fSLTIU(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = (u32)((i16)(opcode & 0xFFFF)); // sign-extend

    // unary operator converts to 0/1
    R3000_fs_reg_write(core, rt, +(core->regs.R[rs] < imm16));
}

void R3000_fANDI(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = opcode & 0xFFFF;

    R3000_fs_reg_write(core, rt, imm16 & core->regs.R[rs]);
}

void R3000_fORI(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = opcode & 0xFFFF;

    R3000_fs_reg_write(core, rt, imm16 | core->regs.R[rs]);
}

void R3000_fXORI(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = opcode & 0xFFFF;

    R3000_fs_reg_write(core, rt, imm16 ^ core->regs.R[rs]);
}

void R3000_fLUI(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = opcode & 0xFFFF;

    R3000_fs_reg_write(core, rt, imm16 << 16);
}

static void R3000_fMFC(u32 opcode, struct R3000_opcode *op, struct R3000 *core, u32 copnum)
{
    // move FROM co
    // rt = cop[rd]
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;
    R3000_fs_reg_delay(core, rt, COP_read_reg(core, copnum, rd));
}

static void R3000_fMTC(u32 opcode, struct R3000_opcode *op, struct R3000 *core, u32 copnum)
{
    // move TO co
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;
    // cop[rd] = reg[rt]
    COP_write_reg(core, copnum, rd, core->regs.R[rt]);
}

static void R3000_fCOP0_RFE(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    // move FROM co
    // rt = cop[rd]
    // The RFE opcode moves some bits in cop0r12 (SR): bit2-3 are copied to bit0-1, all other bits (including bit4-5) are left unchanged.
    u32 r12 = COP_read_reg(core, 0, RCR_SR);
    // bit4-5 are copied to bit2-3
    u32 b23 = (r12 >> 2) & 0x0C; // Move from 4-5 to 2-3
    u32 b01 = (r12 >> 2) & 3; // Move from 2-3 to 0-1
    COP_write_reg(core, 0, 12, (r12 & 0xFFFFFFF0) | b01 | b23);
    if (core->update_sr) core->update_sr(core->update_sr_ptr, core, core->regs.COP0[RCR_SR]);
}

void R3000_fCOP(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
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
                    R3000_fMFC(opcode, op, core, 0);
                    return;
                case 4:
                    R3000_fMTC(opcode, op, core, 0);
                    return;
                case 0x10:
                    R3000_fCOP0_RFE(opcode, op, core);
                    return;
            }
        }
        printf("\nBAD COP0 INSTRUCTION! %08x", opcode);
    }
    else {
        if (copnum != 2) {
            printf("\nBAD COP INS? %08x", opcode);
            return;
        }
        if (opcode & 0x2000000) {
            GTE_command(&core->gte, opcode, R3000_current_clock(core));
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
                if (rt != 0) core->regs.R[rt] = GTE_read_reg(&core->gte, rd);
                return;
            case 2: // CFCn rt = cnt
                if (rt != 0) core->regs.R[rt] = GTE_read_reg(&core->gte, rd+32);
                return;
            case 4: // MTCn  dat = rt
                GTE_write_reg(&core->gte, rd, core->regs.R[rt]);
                return;
            case 6: // CTCn  cnt = rt
                GTE_write_reg(&core->gte, rd+32, core->regs.R[rt]);
                return;
            default:
                printf("\nUNKNOWN COP INSTRUCTION %08x", opcode);
        }
    }
}

void R3000_fLB(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    //lb  rt,imm(rs)    rt=[imm+rs]  ;byte sign-extended
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = (u32)((i16)(opcode & 0xFFFF));
    u32 addr = core->regs.R[rs] + imm16;

    u32 rd = (i32)core->read(core->read_ptr, addr, 1, 1);
    rd = (rd << 24) >> 24;
    R3000_fs_reg_delay(core, rt, (u32)rd);
}

void R3000_fLH(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    //lb  rt,imm(rs)    rt=[imm+rs]  ;byte sign-extended
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = (u32)((i16)(opcode & 0xFFFF));
    u32 addr = core->regs.R[rs] + imm16;

    u32 rd = (u32)((i16)core->read(core->read_ptr, addr, 2, 1));

    //rd = (u32)((rd << 16) >> 16);
    R3000_fs_reg_delay(core, rt, rd);
}

void R3000_fLWL(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = (u32)((i16)(opcode & 0xFFFF));
    u32 addr = core->regs.R[rs] + imm16;


    // Fetch register from delay if it's there, and also clobber it
    u32 cur_v = R3000_fs_reg_delay_read(core, rt);

    u32 aligned_addr = addr & 0xFFFFFFFC;
    u32 aligned_word = core->read(core->read_ptr, aligned_addr, 4, 1);
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
    R3000_fs_reg_delay(core, rt, fv);
}

void R3000_fLW(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = (u32)((i16)(opcode & 0xFFFF));
    u32 addr = core->regs.R[rs] + imm16;

    //printf("\nLW imm16:%04x addr:%08x", imm16, addr);
    R3000_fs_reg_delay(core, rt, core->read(core->read_ptr, addr, 4, 1));
}

void R3000_fLBU(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = (u32)((i16)(opcode & 0xFFFF));
    u32 addr = core->regs.R[rs] + imm16;

    u32 rd = core->read(core->read_ptr, addr, 1, 1);
    R3000_fs_reg_delay(core, rt, rd&0xFF);
}

void R3000_fLHU(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = (u32)((i16)(opcode & 0xFFFF));
    u32 addr = core->regs.R[rs] + imm16;

    u32 rd = core->read(core->read_ptr, addr, 2, 1);
    R3000_fs_reg_delay(core, rt, rd&0xFFFF);
}

void R3000_fLWR(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = (u32)((i16)(opcode & 0xFFFF));
    u32 addr = core->regs.R[rs] + imm16;

    // Fetch register from delay if it's there, and also clobber it
    u32 cur_v = R3000_fs_reg_delay_read(core, rt);

    u32 aligned_addr = addr & 0xFFFFFFFC;
    u32 aligned_word = core->read(core->read_ptr, aligned_addr, 4, 1);
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
    R3000_fs_reg_delay(core, rt, fv);
}

void R3000_fSB(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    //lb  rt,imm(rs)    rt=[imm+rs]  ;byte sign-extended
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = (u32)((i16)(opcode & 0xFFFF));
    u32 addr = core->regs.R[rs] + imm16;

    core->write(core->write_ptr, addr, 1, core->regs.R[rt] & 0xFF);
}

void R3000_fSH(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    //lb  rt,imm(rs)    rt=[imm+rs]  ;byte sign-extended
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = (u32)((i16)(opcode & 0xFFFF));
    u32 addr = core->regs.R[rs] + imm16;

    core->write(core->write_ptr, addr, 2, core->regs.R[rt] & 0xFFFF);
}

void R3000_fSWL(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = (u32)((i16)(opcode & 0xFFFF));
    u32 addr = core->regs.R[rs] + imm16;

    u32 v = core->regs.R[rt];

    u32 aligned_addr = (addr & 0xFFFFFFFC)>>0;
    u32 cur_mem = core->read(core->read_ptr, aligned_addr, 4, 1);

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
    core->write(core->write_ptr, aligned_addr, 4, cur_mem);
}

void R3000_fSW(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    //lb  rt,imm(rs)    rt=[imm+rs]  ;byte sign-extended
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = (u32)((i16)(opcode & 0xFFFF));
    u32 addr = core->regs.R[rs] + imm16;

    core->write(core->write_ptr, addr, 4, core->regs.R[rt]);
}

void R3000_fSWR(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = (u32)((i16)(opcode & 0xFFFF));
    u32 addr = core->regs.R[rs] + imm16;
    u32 v = core->regs.R[rt];

    u32 aligned_addr = (addr & 0xFFFFFFFC)>>0;
    u32 cur_mem = core->read(core->read_ptr, aligned_addr, 4, 1);

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
    core->write(core->write_ptr, aligned_addr, 4, cur_mem);
}

void R3000_fLWC(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    // ;cop#dat_rt = [rs+imm]  ;word
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = (u32)((i16)(opcode & 0xFFFF));
    u32 addr = core->regs.R[rs] + imm16;

    u32 rd = core->read(core->read_ptr, addr, 4, 1);
    // TODO: add the 1-cycle delay to this
    COP_write_reg(core, op->arg, rt, rd);
}

void R3000_fSWC(u32 opcode, struct R3000_opcode *op, struct R3000 *core)
{
    // ;cop#dat_rt = [rs+imm]  ;word
    u32 rs = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 imm16 = (u32)((i16)(opcode & 0xFFFF));
    u32 addr = core->regs.R[rs] + imm16;

    u32 rd = COP_read_reg(core, op->arg, rt);
    // TODO: add the 1-cycle delay to this
    core->write(core->write_ptr, addr, 4, rd);
}
