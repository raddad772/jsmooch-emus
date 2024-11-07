//
// Created by . on 5/15/24.
//

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "m68000.h"
#include "m68000_instructions.h"
#include "generated/generated_disasm.h"

#define ALLOW_REVERSE 1
#define NO_REVERSE 0

#define HOLD 1
#define NO_HOLD 0

#define FORCE_REVERSE 1
#define NO_FORCE 0

struct M68k_ins_t M68k_decoded[65536];

static void M68k_start_push(struct M68k* this, u32 val, u32 sz, enum M68k_states next_state, u32 reverse)
{
    this->regs.A[7] -= sz;
    if (sz == 1) this->regs.A[7]--;
    M68k_start_write(this, this->regs.A[7], val, sz, MAKE_FC(0), reverse, next_state);
}

static void M68k_start_pop(struct M68k* this, u32 sz, u32 FC, enum M68k_states next_state)
{
    M68k_start_read(this, this->regs.A[7], sz, FC, M68K_RW_ORDER_NORMAL, next_state);
    this->regs.A[7] += sz;
    if (sz == 1) this->regs.A[7]++;
    //M68k_inc_SSP(this, sz);
}

static u32 bitsize[5] = { 0, 8, 16, 0, 32 };
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
        default:
            assert(1==0);
    }
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

static u32 CMP(struct M68k* this, u32 source, u32 target, u32 sz) {
    u32 result = (target - source) & clip32[sz];
    u32 carries = target ^ source ^ result;
    u32 overflow = (target ^ result) & (source ^ target);

    this->regs.SR.C = !!((carries ^ overflow) & msb32[sz]);
    this->regs.SR.V = !!(overflow & msb32[sz]);
    this->regs.SR.Z = (result & clip32[sz]) == 0;
    this->regs.SR.N = sgn32(result, sz) < 0;

    return result;
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
        else result &= ~msb32[sz];
    }

    this->regs.SR.C = carry;
    this->regs.SR.V = 0;
    this->regs.SR.Z = (clip32[sz] & result) == 0;
    this->regs.SR.N = !!(msb32[sz] & result);

    return clip32[sz] & result;
}

static u32 ROXL(struct M68k* this, u32 result, u32 shift, u32 sz)
{
    u32 carry = this->regs.SR.X;
    for (u32 i = 0; i < shift; i++) {
        u32 extend = carry;
        carry = !!(result & msb32[sz]);
        result = result << 1 | extend;
    }

    this->regs.SR.C = this->regs.SR.X = carry;
    this->regs.SR.V = 0;
    this->regs.SR.Z = (clip32[sz] & result) == 0;
    this->regs.SR.N = !!(msb32[sz] & result);

    return clip32[sz] & result;
}

static u32 ROXR(struct M68k* this, u32 result, u32 shift, u32 sz)
{
    u32 carry = this->regs.SR.X;
    for (u32 i = 0; i < shift; i++) {
        u32 extend = carry;
        carry = result & 1;
        result >>= 1;
        if (extend) result |= msb32[sz];
        else result &= ~msb32[sz];
    }

    this->regs.SR.C = this->regs.SR.X = carry;
    this->regs.SR.V = 0;
    this->regs.SR.Z = (clip32[sz] & result) == 0;
    this->regs.SR.N = !!(msb32[sz] & result);

    return clip32[sz] & result;
}


static u32 SUB(struct M68k* this, u32 op1, u32 op2, u32 sz, u32 extend, u32 change_x)
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
    if (change_x) this->regs.SR.X = this->regs.SR.C;

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

#define HOLD_WORD_POSTINC             u32 hold = HOLD;\
                    if ((ins->sz == 2) && ((ins->ea[0].kind == M68k_AM_address_register_indirect_with_postincrement) || (ins->ea[1].kind == M68k_AM_address_register_indirect_with_postincrement))) hold = NO_HOLD
#define HOLD_WORD_POSTINC_PREDEC             u32 hold = HOLD;\
                    if ((ins->sz == 2) && ((ins->ea[0].kind == M68k_AM_address_register_indirect_with_predecrement) || (ins->ea[0].kind == M68k_AM_address_register_indirect_with_postincrement) || (ins->ea[1].kind == M68k_AM_address_register_indirect_with_postincrement)) || (ins->ea[1].kind == M68k_AM_address_register_indirect_with_predecrement)) hold = NO_HOLD
#define HOLDL_l_predec_no if ((ins->sz == 4) && ((ins->ea[0].kind == M68k_AM_address_register_indirect_with_predecrement) || (ins->ea[1].kind == M68k_AM_address_register_indirect_with_predecrement))) hold = NO_HOLD
#define HOLDW_imm_predec_no if ((ins->sz == 2) && (ins->ea[0].kind == M68k_AM_quick_immediate) && (ins->ea[1].kind == M68k_AM_address_register_indirect_with_predecrement)) hold = 0;


#define DELAY_IF_REGWRITE(k, n) if ((ins->sz == 4) && ((k == M68k_AM_data_register_direct) || (k == M68k_AM_address_register_direct))) M68k_start_wait(this, n, M68kS_exec);

#define export_M68KINS(x) void M68k_ins_##x(struct M68k* this, struct M68k_ins_t *ins) { \
    switch(this->state.instruction.TCU) {

#define M68KINS(x) static void M68k_ins_##x(struct M68k* this, struct M68k_ins_t *ins) { \
    switch(this->state.instruction.TCU) {

#define export_M68KINS_NOSWITCH(x) void M68k_ins_##x(struct M68k* this, struct M68k_ins_t *ins) {

#define M68KINS_NOSWITCH(x) static void M68k_ins_##x(struct M68k* this, struct M68k_ins_t *ins) {
#define INS_END_NOSWITCH }

static void M68k_set_IPC(struct M68k* this) {
    this->regs.IPC = this->regs.PC;
}

#define STEP0 case 0: { \
            M68k_set_IPC(this);\
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


void M68k_ins_RESET_POWER(struct M68k* this, struct M68k_ins_t *ins) {
    switch (this->state.instruction.TCU) {
        STEP0
            this->pins.RESET = 0;
            M68k_start_read(this, 0, 2, M68k_FC_supervisor_program, M68K_RW_ORDER_NORMAL, M68kS_exec);
        STEP(1)
            // First word will be
            this->state.instruction.result = this->state.bus_cycle.data << 16;
            M68k_start_read(this, 2, 2, M68k_FC_supervisor_program, M68K_RW_ORDER_NORMAL, M68kS_exec);
        STEP(2)
            M68k_set_SSP(this, this->state.instruction.result | this->state.bus_cycle.data);
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

M68KINS(ALINE)
        STEP0
            M68k_start_group2_exception(this, M68kIV_line1010, 4, this->regs.PC-2);
        STEP(1)
INS_END

M68KINS(FLINE)
        STEP0
            M68k_start_group2_exception(this, M68kIV_line1111, 4, this->regs.PC-2);
        STEP(1)
INS_END

M68KINS(TAS)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, 0, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.instruction.result = this->state.op[0].val | 0x80;
            M68k_start_wait(this, 2, M68kS_exec);
        STEP(2)
            // Write or wait
            if ((!this->megadrive_bug) || (ins->ea[0].kind == M68k_AM_data_register_direct)) {
                M68k_start_write_operand(this, 0, 0, M68kS_exec, 1, 0);
            } else {
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
            M68k_start_read_operands(this, 1, ins->sz, M68kS_exec, 0, 0, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
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
            M68k_start_write_operand(this, 0, 1, M68kS_exec, 1, 0);
        STEP(3)
            if (ins->ea[0].kind == M68k_AM_data_register_direct)
                M68k_start_wait(this, 2, M68kS_exec);
        STEP(4)
INS_END

M68KINS(ADD)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.instruction.result = ADD(this, this->state.op[0].val, this->state.op[1].val, ins->sz, 0);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_write_operand(this, 0, 1, M68kS_exec, 1, 0);
        STEP(3)
            if (ins->sz == 4) {
                if (ins->ea[1].kind == M68k_AM_data_register_direct) {
                    switch (ins->ea[0].kind) {
                        case M68k_AM_data_register_direct:
                        case M68k_AM_address_register_direct:
                        case M68k_AM_immediate:
                            M68k_start_wait(this, 4, M68kS_exec);
                            break;
                        default:
                            M68k_start_wait(this, 2, M68kS_exec);
                    }
                }
            }
        STEP(4)
INS_END

M68KINS(ADDA)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            u32 a = sgn32(this->state.op[0].val, ins->sz);
            u32 b = this->regs.A[ins->ea[1].reg];
            this->state.instruction.result = b + a;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            u32 n = ins->ea[1].reg;
            M68k_set_ar(this, n, this->state.instruction.result, 4);
            if ((ins->sz != 4) || (ins->ea[0].kind == M68k_AM_data_register_direct) || (ins->ea[0].kind == M68k_AM_address_register_direct) || (ins->ea[0].kind == M68k_AM_immediate))
                M68k_start_wait(this, 4, M68kS_exec);
            else
                M68k_start_wait(this, 2, M68kS_exec);
        STEP(3)
INS_END

#define STARTI(force, allow) \
            this->state.instruction.result = this->regs.IRC;\
            M68k_start_prefetch(this, (ins->sz >> 1) ? (ins->sz >> 1) : 1, 1, M68kS_exec);\
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
            }  \
            this->regs.IPC += 4;                            \
            HOLD_WORD_POSTINC_PREDEC; \
            HOLDL_l_predec_no;                              \
        M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, (force ? allow : (allow & hold)), NO_REVERSE, MAKE_FC(0));


M68KINS(ADDI)
        STEP0
        if (ins->sz == 2) this->regs.IPC -= 2;
        STARTI(0,1);
        STEP(2)
            M68k_sample_interrupts(this);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(3)
            this->state.instruction.result = ADD(this, this->state.instruction.result, this->state.op[0].val, ins->sz,
                                                 0);
            M68k_start_write_operand(this, 0, 0, M68kS_exec, 1, 0);
        STEP(4)
            if ((ins->sz == 4) && (ins->ea[0].kind == M68k_AM_data_register_direct))
                M68k_start_wait(this, 4, M68kS_exec);
        STEP(5)
INS_END

M68KINS(ADDQ_ar)
        STEP0
            this->state.instruction.result = this->regs.A[ins->ea[1].reg] + ins->ea[0].reg;
            M68k_sample_interrupts(this);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            M68k_set_ar(this, ins->ea[1].reg, this->state.instruction.result, 4);
            M68k_start_wait(this, 4, M68kS_exec);
        STEP(2)
INS_END

M68KINS(ADDQ)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.instruction.result = ADD(this, this->state.op[0].val, this->state.op[1].val, ins->sz, 0);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_write_operand(this, 0, 1, M68kS_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
            if (ins->sz == 4) {
                if ((ins->ea[1].kind == M68k_AM_data_register_direct) && (ins->sz == 4)) {
                    if (ins->ea[0].kind == M68k_AM_address_register_direct)
                        M68k_start_wait(this, 6, M68kS_exec);
                    else
                        M68k_start_wait(this, 4, M68kS_exec);
                }
            }
        STEP(4)
INS_END

M68KINS(ADDX_sz4_predec)
        STEP0
            M68k_start_read_operands(this, 1, ins->sz, M68kS_exec, 0, HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.instruction.result = ADD(this, this->state.op[0].val, this->state.op[1].val, ins->sz, 1);
            this->regs.A[ins->ea[1].reg] += 2;
            M68k_start_write(this, this->regs.A[ins->ea[1].reg], this->state.instruction.result & 0xFFFF, 2, MAKE_FC(0),
                             M68K_RW_ORDER_NORMAL, M68kS_exec);
        STEP(2)
            //this->state.instruction.prefetch++;
            M68k_start_prefetch(this, 1, 1, M68kS_exec); // all prefetches are done before writes
        STEP(3)
            this->regs.A[ins->ea[1].reg] -= 2;
            M68k_start_write(this, this->regs.A[ins->ea[1].reg], this->state.instruction.result >> 16, 2, MAKE_FC(0),
                             M68K_RW_ORDER_NORMAL, M68kS_exec);
        STEP(4)
INS_END

M68KINS(ADDX_sz4_nopredec)
        STEP0
            M68k_start_read_operands(this, 1, ins->sz, M68kS_exec, 0, 0, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.instruction.result = ADD(this, this->state.op[0].val, this->state.op[1].val, ins->sz, 1);
            M68k_start_prefetch(this, 1, 1, M68kS_exec); // all prefetches are done before writes
        STEP(2)
            M68k_start_write_operand(this, 0, 1, M68kS_exec, 1, 0);
        STEP(3)
            M68k_start_wait(this, 4, M68kS_exec);
        STEP(4)
INS_END


M68KINS(ADDX_sz1_2)
        STEP0
            M68k_start_read_operands(this, 1, ins->sz, M68kS_exec, 0, 0, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.instruction.result = ADD(this, this->state.op[0].val, this->state.op[1].val, ins->sz, 1);
            M68k_start_prefetch(this, 1, 1, M68kS_exec); // all prefetches are done before writes
        STEP(2)
            M68k_start_write_operand(this, 0, 1, M68kS_exec, 1, 0);
        STEP(3)
INS_END

M68KINS_NOSWITCH(ADDX)
    if (ins->sz == 4) {
        u32 u = (ins->operand_mode == M68k_OM_ea_ea) || (ins->operand_mode == M68k_OM_r_ea);
        if (u && (ins->ea[1].kind == M68k_AM_address_register_indirect_with_predecrement)) {
            return M68k_ins_ADDX_sz4_predec(this, ins);
        } else {
            return M68k_ins_ADDX_sz4_nopredec(this, ins);
        }
    } else {
        return M68k_ins_ADDX_sz1_2(this, ins);
    }
INS_END_NOSWITCH

M68KINS(AND)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            if (ins->ea[1].kind == M68k_AM_address_register_indirect_with_predecrement) hold = 0;
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            this->state.instruction.result = AND(this, this->state.op[0].val, this->state.op[1].val, ins->sz);
            M68k_start_write_operand(this, 0, 1, M68kS_exec, 1, 0);
            if ((ins->sz == 4) && (ins->ea[1].kind == M68k_AM_data_register_direct)) {
                u32 numcycles = 2;
                if (ins->ea[0].kind == M68k_AM_data_register_direct) numcycles = 4;
                if (ins->ea[0].kind == M68k_AM_immediate) numcycles = 4;
                M68k_start_wait(this, numcycles, M68kS_exec);
            }
        STEP(4)
INS_END

M68KINS(ANDI)
        STEP0
            if (ins->sz == 2)
                this->regs.IPC -= 2;
        STARTI(0,1);
        STEP(2)
            M68k_sample_interrupts(this);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(3)
            this->state.instruction.result = AND(this, this->state.instruction.result, this->state.op[0].val, ins->sz);
            M68k_start_write_operand(this, 0, 0, M68kS_exec, 1, 0);
        STEP(4)
            if ((ins->sz == 4) && ((ins->ea[0].kind == M68k_AM_data_register_direct) ||
                                   (ins->ea[0].kind == M68k_AM_address_register_direct))) {
                M68k_start_wait(this, 4, M68kS_exec);
            }
        STEP(5)
INS_END

M68KINS(ANDI_TO_CCR)
        STEP0
            M68k_sample_interrupts(this);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            u32 v = this->regs.SR.u & 0x1F;
            v &= this->regs.IR;
            this->regs.SR.u = (this->regs.SR.u & 0xFFE0) | v;
            M68k_start_wait(this, 8, M68kS_exec);
        STEP(3)
            M68k_start_read(this, this->regs.PC - 2, 2, MAKE_FC(1), M68K_RW_ORDER_NORMAL, M68kS_exec);
        STEP(4)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(5)
INS_END

M68KINS(ANDI_TO_SR)
        STEP0
            if (!this->regs.SR.S) {
                M68k_start_group2_exception(this, M68kIV_privilege_violation, 4, this->regs.PC - 2);
                return;
            }
            switch(ins->ea[0].kind) {
                case 0:
                    this->regs.IPC -= 2;
                    break;
            }
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            u32 data = this->regs.IR & M68k_get_SR(this);
            M68k_set_SR(this, data, 1);
            M68k_sample_interrupts(this);
            M68k_start_wait(this, 8, M68kS_exec);
        STEP(2)
            M68k_start_read(this, this->regs.PC - 2, 2, MAKE_FC(1), 0, M68kS_exec);
        STEP(3)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(4)
INS_END

M68KINS(ASL_qimm_dr)
        STEP0
            M68k_sample_interrupts(this);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + ins->ea[0].reg * 2, M68kS_exec);
        STEP(2)
            this->state.instruction.result = ASL(this, this->regs.D[ins->ea[1].reg], ins->ea[0].reg, ins->sz);
            M68k_set_dr(this, ins->ea[1].reg, this->state.instruction.result, ins->sz);
        STEP(3)
INS_END

M68KINS(ASL_dr_dr)
        STEP0
            M68k_sample_interrupts(this);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            u32 cnt = this->regs.D[ins->ea[0].reg] & 63;
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + cnt * 2, M68kS_exec);
        STEP(2)
            u32 cnt = this->regs.D[ins->ea[0].reg] & 63;
            this->state.instruction.result = ASL(this, this->regs.D[ins->ea[1].reg], cnt, ins->sz);
            M68k_set_dr(this, ins->ea[1].reg, this->state.instruction.result, ins->sz);
INS_END

M68KINS(ASL_ea)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, 0, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.instruction.result = ASL(this, this->state.op[0].val, 1, ins->sz);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_write_operand(this, 0, 0, M68kS_exec, 1, 0);
        STEP(3)
INS_END

M68KINS(ASR_qimm_dr)
        STEP0
            M68k_sample_interrupts(this);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + ins->ea[0].reg * 2, M68kS_exec);
        STEP(2)
            this->state.instruction.result = ASR(this, this->regs.D[ins->ea[1].reg], ins->ea[0].reg, ins->sz);
            M68k_set_dr(this, ins->ea[1].reg, this->state.instruction.result, ins->sz);
        STEP(3)
INS_END

M68KINS(ASR_dr_dr)
        STEP0
            M68k_sample_interrupts(this);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            u32 cnt = this->regs.D[ins->ea[0].reg] & 63;
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + cnt * 2, M68kS_exec);
        STEP(2)
            u32 cnt = this->regs.D[ins->ea[0].reg] & 63;
            this->state.instruction.result = ASR(this, this->regs.D[ins->ea[1].reg], cnt, ins->sz);
            M68k_set_dr(this, ins->ea[1].reg, this->state.instruction.result, ins->sz);
INS_END

M68KINS(ASR_ea)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, 0, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.instruction.result = ASR(this, this->state.op[0].val, 1, ins->sz);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_write_operand(this, 0, 0, M68kS_exec, 1, 0);
        STEP(3)
INS_END

M68KINS(BCC)
        STEP0
            this->state.instruction.result = condition(this, ins->ea[0].reg);
            this->regs.IPC -= 4;
            M68k_sample_interrupts(this);
            M68k_start_wait(this, this->state.instruction.result ? 2 : 4, M68kS_exec);
        STEP(1)
            if (!this->state.instruction.result) {
                this->regs.IPC += (ins->ea[1].reg == 0) ? 0 : 2;
                M68k_start_prefetch(this, (ins->ea[1].reg == 0) ? 2 : 1, 1, M68kS_exec);
            } else {
                u32 cnt = SIGNe8to32(ins->ea[1].reg);
                if (cnt == 0) cnt = SIGNe16to32(this->regs.IRC);
                this->regs.PC -= 2;
                //this->regs.IPC -= 2;
                this->regs.IPC += 2;
                this->regs.PC += cnt;
                M68k_start_prefetch(this, 2, 1, M68kS_exec);
            }
        STEP(2)
INS_END

M68KINS(BCHG_dr_ea)
        STEP0
            // test then invert a bit. size 4 or 1
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, NO_HOLD, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.op[0].val &= bitsize[ins->sz] - 1;
            this->state.instruction.result = this->state.op[1].val;
            this->regs.SR.Z = (this->state.instruction.result & (1 << this->state.op[0].val)) == 0;
            this->state.instruction.result ^= 1 << this->state.op[0].val;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_write_operand(this, 0, 1, M68kS_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
            u32 numcycles = 0;
            if ((ins->sz == 4) && (ins->ea[1].kind == M68k_AM_data_register_direct)) {
                if (ins->ea[0].kind == M68k_AM_data_register_direct) numcycles = 2;
                else numcycles = 4;
            }
            if (ins->ea[0].kind == M68k_AM_immediate) {
                numcycles = 4;
            }
            if (this->state.op[0].val >= 16) {
                numcycles = 4;
            }
            if (numcycles) M68k_start_wait(this, numcycles, M68kS_exec);
        STEP(4)
INS_END

M68KINS(BCHG_ea)
        STEP0
            this->state.op[1].val = this->regs.IRC;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(2)
            M68k_sample_interrupts(this);
            this->state.instruction.result = this->state.op[0].val;
            this->state.op[0].val = this->state.op[1].val & (bitsize[ins->sz] - 1);
            this->regs.SR.Z = (this->state.instruction.result & (1 << this->state.op[0].val)) == 0;
            this->state.instruction.result ^= 1 << this->state.op[0].val;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(3)
            M68k_start_write_operand(this, 0, 0, M68kS_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(4)
            u32 nomc = 0;
            if (this->state.op[0].val >= 16) nomc = 2;
            if ((ins->sz == 4) && (ins->ea[0].kind == M68k_AM_data_register_direct)) {
                //nomc += this->regs.SR.Z ? 4 : 2;
                nomc += 2;
            }
            if (nomc > 4) nomc = 4;
            if (nomc)
                M68k_start_wait(this, nomc, M68kS_exec);
        STEP(5)
INS_END

M68KINS(BCLR_dr_ea)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, NO_HOLD, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.op[0].val &= bitsize[ins->sz] - 1;
            this->state.instruction.result = this->state.op[1].val;
            this->regs.SR.Z = (this->state.instruction.result & (1 << this->state.op[0].val)) == 0;
            this->state.instruction.result &= (~(1 << this->state.op[0].val)) & clip32[ins->sz];
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_write_operand(this, 0, 1, M68kS_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
            u32 numcycles = 0;
            if (ins->ea[1].kind == M68k_AM_immediate) {
                numcycles = 2;
            }
            if (ins->ea[1].kind == M68k_AM_data_register_direct) {
                if (ins->ea[0].kind == M68k_AM_data_register_direct) numcycles = 2;
                    else numcycles = 4;
                numcycles = 4;
            }
            if (ins->ea[0].kind == M68k_AM_immediate) {
                numcycles = 4;
            }
            if (this->state.op[0].val >= 16) {
                numcycles = numcycles ? 6 : 4;
            }
            if (numcycles) M68k_start_wait(this, numcycles, M68kS_exec);
        STEP(4)
INS_END

M68KINS(BCLR_ea)
        STEP0
            this->state.op[1].val = this->regs.IRC;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(2)
            M68k_sample_interrupts(this);
            this->state.instruction.result = this->state.op[0].val;
            this->state.op[0].val = this->state.op[1].val & (bitsize[ins->sz] - 1);
            this->regs.SR.Z = (this->state.instruction.result & (1 << this->state.op[0].val)) == 0;
            this->state.instruction.result &= (~(1 << this->state.op[0].val)) & clip32[ins->sz];
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(3)
            M68k_start_write_operand(this, 0, 0, M68kS_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(4)
            u32 nomc = 0;
            if (this->state.op[0].val >= 16) nomc = 2;
            if ((ins->sz == 4) && (ins->ea[0].kind == M68k_AM_data_register_direct)) {
                //if (ins->ea[1].kind == M68k_AM_data_register_direct) nomc += 2;
                //else nomc += 4;
                nomc += 4;
            }
            if (nomc > 6) nomc = 6;
            if (nomc)
                M68k_start_wait(this, nomc, M68kS_exec);
        STEP(5)
INS_END

M68KINS(BRA)
        STEP0
            M68k_sample_interrupts(this);
            M68k_start_wait(this, 2, M68kS_exec);
        STEP(1)
            u32 cnt = SIGNe8to32(ins->ea[0].reg);
            if (cnt == 0) cnt = SIGNe16to32(this->regs.IRC);
            this->regs.PC -= 2;
            this->regs.IPC -= 2;
            this->regs.PC += cnt;
            M68k_start_prefetch(this, 2, 1, M68kS_exec);
        STEP(2)
INS_END

M68KINS(BSET_dr_ea)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, NO_HOLD, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.op[0].val &= bitsize[ins->sz] - 1;
            this->state.instruction.result = this->state.op[1].val;
            this->regs.SR.Z = (this->state.instruction.result & (1 << this->state.op[0].val)) == 0;
            this->state.instruction.result |= 1 << this->state.op[0].val;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_write_operand(this, 0, 1, M68kS_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
            u32 numcycles = 0;
            if (ins->ea[1].kind == M68k_AM_immediate) {
                numcycles = 2;
            }
            if (ins->ea[1].kind == M68k_AM_data_register_direct) {
                if (ins->ea[0].kind == M68k_AM_data_register_direct) numcycles = 2;
                //else numcycles = 4;
                else numcycles = 4;
            }
            if (ins->ea[0].kind == M68k_AM_immediate) {
                numcycles = 4;
            }
            if (this->state.op[0].val >= 16) {
                numcycles = 4;
            }
            if (numcycles) M68k_start_wait(this, numcycles, M68kS_exec);
        STEP(4)
INS_END

M68KINS(BSET_ea)
        STEP0
            this->state.op[1].val = this->regs.IRC;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(2)
            M68k_sample_interrupts(this);
            this->state.instruction.result = this->state.op[0].val;
            this->state.op[0].val = this->state.op[1].val & (bitsize[ins->sz] - 1);
            this->regs.SR.Z = (this->state.instruction.result & (1 << this->state.op[0].val)) == 0;
            this->state.instruction.result |= 1 << this->state.op[0].val;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(3)
            M68k_start_write_operand(this, 0, 0, M68kS_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(4)
            u32 nomc = 0;
            if (this->state.op[0].val >= 16) nomc = 2;
            if ((ins->sz == 4) && (ins->ea[0].kind == M68k_AM_data_register_direct)) {
                if (ins->ea[1].kind == M68k_AM_data_register_direct) nomc += 2;
                else nomc += 4;
            }
            if (nomc > 4) nomc = 4;
            if (nomc)
                M68k_start_wait(this, nomc, M68kS_exec);
        STEP(5)
INS_END

M68KINS(BSR)
        STEP0
            M68k_sample_interrupts(this);
            M68k_start_wait(this, 2, M68kS_exec);
        STEP(1)
            this->regs.A[7] -= 4;
            if (ins->ea[0].reg != 0) {
                M68k_start_write(this, this->regs.A[7], this->regs.PC - 2, 4, MAKE_FC(0), M68K_RW_ORDER_NORMAL,
                                 M68kS_exec);
            } else {
                M68k_start_write(this, this->regs.A[7], this->regs.PC, 4, MAKE_FC(0), M68K_RW_ORDER_NORMAL, M68kS_exec);
            }
            this->regs.PC -= 2;
        STEP(2)
            i32 offset;
            if (ins->ea[0].reg != 0) {
                offset = ins->ea[0].reg;
                offset = SIGNe8to32(offset);
            } else {
                offset = SIGNe16to32(this->regs.IRC);
            }

            this->regs.PC += offset;
            this->regs.IPC = this->regs.PC;
            M68k_start_prefetch(this, 2, 1, M68kS_exec);
        STEP(3)
INS_END

M68KINS(BTST_dr_ea)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, NO_HOLD, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.op[0].val &= bitsize[ins->sz] - 1;
            this->state.instruction.result = this->state.op[1].val;
            this->regs.SR.Z = (this->state.instruction.result & (1 << this->state.op[0].val)) == 0;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            u32 numcycles = 0;
            if (ins->ea[1].kind == M68k_AM_immediate) {
                numcycles = 2;
            }
            if ((ins->sz == 4) && (ins->ea[1].kind == M68k_AM_data_register_direct)) {
                //if (ins->ea[0].kind == M68k_AM_data_register_direct) numcycles = 2;
                //else numcycles = 4;
                numcycles = 2;
            }
            if (ins->ea[0].kind == M68k_AM_immediate) {
                numcycles = 4;
            }
            if (this->state.op[0].val >= 16) {
                numcycles = numcycles ? numcycles : 2;
            }
            if (numcycles) M68k_start_wait(this, numcycles, M68kS_exec);
        STEP(3)
INS_END

M68KINS(BTST_ea)
        STEP0
            this->state.op[1].val = this->regs.IRC;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(2)
            M68k_sample_interrupts(this);
            this->state.instruction.result = this->state.op[0].val;
            this->state.op[0].val = this->state.op[1].val & (bitsize[ins->sz] - 1);
            //if (dbg.trace_on) dbg_printf("\nTEST: %02x, BIT: %d", this->state.instruction.result, this->state.op[1].val);
            this->regs.SR.Z = (this->state.instruction.result & (1 << this->state.op[0].val)) == 0;
            //dbg_printf("\nBIT: %d result: %d", this->state.op[0].val, (this->state.instruction.result & (1 << this->state.op[0].val)) == 0);
            //if (dbg.trace_on) dbg_printf("\nZ set to %d", this->regs.SR.Z);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(3)
            u32 nomc = 0;
            if (this->state.op[0].val >= 16) nomc = 2;
            if ((ins->sz == 4) && (ins->ea[0].kind == M68k_AM_data_register_direct)) {
                //if (ins->ea[1].kind == M68k_AM_data_register_direct) nomc = 2;
                //else nomc = 4;
                nomc = 2;
            }
            if (nomc > 4) nomc = 4;
            if (nomc)
                M68k_start_wait(this, nomc, M68kS_exec);
        STEP(5)
INS_END

M68KINS(CHK)
        STEP0
            M68k_start_read_operands(this, 0, 2, M68kS_exec, 0, 0, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            if (ins->ea[0].kind == M68k_AM_address_register_indirect) M68k_start_wait(this, 2, M68kS_exec);
        STEP(2)
            u16 a = this->state.op[1].val;
            u16 b = this->state.op[0].val;
            u32 r = b - a;
            this->regs.SR.u = this->regs.SR.u & 0xFFF0;
            if (!(r & 0xffff))
                this->regs.SR.Z = 1;
            if (r & 0x8000)
                this->regs.SR.N = 1;
            if (r & 0x10000)
                this->regs.SR.C = 1;
            if (((b & (~a) & ~r) | ((~b) & a & r)) & 0x8000)
                this->regs.SR.V = 1;

            u32 m_t = !!(!(this->regs.SR.N || this->regs.SR.V));

            u16 er = 0xFFFF & a;
            this->regs.SR.u = (this->regs.SR.u & 0xFFF0); // keep X
            if (!er)
                this->regs.SR.Z = 1;
            if (er & 0x8000)
                this->regs.SR.N = 1;
            this->regs.SR.C = this->regs.SR.V = 0;

            if (!m_t) {
                i32 num = 8;
                if (ins->ea[0].kind == M68k_AM_address_register_indirect) num = 6;
                M68k_start_group2_exception(this, M68kIV_chk_instruction, num, this->regs.PC);
                return;
            }
        STEP(3)
            if (this->regs.SR.N) {
                i32 num = 10;
                if (ins->ea[0].kind == M68k_AM_address_register_indirect) num = 8;
                M68k_start_group2_exception(this, M68kIV_chk_instruction, num, this->regs.PC);
                return;
            }
        STEP(4)
            u32 num = 6;
            if (ins->ea[0].kind == M68k_AM_address_register_indirect) num = 4;
            M68k_start_wait(this, num, M68kS_exec);
        STEP(5)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(6)
INS_END

M68KINS(CLR)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_sample_interrupts(this);
            this->state.instruction.result = 0;
            M68k_start_write_operand(this, 0, 0, M68kS_exec, 1, 0);
        STEP(3)
            this->regs.SR.C = this->regs.SR.V = this->regs.SR.N = 0;
            this->regs.SR.Z = 1;
            if ((ins->sz == 4) && ((ins->ea[0].kind == M68k_AM_data_register_direct) ||
                                   (ins->ea[0].kind == M68k_AM_address_register_direct)))
                M68k_start_wait(this, 2, M68kS_exec);
        STEP(4)
INS_END

M68KINS(CMP)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            CMP(this, sgn32(this->state.op[0].val, ins->sz), sgn32(this->state.op[1].val, ins->sz), ins->sz);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            if (ins->sz == 4) M68k_start_wait(this, 2, M68kS_exec);
        STEP(3)
INS_END

M68KINS(CMPA)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            // Ignore size for operand 2
            M68k_sample_interrupts(this);
            CMP(this, sgn32(this->state.op[0].val, ins->sz), this->regs.A[ins->ea[1].reg], 4);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_wait(this, 2, M68kS_exec);
        STEP(3)
INS_END

M68KINS(CMPI)
        STEP0
            if (ins->sz == 2) this->regs.IPC -= 2;
            //this->regs.IPC -= 2;
            STARTI(0, 1);
        STEP(2)
            M68k_sample_interrupts(this);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(3)
            CMP(this, sgn32(this->state.instruction.result, ins->sz), sgn32(this->state.op[0].val, ins->sz), ins->sz);
            if ((ins->sz == 4) && (ins->ea[0].kind == M68k_AM_data_register_direct)) M68k_start_wait(this, 2, M68kS_exec);
        STEP(4)
INS_END

M68KINS(CMPM)
        STEP0
            // reg0+, reg1+
            u32 rn = ins->ea[0].reg;
            M68k_start_read(this, this->regs.A[rn], ins->sz, MAKE_FC(0), 0, M68kS_exec);
            if (ins->sz != 4) {
                this->regs.A[ins->ea[0].reg] += ins->sz;
                if ((ins->sz == 1) && (ins->ea[0].reg == 7)) this->regs.A[7]++;
            }
            else this->regs.A[ins->ea[0].reg] += 2;
        STEP(1)
            if (ins->sz == 4) this->regs.A[ins->ea[0].reg] += 2;
            this->state.op[0].val = this->state.bus_cycle.data;
            u32 rn = ins->ea[1].reg;
            M68k_start_read(this, this->regs.A[rn], ins->sz, MAKE_FC(0), 0, M68kS_exec);
        STEP(2)
            this->state.op[1].val = this->state.bus_cycle.data;
            u32 rn = ins->ea[1].reg;
            this->regs.A[rn] += ins->sz;
            if ((ins->sz == 1) && (rn == 7)) this->regs.A[rn]++;

            M68k_sample_interrupts(this);
            CMP(this, sgn32(this->state.op[0].val, ins->sz), sgn32(this->state.op[1].val, ins->sz), ins->sz);

            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(3)
INS_END

M68KINS(DBCC)
        STEP0
          // idle(2);
            M68k_sample_interrupts(this);
            M68k_start_wait(this, 2, M68kS_exec);
        STEP(1)
            this->regs.PC -= 2;
            this->state.instruction.temp = !condition(this, ins->ea[0].reg);

            if (this->state.instruction.temp) {
                //   auto disp = sign<Word>(r.irc);
                this->state.instruction.temp2 = sgn32(this->regs.IRC, 2);
                //  r.pc += disp;
                this->regs.PC += this->state.instruction.temp2;
                //  prefetch();
                M68k_start_prefetch(this, 1, 1, M68kS_exec);
            }
            else {
                //  idle(2);
                M68k_start_wait(this, 2, M68kS_exec);
            }
        STEP(2)
            if (this->state.instruction.temp) {
                //  n16 result = read<Word>(with);
                M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, NO_HOLD, NO_REVERSE, MAKE_FC(0));
            }
            else {
                //  r.pc += 2;
                this->regs.PC += 2;
            }
        STEP(3)
            if (this->state.instruction.temp) {
                //  write<Word>(with, result - 1);
                this->state.instruction.result = (this->state.op[1].val - 1) & 0xFFFF;
                M68k_start_write_operand(this, 0, 1, M68kS_exec, ALLOW_REVERSE, NO_REVERSE);
                //this->state.instruction.result = (this->state.op[1].val + 1) & 0xFFFF;
            }
            else {
            }
        STEP(4)
            if (this->state.instruction.temp) {
                if (this->state.instruction.result != 0xFFFF) { // Take branch
                    //    // branch taken
                    //    prefetch();
                    M68k_start_prefetch(this, 1, 1, M68kS_exec);
                } else {
                    //    // branch not taken
                    //    r.pc -= disp;
                    this->regs.PC -= this->state.instruction.temp2;
                }
            }
            else {
            }
        STEP(5)
            if (this->state.instruction.temp) {
                if (this->state.instruction.result != 0xFFFF) { // Take branch
                    //    return;
                    this->state.instruction.done = 1;
                    return;
                }
            }
            //prefetch();
            //prefetch();
            M68k_start_prefetch(this, 2, 1, M68kS_exec);
        STEP(6)
INS_END

M68KINS(DIVS)
        STEP0
            M68k_start_read_operands(this, 0, 2, M68kS_exec, 0, NO_HOLD, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            i32 divisor = sgn32(this->state.op[0].val, 2);
            i32 dividend =(i32)this->regs.D[ins->ea[1].reg];

            if (divisor == 0) {
                M68k_start_group2_exception(this, M68kIV_zero_divide, 4, this->regs.PC - 4);
                this->regs.SR.u &= 0xFFF0; // Clear ZNVC
                break;
            }

            i32 result = dividend / divisor;
            i32 quotient = dividend % divisor;

            this->regs.SR.C = this->regs.SR.Z = 0;
            u32 ticks = 8;
            if (dividend < 0)
                ticks += 2;
           u32 divi32 = (u32)abs(dividend);
           u32 divo32 = (u32)abs(divisor) << 16;
           if ((divi32 >= divo32) && (divisor != -32768)) {
               // Overflow (detected before calculation)
               M68k_start_wait(this, ticks+4, M68kS_exec);
               this->regs.SR.V = this->regs.SR.N = 1;
               this->state.instruction.result = dividend;
               break;
            }

            // Simulate the cycle time
            if (divisor < 0) {
                // +2 for negative divisor
                ticks += 2;
            }
            else if (dividend < 0) {
                // +4 for positive divisor, negative dividend
                ticks += 4;
            }
            u16 zeroes_num = (abs(result) | 1);
            u32 zeroes_count = 16 - __builtin_popcount(zeroes_num);
            ticks += 108 + zeroes_count*2;

            this->state.instruction.result = (((u32)(u16)quotient) << 16) | (result & 0xFFFF);
            if ((result > 32767) || (result < -32768)) {
                this->regs.SR.V = 1;
                this->regs.SR.N = 1;
                this->state.instruction.result = dividend;
                M68k_start_wait(this, ticks+0, M68kS_exec);
                break;
            }
            M68k_start_wait(this, ticks, M68kS_exec);

            this->regs.SR.Z = result == 0;
            this->regs.SR.N = !!(result & 0x8000);
            this->regs.SR.V = 0;
        STEP(2)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
            this->regs.D[ins->ea[1].reg] = this->state.instruction.result;
        STEP(3)
INS_END

export_M68KINS(DIVU)
        STEP0
            M68k_start_read_operands(this, 0, 2, M68kS_exec, 0, NO_HOLD, NO_REVERSE, MAKE_FC(0));
            this->state.instruction.temp = 0;
            this->state.instruction.temp2 = 0;
        STEP(1)
            M68k_sample_interrupts(this);
            u32 divisor = this->state.op[0].val;
            u32 dividend = this->regs.D[ins->ea[1].reg];
            if (divisor == 0) {
                M68k_start_group2_exception(this, M68kIV_zero_divide, 4, this->regs.PC - 4);
                break;
            }
            u32 result = dividend / divisor;
            u32 quotient = dividend % divisor;

            u32 ticks = 0;

            this->regs.SR.C = this->regs.SR.Z = 0;
            ticks += 6;
            this->state.instruction.result = (quotient << 16) | result;
            if (result > 0xFFFF) {
                this->regs.SR.V = 1;
                this->regs.SR.N = 1;
                M68k_start_wait(this, 6, M68kS_exec);
                this->state.instruction.result = dividend;
                break;
            }

            divisor <<= 16;

            u32 last_msb = 0;
            for (u32 i = 0; i < 15; i++) {
                ticks += 4;
                last_msb = (dividend & 0x80000000) != 0;
                dividend <<= 1;
                if (!last_msb) {
                    ticks += 2;
                    if (dividend < divisor) {
                        ticks += 2;
                    }
                }

                if (last_msb || (dividend >= divisor)) {
                    dividend -= divisor;
                }
            }
            ticks += 6;
            M68k_start_wait(this, ticks, M68kS_exec);
            this->regs.SR.Z = result == 0;
            this->regs.SR.N = !!(result & 0x8000);
            this->regs.SR.V = 0;
        STEP(2)
            this->regs.D[ins->ea[1].reg] = this->state.instruction.result;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(3)
INS_END


M68KINS(EOR)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            this->state.instruction.result = EOR(this, this->state.op[0].val, this->state.op[1].val, ins->sz);
            M68k_start_write_operand(this, 0, 1, M68kS_exec, 1, 0);
        STEP(3)
            if ((ins->sz == 4) && (ins->ea[1].kind == M68k_AM_data_register_direct))
                M68k_start_wait(this, 4, M68kS_exec);
        STEP(4)
INS_END

M68KINS(EORI)
        STEP0
            if (ins->sz == 2) this->regs.IPC -= 2;
            STARTI(0,1);
        STEP(2)
            M68k_sample_interrupts(this);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(3)
            this->state.instruction.result = EOR(this, this->state.instruction.result, this->state.op[0].val, ins->sz);
            M68k_start_write_operand(this, 0, 0, M68kS_exec, 1, 0);
        STEP(4)
            if ((ins->sz == 4) && ((ins->ea[0].kind == M68k_AM_data_register_direct) ||
                                   (ins->ea[0].kind == M68k_AM_address_register_direct))) {
                M68k_start_wait(this, 4, M68kS_exec);
            }
        STEP(5)
INS_END

M68KINS(EORI_TO_CCR)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            M68k_sample_interrupts(this);
            u32 v = this->regs.SR.u & 0x1F;
            v ^= this->regs.IR & 0x1F;
            this->regs.SR.u = (this->regs.SR.u & 0xFFE0) | v;
            M68k_start_wait(this, 8, M68kS_exec);
        STEP(3)
            M68k_start_read(this, this->regs.PC - 2, 2, MAKE_FC(1), M68K_RW_ORDER_NORMAL, M68kS_exec);
        STEP(4)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(5)
INS_END

M68KINS(EORI_TO_SR)
        STEP0
            M68k_sample_interrupts(this);
            if (!this->regs.SR.S) {
                M68k_start_group2_exception(this, M68kIV_privilege_violation, 4, this->regs.PC - 2);
                return;
            }
            switch(ins->ea[0].kind) {
                case 0:
                    this->regs.IPC -= 2;
                    break;
            }
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            u32 data = (this->regs.IR & 0xA71F) ^ this->regs.SR.u;
            M68k_set_SR(this, data, 1);
            M68k_start_wait(this, 8, M68kS_exec);
        STEP(2)
            M68k_start_read(this, this->regs.PC - 2, 2, MAKE_FC(1), 0, M68kS_exec);
        STEP(3)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(4)
INS_END


M68KINS(EXG)
        STEP0
            M68k_sample_interrupts(this);
            this->state.op[0].val = M68k_get_r(this, &ins->ea[0], ins->sz);
            M68k_set_r(this, &ins->ea[0], M68k_get_r(this, &ins->ea[1], ins->sz), ins->sz);
            M68k_set_r(this, &ins->ea[1], this->state.op[0].val, ins->sz);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            M68k_start_wait(this, 2, M68kS_exec);
        STEP(2)
INS_END

M68KINS(EXT)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz >> 1, M68kS_exec, 0, 0, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            this->state.instruction.result = sgn32(this->state.op[0].val, ins->sz >> 1);
            M68k_start_write_operand(this, 0, 0, M68kS_exec, 1, 0);
        STEP(3)
            this->regs.SR.C = this->regs.SR.V = 0;
            this->regs.SR.Z = (this->state.instruction.result & clip32[ins->sz]) == 0;
            this->regs.SR.N = !!(this->state.instruction.result & msb32[ins->sz]);
INS_END

M68KINS(ILLEGAL)
        STEP0
            BADINS;
INS_END

M68KINS(JMP)
        STEP0
            M68k_sample_interrupts(this);
            u32 k = ins->ea[0].kind;
            if ((k == M68k_AM_address_register_indirect_with_index) || (k == M68k_AM_program_counter_with_index))
                M68k_start_wait(this, 6, M68kS_exec);
            if ((k == M68k_AM_address_register_indirect_with_displacement) |
                (k == M68k_AM_program_counter_with_displacement) || (k == M68k_AM_absolute_short_data))
                M68k_start_wait(this, 2, M68kS_exec);
        STEP(1)
            this->state.op[0].ext_words = M68k_AM_ext_words(ins->ea[0].kind, 4);
            this->state.instruction.temp = (this->regs.PC - 4) + (this->state.op[0].ext_words << 2);
            switch (this->state.op[0].ext_words) {
                case 0: {
                    this->regs.PC -= 2;
                    this->state.instruction.temp = this->regs.PC;
                    break;
                }
                case 1: {
                    this->state.instruction.temp = this->regs.PC;
                    break;
                }
                case 2: {
                    M68k_start_read(this, this->regs.PC, 2, MAKE_FC(1), 0, M68kS_exec);
                    this->state.instruction.temp = this->regs.PC + 2;
                    this->regs.PC += 2;
                    break;
                }
                default:
                    assert(1 == 2);
            }
            // #1 M68k(0000)  458504  jmp     (a2)     needs =0
            // #3 M68k(0000)  032f6e  jmp     $4ef8.w  needs -2
            this->regs.IPC = this->regs.PC;
            u32 kk = ins->ea[0].kind;
            switch(kk) {
                case M68k_AM_address_register_indirect_with_displacement:
                case M68k_AM_address_register_indirect_with_index:
                case M68k_AM_program_counter_with_displacement:
                case M68k_AM_program_counter_with_index:
                case M68k_AM_absolute_short_data:
                    this->regs.IPC -= 2; break;
                case M68k_AM_absolute_long_data:
                    this->regs.IPC -= 4; break;
                default:
                    break;
            }
        STEP(2)
            this->regs.PC += this->state.op[0].ext_words << 1;
            // If the operand had two words, assemble from IRC and bus_cycle.data
            u32 prefetch = 0;
            if (this->state.op[0].ext_words == 2) {
                prefetch = (this->regs.IRC << 16) | this->state.bus_cycle.data;
            } else if (this->state.op[0].ext_words == 1) {// Else if it had 1 assemble from IRC
                prefetch = this->regs.IRC;
            }
            // 2, 5, 6 and 7 0-3  . addr reg ind, ind with index, ind with displ.
            // abs short, abs long, pc rel index, pc rel displ.
            // all require another read.
            this->state.instruction.result = M68k_read_ea_addr(this, 0, 4, NO_HOLD, prefetch);
            M68k_start_read(this, this->state.instruction.result, 2, MAKE_FC(1), NO_REVERSE, M68kS_exec);
            this->state.instruction.result += 2;
        STEP(3)
            this->regs.IR = this->regs.IRC;
            this->regs.IRC = this->state.bus_cycle.data;
            this->regs.PC = this->state.instruction.result;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
            //M68k_start_push(this, this->state.instruction.temp, 4, M68kS_exec, 0);
        STEP(5)
INS_END

M68KINS(JSR)
        STEP0
            M68k_sample_interrupts(this);
            u32 k = ins->ea[0].kind;
            if ((k == M68k_AM_address_register_indirect_with_index) || (k == M68k_AM_program_counter_with_index))
                M68k_start_wait(this, 6, M68kS_exec);
            if ((k == M68k_AM_address_register_indirect_with_displacement) |
                (k == M68k_AM_program_counter_with_displacement) || (k == M68k_AM_absolute_short_data))
                M68k_start_wait(this, 2, M68kS_exec);
        STEP(1)
            this->state.op[0].ext_words = M68k_AM_ext_words(ins->ea[0].kind, 4);
            this->state.instruction.temp = (this->regs.PC - 4) + (this->state.op[0].ext_words << 2);
            switch (this->state.op[0].ext_words) {
                case 0: {
                    this->regs.PC -= 2;
                    this->state.instruction.temp = this->regs.PC;
                    break;
                }
                case 1: {
                    this->state.instruction.temp = this->regs.PC;
                    break;
                }
                case 2: {
                    M68k_start_read(this, this->regs.PC, 2, MAKE_FC(1), 0, M68kS_exec);
                    this->state.instruction.temp = this->regs.PC + 2;
                    this->regs.PC += 2;
                    break;
                }
                default:
                    assert(1 == 2);
            }
            this->regs.IPC = this->regs.PC;
        STEP(2)
            this->regs.PC += this->state.op[0].ext_words << 1;
            // If the operand had two words, assemble from IRC and bus_cycle.data
            u32 prefetch = 0;
            if (this->state.op[0].ext_words == 2) {
                prefetch = (this->regs.IRC << 16) | this->state.bus_cycle.data;
            } else if (this->state.op[0].ext_words == 1) {// Else if it had 1 assemble from IRC
                prefetch = this->regs.IRC;
            }
            this->state.instruction.result = M68k_read_ea_addr(this, 0, 4, NO_HOLD, prefetch);
            M68k_start_read(this, this->state.instruction.result, 2, MAKE_FC(1), NO_REVERSE, M68kS_exec);
            this->state.instruction.result += 2;
        STEP(3)
            this->regs.IR = this->regs.IRC;
            this->regs.IRC = this->state.bus_cycle.data;
            M68k_start_push(this, this->state.instruction.temp, 4, M68kS_exec, 0);
        STEP(4)
            this->regs.PC = this->state.instruction.result;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(5)
INS_END

M68KINS(LEA)
       STEP0
            M68k_start_read_operand_for_ea(this, 0, ins->sz, M68kS_exec, 0, HOLD, ALLOW_REVERSE);
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.op[0].addr = M68k_read_ea_addr(this, 0, this->ins->sz,
                                                       0, this->state.op[0].ext_words);
            this->state.instruction.result = this->state.op[0].addr;
            M68k_start_write_operand(this, 0, 1, M68kS_exec, NO_REVERSE, NO_REVERSE);
        STEP(2)
            if ((ins->ea[0].kind == M68k_AM_address_register_indirect_with_index) || (ins->ea[0].kind == M68k_AM_program_counter_with_index))
                M68k_start_wait(this, 2, M68kS_exec);
        STEP(3)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(4)
INS_END

M68KINS(LINK)
        STEP0
            this->state.op[1].val = this->regs.IRC;
            this->state.op[1].addr = this->regs.A[7];
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
            M68k_sample_interrupts(this);
        STEP(1)
            M68k_start_push(this, this->regs.A[ins->ea[0].reg], 4, M68kS_exec, NO_REVERSE);
            this->regs.A[ins->ea[0].reg] = this->regs.A[7];
            this->regs.A[7] = this->regs.A[7] + sgn32(this->state.op[1].val, 2);
        STEP(2)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(3)
INS_END

M68KINS(LSL_qimm_dr)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
            M68k_sample_interrupts(this);
        STEP(1)
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + ins->ea[0].reg * 2, M68kS_exec);
        STEP(2)
            this->state.instruction.result = LSL(this, this->regs.D[ins->ea[1].reg], ins->ea[0].reg, ins->sz);
            M68k_set_dr(this, ins->ea[1].reg, this->state.instruction.result, ins->sz);
INS_END


M68KINS(LSL_dr_dr)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
            M68k_sample_interrupts(this);
        STEP(1)
            u32 cnt = this->regs.D[ins->ea[0].reg] & 63;
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + cnt * 2, M68kS_exec);
        STEP(2)
            u32 cnt = this->regs.D[ins->ea[0].reg] & 63;
            this->state.instruction.result = LSL(this, this->regs.D[ins->ea[1].reg], cnt, ins->sz);
            M68k_set_dr(this, ins->ea[1].reg, this->state.instruction.result, ins->sz);
INS_END

M68KINS(LSL_ea)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, 0, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.instruction.result = LSL(this, this->state.op[0].val, 1, ins->sz);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_write_operand(this, 0, 0, M68kS_exec, 1, 0);
        STEP(3)
INS_END


M68KINS(LSR_qimm_dr)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
            M68k_sample_interrupts(this);
        STEP(1)
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + ins->ea[0].reg * 2, M68kS_exec);
        STEP(2)
            this->state.instruction.result = LSR(this, this->regs.D[ins->ea[1].reg], ins->ea[0].reg, ins->sz);
            M68k_set_dr(this, ins->ea[1].reg, this->state.instruction.result, ins->sz);
INS_END


M68KINS(LSR_dr_dr)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
            M68k_sample_interrupts(this);
        STEP(1)
            u32 cnt = this->regs.D[ins->ea[0].reg] & 63;
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + cnt * 2, M68kS_exec);
        STEP(2)
            u32 cnt = this->regs.D[ins->ea[0].reg] & 63;
            this->state.instruction.result = LSR(this, this->regs.D[ins->ea[1].reg], cnt, ins->sz);
            M68k_set_dr(this, ins->ea[1].reg, this->state.instruction.result, ins->sz);
INS_END

M68KINS(LSR_ea)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, 0, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.instruction.result = LSR(this, this->state.op[0].val, 1, ins->sz);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_write_operand(this, 0, 0, M68kS_exec, 1, 0);
        STEP(3)
INS_END

static void correct_IPC_MOVE_l_pf0(struct M68k* this, u32 opnum)
{
    switch(this->ins->ea[opnum].kind) {
        case M68k_AM_address_register_indirect:
        case M68k_AM_address_register_indirect_with_predecrement:
            this->regs.IPC -= 2; break;
        case M68k_AM_absolute_short_data:
            this->regs.IPC += 2; break;
        case M68k_AM_address_register_indirect_with_displacement:
            if (this->ins->ea[opnum ^ 1].kind == M68k_AM_address_register_indirect_with_postincrement)
                this->regs.IPC += 2;
            break;
        case M68k_AM_address_register_indirect_with_index:
            this->regs.IPC += 4; break;
        default:
            break;
    }
}

export_M68KINS(MOVE)
        STEP0
            u32 k = ins->ea[0].kind;
            u32 k2 = ins->ea[1].kind;
            u32 numcycles = 0;
            switch(k) {
                case M68k_AM_address_register_indirect_with_index:
                case M68k_AM_program_counter_with_index:
                    numcycles = 2;
                    break;
            }
            if (numcycles) M68k_start_wait(this, 2, M68kS_exec);
        STEP(1)
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            // 1 ext word, 2 reads       1 read
            // 042 MOVE.l (d8, A6, Xn), -(A4) 2936  needs hold
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
            if ((ins->sz == 4) && (ins->ea[1].kind == M68k_AM_address_register_indirect_with_predecrement))
                this->state.op[1].hold = HOLD;
            if ((ins->sz == 4) && (ins->ea[0].kind == M68k_AM_address_register_indirect_with_postincrement))
                this->state.op[0].hold = HOLD;
            if ((ins->sz == 2) && (ins->ea[0].kind == M68k_AM_address_register_indirect_with_predecrement))
                this->state.op[1].hold = NO_HOLD;
            this->state.current = M68kS_exec;
            // Do prefetch 0
            if (this->state.operands.state[M68kOS_prefetch1]) {
                this->state.operands.TCU = 0;
                M68k_read_operands_prefetch(this, 0);
                this->state.bus_cycle.next_state = M68kS_exec;
                this->regs.IPC -= 4;
            }
        STEP(2)
            // finish prefecth 0
            this->state.op[0].t = this->state.operands.state[M68kOS_prefetch1];
            this->state.op[1].t2 = 0;
            if (this->state.op[0].t) {
                if (!this->state.operands.state[M68kOS_read1]) this->regs.IPC += 4;
                M68k_read_operands_prefetch(this, 0);
                this->state.operands.TCU = 0;
                this->state.bus_cycle.next_state = M68kS_exec;
            }

            // Do pause if needed, in between p0 and r0
            u32 k = ins->ea[0].kind;
            u32 k2 = ins->ea[1].kind;
            u32 numcycles = 0;
            switch(k) {
                case M68k_AM_address_register_indirect_with_predecrement:
                    numcycles = 2; break;
            }
            if (numcycles) M68k_start_wait(this, numcycles, M68kS_exec);
        STEP(3)
            // Do read 0
            if (this->state.operands.state[M68kOS_read1]) {
                u32 k = ins->ea[0].kind;
                u32 k2 = ins->ea[1].kind;
                switch(k) {
                    case M68k_AM_address_register_indirect_with_postincrement:
                        switch(k2) {
                            case M68k_AM_address_register_indirect_with_postincrement:
                                this->regs.IPC += 2;
                                break;
                        }
                        break;
                    case M68k_AM_address_register_indirect:
                        // 011 MOVE.w (A1), (d8, A7, Xn)
                        if (k2 != M68k_AM_data_register_direct)
                            this->regs.IPC -= 2;
                        if (k2 == M68k_AM_address_register_indirect_with_postincrement)
                            this->regs.IPC += 2;
                        break;
                    case M68k_AM_address_register_indirect_with_predecrement:
                        switch(k2) {
                            case M68k_AM_address_register_indirect_with_postincrement:
                                this->regs.IPC += 2;
                                break;
                        }
                        break;
                    case M68k_AM_program_counter_with_displacement:
                    case M68k_AM_address_register_indirect_with_displacement:
                        switch(k2) {
                            case M68k_AM_data_register_direct:
                                this->regs.IPC += 4;
                                break;
                            case M68k_AM_address_register_indirect_with_postincrement:
                                this->regs.IPC += 2;
                                break;
                        }
                        break;
                    case M68k_AM_absolute_short_data:
                        switch(k2) {
                            case M68k_AM_address_register_indirect:
                            case M68k_AM_address_register_indirect_with_index:
                            case M68k_AM_data_register_direct:
                            case M68k_AM_address_register_indirect_with_displacement:
                            case M68k_AM_address_register_indirect_with_predecrement:
                                this->regs.IPC += 2;
                                break;
                            case M68k_AM_address_register_indirect_with_postincrement:
                                this->regs.IPC += 4;
                                break;
                        }
                        break;
                    case M68k_AM_address_register_indirect_with_index:
                        switch(k2) {
                            case M68k_AM_data_register_direct:
                                this->regs.IPC += 4;
                                break;
                            case M68k_AM_address_register_indirect_with_postincrement:
                                this->regs.IPC += 2;
                                break;
                        }
                        break;
                    case M68k_AM_absolute_long_data:
                        switch(k2) {
                            case M68k_AM_data_register_direct:
                            case M68k_AM_address_register_indirect_with_postincrement:
                                this->regs.IPC += 4;
                                break;
                            case M68k_AM_address_register_indirect:
                            case M68k_AM_address_register_indirect_with_index:
                            case M68k_AM_address_register_indirect_with_displacement:
                            case M68k_AM_address_register_indirect_with_predecrement:
                                this->regs.IPC += 2;
                                break;
                        }
                        break;
                    case M68k_AM_program_counter_with_index:
                        switch(k2) {
                            case M68k_AM_address_register_indirect_with_postincrement:
                                this->regs.IPC += 2;
                                break;
                            case M68k_AM_data_register_direct:
                                this->regs.IPC += 4;
                                break;
                        }

                        break;
                }
                M68k_read_operands_read(this, 0, 1);
                this->state.bus_cycle.next_state = M68kS_exec;
                if (ins->sz == 4) correct_IPC_MOVE_l_pf0(this, 0);
            }
        STEP(4)
            // Finish read 0
            if (this->state.operands.state[M68kOS_read1]) {
                if (this->state.op[0].t) this->regs.IPC += 4;
                M68k_read_operands_read(this, 0, 1);
                this->state.bus_cycle.next_state = M68kS_exec;
            }

            // Potentially wait, in between r0 and p1
            if (ins->ea[1].kind == M68k_AM_address_register_indirect_with_index) M68k_start_wait(this, 2, M68kS_exec);
        STEP(5)
            this->state.instruction.result = this->state.op[0].val;
            // Do prefetch 1
            if (ins->sz == 2) {
                this->regs.SR.Z = (clip32[ins->sz] & this->state.instruction.result) == 0;
                this->regs.SR.N = !!(this->state.instruction.result & msb32[ins->sz]);
            }
        STEP(6)
            if (this->state.operands.state[M68kOS_prefetch2]) {
                M68k_read_operands_prefetch(this, 1);
                if ((ins->ea[1].kind == M68k_AM_absolute_long_data) && (ins->ea[0].kind != M68k_AM_address_register_direct) && (ins->ea[0].kind != M68k_AM_data_register_direct) && (ins->ea[0].kind != M68k_AM_immediate)) {
                    this->state.current = M68kS_read16;
                    this->state.op[1].t2 = 1;
                }
                this->state.bus_cycle.next_state = M68kS_exec;
            }
        STEP(7)
            if (this->state.operands.state[M68kOS_prefetch2]) {
                if (this->state.op[1].t2) {
                    this->regs.IR = this->regs.IRC;
                    this->regs.IRC = this->state.bus_cycle.data & 0xFFFF;
                    this->state.op[1].addr = ((this->regs.IR << 16) | this->regs.IRC);
                    this->regs.PC += 2;
                }
                else {
                    M68k_read_operands_prefetch(this, 1);
                }
                this->state.bus_cycle.next_state = M68kS_exec;
            }
        STEP(8)
            switch(ins->ea[1].kind) {
                case M68k_AM_absolute_long_data:
                    if (!this->state.op[1].t2) {
                        this->state.op[1].addr = M68k_read_ea_addr(this, 1, ins->sz, NO_HOLD, this->state.op[1].ext_words);
                    }
                    __attribute__ ((fallthrough));;
                case M68k_AM_imm16:
                case M68k_AM_imm32:
                case M68k_AM_immediate:
                    this->state.operands.state[M68kOS_read2] = 0;
                    break;
                    break;
                default:
                    break;
            }
            if (this->state.operands.state[M68kOS_read2]) {
                this->state.op[1].hold = 1;
                this->state.operands.TCU = 0;
                // 023 MOVE.w (xxx).w, (A3)  needs -2
                M68k_read_operands_read(this, 1, 0);
                this->state.bus_cycle.next_state = M68kS_exec;
                this->state.current = M68kS_exec;
            } else {
                this->state.current = M68kS_exec;
            }
            this->regs.SR.C = this->regs.SR.V = 0;
            switch(ins->ea[1].kind) {
                case M68k_AM_address_register_indirect:
                default:
                    this->state.instruction.temp = NO_REVERSE;
                    this->state.op[0].addr = NO_REVERSE;
                    break;
            }
            this->state.op[1].t = 0;
            switch(ins->ea[1].kind) {
                case M68k_AM_address_register_indirect_with_predecrement:
                    this->state.op[1].t = 1;
                    break;
                default:
                    break;
            }
            if (this->state.op[1].t) {
                M68k_start_prefetch(this, 1, 1, M68kS_exec);
            }
            else {
                if (ins->sz <= 2) {
                    u32 k = this->ins->ea[0].kind;
                    u32 k2 = this->ins->ea[1].kind;
                    switch(k) {
                        case M68k_AM_program_counter_with_displacement:
                            switch(k2) {
                                case M68k_AM_address_register_indirect_with_displacement:
                                    this->regs.IPC -= 2;
                                    break;
                            }
                            break;
                        case M68k_AM_program_counter_with_index:
                            switch(k2) {
                                case M68k_AM_address_register_indirect_with_index:
                                case M68k_AM_address_register_indirect_with_displacement:
                                    this->regs.IPC -= 2;
                                    break;
                            }
                            break;
                        case M68k_AM_address_register_indirect_with_postincrement:
                            switch(k2) {
                                case M68k_AM_address_register_indirect:
                                case M68k_AM_address_register_indirect_with_postincrement:
                                    this->regs.IPC += 2;
                                    break;
                                case M68k_AM_absolute_long_data:
                                    this->regs.IPC -= 2;
                                    break;
                            }
                            break;
                        case M68k_AM_address_register_indirect_with_index:
                            // 088 MOVE.w (d8, A3, Xn), (A6)+ -2
                            // 045 MOVE.w (d8, A5, Xn), (d16, A3) -2
                            switch(k2) {
                                case M68k_AM_data_register_direct:
                                    this->regs.IPC += 2;
                                    break;
                                case M68k_AM_address_register_indirect_with_displacement:
                                case M68k_AM_address_register_indirect_with_index:
                                case M68k_AM_absolute_short_data:
                                    this->regs.IPC -= 2;
                                    break;
                            }
                            break;
                        case M68k_AM_data_register_direct:
                            /*switch(k2) {
                                case M68k_AM_absolute_short_data:
                                    this->regs.IPC -= 2;
                                    break;
                            }*/
                            switch(k2) {
                                case M68k_AM_address_register_indirect:
                                case M68k_AM_address_register_indirect_with_postincrement:
                                case M68k_AM_address_register_indirect_with_displacement:
                                case M68k_AM_address_register_indirect_with_index:
                                    this->regs.IPC += 2;
                                    break;
                            }
                            break;
                        case M68k_AM_address_register_direct:
                            switch(k2) {
                                case M68k_AM_address_register_indirect_with_displacement:
                                case M68k_AM_address_register_indirect_with_index:
                                case M68k_AM_address_register_indirect_with_postincrement:
                                case M68k_AM_address_register_indirect:
                                    this->regs.IPC += 2;
                                    break;
                            }
                            break;
                        case M68k_AM_address_register_indirect_with_displacement:
                            switch(k2) {
                                case M68k_AM_address_register_indirect_with_index:
                                case M68k_AM_address_register_indirect_with_displacement:
                                    this->regs.IPC -= 2;
                                    break;
                                case M68k_AM_absolute_long_data:
                                    this->regs.IPC -= 4;
                                    break;
                            }
                            break;
                        case M68k_AM_address_register_indirect:
                            switch(k2) {
                                case M68k_AM_address_register_indirect_with_postincrement:
                                case M68k_AM_address_register_indirect:
                                    this->regs.IPC += 2;
                                    break;
                                case M68k_AM_absolute_long_data:
                                    this->regs.IPC -= 2;
                                    break;
                            }
                            break;
                        case M68k_AM_absolute_short_data:
                            switch(k2) {
                                case M68k_AM_address_register_indirect_with_postincrement:
                                case M68k_AM_absolute_short_data:
                                    this->regs.IPC -= 2;
                                    break;
                                case M68k_AM_address_register_indirect_with_displacement:
                                case M68k_AM_address_register_indirect_with_index:
                                    this->regs.IPC -= 4;
                                    break;
                            }
                            break;
                        case M68k_AM_absolute_long_data:
                            // 093 MOVE.w (xxx).l, (A3)
                            switch(k2) {
                                case M68k_AM_address_register_indirect:
                                case M68k_AM_absolute_short_data:
                                    this->regs.IPC -= 2;
                                    break;
                                case M68k_AM_address_register_indirect_with_index:
                                case M68k_AM_address_register_indirect_with_displacement:
                                    this->regs.IPC -= 4;
                                    break;
                            }
                            break;
                        case M68k_AM_address_register_indirect_with_predecrement:
                            switch(k2) {
                                case M68k_AM_absolute_long_data:
                                    this->regs.IPC -= 4;
                                    break;
                                case M68k_AM_address_register_indirect_with_displacement:
                                case M68k_AM_address_register_indirect_with_index:
                                case M68k_AM_absolute_short_data:
                                    this->regs.IPC -= 2;
                                    break;
                            }
                            break;
                        case M68k_AM_immediate:
                            switch(k2) {
                                case M68k_AM_address_register_indirect_with_postincrement:
                                    this->regs.IPC += 2;
                                    break;
                                case M68k_AM_absolute_short_data:
                                case M68k_AM_address_register_indirect_with_index:
                                    this->regs.IPC -= 2;
                                    break;
                            }
                            break;
                        default:
                            break;
                    }
                }
                M68k_sample_interrupts(this);
                M68k_start_write_operand(this, 0, 1, M68kS_exec, this->state.instruction.temp, this->state.op[0].addr);
            }
        STEP(9)
            if (this->state.op[1].t) {
                u32 commit = 0;
                // 019 MOVE.l -(A3), (xxx).l needs commit 0
                if (ins->sz <= 2) commit = 1;

                // 027 MOVE.w D6, -(A3) needs +0
                // 037 MOVE.w (d16, A6), -(A0) needs +0
                // 038 MOVE.w (A0)+, -(A4) needs +2
                if (ins->sz <= 2) {
                    u32 k = ins->ea[0].kind;
                    u32 k2 = ins->ea[1].kind;
                    //if (k2 == M68k_AM_address_register_indirect_with_predecrement)
                    //    this->regs.IPC += 2;
                    switch (k) {
                        case M68k_AM_address_register_indirect:
                            this->regs.IPC += 2;
                            break;
                        case M68k_AM_address_register_indirect_with_postincrement:
                            if (k2 == M68k_AM_address_register_indirect_with_predecrement)
                                this->regs.IPC += 2;
                            break;
                        case M68k_AM_absolute_short_data:
                            switch(k2) {
                                case M68k_AM_address_register_indirect_with_predecrement:
                                    this->regs.IPC -= 2;
                                    break;
                            }
                            break;
                        case M68k_AM_absolute_long_data:
                            switch(k2) {
                                case M68k_AM_address_register_indirect_with_predecrement:
                                    this->regs.IPC -= 2;
                                    break;
                            }
                            break;
                    }
                }
                M68k_sample_interrupts(this);
                M68k_start_write_operand(this, commit, 1, M68kS_exec, ALLOW_REVERSE, 0);
            }
            else {
                this->regs.SR.Z = (clip32[ins->sz] & this->state.instruction.result) == 0;
                this->regs.SR.N = !!(this->state.instruction.result & msb32[ins->sz]);
                M68k_finalize_ea(this, 1);
                if (this->state.op[1].t2)
                    M68k_start_prefetch(this, 2, 1, M68kS_exec);
                else
                    M68k_start_prefetch(this, 1, 1, M68kS_exec);
            }
        STEP(10)
            if (this->state.op[1].t)
                M68k_finalize_ea(this, 1);

            if ((ins->ea[1].kind == M68k_AM_address_register_indirect_with_postincrement) || (ins->ea[1].kind == M68k_AM_address_register_indirect_with_predecrement)) {
                this->regs.A[ins->ea[1].reg] = this->state.op[1].new_val;
            }
            this->regs.SR.Z = (clip32[ins->sz] & this->state.instruction.result) == 0;
            this->regs.SR.N = !!(this->state.instruction.result & msb32[ins->sz]);
INS_END

M68KINS(MOVEA)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            //if (ins->sz == 4)
            // 000 MOVEA.l (d16, Ax), Ax
            // FC(0) for operand read
            //
            // 046 MOVEA.l (d8, PC, Xn), Ax
            // FC(1) for operand read
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            u32 v = this->state.op[0].val;
            if (ins->sz == 2) v = SIGNe16to32(v);
            if (ins->sz == 1) v = SIGNe8to32(v);
            this->regs.A[ins->ea[1].reg] = v;
            //M68k_set_ar(this, ins->ea[1].reg, v, ins->sz);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(3)
INS_END

M68KINS(MOVEM_TO_MEM)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.op[1].val = this->regs.IR;
            this->state.op[1].addr = 0;
            this->state.instruction.result = 200;
            u32 waits = 0;
            if (ins->ea[0].kind == M68k_AM_address_register_indirect_with_index)
                waits = 2;
            M68k_start_read_operand_for_ea(this, 0, ins->sz, M68kS_exec, waits, HOLD, ALLOW_REVERSE);
            this->state.operands.state[M68kOS_pause1] = 0;
        STEP(2)
            this->state.op[0].val = M68k_read_ea_addr(this, 0, 4, 1, this->state.op[0].ext_words);
            if (ins->ea[0].kind == M68k_AM_address_register_indirect_with_predecrement) this->state.op[0].val += ins->sz;
            if (ins->ea[0].kind == M68k_AM_address_register_indirect_with_postincrement) this->state.op[0].val -= ins->sz;
            if ((ins->sz == 4) && (ins->ea[0].kind == M68k_AM_address_register_indirect_with_predecrement)) this->state.op[0].val -= ins->sz;
        STEP(3)
            // Loop here up to 16 times..
            this->regs.IPC = this->regs.PC;
            if (this->state.op[1].addr < 16) {
                // If our value is not 0, we will have a new value for a register
                u32 index = ins->ea[0].kind == M68k_AM_address_register_indirect_with_predecrement ? 15 - this->state.op[1].addr : this->state.op[1].addr;
                // Loop!
                u32 a = this->state.op[1].addr;
                this->state.instruction.TCU--;
                this->state.op[1].addr++;

                // If this bit isn't set, don't do anything else
                if (!(this->state.op[1].val & (1 << a))) {
                    break;
                }

                // Start a read
                u32 do_reverse = M68K_RW_ORDER_NORMAL;
                u32 kk = ins->ea[0].kind;
                if (kk == M68k_AM_address_register_indirect_with_predecrement) do_reverse = M68K_RW_ORDER_REVERSE;
                u32 v = (index < 8) ? this->regs.D[index] : this->regs.A[index - 8];
                M68k_start_write(this, this->state.op[0].val, v, ins->sz, MAKE_FC(0), do_reverse, M68kS_exec);
                if (ins->ea[0].kind == M68k_AM_address_register_indirect_with_predecrement) this->state.op[0].val -= ins->sz;
                if (ins->ea[0].kind != M68k_AM_address_register_indirect_with_predecrement) this->state.op[0].val += ins->sz;
            }
        STEP(5)
            if (ins->ea[0].kind == M68k_AM_address_register_indirect_with_predecrement) this->state.op[0].val += ins->sz;
            if (ins->ea[0].kind == M68k_AM_address_register_indirect_with_predecrement) M68k_set_ar(this, ins->ea[0].reg, this->state.op[0].val, 4);
            if (ins->ea[0].kind == M68k_AM_address_register_indirect_with_postincrement) M68k_set_ar(this, ins->ea[0].reg, this->state.op[0].val, 4);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(6)
INS_END

export_M68KINS(MOVEM_TO_REG)
        STEP0
            M68k_sample_interrupts(this);
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
            u32 waits = 0;
            if (ins->ea[0].kind == M68k_AM_address_register_indirect_with_index)
                waits = 2;
            M68k_start_read_operand_for_ea(this, 0, ins->sz, M68kS_exec, 0, HOLD, ALLOW_REVERSE);
        STEP(2)
            this->state.op[0].val = M68k_read_ea_addr(this, 0, 4, 1, this->state.op[0].ext_words);
            this->regs.IPC = this->regs.PC;
        STEP(3)
        // Loop here up to 16 times..
            if (this->state.op[1].addr < 16) {
                // If our value is not 0, we will have a new value for a register
                u32 index = ins->ea[0].kind == M68k_AM_address_register_indirect_with_predecrement ? 15 - this->state.op[1].addr : this->state.op[1].addr;
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
                if (ins->ea[0].kind == M68k_AM_address_register_indirect_with_predecrement) this->state.op[0].val -= ins->sz;
                u32 fc = MAKE_FC(0);
                if ((ins->ea[0].kind == M68k_AM_program_counter_with_index) || (ins->ea[0].kind == M68k_AM_program_counter_with_displacement))
                    fc = MAKE_FC(1);
                M68k_start_read(this, this->state.op[0].val, ins->sz, fc, M68K_RW_ORDER_NORMAL, M68kS_exec);
                if (ins->ea[0].kind != M68k_AM_address_register_indirect_with_predecrement) this->state.op[0].val += ins->sz;
            }
        STEP(4)
            // Finish last read, if it's there
            if (this->state.instruction.result != 200) {
                if (this->state.instruction.result < 8) M68k_set_dr(this, this->state.instruction.result, sgn32(this->state.bus_cycle.data, ins->sz), 4);
                else M68k_set_ar(this, this->state.instruction.result-8, sgn32(this->state.bus_cycle.data, ins->sz), 4);
                this->state.instruction.result = 200;
            }
            if (ins->ea[0].kind == M68k_AM_address_register_indirect_with_predecrement) {
                this->state.op[0].val -= 2;
            }
            u32 fc = MAKE_FC(0);
            if ((ins->ea[0].kind == M68k_AM_program_counter_with_index) || (ins->ea[0].kind == M68k_AM_program_counter_with_displacement))
                fc = MAKE_FC(1);
            M68k_start_read(this, this->state.op[0].val, 2, fc, 0, M68kS_exec);
        STEP(5)
            if (ins->ea[0].kind == M68k_AM_address_register_indirect_with_predecrement) M68k_set_ar(this, ins->ea[0].reg, this->state.op[0].val, 4);
            if (ins->ea[0].kind == M68k_AM_address_register_indirect_with_postincrement) M68k_set_ar(this, ins->ea[0].reg, this->state.op[0].val, 4);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(6)
INS_END

M68KINS(MOVEP_dr_ea)
    STEP0
        M68k_start_read_operand_for_ea(this, 0, ins->sz, M68kS_exec, 0, HOLD, NO_REVERSE);
    STEP(1)
        M68k_sample_interrupts(this);
        this->state.op[0].val = this->regs.D[ins->ea[0].reg]; // data
        this->state.op[1].val = M68k_read_ea_addr(this, 1, ins->sz, 0, this->state.op[1].ext_words);
        this->state.op[1].addr = ins->sz * 8; // shift
        this->state.operands.cur_op_num = 0; // i
    STEP(2) // loop
        this->state.op[1].addr -= 8;
        M68k_start_write(this, this->state.op[1].val, (this->state.op[0].val >> this->state.op[1].addr) & 0xFF, 1, MAKE_FC(0), M68K_RW_ORDER_NORMAL, M68kS_exec);
        this->state.op[1].val += 2;
        this->state.operands.cur_op_num++;
        if (this->state.operands.cur_op_num < ins->sz) {
            this->state.instruction.TCU--;
        }
    STEP(3)
        M68k_start_prefetch(this, 1, 1, M68kS_exec);
    STEP(4)
INS_END

M68KINS(MOVEP_ea_dr)
    STEP0
        M68k_start_read_operand_for_ea(this, 0, ins->sz, M68kS_exec, 0, HOLD, ALLOW_REVERSE);
    STEP(1)
        M68k_sample_interrupts(this);
        this->state.operands.cur_op_num = 0; // i
        this->state.op[0].addr = ins->sz * 8;// shift
        this->state.op[0].val = M68k_read_ea_addr(this, 0, ins->sz, 0, this->state.op[0].ext_words);
    STEP(2)
        if (this->state.operands.cur_op_num > 0)
            this->regs.D[ins->ea[1].reg] |= this->state.bus_cycle.data << this->state.op[0].addr;
        this->state.op[0].addr -= 8;
        this->regs.D[ins->ea[1].reg] &= ~(0xff << this->state.op[0].addr);
        M68k_start_read(this, this->state.op[0].val, 1, MAKE_FC(0), M68K_RW_ORDER_NORMAL, M68kS_exec);
        this->state.op[0].val += 2;
        this->state.operands.cur_op_num++;
        if (this->state.operands.cur_op_num < ins->sz) this->state.instruction.TCU--;
    STEP(3)
        this->regs.D[ins->ea[1].reg] |= this->state.bus_cycle.data << this->state.op[0].addr;
        M68k_start_prefetch(this, 1, 1, M68kS_exec);
    STEP(4)
INS_END

M68KINS_NOSWITCH(MOVEP)
    if (ins->ea[0].kind == M68k_AM_data_register_direct) return M68k_ins_MOVEP_dr_ea(this, ins);
    else return M68k_ins_MOVEP_ea_dr(this, ins);
INS_END_NOSWITCH

M68KINS(MOVEQ)
        STEP0
            this->regs.D[ins->ea[1].reg] = SIGNe8to32(ins->ea[0].reg);
            this->regs.SR.C = this->regs.SR.V = 0;
            this->regs.SR.Z = this->regs.D[ins->ea[1].reg] == 0;
            this->regs.SR.N = !!(this->regs.D[ins->ea[1].reg] & 0x80000000);
            M68k_sample_interrupts(this);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
INS_END

M68KINS(MOVE_FROM_SR)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, 0, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            this->state.instruction.result = M68k_get_SR(this);
            if (ins->ea[0].kind == M68k_AM_data_register_direct) M68k_start_wait(this, 2, M68kS_exec);
        STEP(3)
            M68k_start_write_operand(this, 0, 0, M68kS_exec, 1, 0);
        STEP(4)
INS_END

M68KINS(MOVE_FROM_USP)
        STEP0
            M68k_sample_interrupts(this);
            if (!this->regs.SR.S) {
                M68k_start_group2_exception(this, M68kIV_privilege_violation, 4, this->regs.PC - 2);
                return;
            }
            switch(ins->ea[0].kind) {
                case 0:
                    this->regs.IPC -= 2;
                    break;
            }
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, 0, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            this->state.instruction.result = this->regs.ASP;
            if (ins->ea[0].kind == M68k_AM_data_register_direct) M68k_start_wait(this, 2, M68kS_exec);
        STEP(3)
            M68k_start_write_operand(this, 0, 0, M68kS_exec, 1, 0);
        STEP(4)
INS_END

M68KINS(MOVE_TO_CCR)
        STEP0
            M68k_start_read_operands(this, 0, 2, M68kS_exec, 0, 0, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->regs.IPC -= 2;
            this->regs.PC -= 2;
            u32 data = (this->regs.SR.u & 0xFFE0) | (this->state.op[0].val & 0x001F);
            M68k_set_SR(this, data, 0);
            M68k_start_wait(this, 4, M68kS_exec);
        STEP(2)
            M68k_start_prefetch(this, 2, 1, M68kS_exec);
        STEP(3)
INS_END

M68KINS(MOVE_TO_SR)
        STEP0
            if (!this->regs.SR.S) {
                M68k_start_group2_exception(this, M68kIV_privilege_violation, 4, this->regs.PC-2);
                return;
            }
            M68k_start_read_operands(this, 0, 2, M68kS_exec, 0, 0, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->regs.IPC -= 2;
            this->regs.PC -= 2;
            M68k_start_wait(this, 4, M68kS_exec);
        STEP(2)
            u32 data = (this->state.op[0].val & 0xA71F);
            M68k_set_SR(this, data, 1);
            M68k_start_prefetch(this, 2, 1, M68kS_exec);
        STEP(3)
INS_END

M68KINS(MOVE_TO_USP)
        STEP0
            M68k_sample_interrupts(this);
            if (!this->regs.SR.S) {
                M68k_start_group2_exception(this, M68kIV_privilege_violation, 4, this->regs.PC - 2);
                return;
            }
            M68k_start_read_operands(this, 0, 4, M68kS_exec, 0, NO_HOLD, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            // We're in supervisor, so we'll always be ASP
            this->regs.ASP = this->state.op[0].val;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
INS_END

M68KINS(MULS)
        STEP0
            M68k_start_read_operands(this, 0, 2, M68kS_exec, 0, NO_HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.instruction.result = (i16)this->state.op[0].val * (i16)this->state.op[1].val;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            u32 bc = 0;
            // Get transitions 0->1
            u16 n = ((this->state.op[0].val & 0xFFFF) << 1) ^ this->state.op[0].val;
            while (n) { // Count 1's algorithm
                bc += n & 1;
                n >>= 1;
            }
            u32 cycles = 34 + bc * 2;
            M68k_start_wait(this, cycles, M68kS_exec);
        STEP(3)
            M68k_set_dr(this, ins->ea[1].reg, this->state.instruction.result, 4);
            this->regs.SR.C = 0;
            this->regs.SR.V = 0;
            this->regs.SR.Z = (this->state.instruction.result & 0xFFFFFFFF) == 0;
            this->regs.SR.N = !!(this->state.instruction.result & 0x80000000);

INS_END

M68KINS(MULU)
        STEP0
            M68k_start_read_operands(this, 0, 2, M68kS_exec, 0, NO_HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.instruction.result = (u16)this->state.op[0].val * (u16)this->state.op[1].val;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            u32 bc = 0;
            // Get transitions 0->1
            //u16 n = ((this->state.op[0].val & 0xFFFF) << 1) ^ this->state.op[0].val;
            u16 n = this->state.op[0].val;
            while (n) { // Count 1's algorithm
                bc += n & 1;
                n >>= 1;
            }
            u32 cycles = 34 + bc * 2;
            M68k_start_wait(this, cycles, M68kS_exec);
        STEP(3)
            M68k_set_dr(this, ins->ea[1].reg, this->state.instruction.result, 4);
            this->regs.SR.C = 0;
            this->regs.SR.V = 0;
            this->regs.SR.Z = (this->state.instruction.result & 0xFFFFFFFF) == 0;
            this->regs.SR.N = !!(this->state.instruction.result & 0x80000000);

INS_END

M68KINS(NBCD)
        STEP0
            M68k_start_read_operands(this, 0, 1, M68kS_exec, 0, 1, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            u32 source = this->state.op[0].val;
            u32 target = 0u;
            u32 result = target - source - this->regs.SR.X;
            u32 c = 0, v = 0;

            u32 adjustLo = !!((target ^ source ^ result) & 0x10);
            u32 adjustHi = !!(result & 0x100);

            if(adjustLo) {
                u32 previous = result;
                result -= 0x06;
                c  = !!((~previous & 0x80) & ( result & 0x80));
                v |= !!((previous & 0x80) & (~result & 0x80));
            }

            if(adjustHi) {
                u32 previous = result;
                result -= 0x60;
                c  = 1;
                v |= !!((previous & 0x80) & (~result & 0x80));
            }
            this->state.instruction.result = result;

            this->regs.SR.C = this->regs.SR.X = c;
            this->regs.SR.V = v;

            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_write_operand(this, 0, 0, M68kS_exec, 1, 0);
        STEP(3)
            if ((ins->ea[0].kind == M68k_AM_data_register_direct) || (ins->ea[0].kind == M68k_AM_address_register_direct))
                M68k_start_wait(this, 2, M68kS_exec);
        STEP(4)
            this->regs.SR.Z = (0xFF & this->state.instruction.result) ? 0 : this->regs.SR.Z;
            this->regs.SR.N = !!(0x80 & this->state.instruction.result);
INS_END

M68KINS(NEG)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.instruction.result = SUB(this, this->state.op[0].val, 0, ins->sz, 0, 1);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            if ((ins->sz == 4) && ((ins->ea[0].kind == M68k_AM_address_register_direct) || (ins->ea[0].kind == M68k_AM_data_register_direct))) {
                M68k_start_wait(this, 2, M68kS_exec);
            }
        STEP(3)
            M68k_start_write_operand(this, 0, 0, M68kS_exec, 1, 0);
        STEP(4)
INS_END

M68KINS(NEGX)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            this->state.instruction.result = SUB(this, this->state.op[0].val, 0, ins->sz, 1, 1);
            M68k_start_write_operand(this, 0, 0, M68kS_exec, 1, 1);
        STEP(3)
            if (ins->sz == 4) {
                if ((ins->ea[0].kind == M68k_AM_data_register_direct) ||
                    (ins->ea[0].kind == M68k_AM_address_register_direct)) {
                    M68k_start_wait(this, 2, M68kS_exec);
                }
            }
        STEP(4)
INS_END

M68KINS(NOP)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
INS_END

M68KINS(NOT)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.instruction.result = (~this->state.op[0].val) & clip32[ins->sz];
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_write_operand(this, 0, 0, M68kS_exec, 1, 0);
            DELAY_IF_REGWRITE(ins->ea[0].kind, 2);
            this->regs.SR.C = this->regs.SR.V = 0;
            this->regs.SR.Z = this->state.instruction.result == 0;
            this->regs.SR.N = !!(this->state.instruction.result & msb32[ins->sz]);
        STEP(3)
INS_END

M68KINS(OR)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.instruction.result = OR(this, this->state.op[0].val, this->state.op[1].val, ins->sz);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_write_operand(this, 0, 1, M68kS_exec, 1, 0);
        STEP(3)
            if ((ins->sz == 4) && (ins->ea[1].kind == M68k_AM_data_register_direct)) {
                if ((ins->ea[0].kind == M68k_AM_data_register_direct) || (ins->ea[0].kind == M68k_AM_immediate))
                    M68k_start_wait(this, 4, M68kS_exec);
                else
                    M68k_start_wait(this, 2, M68kS_exec);
            }
        STEP(4)

INS_END

M68KINS(ORI)
        STEP0
            if (ins->sz == 2) this->regs.IPC -= 2;
            STARTI(0,1);
        STEP(2)
            M68k_sample_interrupts(this);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(3)
            this->state.instruction.result = OR(this, this->state.instruction.result, this->state.op[0].val, ins->sz);
            M68k_start_write_operand(this, 0, 0, M68kS_exec, 1, 0);
        STEP(4)
            if ((ins->sz == 4) && (ins->ea[0].kind == M68k_AM_data_register_direct))
                M68k_start_wait(this, 4, M68kS_exec);
        STEP(5)
INS_END

M68KINS(ORI_TO_CCR)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            M68k_sample_interrupts(this);
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
            M68k_sample_interrupts(this);
            if (!this->regs.SR.S) {
                M68k_start_group2_exception(this, M68kIV_privilege_violation, 4, this->regs.PC - 2);
                return;
            }
            switch(ins->ea[0].kind) {
                case 0:
                    this->regs.IPC -= 2;
                    break;
            }
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            u32 data = this->regs.IR | M68k_get_SR(this);
            M68k_set_SR(this, data, 1);
            M68k_start_wait(this, 8, M68kS_exec);
        STEP(2)
            M68k_start_read(this, this->regs.PC - 2, 2, MAKE_FC(1), 0, M68kS_exec);
        STEP(3)
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(4)
INS_END

M68KINS(PEA)
        STEP0
            M68k_start_read_operand_for_ea(this, 0, ins->sz, M68kS_exec, 0, HOLD, ALLOW_REVERSE);
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.op[0].addr = M68k_read_ea_addr(this, 0, this->ins->sz,
                                                            0, this->state.op[0].ext_words);
            this->state.instruction.result = this->state.op[0].addr;
            if ((ins->ea[0].kind == M68k_AM_address_register_indirect_with_index) ||
                        (ins->ea[0].kind == M68k_AM_program_counter_with_index))
                    M68k_start_wait(this, 2, M68kS_exec);
        STEP(2)
            if ((ins->ea[0].kind == M68k_AM_absolute_short_data) || (ins->ea[0].kind == M68k_AM_absolute_long_data)) {
                M68k_start_push(this, this->state.instruction.result, 4, M68kS_exec, 0);
            }
            else {
                M68k_start_prefetch(this, 1, 1, M68kS_exec);
            }
        STEP(3)
            if ((ins->ea[0].kind == M68k_AM_absolute_short_data) || (ins->ea[0].kind == M68k_AM_absolute_long_data)) {
                M68k_start_prefetch(this, 1, 1, M68kS_exec);
            }
            else {
                M68k_start_push(this, this->state.instruction.result, 4, M68kS_exec, 0);
            }
        STEP(4)
    INS_END

M68KINS(RESET)
        STEP0
            M68k_sample_interrupts(this);
            if (!this->regs.SR.S) {
                M68k_start_group2_exception(this, M68kIV_privilege_violation, 4, this->regs.PC - 2);
                return;
            }
            this->pins.RESET = 1;
            M68k_start_wait(this, 128, M68kS_exec);
        STEP(1)
            this->pins.RESET = 0;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
INS_END

M68KINS(ROL_qimm_dr)
        STEP0
            M68k_sample_interrupts(this);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + ins->ea[0].reg * 2, M68kS_exec);
        STEP(2)
            this->state.instruction.result = ROL(this, this->regs.D[ins->ea[1].reg], ins->ea[0].reg, ins->sz);
            M68k_set_dr(this, ins->ea[1].reg, this->state.instruction.result, ins->sz);
        STEP(3)
INS_END

M68KINS(ROL_dr_dr)
        STEP0
            M68k_sample_interrupts(this);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            u32 cnt = this->regs.D[ins->ea[0].reg] & 63;
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + cnt * 2, M68kS_exec);
        STEP(2)
            u32 cnt = this->regs.D[ins->ea[0].reg] & 63;
            this->state.instruction.result = ROL(this, this->regs.D[ins->ea[1].reg], cnt, ins->sz);
            M68k_set_dr(this, ins->ea[1].reg, this->state.instruction.result, ins->sz);
INS_END

M68KINS(ROL_ea)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, 0, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.instruction.result = ROL(this, this->state.op[0].val, 1, ins->sz);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_write_operand(this, 0, 0, M68kS_exec, 1, 0);
        STEP(3)
INS_END

M68KINS(ROR_qimm_dr)
        STEP0
            M68k_sample_interrupts(this);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + ins->ea[0].reg * 2, M68kS_exec);
        STEP(2)
            this->state.instruction.result = ROR(this, this->regs.D[ins->ea[1].reg], ins->ea[0].reg, ins->sz);
            M68k_set_dr(this, ins->ea[1].reg, this->state.instruction.result, ins->sz);
        STEP(3)
INS_END

M68KINS(ROR_dr_dr)
        STEP0
            M68k_sample_interrupts(this);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            u32 cnt = this->regs.D[ins->ea[0].reg] & 63;
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + cnt * 2, M68kS_exec);
        STEP(2)
            u32 cnt = this->regs.D[ins->ea[0].reg] & 63;
            this->state.instruction.result = ROR(this, this->regs.D[ins->ea[1].reg], cnt, ins->sz);
            M68k_set_dr(this, ins->ea[1].reg, this->state.instruction.result, ins->sz);
INS_END

M68KINS(ROR_ea)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, 0, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.instruction.result = ROR(this, this->state.op[0].val, 1, ins->sz);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_write_operand(this, 0, 0, M68kS_exec, 1, 0);
        STEP(3)
INS_END

M68KINS(ROXL_qimm_dr)
        STEP0
            M68k_sample_interrupts(this);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + ins->ea[0].reg * 2, M68kS_exec);
        STEP(2)
            this->state.instruction.result = ROXL(this, this->regs.D[ins->ea[1].reg], ins->ea[0].reg, ins->sz);
            M68k_set_dr(this, ins->ea[1].reg, this->state.instruction.result, ins->sz);
        STEP(3)
INS_END

M68KINS(ROXL_dr_dr)
        STEP0
            M68k_sample_interrupts(this);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            u32 cnt = this->regs.D[ins->ea[0].reg] & 63;
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + cnt * 2, M68kS_exec);
        STEP(2)
            u32 cnt = this->regs.D[ins->ea[0].reg] & 63;
            this->state.instruction.result = ROXL(this, this->regs.D[ins->ea[1].reg], cnt, ins->sz);
            M68k_set_dr(this, ins->ea[1].reg, this->state.instruction.result, ins->sz);
INS_END

M68KINS(ROXL_ea)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, 0, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.instruction.result = ROXL(this, this->state.op[0].val, 1, ins->sz);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_write_operand(this, 0, 0, M68kS_exec, 1, 0);
        STEP(3)
INS_END

M68KINS(ROXR_qimm_dr)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
            M68k_sample_interrupts(this);
        STEP(1)
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + ins->ea[0].reg * 2, M68kS_exec);
        STEP(2)
            this->state.instruction.result = ROXR(this, this->regs.D[ins->ea[1].reg], ins->ea[0].reg, ins->sz);
            M68k_set_dr(this, ins->ea[1].reg, this->state.instruction.result, ins->sz);
        STEP(3)
INS_END

M68KINS(ROXR_dr_dr)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
            M68k_sample_interrupts(this);
        STEP(1)
            u32 cnt = this->regs.D[ins->ea[0].reg] & 63;
            M68k_start_wait(this, (ins->sz == 4 ? 4 : 2) + cnt * 2, M68kS_exec);
        STEP(2)
            u32 cnt = this->regs.D[ins->ea[0].reg] & 63;
            this->state.instruction.result = ROXR(this, this->regs.D[ins->ea[1].reg], cnt, ins->sz);
            M68k_set_dr(this, ins->ea[1].reg, this->state.instruction.result, ins->sz);
INS_END

M68KINS(ROXR_ea)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, 0, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.instruction.result = ROXR(this, this->state.op[0].val, 1, ins->sz);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_write_operand(this, 0, 0, M68kS_exec, 1, 0);
        STEP(3)
INS_END

M68KINS(RTE)
        STEP0
            M68k_sample_interrupts(this);
            if (!this->regs.SR.S) {
                M68k_start_group2_exception(this, M68kIV_privilege_violation, 4, this->regs.PC-2);
                return;
            }
            this->state.internal_interrupt_level = 0; // We can get all? interrupts again!
            this->state.instruction.result = M68k_get_SSP(this);
            M68k_inc_SSP(this, 6);
            M68k_start_read(this, this->state.instruction.result+0, 2, MAKE_FC(0), M68K_RW_ORDER_NORMAL, M68kS_exec);
        STEP(1)
            this->state.op[1].val = MAKE_FC(0);
            M68k_set_SR(this, this->state.bus_cycle.data, 1);
            M68k_start_read(this, this->state.instruction.result+2, 2, this->state.op[1].val, M68K_RW_ORDER_NORMAL, M68kS_exec);
        STEP(2)
            this->state.op[0].val = this->state.bus_cycle.data << 16;
            M68k_start_read(this, this->state.instruction.result+4, 2, this->state.op[1].val, M68K_RW_ORDER_NORMAL, M68kS_exec);
        STEP(3)
            this->state.op[0].val |= this->state.bus_cycle.data;
            this->regs.IPC -= 2;
            M68k_start_read(this, this->state.op[0].val, 2, MAKE_FC(1), M68K_RW_ORDER_NORMAL, M68kS_exec);
        STEP(4)
            this->regs.IR = this->regs.IRC;
            this->regs.IRC = this->state.bus_cycle.data;
            M68k_start_read(this, this->state.op[0].val+2, 2, MAKE_FC(1), M68K_RW_ORDER_NORMAL, M68kS_exec);
        STEP(5)
            this->regs.IR = this->regs.IRC;
            this->regs.IRC = this->state.bus_cycle.data;
            this->regs.PC = this->state.op[0].val+4;
INS_END

M68KINS(RTR)
        STEP0
            this->regs.IPC -= 2;
            M68k_sample_interrupts(this);
            M68k_start_pop(this, 2, MAKE_FC(0), M68kS_exec);
        STEP(1)
            this->regs.SR.u = (this->regs.SR.u & 0xFFE0) | (this->state.bus_cycle.data & 0x1F);
            M68k_start_pop(this, 4, MAKE_FC(0), M68kS_exec);
        STEP(2)
            this->regs.PC = this->state.bus_cycle.data;
            M68k_start_prefetch(this, 2, 1, M68kS_exec);
        STEP(3)
INS_END

M68KINS(RTS)
        STEP0
            this->regs.IPC -= 2;
            M68k_sample_interrupts(this);
            M68k_start_pop(this, 4, MAKE_FC(0), M68kS_exec);
        STEP(1)
            this->regs.PC = this->state.bus_cycle.data;
            M68k_start_prefetch(this, 2, 1, M68kS_exec);
        STEP(2)
INS_END

M68KINS(SBCD)
        STEP0
            M68k_start_read_operands(this, 1, ins->sz, M68kS_exec, 0, 0, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            // Thanks CLK for logic
            u8 rhs = this->state.op[0].val;
            u8 lhs = this->state.op[1].val;
            u8 extend = this->regs.SR.X;
            int unadjusted_result = lhs - rhs - extend;
            const int top = (lhs & 0xf0) - (rhs & 0xf0) - (0x60 & (unadjusted_result >> 4));
            int result = (lhs & 0xf) - (rhs & 0xf) - extend;
            const int low_adjustment = 0x06 & (result >> 4);
            this->regs.SR.X = this->regs.SR.C = !!((unadjusted_result - low_adjustment) & 0x300);
            result = result + top - low_adjustment;

            this->regs.SR.Z = (result & 0xFF) ? 0 : this->regs.SR.Z;
            this->regs.SR.N = !!(result & 0x80);
            this->regs.SR.V = !!(unadjusted_result & ~result & 0x80);
            this->state.instruction.result = result;
            M68k_start_write_operand(this, 0, 1, M68kS_exec, 1, 0);
        STEP(3)
            if (ins->ea[0].kind == M68k_AM_data_register_direct)
                M68k_start_wait(this, 2, M68kS_exec);
        STEP(4)
INS_END

M68KINS(SCC)
        STEP0
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, 0, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            if (!condition(this, ins->ea[0].reg)) {
                this->state.instruction.result = 0;
            }
            else {
                this->state.instruction.result = clip32[ins->sz] & 0xFFFFFFFF;
                if (ins->ea[1].kind == M68k_AM_data_register_direct) M68k_start_wait(this, 2, M68kS_exec);
            }
        STEP(3)
            M68k_start_write_operand(this, 0, 1, M68kS_exec, 1, 0);
        STEP(4)
INS_END

M68KINS(STOP)
        STEP0
            printf("\nSTOP!");
            if (!this->regs.SR.S) {
                M68k_start_group2_exception(this, M68kIV_privilege_violation, 4, this->regs.PC-2);
                return;
            }
            this->state.instruction.result = this->regs.IRC;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            M68k_set_SR(this, this->state.instruction.result, 0);
            this->state.current = M68kS_stopped;
            this->state.stopped.next_state = M68kS_exec;
            M68k_sample_interrupts(this);
        STEP(2)
            if (this->state.current == M68kS_stopped) return;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(3)

INS_END

M68KINS(SUB)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            if ((ins->sz == 2) && (ins->ea[0].kind == M68k_AM_data_register_direct) && (ins->ea[1].kind == M68k_AM_address_register_indirect_with_predecrement))
                hold = 0;
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.instruction.result = SUB(this, this->state.op[0].val, this->state.op[1].val, ins->sz, 0, 1);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            M68k_start_write_operand(this, 0, 1, M68kS_exec, 1, 0);
        STEP(3)
            if (ins->sz == 4) {
                if (ins->ea[1].kind == M68k_AM_data_register_direct) {
                    switch (ins->ea[0].kind) {
                        case M68k_AM_data_register_direct:
                        case M68k_AM_address_register_direct:
                        case M68k_AM_immediate:
                            M68k_start_wait(this, 4, M68kS_exec);
                            break;
                        default:
                            M68k_start_wait(this, 2, M68kS_exec);
                    }
                }
            } else {
            }
        STEP(4)
INS_END

M68KINS(SUBA)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            u32 a = sgn32(this->state.op[0].val, ins->sz);
            u32 b = this->regs.A[ins->ea[1].reg];
            this->state.instruction.result = b - a;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            this->regs.A[ins->ea[1].reg] = this->state.instruction.result;
            if ((ins->sz != 4) || (ins->ea[0].kind == M68k_AM_data_register_direct) || (ins->ea[0].kind == M68k_AM_address_register_direct) || (ins->ea[0].kind == M68k_AM_immediate))
                M68k_start_wait(this, 4, M68kS_exec);
            else
                M68k_start_wait(this, 2, M68kS_exec);
        STEP(3)
INS_END

M68KINS(SUBI_21)
        STEP0
            if (ins->sz == 2) this->regs.IPC -= 2;
            STARTI(1, 0);
        STEP(2)
            M68k_sample_interrupts(this);
        STEP(3)
INS_END

M68KINS_NOSWITCH(SUBI)
    if ((ins->sz <= 2) && (this->state.instruction.TCU < 2)) return M68k_ins_SUBI_21(this, ins);
    switch(this->state.instruction.TCU) {
        STEP0
            STARTI(0, 1);
        STEP(2)
            M68k_sample_interrupts(this);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(3)
            this->state.instruction.result = SUB(this, this->state.instruction.result, this->state.op[0].val, ins->sz,
                                                 0, 1);
            M68k_start_write_operand(this, 0, 0, M68kS_exec, 1, 0);
        STEP(4)
            if ((ins->sz == 4) && (ins->ea[0].kind == M68k_AM_data_register_direct))
                M68k_start_wait(this, 4, M68kS_exec);
        STEP(5)
INS_END

M68KINS(SUBQ)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.instruction.result = SUB(this, this->state.op[0].val, this->state.op[1].val, ins->sz, 0, 1);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
            /*if (ins->ea[1].kind == M68k_AM_address_register_indirect_with_predecrement) {
                this->state.instruction.result += ins->sz;
            }*/
            M68k_start_write_operand(this, 0, 1, M68kS_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
            if ((ins->ea[1].kind == M68k_AM_data_register_direct) && (ins->sz == 4)) {
                if (ins->ea[0].kind == M68k_AM_address_register_direct)
                    M68k_start_wait(this, 6, M68kS_exec);
                else
                    M68k_start_wait(this, 4, M68kS_exec);
            }
        STEP(4)
INS_END

M68KINS(SUBQ_ar)
        STEP0
            M68k_sample_interrupts(this);
            this->state.instruction.result = this->regs.A[ins->ea[1].reg] - ins->ea[0].reg;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(1)
            M68k_set_ar(this, ins->ea[1].reg, this->state.instruction.result, 4);
            M68k_start_wait(this, 4, M68kS_exec);
        STEP(2)
INS_END

M68KINS(SUBX_sz4_predec)
        STEP0
            M68k_start_read_operands(this, 1, ins->sz, M68kS_exec, 0, HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.instruction.result = SUB(this, this->state.op[0].val, this->state.op[1].val, ins->sz, 1, 1);
            this->regs.A[ins->ea[1].reg] += 4;
        STEP(2)
            this->regs.A[ins->ea[1].reg] -= 2;
            M68k_start_write(this, this->regs.A[ins->ea[1].reg], this->state.instruction.result & 0xFFFF, 2, MAKE_FC(0), M68K_RW_ORDER_NORMAL, M68kS_exec);
        STEP(3)
            this->state.instruction.prefetch++;
            M68k_start_prefetch(this, 1, 1, M68kS_exec); // all prefetches are done before writes
        STEP(4)
            this->regs.A[ins->ea[1].reg] -= 2;
            M68k_start_write(this, this->regs.A[ins->ea[1].reg], this->state.instruction.result >> 16, 2, MAKE_FC(0), M68K_RW_ORDER_NORMAL, M68kS_exec);
        STEP(5)
INS_END

M68KINS(SUBX_sz4_nopredec)
        STEP0
            M68k_start_read_operands(this, 1, ins->sz, M68kS_exec, 0, 0, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.instruction.result = SUB(this, this->state.op[0].val, this->state.op[1].val, ins->sz, 1, 1);
            M68k_start_prefetch(this, 1, 1, M68kS_exec); // all prefetches are done before writes
        STEP(2)
            M68k_start_write_operand(this, 0, 1, M68kS_exec, 1, 0);
        STEP(3)
            M68k_start_wait(this, 4, M68kS_exec);
        STEP(4)
INS_END

M68KINS(SUBX_sz1_2)
        STEP0
            M68k_start_read_operands(this, 1, ins->sz, M68kS_exec, 0, 0, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->state.instruction.result = SUB(this, this->state.op[0].val, this->state.op[1].val, ins->sz, 1, 1);
            M68k_start_prefetch(this, 1, 1, M68kS_exec); // all prefetches are done before writes
        STEP(2)
            M68k_start_write_operand(this, 0, 1, M68kS_exec, 1, 0);
        STEP(3)
INS_END

M68KINS_NOSWITCH(SUBX)
    if (ins->sz == 4) {
        u32 u = (ins->operand_mode == M68k_OM_ea_ea) || (ins->operand_mode == M68k_OM_r_ea);
        if (u && (ins->ea[1].kind == M68k_AM_address_register_indirect_with_predecrement)) {
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
            u32 a = this->regs.D[ins->ea[0].reg];
            a = (a << 16) | (a >> 16);
            this->regs.D[ins->ea[0].reg] = a;
            this->regs.SR.C = this->regs.SR.V = 0;
            this->regs.SR.Z = a == 0;
            this->regs.SR.N = !!(a & 0x80000000);
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
            M68k_sample_interrupts(this);
        STEP(1)
INS_END

M68KINS(TRAP)
        STEP0
            M68k_start_group2_exception(this, ins->ea[0].reg + 32, 4, this->regs.PC);
        STEP(1)
            M68k_sample_interrupts(this);
INS_END

M68KINS(TRAPV)
        STEP0
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
            M68k_sample_interrupts(this);
        STEP(1)
            if (this->regs.SR.V) {
                M68k_start_group2_exception(this, M68kIV_trapv_instruction, 0, this->regs.PC-2);
                return;
            }
        STEP(2)
INS_END

M68KINS(TST)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            M68k_start_read_operands(this, 0, ins->sz, M68kS_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            M68k_sample_interrupts(this);
            this->regs.SR.C = this->regs.SR.V = 0;
            u32 v = clip32[ins->sz] & this->state.op[0].val;
            this->regs.SR.Z = v == 0;
            if (ins->sz == 1) v = SIGNe8to32(v);
            else if (ins->sz == 2) v = SIGNe16to32(v);
            this->regs.SR.N = v >= msb32[ins->sz];
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
INS_END

M68KINS(UNLK)
        STEP0
            M68k_sample_interrupts(this);
            this->state.op[0].addr = this->regs.A[7];
            this->state.instruction.result = this->regs.A[ins->ea[0].reg];
            M68k_start_read(this, this->state.instruction.result, 4, MAKE_FC(0), NO_REVERSE, M68kS_exec);
        STEP(1)
            this->regs.A[7] = this->state.instruction.result + 4;
            this->regs.A[ins->ea[0].reg] = this->state.bus_cycle.data;
            M68k_start_prefetch(this, 1, 1, M68kS_exec);
        STEP(2)
INS_END



static u32 M68k_disasm_BAD(struct M68k_ins_t *ins, struct jsm_debug_read_trace *rt, struct jsm_string *out)
{
    printf("\nERROR UNIMPLEMENTED DISASSEMBLY %04x", ins->opcode);
    jsm_string_sprintf(out, "UNIMPLEMENTED DISASSEMBLY %s", __func__);
    return 0;
}


M68KINS(NOINS)
    STEP0
        printf("\nERROR UNIMPLEMENTED M68K INSTRUCTION %04x at PC %06x cyc:%lld", ins->opcode, this->regs.PC, *this->trace.cycles);
        //assert(1==0);
        //dbg_break("UNIMPLEMENTED INSTRUCTION", *this->trace.cycles);
        M68k_start_wait(this, 2, M68kS_exec);
    STEP(1)
INS_END

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
    *ins = (struct M68k_ins_t) {
        .opcode = out,
        .disasm = disasm_func,
        .exec = exec_func,
        .sz = sz,
        .variant = variant,
        .operand_mode = operand_mode
    };
    if (ea1)
        memcpy(&ins->ea[0], ea1, sizeof(struct M68k_EA));
    if (ea2)
        memcpy(&ins->ea[1], ea2, sizeof(struct M68k_EA));
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
