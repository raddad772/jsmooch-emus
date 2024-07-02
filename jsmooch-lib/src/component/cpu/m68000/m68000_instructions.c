//
// Created by . on 5/15/24.
//

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "m68000.h"
#include "m68000_instructions.h"
#include "generated/generated_disasm.h"

struct M68k_ins_t M68k_decoded[65536];

static void M68k_start_push(struct M68k* this, u32 val, u32 sz, enum M68k_states next_state)
{
    M68k_start_write(this, M68k_dec_SSP(this, sz == 1 ? 2 : sz), val, sz, MAKE_FC(0), M68K_RW_ORDER_REVERSE, next_state);
}

static void M68k_start_pop(struct M68k* this, u32 sz, u32 FC, enum M68k_states next_state)
{
    M68k_start_read(this, M68k_get_SSP(this), sz, FC, M68K_RW_ORDER_NORMAL, next_state);
    M68k_inc_SSP(this, sz);
}

static u32 clip32[5] = { 0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF };
static u32 msb32[5] = { 0, 0x80, 0x8000, 0, 0x80000000 };

static i32 sgn32(u32 num, u32 sz) {
    switch(sz) {
        case 1:
            return (i32)(i8)num;
        case 2:
            return (i32)(i16)num;
        case 4:
            return (i32)num;
    }
    assert(1==0);
    return 0;
}

static u32 ADD(struct M68k* this, u32 op1, u32 op2, u32 sz, u32 extend)
{
    // Thanks Ares
    u32 target = clip32[sz] & op1;
    u32 source = clip32[sz] & op2;
    u32 result = target + source + (extend ? this->regs.SR.X : 0);
    u32 carries = target ^ source ^ result;
    u32 overflow = (target ^ result) & (source ^ result);

    this->regs.SR.C = !!((carries ^ overflow) & msb32[sz]);
    this->regs.SR.V = !!(overflow & msb32[sz]);
    this->regs.SR.Z = !!((clip32[sz] & result) ? 0 : (extend ? this->regs.SR.Z : 1));
    printf("\nRESULT:%08x SZ:%d", result, sz);
    this->regs.SR.N = sgn32(result,sz) < 0;
    this->regs.SR.X = this->regs.SR.C;

    return clip32[sz] & result;
}

static u32 AND(struct M68k* this, u32 source, u32 target, u32 sz)
{
    u32 result = source & target;
    this->regs.SR.C = this->regs.SR.V = 0;
    this->regs.SR.Z = (clip32[sz] & result) == 0;
    this->regs.SR.N = !!(result & msb32[sz]);
    return clip32[sz] & result;
}


static u32 ASL(struct M68k* this, u32 result, u32 shift, u32 sz)
{
    u32 carry = 0;
    u32 overflow = 0;
    u32 before;
    for (u32 i = 0; i < shift; i++) {
        carry = !!(result & msb32[sz]);
        before = result;
        result <<= 1;
        overflow |= before ^ result;
    }

    this->regs.SR.C = carry;
    this->regs.SR.V = sgn32(overflow, sz) < 0;
    this->regs.SR.Z = (clip32[sz] & result) == 0;
    this->regs.SR.N = !!(msb32[sz] & result);
    if (shift > 0) this->regs.SR.X = this->regs.SR.C;

    return clip32[sz] & result;
}

static u32 ASR(struct M68k* this, u32 result, u32 shift, u32 sz)
{
    u32 carry = 0;
    u32 overflow = 0;
    u32 before;
    for (u32 i = 0; i < shift; i++) {
        carry = result & 1;
        before = result;
        result = sgn32(result, sz) >> 1;
        overflow |= before ^ result;
    }

    this->regs.SR.C = carry;
    this->regs.SR.V = sgn32(overflow, sz) < 0;
    this->regs.SR.Z = (clip32[sz] & result) == 0;
    this->regs.SR.N = !!(msb32[sz] & result);
    if (shift > 0) this->regs.SR.X = this->regs.SR.C;

    return clip32[sz] & result;
}


static u32 CMP(struct M68k* this, i32 source, i32 target, u32 sz) {
    target &= clip32[sz];
    source &= clip32[sz];
    u32 result = target - source;
    u32 carries = target ^ source ^ result;
    u32 overflow = (target ^ result) & (source ^ target);

    this->regs.SR.C = !!((carries ^ overflow) & msb32[sz]);
    this->regs.SR.V = !!(overflow & msb32[sz]);
    this->regs.SR.Z = (result & clip32[sz]) == 0;
    this->regs.SR.N = sgn32(result, sz) < 0;

    return result & clip32[sz];
}

static u32 ROL(struct M68k* this, u32 result, u32 shift, u32 sz)
{
    u32 carry = 0;
    for (u32 i = 0; i < shift; i++) {
        carry = !!(result & msb32[sz]);
        result = (result << 1) | carry;
    }

    this->regs.SR.C = carry;
    this->regs.SR.V = 0;
    this->regs.SR.Z = (clip32[sz] & result) == 0;
    this->regs.SR.N = !!(msb32[sz] & result);

    return clip32[sz] & result;
}

static u32 ROR(struct M68k* this, u32 result, u32 shift, u32 sz)
{
    u32 carry = 0;
    for (u32 i = 0; i < shift; i++) {
        carry = result & 1;
        result >>= 1;
        if (carry) result |= msb32[sz];
    }

    this->regs.SR.C = carry;
    this->regs.SR.V = 0;
    this->regs.SR.Z = (clip32[sz] & result) == 0;
    this->regs.SR.N = !!(msb32[sz] & result);

    return clip32[sz] & result;
}

static u32 SUB(struct M68k* this, u32 op1, u32 op2, u32 sz, u32 extend)
{
    u32 target = clip32[sz] & op2;
    u32 source = clip32[sz] & op1;
    u32 result = target - source - (extend ? this->regs.SR.X : 0);
    u32 carries = target ^ source ^ result;
    u32 overflow = (target ^ result) & (source ^ target);

    this->regs.SR.C = !!((carries ^ overflow) & msb32[sz]);
    this->regs.SR.V = !!(overflow & msb32[sz]);
    this->regs.SR.Z = !!((clip32[sz] & result) ? 0 : (extend ? this->regs.SR.Z : 1));
    this->regs.SR.N = sgn32(result,sz) < 0;
    this->regs.SR.X = this->regs.SR.C;

    return result & clip32[sz];
}

static u32 LSL(struct M68k* this, u32 result, u32 shift, u32 sz)
{
    u32 carry = 0;
    for (u32 i = 0; i < shift; i++) {
        carry = !!(result & msb32[sz]);
        result <<= 1;
    }

    this->regs.SR.C = carry;
    this->regs.SR.V = 0;
    this->regs.SR.Z = (clip32[sz] & result) == 0;
    this->regs.SR.N = !!(msb32[sz] & result);
    if (shift > 0) this->regs.SR.X = this->regs.SR.C;

    return clip32[sz] & result;
}

static u32 LSR(struct M68k* this, u32 result, u32 shift, u32 sz)
{
    u32 carry = 0;
    result &= clip32[sz];
    for (u32 i = 0; i < shift; i++) {
        carry = result & 1;
        result >>= 1;
    }

    this->regs.SR.C = carry;
    this->regs.SR.V = 0;
    this->regs.SR.Z = (clip32[sz] & result) == 0;
    this->regs.SR.N = !!(msb32[sz] & result);
    if (shift > 0) this->regs.SR.X = this->regs.SR.C;

    return clip32[sz] & result;
}

static u32 EOR(struct M68k* this, u32 source, u32 target, u32 sz)
{
    u32 result = target ^ source;
    this->regs.SR.C = this->regs.SR.V = 0;
    result &= clip32[sz];
    this->regs.SR.Z = result == 0;
    this->regs.SR.N = !!(msb32[sz] & result);

    return result;
}

static u32 OR(struct M68k* this, u32 source, u32 target, u32 sz)
{
    u32 result = source | target;
    this->regs.SR.C = this->regs.SR.V = 0;
    this->regs.SR.Z = (clip32[sz] & result) == 0;
    this->regs.SR.N = (sgn32(result, sz)) < 0;

    return clip32[sz] & result;
}

static u32 condition(struct M68k* this, u32 condition) {
    switch(condition) {
        case 0: return 1;
        case 1: return 0;
        case  2: return !this->regs.SR.C && !this->regs.SR.Z;  //HI
        case  3: return  this->regs.SR.C ||  this->regs.SR.Z;  //LS
        case  4: return !this->regs.SR.C;  //CC,HS
        case  5: return  this->regs.SR.C;  //CS,LO
        case  6: return !this->regs.SR.Z;  //NE
        case  7: return  this->regs.SR.Z;  //EQ
        case  8: return !this->regs.SR.V;  //VC
        case  9: return  this->regs.SR.V;  //VS
        case 10: return !this->regs.SR.N;  //PL
        case 11: return  this->regs.SR.N;  //MI
        case 12: return  this->regs.SR.N ==  this->regs.SR.V;  //GE
        case 13: return  this->regs.SR.N !=  this->regs.SR.V;  //LT
        case 14: return  this->regs.SR.N ==  this->regs.SR.V && !this->regs.SR.Z;  //GT
        case 15: return  this->regs.SR.N !=  this->regs.SR.V ||  this->regs.SR.Z;  //LE
        default: assert(1==0);        
    }
}

#define DELAY_IF_REGWRITE(k, n) if ((ins->sz == 4) && ((k == M68k_AM_data_register_direct) || (k == M68k_AM_address_register_direct))) M68k_start_wait(this, n, M68kS_exec);

#define export_M68KINS(x) void M68k_ins_##x(struct M68k* this, struct M68k_ins_t *ins) { \
    switch(this->state.instruction.TCU) {

#define M68KINS(x) static void M68k_ins_##x(struct M68k* this, struct M68k_ins_t *ins) { \
    switch(this->state.instruction.TCU) {
#define M68KINS_NOSWITCH(x) static void M68k_ins_##x(struct M68k* this, struct M68k_ins_t *ins) {
#define INS_END_NOSWITCH }

#define STEP0 case 0: {\
            this->state.instruction.done = 0;

#define STEP(x) break; }\
                case x: {

#define INS_END \
            this->state.instruction.done = 1; \
            break; }\
    }\
    this->state.instruction.TCU++;            \
}

#define INS_END_NOCOMPLETE \
            break; }\
    }\
    this->state.instruction.TCU++;            \
}


void M68k_ins_RESET(struct M68k* this, struct M68k_ins_t *ins) {
    switch(this->state.instruction.TCU) {
        // $0 (32-bit), indirect, -> SSP
        // $4 (32-bit) -> PC
        // SR->interrupt level = 7
        STEP0
            printf("\n\nRESET!");
            this->state.bus_cycle.next_state = M68kS_exec;
            M68k_start_read(this, 0, 2, M68k_FC_supervisor_program, M68K_RW_ORDER_NORMAL, M68kS_exec);
        STEP(1)
            // First word will be
            this->state.instruction.result = this->state.bus_cycle.data << 16;
            M68k_start_read(this, 2, 2, M68k_FC_supervisor_program, M68K_RW_ORDER_NORMAL, M68kS_exec);
        STEP(2)
            this->regs.ASP = this->state.instruction.result | this->state.bus_cycle.data;
            this->state.instruction.result = 0;
            M68k_start_read(this, 4, 2, M68k_FC_supervisor_program, M68K_RW_ORDER_NORMAL, M68kS_exec);
        STEP(3)
            this->state.instruction.result = this->state.bus_cycle.data << 16;
            M68k_start_read(this, 6, 2, M68k_FC_supervisor_program, M68K_RW_ORDER_NORMAL, M68kS_exec);
        STEP(4)
            this->regs.PC = this->state.instruction.result | this->state.bus_cycle.data;
            this->regs.SR.I = 7;
            // Start filling prefetch queue
            M68k_start_read(this, this->regs.PC, 2, M68k_FC_supervisor_program, M68K_RW_ORDER_NORMAL, M68kS_exec);
            this->regs.PC += 2;
        STEP(5)
            this->regs.IRC = this->state.bus_cycle.data;
            this->regs.IR = this->regs.IRC;
            M68k_start_read(this, this->regs.PC, 2, M68k_FC_supervisor_program, M68K_RW_ORDER_NORMAL, M68kS_exec);
            this->regs.PC += 2;
        STEP(6)
            this->regs.IRC = this->state.bus_cycle.data;
            this->regs.IRD = this->regs.IR;
INS_END

// IRD holds currently-executing instruction,
// IR holds currently-decoding instruction,
// IRC holds last pre-fetched word

#define BADINS { printf("\nUNIMPLEMENTED INSTRUCTION! %s", __func__); assert(1==0); }

M68KINS(TAS)
    STEP0
        M68k_start_read_operands(this, 0, ins->sz, M68kS_exec);
    STEP(1)
        this->state.instruction.result = this->state.op[0].val | 0x80;
        M68k_start_wait(this, 2, M68kS_exec);
    STEP(2)
        // Write or wait
        if ((!this->megadrive_bug) || (ins->ea1.kind == M68k_AM_data_register_direct)) {
            M68k_start_write_operand(this, 0, 0, M68kS_exec);
        }
        else {
            M68k_start_wait(this, 4, M68kS_exec);
        }
    STEP(3)
        M68k_start_prefetch(this, 1, 1, M68kS_exec);
        this->regs.SR.C = this->regs.SR.V = 0;
        this->regs.SR.Z = (this->state.op[0].val & 0XFF) == 0;
        this->regs.SR.V = !!(this->state.op[0].val & 0x80);
    STEP(4)

INS_END


M68KINS(ABCD)
        STEP0
            M68k_start_read_operands(this, 1, ins->sz, M68kS_exec);
        STEP(1)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            u8 source = this->state.op[0].val;
            u8 dest = this->state.op[1].val;
            u32 extend = this->regs.SR.X;
            int unadjusted_result = dest + source + extend;

            int result = (dest & 15) + (source & 15) + extend;
            result += (dest & 0xF0) + (source & 0xF0) + (((9 - result) >> 4) & 6);
            result += ((0x9f - result) >> 4) & 0x60;

            this->regs.SR.Z = (result & 0xFF) ? 0 : this->regs.SR.Z;
            this->regs.SR.X = this->regs.SR.C = result > 0xFF;
            this->regs.SR.N = !!(result & 0x80);
            this->regs.SR.V = !!(~unadjusted_result & result & 0x80);
            this->state.instruction.result = result;
            M68k_start_write_operand(this, 0, 1, M68kS_exec);
        STEP(3)
            if (ins->ea1.kind == M68k_AM_data_register_direct)
                M68k_start_wait(this, 2, M68kS_exec);
        STEP(4)
INS_END

M68KINS(ADD)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec);
        STEP(1)
            this->state.instruction.result = ADD(this, this->state.op[0].val, this->state.op[1].val, ins->sz, 0);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_write_operand(this, 0, 1, M68kS_exec);
        STEP(3)
            if (ins->ea2.kind == M68k_AM_data_register_direct) {
                M68k_start_wait(this, ins->sz == 4 ? 4 : 2, M68kS_exec);
            }
        STEP(4)
INS_END

M68KINS(ADDA)
        STEP0
            BADINS;
INS_END

#define STARTI \
            this->state.instruction.result = this->regs.IRC;\
            M68k_start_prefetch(this, ins->sz >> 1, 1, M68kS_exec);\
        STEP(1)\
            u32 o = this->state.instruction.result;\
            switch(ins->sz) {\
                case 1:\
                    this->state.instruction.result = o & 0xFF;\
                    break;\
                case 2:\
                    this->state.instruction.result = o;\
                    break;\
                case 4:\
                    this->state.instruction.result = (o << 16) | this->regs.IR;\
                    break;\
                default:\
                    assert(1==0);\
            }\
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec);


M68KINS(ADDI)
        STEP0
            STARTI
        STEP(2)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(3)
            this->state.instruction.result = ADD(this, this->state.instruction.result, this->state.op[0].val, ins->sz, 0);
            M68k_start_write_operand(this, 0, 0, M68kS_exec);
        STEP(4)
INS_END

M68KINS(ADDQ_ar)
        STEP0
            this->state.instruction.result = this->regs.A[ins->ea2.reg] + ins->ea1.reg;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            M68k_set_ar(this, ins->ea2.reg, this->state.instruction.result, 4);
            M68k_start_wait(this, 4, M68kS_exec);
        STEP(2)
INS_END

M68KINS(ADDQ)
        STEP0
            // if op2 is AR, size is ignored and it uses long
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec);
        STEP(1)
            this->state.instruction.result = ADD(this, this->state.op[0].val, this->state.op[1].val, ins->sz, 0);
            //printf("\nop1:%08x  op2:%08x  result:%08x...%08x", this->state.instruction.result);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_write_operand(this, 0, 1, M68kS_exec);
        STEP(3)
            if ((ins->ea2.kind == M68k_AM_data_register_direct) && (ins->sz == 4))
                M68k_start_wait(this, 4, M68kS_exec);
        STEP(4)
INS_END

M68KINS(ADDX_sz4_predec)
        STEP0
            M68k_start_read_operands(this, 1, ins->sz, M68kS_exec);
        STEP(1)
            this->state.instruction.result = ADD(this, this->state.op[0].val, this->state.op[1].val, ins->sz, 1);
        STEP(2)
            this->regs.A[ins->ea2.reg] -= 2;
            M68k_start_write(this, this->regs.A[ins->ea2.reg], this->state.instruction.result & 0xFFFF, 2, MAKE_FC(0), M68K_RW_ORDER_NORMAL, M68kS_exec);
        STEP(3)
            this->state.instruction.prefetch++;
            M68k_start_prefetch(this, 1, 1, M68kS_exec); // all prefetches are done before writes
        STEP(4)
            this->regs.A[ins->ea2.reg] -= 2;
            M68k_start_write(this, this->regs.A[ins->ea2.reg], this->state.instruction.result >> 16, 2, MAKE_FC(0), M68K_RW_ORDER_NORMAL, M68kS_exec);
        STEP(5)
INS_END

M68KINS(ADDX_sz4_nopredec)
        STEP0
            M68k_start_read_operands(this, 1, ins->sz, M68kS_exec);
        STEP(1)
            this->state.instruction.result = ADD(this, this->state.op[0].val, this->state.op[1].val, ins->sz, 1);
            M68k_start_prefetch(this, 1, 1, M68kS_exec); // all prefetches are done before writes
        STEP(2)
            M68k_start_write_operand(this, 0, 1, M68kS_exec);
        STEP(3)
            M68k_start_wait(this, 4, M68kS_exec);
        STEP(4)
INS_END


M68KINS(ADDX_sz1_2)
        STEP0
            M68k_start_read_operands(this, 1, ins->sz, M68kS_exec);
        STEP(1)
            this->state.instruction.result = ADD(this, this->state.op[0].val, this->state.op[1].val, ins->sz, 1);
            M68k_start_prefetch(this, 1, 1, M68kS_exec); // all prefetches are done before writes
        STEP(2)
            M68k_start_write_operand(this, 0, 1, M68kS_exec);
        STEP(3)
INS_END

M68KINS_NOSWITCH(ADDX)
    if (ins->sz == 4) {
        u32 u = (ins->operand_mode == M68k_OM_ea_ea) || (ins->operand_mode == M68k_OM_r_ea);
        if (u && (ins->ea2.kind == M68k_AM_address_register_indirect_with_predecrement)) {
            return M68k_ins_ADDX_sz4_predec(this, ins);
        }
        else {
            return M68k_ins_ADDX_sz4_nopredec(this, ins);
        }
    } else {
        return M68k_ins_ADDX_sz1_2(this, ins);
    }
INS_END_NOSWITCH

M68KINS(AND)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec);
        STEP(1)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            this->state.instruction.result = AND(this, this->state.op[0].val, this->state.op[1].val, ins->sz);
            M68k_start_write_operand(this, 0, 1, M68kS_exec);
            if ((ins->sz == 4) && (ins->ea2.kind == M68k_AM_data_register_direct)) M68k_start_wait(this, 4, M68kS_exec);
        STEP(4)
INS_END

M68KINS(ANDI)
        STEP0
        STARTI;
        STEP(2)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(3)
            this->state.instruction.result = AND(this, this->state.instruction.result, this->state.op[0].val, ins->sz);
            M68k_start_write_operand(this, 0, 0, M68kS_exec);
        STEP(4)
INS_END

M68KINS(ANDI_TO_CCR)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            u32 v = this->regs.SR.u & 0x1F;
            v &= this->regs.IR;
            this->regs.SR.u = (this->regs.SR.u & 0xFFE0) | v;
            M68k_start_wait(this, 8, M68kS_exec);
        STEP(3)
            M68k_start_read(this, this->regs.PC-2, 2, MAKE_FC(1), M68K_RW_ORDER_NORMAL, M68kS_exec);
        STEP(4)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(5)
INS_END

M68KINS(ANDI_TO_SR)
        STEP0
            BADINS;
INS_END

M68KINS(ASL_qimm_dr)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + ins->ea1.reg * 2, M68kS_exec);
        STEP(2)
            this->state.instruction.result = ASL(this, this->regs.D[ins->ea2.reg], ins->ea1.reg, ins->sz);
            M68k_set_dr(this, ins->ea2.reg, this->state.instruction.result, ins->sz);
        STEP(3)
INS_END

M68KINS(ASL_dr_dr)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            u32 cnt = this->regs.D[ins->ea1.reg] & 63;
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + cnt * 2, M68kS_exec);
        STEP(2)
            u32 cnt = this->regs.D[ins->ea1.reg] & 63;
            this->state.instruction.result = ASL(this, this->regs.D[ins->ea2.reg], cnt, ins->sz);
            M68k_set_dr(this, ins->ea2.reg, this->state.instruction.result, ins->sz);
INS_END

M68KINS(ASL_ea)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec);
        STEP(1)
            this->state.instruction.result = ASL(this, this->state.op[0].val, 1, ins->sz);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_write_operand(this, 0, 0, M68kS_exec);
        STEP(3)
INS_END

M68KINS(ASR_qimm_dr)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + ins->ea1.reg * 2, M68kS_exec);
        STEP(2)
            this->state.instruction.result = ASR(this, this->regs.D[ins->ea2.reg], ins->ea1.reg, ins->sz);
            M68k_set_dr(this, ins->ea2.reg, this->state.instruction.result, ins->sz);
        STEP(3)
INS_END

M68KINS(ASR_dr_dr)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
            printf("\nTHIS ONE!");
        STEP(1)
            u32 cnt = this->regs.D[ins->ea1.reg] & 63;
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + cnt * 2, M68kS_exec);
        STEP(2)
            u32 cnt = this->regs.D[ins->ea1.reg] & 63;
            this->state.instruction.result = ASR(this, this->regs.D[ins->ea2.reg], cnt, ins->sz);
            M68k_set_dr(this, ins->ea2.reg, this->state.instruction.result, ins->sz);
INS_END

M68KINS(ASR_ea)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec);
        STEP(1)
            this->state.instruction.result = ASR(this, this->state.op[0].val, 1, ins->sz);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_write_operand(this, 0, 0, M68kS_exec);
        STEP(3)
INS_END

M68KINS(BCC)
        STEP0
            this->state.instruction.result = condition(this, ins->ea1.reg);
            M68k_start_wait(this, this->state.instruction.result ? 2 : 4, M68kS_exec);
        STEP(1)
            if (!this->state.instruction.result) {
                M68k_start_prefetch(this, (ins->ea2.reg == 0) ? 2 : 1, 1, M68kS_exec);
            }
            else {
                u32 cnt = SIGNe8to32(ins->ea2.reg);
                if (cnt == 0) cnt = SIGNe16to32(this->regs.IRC);
                this->regs.PC -= 2;
                this->regs.PC += cnt;
                M68k_start_prefetch(this, 2, 1, M68kS_exec);
            }
        STEP(2)
INS_END

M68KINS(BCHG)
        STEP0
            BADINS;
INS_END

M68KINS(BCLR)
        STEP0
            BADINS;
INS_END

M68KINS(BRA)
        STEP0
            M68k_start_wait(this, 2, M68kS_exec);
        STEP(1)
            u32 cnt = SIGNe8to32(ins->ea1.reg);
            if (cnt == 0) cnt = SIGNe16to32(this->regs.IRC);
            this->regs.PC -= 2;
            this->regs.PC += cnt;
            M68k_start_prefetch(this, 2, 1, M68kS_exec);
        STEP(2)
INS_END

M68KINS(BSET)
        STEP0
            BADINS;
INS_END

/*
 * 0 - currently exec, IRD  -4
 * 0 - IR (next instruction) -4
 * 2 - IRC   -2
 * 4 - next fetch  -0
 */

M68KINS(BSR)
       STEP0
           // at this point, my PC is 6 ahead of actual executing instruction. so we must write -4 to stack
           this->regs.PC -= 2;
           this->regs.A[7] -= 4;
           if (ins->ea1.reg != 0) {
               M68k_start_write(this, this->regs.A[7], this->regs.PC, 4, MAKE_FC(0), M68K_RW_ORDER_NORMAL, M68kS_exec);
           }
           else {
               M68k_start_write(this, this->regs.A[7], this->regs.PC+2, 4, MAKE_FC(0), M68K_RW_ORDER_NORMAL, M68kS_exec);
           }
           //this->regs.PC -= 2;
        STEP(1)
            i32 offset;
            if (ins->ea1.reg != 0) {
                offset = ins->ea1.reg;
                offset = SIGNe8to32(offset);
            }
            else {
                offset = SIGNe16to32(this->regs.IRC);
            }
            printf("\nBSR PC %06x   OFFS %08x", this->regs.PC, offset);

            this->regs.PC += offset;
            printf("\nNEW PC! %06x", this->regs.PC);
            M68k_start_prefetch(this, 2, 0, M68kS_exec);
        STEP(2)
INS_END

M68KINS(BTST)
        STEP0
            BADINS;
INS_END

M68KINS(CHK)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec);
        STEP(1)
            u32 target = this->state.op[0].val;
            u32 source = this->state.op[1].val;
            i64 result = (i64)target - (i64)source;
            this->regs.SR.C = (i64)(i16)(result >> 1) < 0;
            this->regs.SR.V = (i32)(i16)((target ^ source) & (target ^ result)) < 0;
            this->regs.SR.N = ((i32)(i16)(result) < 0) ^ this->regs.SR.V;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            if(!this->regs.SR.N && !this->regs.SR.Z) {
                M68k_start_group2_exception(this, M68kIV_chk_instruction, 0, this->regs.PC-4);
                return;
            }
            u32 target = this->state.op[0].val & 0xFFFF;
            this->regs.SR.Z = target == 0;
            this->regs.SR.N = (i32)(i16)target < 0;
            if(this->regs.SR.N) {
                M68k_start_group2_exception(this, M68kIV_chk_instruction, 2, this->regs.PC-4);
            }
        STEP(3)
            M68k_start_wait(this, 6, M68kS_exec);
        STEP(4)
INS_END

M68KINS(CLR)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec);
        STEP(1)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            this->state.instruction.result = 0;
            M68k_start_write_operand(this, 0, 0, M68kS_exec);
        STEP(3)
            this->regs.SR.C = this->regs.SR.V = this->regs.SR.N = 0;
            this->regs.SR.Z = 1;
            if ((ins->sz == 4) && ((ins->ea1.kind == M68k_AM_data_register_direct) || (ins->ea1.kind == M68k_AM_address_register_direct)))
                M68k_start_wait(this, 2, M68kS_exec);
        STEP(4)
INS_END

M68KINS(CMP)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec);
        STEP(1)
            CMP(this, sgn32(this->state.op[0].val, ins->sz), sgn32(this->state.op[1].val, ins->sz), ins->sz);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            if (ins->sz == 4) M68k_start_wait(this, 2, M68kS_exec);
        STEP(3)
INS_END

M68KINS(CMPA)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec);
        STEP(1)
            // Ignore size for operand 2
            this->state.op[1].val = this->regs.A[ins->ea2.reg];
            CMP(this, sgn32(this->state.op[0].val, ins->sz), (i32)this->state.op[1].val, ins->sz);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_wait(this, 2, M68kS_exec);
        STEP(3)
INS_END

M68KINS(CMPI)
        STEP0
        STARTI;
        STEP(2)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(3)
            CMP(this, sgn32(this->state.instruction.result, ins->sz), sgn32(this->state.op[0].val, ins->sz), ins->sz);
            if (ins->sz == 4) M68k_start_wait(this, 2, M68kS_exec);
        STEP(4)
INS_END

M68KINS(CMPM)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec);
        STEP(1)
            CMP(this, sgn32(this->state.op[0].val, ins->sz), sgn32(this->state.op[1].val, ins->sz), ins->sz);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
INS_END

M68KINS(DBCC)
        STEP0
            BADINS;
INS_END

M68KINS(DIVS)
        STEP0
            BADINS;
INS_END

export_M68KINS(DIVU)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec);
        STEP(1)
            this->regs.tmp = this->regs.PC - 2;
            // ea is divisor
            // ea is op1
            // dr is target is op0
            this->regs.SR.C = 0;
            u32 dividend = this->regs.D[ins->ea2.reg];
            u32 divisor = this->state.op[0].val & 0xFFFF;
            if (divisor == 0) {
                this->regs.SR.N = this->regs.SR.V = 0;
                this->regs.SR.Z = 1;
                M68k_start_group2_exception(this, M68kIV_zero_divide, 4, this->regs.PC - 2);
                return;
            }
            u64 quotient = (u64)dividend / (u64)divisor;
            if (quotient > 0xFFFF) {
                M68k_start_wait(this, 6, M68kS_exec);
                this->regs.SR.V = 1;
                break;
            }
            u16 remainder =(u16)(dividend % divisor);
            this->state.instruction.result = (u32)(((u32)remainder << 16) | (quotient & 0xFFFF));

            this->regs.SR.V = 0;
            this->regs.SR.Z = (quotient & 0xFFFF) == 0;
            this->regs.SR.N = !!(quotient & 0x8000);
            M68k_start_wait(this, 120, M68kS_exec);
        STEP(3)
            if (this->regs.SR.V) {
                M68k_start_prefetch(this, 1, 1, M68kS_exec);
                break;
            }
            //M68k_start_write_operand(this, 0, 1, M68kS_exec);
            this->regs.D[ins->ea2.reg] = this->state.instruction.result;
        STEP(4)
            if (!this->regs.SR.V) M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(5)
INS_END

M68KINS(EOR)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec);
        STEP(1)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            this->state.instruction.result = EOR(this, this->state.op[0].val, this->state.op[1].val, ins->sz);
            M68k_start_write_operand(this, 0, 1, M68kS_exec);
        STEP(3)
            if ((ins->sz == 4) && (ins->ea2.kind == M68k_AM_data_register_direct)) M68k_start_wait(this, 4, M68kS_exec);
        STEP(4)
INS_END

M68KINS(EORI)
       STEP0
            STARTI;
        STEP(2)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(3)
            this->state.instruction.result = EOR(this, this->state.instruction.result, this->state.op[0].val, ins->sz);
            M68k_start_write_operand(this, 0, 0, M68kS_exec);
        STEP(4)
INS_END

M68KINS(EORI_TO_CCR)
        STEP0
            BADINS;
INS_END

M68KINS(EORI_TO_SR)
        STEP0
            if (!this->regs.SR.S) {
                M68k_start_group1_exception(this, M68kIV_privilege_violation, 0);
                return;
            }
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            u32 data = this->regs.IR ^ M68k_get_SR(this);
            M68k_set_SR(this, data);
            M68k_start_wait(this, 8, M68kS_exec);
        STEP(2)
            M68k_start_read(this, this->regs.PC-2, 2, MAKE_FC(1), 0, M68kS_exec);
        STEP(3)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(4)
INS_END

M68KINS(EXG)
        STEP0
            BADINS;
INS_END

M68KINS(EXT)
        STEP0
            BADINS;
INS_END

M68KINS(ILLEGAL)
        STEP0
            BADINS;
INS_END

M68KINS(JMP)
        STEP0
            BADINS;
INS_END

M68KINS(JSR)
        STEP0
            this->state.op[0].ext_words = M68k_AM_ext_words(ins->ea1.kind, 4);
            printf("\nEXT WRODS: %d", this->state.op[0].ext_words);
            if (this->state.op[0].ext_words == 2) M68k_start_read(this, this->regs.PC, 2, MAKE_FC(1), 0, M68kS_exec);
            this->regs.PC += this->state.op[0].ext_words << 1;
        STEP(1)
            // If the operand had two words, assemble from IRC and bus_cycle.data
            u32 prefetch = 0;
            if (this->state.op[0].ext_words == 2) {
                prefetch = (this->regs.IRC << 16) | this->state.bus_cycle.data;
            }
            else if (this->state.op[0].ext_words == 1){// Else if it had 1 assemble from IRC
                prefetch = this->regs.IRC;
            }
            // 2, 5, 6 and 7 0-3  . addr reg ind, ind with index, ind with displ.
            // abs short, abs long, pc rel index, pc rel displ.
            // all require another read.
            this->state.instruction.result = this->regs.PC - 2;
            this->regs.PC = M68k_read_ea_addr(this, &ins->ea1, 4, 0, prefetch);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_push(this, this->state.instruction.result, 4, M68kS_exec);
        STEP(3)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(4)
INS_END

M68KINS(LEA)
        STEP0
            BADINS;
INS_END

M68KINS(LINK)
        STEP0
            BADINS;
INS_END

M68KINS(LSL_qimm_dr)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + ins->ea1.reg * 2, M68kS_exec);
        STEP(2)
            this->state.instruction.result = LSL(this, this->regs.D[ins->ea2.reg], ins->ea1.reg, ins->sz);
            M68k_set_dr(this, ins->ea2.reg, this->state.instruction.result, ins->sz);
INS_END


M68KINS(LSL_dr_dr)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            u32 cnt = this->regs.D[ins->ea1.reg] & 63;
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + cnt * 2, M68kS_exec);
        STEP(2)
            u32 cnt = this->regs.D[ins->ea1.reg] & 63;
            this->state.instruction.result = LSL(this, this->regs.D[ins->ea2.reg], cnt, ins->sz);
            M68k_set_dr(this, ins->ea2.reg, this->state.instruction.result, ins->sz);
INS_END

M68KINS(LSL_ea)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec);
        STEP(1)
            this->state.instruction.result = LSL(this, this->state.op[0].val, 1, ins->sz);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_write_operand(this, 0, 0, M68kS_exec);
        STEP(3)
INS_END


M68KINS(LSR_qimm_dr)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + ins->ea1.reg * 2, M68kS_exec);
        STEP(2)
            this->state.instruction.result = LSR(this, this->regs.D[ins->ea2.reg], ins->ea1.reg, ins->sz);
            M68k_set_dr(this, ins->ea2.reg, this->state.instruction.result, ins->sz);
INS_END


M68KINS(LSR_dr_dr)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            u32 cnt = this->regs.D[ins->ea1.reg] & 63;
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + cnt * 2, M68kS_exec);
        STEP(2)
            u32 cnt = this->regs.D[ins->ea1.reg] & 63;
            this->state.instruction.result = LSR(this, this->regs.D[ins->ea2.reg], cnt, ins->sz);
            M68k_set_dr(this, ins->ea2.reg, this->state.instruction.result, ins->sz);
INS_END

M68KINS(LSR_ea)
        STEP0
           M68k_start_read_operands(this, 0, ins->sz, M68kS_exec);
        STEP(1)
            this->state.instruction.result = LSR(this, this->state.op[0].val, 1, ins->sz);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_write_operand(this, 0, 0, M68kS_exec);
        STEP(3)
INS_END

M68KINS(MOVE)
        STEP0
            BADINS;
INS_END

M68KINS(MOVEA)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec);
        STEP(1)
            u32 v = this->state.op[0].val;
            if (ins->sz == 2) v = SIGNe8to32(v);
            M68k_set_ar(this, ins->ea2.reg, v, ins->sz);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(3)
INS_END

M68KINS(MOVEM_TO_MEM)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            this->state.op[1].val = this->regs.IR;
            this->state.op[1].addr = 0;
            this->state.instruction.result = 200;
            /*
             * Simulate loop with addr.
             * Read memory, stick into register.
             * If we're not loop 0, then we have something for the old register
             */
            M68k_start_read_operand_for_ea(this, 0, ins->sz, M68kS_exec);
        STEP(2)
            this->state.op[0].val = M68k_read_ea_addr(this, &ins->ea1, 4, 1, this->state.op[0].ext_words);
        STEP(3)
            // Loop here up to 16 times..
            if (this->state.op[1].addr < 16) {
                // If our value is not 0, we will have a new value for a register
                u32 index = ins->ea1.kind == M68k_AM_address_register_indirect_with_predecrement ? 15 - this->state.op[1].addr : this->state.op[1].addr;
                // Loop!
                u32 a = this->state.op[1].addr;
                this->state.instruction.TCU--;
                this->state.op[1].addr++;

                // If this bit isn't set, don't do anything else
                if (!(this->state.op[1].val & (1 << a))) {
                    break;
                }

                // Start a read
                if (ins->ea1.kind == M68k_AM_address_register_indirect_with_predecrement) this->state.op[0].val -= ins->sz;
                u32 v = (index < 8) ? this->regs.D[index] : this->regs.A[index - 8];
                M68k_start_write(this, this->state.op[0].val, v, ins->sz, MAKE_FC(0), M68K_RW_ORDER_NORMAL, M68kS_exec);
                if (ins->ea1.kind != M68k_AM_address_register_indirect_with_predecrement) this->state.op[0].val += ins->sz;
            }
        STEP(5)
            if (ins->ea1.kind == M68k_AM_address_register_indirect_with_predecrement) M68k_set_ar(this, ins->ea1.reg, this->state.op[0].val, 4);
            if (ins->ea1.kind == M68k_AM_address_register_indirect_with_postincrement) M68k_set_ar(this, ins->ea1.reg, this->state.op[0].val, 4);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(6)
INS_END


struct m68k_gentest_item {
    char name[50];
    char full_name[50];
    u32 num_opcodes;
    u32 *opcodes;
};

struct m68k_gentest_item a = (struct m68k_gentest_item) { .name = "Hello", .full_name = "Hello goodbye", .num_opcodes = 2, .opcodes = (u32 []) { 1, 2 } };
        /*
 * name
 * full name
 * num_
 */

export_M68KINS(MOVEM_TO_REG)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            this->state.op[1].val = this->regs.IR;
            this->state.op[1].addr = 0;
            this->state.instruction.result = 200;
            /*
             * Simulate loop with addr.
             * Read memory, stick into register.
             * If we're not loop 0, then we have something for the old register
             */
            M68k_start_read_operand_for_ea(this, 0, ins->sz, M68kS_exec);
        STEP(2)
            this->state.op[0].val = M68k_read_ea_addr(this, &ins->ea1, 4, 1, this->state.op[0].ext_words);
        STEP(3)
        // Loop here up to 16 times..
            if (this->state.op[1].addr < 16) {
                // If our value is not 0, we will have a new value for a register
                u32 index = ins->ea1.kind == M68k_AM_address_register_indirect_with_predecrement ? 15 - this->state.op[1].addr : this->state.op[1].addr;
                if (this->state.instruction.result != 200) {
                    if (this->state.instruction.result < 8) M68k_set_dr(this, this->state.instruction.result, sgn32(this->state.bus_cycle.data, ins->sz), 4);
                    else M68k_set_ar(this, this->state.instruction.result - 8, sgn32(this->state.bus_cycle.data, ins->sz), 4);
                    this->state.instruction.result = 200;
                }

                // Loop!
                u32 a = this->state.op[1].addr;
                this->state.instruction.TCU--;
                this->state.op[1].addr++;

                // If this bit isn't set, don't do anything else
                if (!(this->state.op[1].val & (1 << a))) {
                    break;
                }

                // Start a read
                this->state.instruction.result = index;
                if (ins->ea1.kind == M68k_AM_address_register_indirect_with_predecrement) this->state.op[0].val -= ins->sz;
                M68k_start_read(this, this->state.op[0].val, ins->sz, MAKE_FC(0), M68K_RW_ORDER_NORMAL, M68kS_exec);
                if (ins->ea1.kind != M68k_AM_address_register_indirect_with_predecrement) this->state.op[0].val += ins->sz;
            }
        STEP(4)
            // Finish last read, if it's there
            if (this->state.instruction.result != 200) {
                if (this->state.instruction.result < 8) M68k_set_dr(this, this->state.instruction.result, sgn32(this->state.bus_cycle.data, ins->sz), 4);
                else M68k_set_ar(this, this->state.instruction.result-8, sgn32(this->state.bus_cycle.data, ins->sz), 4);
                this->state.instruction.result = 200;
            }
            if (ins->ea1.kind == M68k_AM_address_register_indirect_with_predecrement) {
                this->state.op[0].val -= 2;
            }
            M68k_start_read(this, this->state.op[0].val, 2, MAKE_FC(0), 0, M68kS_exec);
        STEP(5)
            if (ins->ea1.kind == M68k_AM_address_register_indirect_with_predecrement) M68k_set_ar(this, ins->ea1.reg, this->state.op[0].val, 4);
            if (ins->ea1.kind == M68k_AM_address_register_indirect_with_postincrement) M68k_set_ar(this, ins->ea1.reg, this->state.op[0].val, 4);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(6)
INS_END

M68KINS(MOVEP)
        STEP0
            BADINS;
INS_END

M68KINS(MOVEQ)
        STEP0
            BADINS;
INS_END

M68KINS(MOVE_FROM_SR)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec);
        STEP(1)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            this->state.instruction.result = M68k_get_SR(this);
            M68k_start_write_operand(this, 0, 0, M68kS_exec);
        STEP(3)
INS_END

M68KINS(MOVE_FROM_USP)
        STEP0
            BADINS;
INS_END

M68KINS(MOVE_TO_CCR)
        STEP0
            BADINS;
INS_END

M68KINS(MOVE_TO_SR)
        STEP0
            BADINS;
INS_END

M68KINS(MOVE_TO_USP)
        STEP0
            BADINS;
INS_END

M68KINS(MULS)
        STEP0
            BADINS;
INS_END

M68KINS(MULU)
        STEP0
            BADINS;
INS_END

M68KINS(NBCD)
        STEP0
            BADINS;
INS_END

M68KINS(NEG)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec);
        STEP(1)
            this->state.instruction.result = SUB(this, this->state.op[0].val, 0, ins->sz, 0);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            if ((ins->ea1.kind == M68k_AM_address_register_direct) || (ins->ea1.kind == M68k_AM_data_register_direct)) {
                M68k_start_wait(this, 2, M68kS_exec);
            }
        STEP(3)
            M68k_start_write_operand(this, 0, 0, M68kS_exec);
        STEP(4)
INS_END

M68KINS(NEGX)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec);
        STEP(1)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            this->state.instruction.result = SUB(this, this->state.op[0].val, 0, ins->sz, 1);
            M68k_start_write_operand(this, 0, 0, M68kS_exec);
        STEP(3)
            if ((ins->ea1.kind == M68k_AM_data_register_direct) || (ins->ea1.kind == M68k_AM_address_register_direct)) {
                M68k_start_wait(this, 2, M68kS_exec);
            }
        STEP(4)
INS_END

M68KINS(NOP)
        STEP0
            BADINS;
INS_END

M68KINS(NOT)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec);
        STEP(1)
            this->state.instruction.result = (~this->state.op[0].val) & clip32[ins->sz];
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_write_operand(this, 0, 0, M68kS_exec);
            DELAY_IF_REGWRITE(ins->ea1.kind, 2);
            this->regs.SR.C = this->regs.SR.V = 0;
            this->regs.SR.Z = this->state.instruction.result == 0;
            this->regs.SR.N = !!(this->state.instruction.result & msb32[ins->sz]);
        STEP(3)
INS_END

M68KINS(OR)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec);
        STEP(1)
            this->state.instruction.result = OR(this, this->state.op[0].val, this->state.op[1].val, ins->sz);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_write_operand(this, 0, 1, M68kS_exec);
        STEP(3)
            if ((ins->sz == 4) && (ins->ea2.kind == M68k_AM_data_register_direct)) {
                if ((ins->ea1.kind == M68k_AM_data_register_direct) || (ins->ea1.kind == M68k_AM_immediate))
                    M68k_start_wait(this, 4, M68kS_exec);
                else
                    M68k_start_wait(this, 2, M68kS_exec);
            }
        STEP(4)

INS_END

M68KINS(ORI)
        STEP0
            STARTI;
        STEP(2)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(3)
            printf("\nOP0 VAL: %08x", this->state.op[0].val);
            printf("\nOp1 VAL: %08x", this->state.instruction.result);
            this->state.instruction.result = OR(this, this->state.instruction.result, this->state.op[0].val, ins->sz);
            M68k_start_write_operand(this, 0, 0, M68kS_exec);
        STEP(4)
            if ((ins->sz == 4) && (ins->ea1.kind == M68k_AM_data_register_direct))
                M68k_start_wait(this, 4, M68kS_exec);
        STEP(5)
INS_END

M68KINS(ORI_TO_CCR)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            u32 v = this->regs.SR.u & 0x1F;
            v |= (this->regs.IR & 0x1F);
            this->regs.SR.u = (this->regs.SR.u & 0xFFE0) | v;
            M68k_start_wait(this, 8, M68kS_exec);
        STEP(3)
            M68k_start_read(this, this->regs.PC-2, 2, MAKE_FC(1), M68K_RW_ORDER_NORMAL, M68kS_exec);
        STEP(4)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(5)
INS_END

M68KINS(ORI_TO_SR)
        STEP0
            BADINS;
INS_END

M68KINS(PEA)
        STEP0
            M68k_start_read_operand_for_ea(this, 0, ins->sz, M68kS_exec);
        STEP(1)
            this->state.op[0].addr = M68k_read_ea_addr(this, &this->ins->ea1, this->ins->sz,
                                                            0, this->state.op[0].ext_words);
            this->state.instruction.result = this->state.op[0].addr;
            if ((ins->ea1.kind == M68k_AM_address_register_indirect_with_index) ||
                        (ins->ea1.kind == M68k_AM_program_counter_with_index))
                    M68k_start_wait(this, 2, M68kS_exec);
        STEP(2)
            if ((ins->ea1.kind == M68k_AM_absolute_short_data) || (ins->ea1.kind == M68k_AM_absolute_long_data)) {
                M68k_start_push(this, this->state.instruction.result, 4, M68kS_exec);
            }
            else {
                M68k_start_prefetch(this, 1, 1, M68kS_exec);
            }
        STEP(3)
            if ((ins->ea1.kind == M68k_AM_absolute_short_data) || (ins->ea1.kind == M68k_AM_absolute_long_data)) {
                M68k_start_prefetch(this, 1, 1, M68kS_exec);
            }
            else {
                M68k_start_push(this, this->state.instruction.result, 4, M68kS_exec);
            }
        STEP(4)
    INS_END

M68KINS(ROL_qimm_dr)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + ins->ea1.reg * 2, M68kS_exec);
        STEP(2)
            this->state.instruction.result = ROL(this, this->regs.D[ins->ea2.reg], ins->ea1.reg, ins->sz);
            M68k_set_dr(this, ins->ea2.reg, this->state.instruction.result, ins->sz);
        STEP(3)
INS_END

M68KINS(ROL_dr_dr)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            u32 cnt = this->regs.D[ins->ea1.reg] & 63;
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + cnt * 2, M68kS_exec);
        STEP(2)
            u32 cnt = this->regs.D[ins->ea1.reg] & 63;
            this->state.instruction.result = ROL(this, this->regs.D[ins->ea2.reg], cnt, ins->sz);
            M68k_set_dr(this, ins->ea2.reg, this->state.instruction.result, ins->sz);
INS_END

M68KINS(ROL_ea)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec);
        STEP(1)
            this->state.instruction.result = ROL(this, this->state.op[0].val, 1, ins->sz);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_write_operand(this, 0, 0, M68kS_exec);
        STEP(3)
INS_END

M68KINS(ROR_qimm_dr)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + ins->ea1.reg * 2, M68kS_exec);
        STEP(2)
            this->state.instruction.result = ROR(this, this->regs.D[ins->ea2.reg], ins->ea1.reg, ins->sz);
            M68k_set_dr(this, ins->ea2.reg, this->state.instruction.result, ins->sz);
        STEP(3)
INS_END

M68KINS(ROR_dr_dr)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            u32 cnt = this->regs.D[ins->ea1.reg] & 63;
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + cnt * 2, M68kS_exec);
        STEP(2)
            u32 cnt = this->regs.D[ins->ea1.reg] & 63;
            this->state.instruction.result = ROR(this, this->regs.D[ins->ea2.reg], cnt, ins->sz);
            M68k_set_dr(this, ins->ea2.reg, this->state.instruction.result, ins->sz);
INS_END

M68KINS(ROR_ea)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec);
        STEP(1)
            this->state.instruction.result = ROR(this, this->state.op[0].val, 1, ins->sz);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_write_operand(this, 0, 0, M68kS_exec);
        STEP(3)
INS_END

M68KINS(ROXL)
        STEP0
            BADINS;
INS_END

M68KINS(ROXR)
        STEP0
            BADINS;
INS_END

M68KINS(RTE)
        STEP0
            if (!this->regs.SR.S) {
                M68k_start_group1_exception(this, M68kIV_privilege_violation, 0);
                this->state.instruction.done = 1;
                return;
            }
            this->state.instruction.result = M68k_get_SSP(this);
            M68k_inc_SSP(this, 6);
            M68k_start_read(this, this->state.instruction.result+2, 2, MAKE_FC(0), M68K_RW_ORDER_NORMAL, M68kS_exec);
        STEP(1)
            this->regs.PC = this->state.bus_cycle.data << 16;
            M68k_start_read(this, this->state.instruction.result+0, 2, MAKE_FC(0), M68K_RW_ORDER_NORMAL, M68kS_exec);
        STEP(2)
            M68k_start_read(this, this->state.instruction.result+4, 2, MAKE_FC(0), M68K_RW_ORDER_NORMAL, M68kS_exec);
            M68k_set_SR(this, this->state.bus_cycle.data);
        STEP(3)
            this->regs.PC |= this->state.bus_cycle.data;
            M68k_start_prefetch(this, 2, 1, M68kS_exec);
        STEP(4)
INS_END

M68KINS(RTR)
        STEP0
            BADINS;
INS_END

M68KINS(RTS)
        STEP0
            BADINS;
INS_END

M68KINS(SBCD)
        STEP0
            BADINS;
INS_END

M68KINS(SCC)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec);
        STEP(1)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            if (!condition(this, ins->ea1.reg)) {
                this->state.instruction.result = 0;
            }
            else {
                this->state.instruction.result = clip32[ins->sz] & 0xFFFFFFFF;
            }
            if (ins->ea2.kind == M68k_AM_data_register_direct) M68k_start_wait(this, 2, M68kS_exec);
            M68k_start_write_operand(this, 0, 1, M68kS_exec);
        STEP(3)
INS_END

M68KINS(STOP)
        STEP0
            BADINS;
INS_END

M68KINS(SUB)
        STEP0
            BADINS;
INS_END

M68KINS(SUBA)
        STEP0
            BADINS;
INS_END

M68KINS(SUBI)
        STEP0
            BADINS;
INS_END

M68KINS(SUBQ)
        STEP0
            BADINS;
INS_END

M68KINS(SUBQ_ar)
        STEP0
            BADINS;
INS_END

M68KINS(SUBX_sz4_predec)
        STEP0
            M68k_start_read_operands(this, 1, ins->sz, M68kS_exec);
        STEP(1)
            this->state.instruction.result = SUB(this, this->state.op[0].val, this->state.op[1].val, ins->sz, 1);
            this->regs.A[ins->ea2.reg] += 4;
        STEP(2)
            this->regs.A[ins->ea2.reg] -= 2;
            M68k_start_write(this, this->regs.A[ins->ea2.reg], this->state.instruction.result & 0xFFFF, 2, MAKE_FC(0), M68K_RW_ORDER_NORMAL, M68kS_exec);
        STEP(3)
            this->state.instruction.prefetch++;
            M68k_start_prefetch(this, 1, 1, M68kS_exec); // all prefetches are done before writes
        STEP(4)
            this->regs.A[ins->ea2.reg] -= 2;
            M68k_start_write(this, this->regs.A[ins->ea2.reg], this->state.instruction.result >> 16, 2, MAKE_FC(0), M68K_RW_ORDER_NORMAL, M68kS_exec);
        STEP(5)
INS_END

M68KINS(SUBX_sz4_nopredec)
        STEP0
            M68k_start_read_operands(this, 1, ins->sz, M68kS_exec);
        STEP(1)
            this->state.instruction.result = SUB(this, this->state.op[0].val, this->state.op[1].val, ins->sz, 1);
            M68k_start_prefetch(this, 1, 1, M68kS_exec); // all prefetches are done before writes
        STEP(2)
            M68k_start_write_operand(this, 0, 1, M68kS_exec);
        STEP(3)
            M68k_start_wait(this, 4, M68kS_exec);
        STEP(4)
INS_END

M68KINS(SUBX_sz1_2)
        STEP0
            M68k_start_read_operands(this, 1, ins->sz, M68kS_exec);
        STEP(1)
            this->state.instruction.result = SUB(this, this->state.op[0].val, this->state.op[1].val, ins->sz, 1);
            M68k_start_prefetch(this, 1, 1, M68kS_exec); // all prefetches are done before writes
        STEP(2)
            M68k_start_write_operand(this, 0, 1, M68kS_exec);
        STEP(3)
INS_END

M68KINS_NOSWITCH(SUBX)
    if (ins->sz == 4) {
        u32 u = (ins->operand_mode == M68k_OM_ea_ea) || (ins->operand_mode == M68k_OM_r_ea);
        if (u && (ins->ea2.kind == M68k_AM_address_register_indirect_with_predecrement)) {
            return M68k_ins_SUBX_sz4_predec(this, ins);
        }
        else {
            return M68k_ins_SUBX_sz4_nopredec(this, ins);
        }
    } else {
        return M68k_ins_SUBX_sz1_2(this, ins);
    }
INS_END_NOSWITCH


    M68KINS(SWAP)
        STEP0
            BADINS;
INS_END

M68KINS(TRAP)
        STEP0
            //this->regs.IR = this->regs.IRC;
            //this->regs.IRC = 0;
            //this->regs.PC += 2;
            M68k_start_group2_exception(this, ins->ea1.reg + 32, 0, this->regs.PC-2);
        STEP(1)
INS_END

M68KINS(TRAPV)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            if (this->regs.SR.V) M68k_start_group2_exception(this, M68kIV_trapv_instruction, 0, this->regs.PC-4);
        STEP(2)
INS_END

M68KINS(TST)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec);
        STEP(1)
            this->regs.SR.C = this->regs.SR.V = 0;
            u32 v = clip32[ins->sz] & this->state.op[0].val;
            this->regs.SR.Z = v == 0;
            if (ins->sz == 1) v = SIGNe8to32(v);
            else if (ins->sz == 2) v = SIGNe16to32(v);
            this->regs.SR.N = v >= 0x80000000;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
INS_END

M68KINS(UNLK)
        STEP0
            // An --> SP; (SP) --> An; SP + 4 --> SP
            this->state.op[0].addr = this->regs.A[7];
            this->regs.A[7] = this->regs.A[ins->ea1.reg];
            M68k_start_pop(this, 4, MAKE_FC(0), M68kS_exec);
        STEP(1)
            this->regs.A[ins->ea1.reg] = this->state.bus_cycle.data;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
INS_END



static u32 M68k_disasm_BAD(struct M68k_ins_t *ins, struct jsm_debug_read_trace *rt, struct jsm_string *out)
{
    printf("\nERROR UNIMPLEMENTED DISASSEMBLY %04x", ins->opcode);
    jsm_string_sprintf(out, "UNIMPLEMENTED DISASSEMBLY %s", __func__);
    return 0;
}


static void M68k_ins_NOINS(struct M68k* this, struct M68k_ins_t *ins)
{
    printf("\nERROR UNIMPLEMENTED M68K INSTRUCTION %04x at PC %06x", ins->opcode, this->regs.PC);
}

#undef M68KINS
#undef INS_END
#undef STEP0
#undef STEP

static int M68k_already_decoded = 0;

#define MF(x) &M68k_ins_##x
#define MD(x) &M68k_disasm_##x

struct m68k_str_ret
{
    u32 t_max, s_max, ea_mode, ea_reg, size_max, rm;

};

static void transform_ea(struct M68k_EA *ea)
{
    if (ea->kind == 7) {
        if (ea->reg == 1) ea->kind = M68k_AM_absolute_long_data;
        else if (ea->reg == 2) ea->kind = M68k_AM_program_counter_with_displacement;
        else if (ea->reg == 3) ea->kind = M68k_AM_program_counter_with_index;
        else if (ea->reg == 4) ea->kind = M68k_AM_immediate;
    }
}

static void bind_opcode(const char* inpt, u32 sz, M68k_ins_func exec_func, M68k_disassemble_t disasm_func, u32 op_or, struct M68k_EA *ea1, struct M68k_EA *ea2, u32 variant, enum M68k_operand_modes operand_mode)
{
    const char *ptr;
    u32 orer = 0x8000;
    u32 out = 0;

    for (ptr = inpt; orer != 0; ptr++) {
        if (*ptr == ' ') continue;
        if (*ptr == '1') out |= orer;
        orer >>= 1;
    }

    out |= op_or;

    assert(out < 65536); assert(out >= 0);

    enum M68k_address_modes k1, k2;
    if (operand_mode == M68k_OM_ea) {
        if ((ea1->kind == M68k_AM_address_register_direct) || (ea1->kind == M68k_AM_data_register_direct)) {
            operand_mode = M68k_OM_r;
        }
    }
    else if (operand_mode == M68k_OM_ea_r) {
        if ((ea1->kind == M68k_AM_address_register_direct) || (ea1->kind == M68k_AM_data_register_direct)) {
            operand_mode = M68k_OM_r_r;
        }
    }
    else if (operand_mode == M68k_OM_r_ea) {
        if ((ea2->kind == M68k_AM_address_register_direct) || (ea2->kind == M68k_AM_data_register_direct)) {
            operand_mode = M68k_OM_r_r;
        }
    }
    else if (operand_mode == M68k_OM_ea_ea) {
        if (((ea1->kind == M68k_AM_address_register_direct) || (ea1->kind == M68k_AM_data_register_direct)) && ((ea2->kind == M68k_AM_address_register_direct) || (ea2->kind == M68k_AM_data_register_direct))) {
            operand_mode = M68k_OM_r_r;
        }
        else {
            if ((ea1->kind == M68k_AM_address_register_direct) || (ea1->kind == M68k_AM_data_register_direct))
                operand_mode = M68k_OM_r_ea;
            else {
                if ((ea2->kind == M68k_AM_address_register_direct) || (ea2->kind == M68k_AM_data_register_direct)) {
                    operand_mode = M68k_OM_ea_r;
                }
            }
        }
    }
    else if (operand_mode == M68k_OM_qimm_ea) {
        if ((ea2->kind == M68k_AM_address_register_direct) || (ea2->kind == M68k_AM_data_register_direct)) {
            operand_mode = M68k_OM_qimm_r;
        }
    }


    if (operand_mode == M68k_OM_ea) {
        transform_ea(ea1);
    }
    if (operand_mode == M68k_OM_r_ea) {
        transform_ea(ea2);
    }
    if (operand_mode == M68k_OM_ea_r) {
        transform_ea(ea1);
    }
    if (operand_mode == M68k_OM_ea_ea) {
        transform_ea(ea1);
        transform_ea(ea2);
    }
    if (operand_mode == M68k_OM_qimm_ea) {
        transform_ea(ea2);
    }

    struct M68k_ins_t *ins = &M68k_decoded[out];
    //assert(ins->disasm == MD(BAD));
    *ins = (struct M68k_ins_t) {
        .opcode = out,
        .disasm = disasm_func,
        .exec = exec_func,
        .sz = sz,
        .variant = variant,
        .operand_mode = operand_mode
    };
    if (ea1)
        memcpy(&ins->ea1, ea1, sizeof(struct M68k_EA));
    if (ea2)
        memcpy(&ins->ea2, ea2, sizeof(struct M68k_EA));
}

static struct M68k_EA mk_ea(u32 mode, u32 reg) {
    return (struct M68k_EA) {.kind=mode, .reg=reg};
}
#undef BADINS

void do_M68k_decode() {
    if (M68k_already_decoded) return;
    M68k_already_decoded = 1;
    for (u32 i = 0; i < 65536; i++) {
        M68k_decoded[i] = (struct M68k_ins_t) {
                .opcode = i,
                .exec = MF(NOINS),
                .disasm = MD(BADINS),
                .variant = 0
        };
    }

    struct M68k_EA op1, op2;

    // NOLINTNEXTLINE(bugprone-suspicious-include)
    #include "generated/generated_decodes.c"
}
