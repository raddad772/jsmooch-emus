//
// Created by . on 5/15/24.
//

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cmath>

#include "helpers/intrinsics.h"

#include "m68000.h"
#include "m68000_instructions.h"
#include "generated/generated_disasm.h"

#define ALLOW_REVERSE true
#define NO_REVERSE false

#define HOLD true
#define NO_HOLD false

#define FORCE_REVERSE true
#define NO_FORCE false
namespace M68k {
ins_t decoded[65536];

void core::start_push(u32 val, u32 sz, states next_state, u32 reverse)
{
    regs.A[7] -= sz;
    if (sz == 1) regs.A[7]--;
    start_write(regs.A[7], val, sz, MAKE_FC(0), reverse, next_state);
}

void core::start_pop(u32 sz, u32 FC, states next_state)
{
    start_read(regs.A[7], sz, FC, M68K_RW_ORDER_NORMAL, next_state);
    regs.A[7] += sz;
    if (sz == 1) regs.A[7]++;
    //inc_SSP(sz);
}

static u32 bitsize[5] = { 0, 8, 16, 0, 32 };
static u32 clip32[5] = { 0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF };
static u32 msb32[5] = { 0, 0x80, 0x8000, 0, 0x80000000 };

static i32 sgn32(u32 num, u32 sz) {
    switch(sz) {
        case 1:
            return static_cast<i8>(num);
        case 2:
            return static_cast<i16>(num);
        case 4:
            return static_cast<i32>(num);
        default:
            assert(1==0);
    }
    return 0;
}

u32 core::ADD(u32 op1, u32 op2, u32 sz, u32 extend)
{
    // Thanks Ares
    u32 target = clip32[sz] & op1;
    u32 source = clip32[sz] & op2;
    u32 result = target + source + (extend ? regs.SR.X : 0);
    u32 carries = target ^ source ^ result;
    u32 overflow = (target ^ result) & (source ^ result);

    regs.SR.C = !!((carries ^ overflow) & msb32[sz]);
    regs.SR.V = !!(overflow & msb32[sz]);
    regs.SR.Z = !!((clip32[sz] & result) ? 0 : (extend ? regs.SR.Z : 1));
    regs.SR.N = sgn32(result,sz) < 0;
    regs.SR.X = regs.SR.C;

    return clip32[sz] & result;
}

u32 core::AND(u32 source, u32 target, u32 sz)
{
    u32 result = source & target;
    regs.SR.C = regs.SR.V = 0;
    regs.SR.Z = (clip32[sz] & result) == 0;
    regs.SR.N = !!(result & msb32[sz]);
    return clip32[sz] & result;
}


u32 core::ASL(u32 result, u32 shift, u32 sz)
{
    u32 carry = 0;
    u32 overflow = 0;
    for (u32 i = 0; i < shift; i++) {
        carry = !!(result & msb32[sz]);
        u32 before = result;
        result <<= 1;
        overflow |= before ^ result;
    }

    regs.SR.C = carry;
    regs.SR.V = sgn32(overflow, sz) < 0;
    regs.SR.Z = (clip32[sz] & result) == 0;
    regs.SR.N = !!(msb32[sz] & result);
    if (shift > 0) regs.SR.X = regs.SR.C;

    return clip32[sz] & result;
}

u32 core::ASR(u32 result, u32 shift, u32 sz)
{
    u32 carry = 0;
    u32 overflow = 0;
    for (u32 i = 0; i < shift; i++) {
        carry = result & 1;
        u32 before = result;
        result = sgn32(result, sz) >> 1;
        overflow |= before ^ result;
    }

    regs.SR.C = carry;
    regs.SR.V = sgn32(overflow, sz) < 0;
    regs.SR.Z = (clip32[sz] & result) == 0;
    regs.SR.N = !!(msb32[sz] & result);
    if (shift > 0) regs.SR.X = regs.SR.C;

    return clip32[sz] & result;
}

u32 core::CMP(u32 source, u32 target, u32 sz) {
    u32 result = (target - source) & clip32[sz];
    u32 carries = target ^ source ^ result;
    u32 overflow = (target ^ result) & (source ^ target);

    regs.SR.C = !!((carries ^ overflow) & msb32[sz]);
    regs.SR.V = !!(overflow & msb32[sz]);
    regs.SR.Z = (result & clip32[sz]) == 0;
    regs.SR.N = sgn32(result, sz) < 0;

    return result;
}

u32 core::ROL(u32 result, u32 shift, u32 sz)
{
    u32 carry = 0;
    for (u32 i = 0; i < shift; i++) {
        carry = !!(result & msb32[sz]);
        result = (result << 1) | carry;
    }

    regs.SR.C = carry;
    regs.SR.V = 0;
    regs.SR.Z = (clip32[sz] & result) == 0;
    regs.SR.N = !!(msb32[sz] & result);

    return clip32[sz] & result;
}

u32 core::ROR(u32 result, u32 shift, u32 sz)
{
    u32 carry = 0;
    for (u32 i = 0; i < shift; i++) {
        carry = result & 1;
        result >>= 1;
        if (carry) result |= msb32[sz];
        else result &= ~msb32[sz];
    }

    regs.SR.C = carry;
    regs.SR.V = 0;
    regs.SR.Z = (clip32[sz] & result) == 0;
    regs.SR.N = !!(msb32[sz] & result);

    return clip32[sz] & result;
}

u32 core::ROXL(u32 result, u32 shift, u32 sz)
{
    u32 carry = regs.SR.X;
    for (u32 i = 0; i < shift; i++) {
        u32 extend = carry;
        carry = !!(result & msb32[sz]);
        result = result << 1 | extend;
    }

    regs.SR.C = regs.SR.X = carry;
    regs.SR.V = 0;
    regs.SR.Z = (clip32[sz] & result) == 0;
    regs.SR.N = !!(msb32[sz] & result);

    return clip32[sz] & result;
}

u32 core::ROXR(u32 result, u32 shift, u32 sz)
{
    u32 carry = regs.SR.X;
    for (u32 i = 0; i < shift; i++) {
        u32 extend = carry;
        carry = result & 1;
        result >>= 1;
        if (extend) result |= msb32[sz];
        else result &= ~msb32[sz];
    }

    regs.SR.C = regs.SR.X = carry;
    regs.SR.V = 0;
    regs.SR.Z = (clip32[sz] & result) == 0;
    regs.SR.N = !!(msb32[sz] & result);

    return clip32[sz] & result;
}


u32 core::SUB(u32 op1, u32 op2, u32 sz, u32 extend, u32 change_x)
{
    u32 target = clip32[sz] & op2;
    u32 source = clip32[sz] & op1;
    u32 result = target - source - (extend ? regs.SR.X : 0);
    u32 carries = target ^ source ^ result;
    u32 overflow = (target ^ result) & (source ^ target);

    regs.SR.C = !!((carries ^ overflow) & msb32[sz]);
    regs.SR.V = !!(overflow & msb32[sz]);
    regs.SR.Z = !!((clip32[sz] & result) ? 0 : (extend ? regs.SR.Z : 1));
    regs.SR.N = sgn32(result,sz) < 0;
    if (change_x) regs.SR.X = regs.SR.C;

    return result & clip32[sz];
}

u32 core::LSL(u32 result, u32 shift, u32 sz)
{
    u32 carry = 0;
    for (u32 i = 0; i < shift; i++) {
        carry = !!(result & msb32[sz]);
        result <<= 1;
    }

    regs.SR.C = carry;
    regs.SR.V = 0;
    regs.SR.Z = (clip32[sz] & result) == 0;
    regs.SR.N = !!(msb32[sz] & result);
    if (shift > 0) regs.SR.X = regs.SR.C;

    return clip32[sz] & result;
}

u32 core::LSR(u32 result, u32 shift, u32 sz)
{
    u32 carry = 0;
    result &= clip32[sz];
    for (u32 i = 0; i < shift; i++) {
        carry = result & 1;
        result >>= 1;
    }

    regs.SR.C = carry;
    regs.SR.V = 0;
    regs.SR.Z = (clip32[sz] & result) == 0;
    regs.SR.N = !!(msb32[sz] & result);
    if (shift > 0) regs.SR.X = regs.SR.C;

    return clip32[sz] & result;
}

u32 core::EOR(u32 source, u32 target, u32 sz)
{
    u32 result = target ^ source;
    regs.SR.C = regs.SR.V = 0;
    result &= clip32[sz];
    regs.SR.Z = result == 0;
    regs.SR.N = !!(msb32[sz] & result);

    return result;
}

u32 core::OR(u32 source, u32 target, u32 sz)
{
    u32 result = source | target;
    regs.SR.C = regs.SR.V = 0;
    regs.SR.Z = (clip32[sz] & result) == 0;
    regs.SR.N = (sgn32(result, sz)) < 0;

    return clip32[sz] & result;
}

u32 core::condition(u32 condition) const {
    switch(condition) {
        case 0: return 1;
        case 1: return 0;
        case  2: return !regs.SR.C && !regs.SR.Z;  //HI
        case  3: return  regs.SR.C ||  regs.SR.Z;  //LS
        case  4: return !regs.SR.C;  //CC,HS
        case  5: return  regs.SR.C;  //CS,LO
        case  6: return !regs.SR.Z;  //NE
        case  7: return  regs.SR.Z;  //EQ
        case  8: return !regs.SR.V;  //VC
        case  9: return  regs.SR.V;  //VS
        case 10: return !regs.SR.N;  //PL
        case 11: return  regs.SR.N;  //MI
        case 12: return  regs.SR.N ==  regs.SR.V;  //GE
        case 13: return  regs.SR.N !=  regs.SR.V;  //LT
        case 14: return  regs.SR.N ==  regs.SR.V && !regs.SR.Z;  //GT
        case 15: return  regs.SR.N !=  regs.SR.V ||  regs.SR.Z;  //LE
        default: assert(1==2);
    }
    NOGOHERE;
    return 0;
}

#define HOLD_WORD_POSTINC             u32 hold = HOLD;\
                    if ((ins->sz == 2) && ((ins->ea[0].kind == AM_address_register_indirect_with_postincrement) || (ins->ea[1].kind == AM_address_register_indirect_with_postincrement))) hold = NO_HOLD
#define HOLD_WORD_POSTINC_PREDEC             u32 hold = HOLD;\
                    if ((ins->sz == 2) && ((ins->ea[0].kind == AM_address_register_indirect_with_predecrement) || (ins->ea[0].kind == AM_address_register_indirect_with_postincrement) || (ins->ea[1].kind == AM_address_register_indirect_with_postincrement)) || (ins->ea[1].kind == AM_address_register_indirect_with_predecrement)) hold = NO_HOLD
#define HOLDL_l_predec_no if ((ins->sz == 4) && ((ins->ea[0].kind == AM_address_register_indirect_with_predecrement) || (ins->ea[1].kind == AM_address_register_indirect_with_predecrement))) hold = NO_HOLD
#define HOLDW_imm_predec_no if ((ins->sz == 2) && (ins->ea[0].kind == AM_quick_immediate) && (ins->ea[1].kind == AM_address_register_indirect_with_predecrement)) hold = 0;


#define DELAY_IF_REGWRITE(k, n) if ((ins->sz == 4) && ((k == AM_data_register_direct) || (k == AM_address_register_direct))) start_wait(n, S_exec);

#define export_INS(x) void core::ins_##x() { \
    switch(state.instruction.TCU) {

#define INS(x) void core::ins_##x() { \
    switch(state.instruction.TCU) {

#define export_INS_NOSWITCH(x) void core::ins_##x() {

#define INS_NOSWITCH(x) void core::ins_##x() {
#define INS_END_NOSWITCH }

void core::set_IPC() {
    regs.IPC = regs.PC;
}

#define STEP0 case 0: { \
            set_IPC();\
            state.instruction.done = 0;

#define STEP(x) break; }\
                case x: {

#define INS_END \
            state.instruction.done = 1; \
            break; }\
    }\
    state.instruction.TCU++;            \
}

#define INS_END_NOCOMPLETE \
            break; }\
    }\
    state.instruction.TCU++;            \
}


INS(RESET_POWER)
        STEP0
            pins.RESET = 0;
            start_read(0, 2, FC_supervisor_program, M68K_RW_ORDER_NORMAL, S_exec);
        STEP(1)
            // First word will be
            state.instruction.result = state.bus_cycle.data << 16;
            start_read(2, 2, FC_supervisor_program, M68K_RW_ORDER_NORMAL, S_exec);
        STEP(2)
            set_SSP(state.instruction.result | state.bus_cycle.data);
            state.instruction.result = 0;
            start_read(4, 2, FC_supervisor_program, M68K_RW_ORDER_NORMAL, S_exec);
        STEP(3)
            state.instruction.result = state.bus_cycle.data << 16;
            start_read(6, 2, FC_supervisor_program, M68K_RW_ORDER_NORMAL, S_exec);
        STEP(4)
            regs.PC = state.instruction.result | state.bus_cycle.data;
            regs.SR.I = 7;
            // Start filling prefetch queue
            start_read(regs.PC, 2, FC_supervisor_program, M68K_RW_ORDER_NORMAL, S_exec);
            regs.PC += 2;
        STEP(5)
            regs.IRC = state.bus_cycle.data;
            regs.IR = regs.IRC;
            start_read(regs.PC, 2, FC_supervisor_program, M68K_RW_ORDER_NORMAL, S_exec);
            regs.PC += 2;
        STEP(6)
            regs.IRC = state.bus_cycle.data;
            regs.IRD = regs.IR;
INS_END

// IRD holds currently-executing instruction,
// IR holds currently-decoding instruction,
// IRC holds last pre-fetched word

#define BADINS { printf("\nUNIMPLEMENTED INSTRUCTION! %s", __func__); assert(1==0); }

INS(ALINE)
        STEP0
            start_group2_exception(IV_line1010, 4, regs.PC-2);
        STEP(1)
INS_END

INS(FLINE)
        STEP0
            start_group2_exception(IV_line1111, 4, regs.PC-2);
        STEP(1)
INS_END

INS(TAS)
        STEP0
            start_read_operands(0, ins->sz, S_exec, 0, NO_HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.instruction.result = state.op[0].val | 0x80;
            start_wait(2, S_exec);
        STEP(2)
            // Write or wait
            if ((!megadrive_bug) || (ins->ea[0].kind == AM_data_register_direct)) {
                start_write_operand(0, 0, S_exec, ALLOW_REVERSE, NO_FORCE);
            } else {
                start_wait(4, S_exec);
            }
        STEP(3)
            start_prefetch(1, 1, S_exec);
            regs.SR.C = regs.SR.V = 0;
            regs.SR.Z = (state.op[0].val & 0XFF) == 0;
            regs.SR.V = !!(state.op[0].val & 0x80);
        STEP(4)

INS_END


INS(ABCD)
        STEP0
            start_read_operands(1, ins->sz, S_exec, 0, false, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(2)
            u8 source = state.op[0].val;
            u8 dest = state.op[1].val;
            u32 extend = regs.SR.X;
            u32 unadjusted_result = dest + source + extend;

            u32 result = (dest & 15) + (source & 15) + extend;
            result += (dest & 0xF0) + (source & 0xF0) + (((9 - result) >> 4) & 6);
            result += ((0x9f - result) >> 4) & 0x60;

            regs.SR.Z = (result & 0xFF) ? 0 : regs.SR.Z;
            regs.SR.X = regs.SR.C = result > 0xFF;
            regs.SR.N = !!(result & 0x80);
            regs.SR.V = !!(~unadjusted_result & result & 0x80);
            state.instruction.result = result;
            start_write_operand(0, 1, S_exec, true, false);
        STEP(3)
            if (ins->ea[0].kind == AM_data_register_direct)
                start_wait(2, S_exec);
        STEP(4)
INS_END

INS(ADD)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            start_read_operands(0, ins->sz, S_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.instruction.result = ADD(state.op[0].val, state.op[1].val, ins->sz, 0);
            start_prefetch(1, 1, S_exec);
        STEP(2)
            start_write_operand(0, 1, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
            if (ins->sz == 4) {
                if (ins->ea[1].kind == AM_data_register_direct) {
                    switch (ins->ea[0].kind) {
                        case AM_data_register_direct:
                        case AM_address_register_direct:
                        case AM_immediate:
                            start_wait(4, S_exec);
                            break;
                        default:
                            start_wait(2, S_exec);
                    }
                }
            }
        STEP(4)
INS_END

INS(ADDA)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            start_read_operands(0, ins->sz, S_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            u32 a = sgn32(state.op[0].val, ins->sz);
            u32 b = regs.A[ins->ea[1].reg];
            state.instruction.result = b + a;
            start_prefetch(1, 1, S_exec);
        STEP(2)
            u32 n = ins->ea[1].reg;
            set_ar(n, state.instruction.result, 4);
            if ((ins->sz != 4) || (ins->ea[0].kind == AM_data_register_direct) || (ins->ea[0].kind == AM_address_register_direct) || (ins->ea[0].kind == AM_immediate))
                start_wait(4, S_exec);
            else
                start_wait(2, S_exec);
        STEP(3)
INS_END

#define STARTI(force, allow) \
            state.instruction.result = regs.IRC;\
            start_prefetch((ins->sz >> 1) ? (ins->sz >> 1) : 1, 1, S_exec);\
        STEP(1)\
            u32 o = state.instruction.result;\
            switch(ins->sz) {\
                case 1:\
                    state.instruction.result = o & 0xFF;\
                    break;\
                case 2:\
                    state.instruction.result = o;\
                    break;\
                case 4:\
                    state.instruction.result = (o << 16) | regs.IR;\
                    break;\
                default:\
                    assert(1==0);\
            }  \
            regs.IPC += 4;                            \
            HOLD_WORD_POSTINC_PREDEC; \
            HOLDL_l_predec_no;                              \
        start_read_operands(0, ins->sz, S_exec, 0, (force ? allow : (allow & hold)), NO_REVERSE, MAKE_FC(0));


INS(ADDI)
        STEP0
        if (ins->sz == 2) regs.IPC -= 2;
        STARTI(0,1);
        STEP(2)
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(3)
            state.instruction.result = ADD(state.instruction.result, state.op[0].val, ins->sz,
                                                 0);
            start_write_operand(0, 0, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(4)
            if ((ins->sz == 4) && (ins->ea[0].kind == AM_data_register_direct))
                start_wait(4, S_exec);
        STEP(5)
INS_END

INS(ADDQ_ar)
        STEP0
            state.instruction.result = regs.A[ins->ea[1].reg] + ins->ea[0].reg;
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(1)
            set_ar(ins->ea[1].reg, state.instruction.result, 4);
            start_wait(4, S_exec);
        STEP(2)
INS_END

INS(ADDQ)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            start_read_operands(0, ins->sz, S_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.instruction.result = ADD(state.op[0].val, state.op[1].val, ins->sz, 0);
            start_prefetch(1, 1, S_exec);
        STEP(2)
            start_write_operand(0, 1, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
            if (ins->sz == 4) {
                if ((ins->ea[1].kind == AM_data_register_direct) && (ins->sz == 4)) {
                    if (ins->ea[0].kind == AM_address_register_direct)
                        start_wait(6, S_exec);
                    else
                        start_wait(4, S_exec);
                }
            }
        STEP(4)
INS_END

INS(ADDX_sz4_predec)
        STEP0
            start_read_operands(1, ins->sz, S_exec, 0, HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.instruction.result = ADD(state.op[0].val, state.op[1].val, ins->sz, 1);
            regs.A[ins->ea[1].reg] += 2;
            start_write(regs.A[ins->ea[1].reg], state.instruction.result & 0xFFFF, 2, MAKE_FC(0),
                             M68K_RW_ORDER_NORMAL, S_exec);
        STEP(2)
            //state.instruction.prefetch++;
            start_prefetch(1, 1, S_exec); // all prefetches are done before writes
        STEP(3)
            regs.A[ins->ea[1].reg] -= 2;
            start_write(regs.A[ins->ea[1].reg], state.instruction.result >> 16, 2, MAKE_FC(0),
                             M68K_RW_ORDER_NORMAL, S_exec);
        STEP(4)
INS_END

INS(ADDX_sz4_nopredec)
        STEP0
            start_read_operands(1, ins->sz, S_exec, 0, NO_HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.instruction.result = ADD(state.op[0].val, state.op[1].val, ins->sz, 1);
            start_prefetch(1, 1, S_exec); // all prefetches are done before writes
        STEP(2)
            start_write_operand(0, 1, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
            start_wait(4, S_exec);
        STEP(4)
INS_END


INS(ADDX_sz1_2)
        STEP0
            start_read_operands(1, ins->sz, S_exec, 0, NO_HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.instruction.result = ADD(state.op[0].val, state.op[1].val, ins->sz, 1);
            start_prefetch(1, 1, S_exec); // all prefetches are done before writes
        STEP(2)
            start_write_operand(0, 1, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
INS_END

INS_NOSWITCH(ADDX)
    if (ins->sz == 4) {
        u32 u = (ins->operand_mode == OM_ea_ea) || (ins->operand_mode == OM_r_ea);
        if (u && (ins->ea[1].kind == AM_address_register_indirect_with_predecrement)) {
            return ins_ADDX_sz4_predec();
        } else {
            return ins_ADDX_sz4_nopredec();
        }
    } else {
        return ins_ADDX_sz1_2();
    }
INS_END_NOSWITCH

INS(AND)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            if (ins->ea[1].kind == AM_address_register_indirect_with_predecrement) hold = 0;
            start_read_operands(0, ins->sz, S_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(2)
            state.instruction.result = AND(state.op[0].val, state.op[1].val, ins->sz);
            start_write_operand(0, 1, S_exec, ALLOW_REVERSE, NO_FORCE);
            if ((ins->sz == 4) && (ins->ea[1].kind == AM_data_register_direct)) {
                u32 numcycles = 2;
                if (ins->ea[0].kind == AM_data_register_direct) numcycles = 4;
                if (ins->ea[0].kind == AM_immediate) numcycles = 4;
                start_wait(numcycles, S_exec);
            }
        STEP(4)
INS_END

INS(ANDI)
        STEP0
            if (ins->sz == 2)
                regs.IPC -= 2;
        STARTI(0,1);
        STEP(2)
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(3)
            state.instruction.result = AND(state.instruction.result, state.op[0].val, ins->sz);
            start_write_operand(0, 0, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(4)
            if ((ins->sz == 4) && ((ins->ea[0].kind == AM_data_register_direct) ||
                                   (ins->ea[0].kind == AM_address_register_direct))) {
                start_wait(4, S_exec);
            }
        STEP(5)
INS_END

INS(ANDI_TO_CCR)
        STEP0
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(1)
            u32 v = regs.SR.u & 0x1F;
            v &= regs.IR;
            regs.SR.u = (regs.SR.u & 0xFFE0) | v;
            start_wait(8, S_exec);
        STEP(3)
            start_read(regs.PC - 2, 2, MAKE_FC(1), M68K_RW_ORDER_NORMAL, S_exec);
        STEP(4)
            start_prefetch(1, 1, S_exec);
        STEP(5)
INS_END

INS(ANDI_TO_SR)
        STEP0
            if (!regs.SR.S) {
                start_group2_exception(IV_privilege_violation, 4, regs.PC - 2);
                return;
            }
            if (ins->ea[0].kind == 0)
                regs.IPC -= 2;
            start_prefetch(1, 1, S_exec);
        STEP(1)
            u32 data = regs.IR & get_SR();
            set_SR(data, 1);
            sample_interrupts();
            start_wait(8, S_exec);
        STEP(2)
            start_read(regs.PC - 2, 2, MAKE_FC(1), 0, S_exec);
        STEP(3)
            start_prefetch(1, 1, S_exec);
        STEP(4)
INS_END

INS(ASL_qimm_dr)
        STEP0
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(1)
            start_wait((ins->sz == 4 ? 4 : 2) + ins->ea[0].reg * 2, S_exec);
        STEP(2)
            state.instruction.result = ASL(regs.D[ins->ea[1].reg], ins->ea[0].reg, ins->sz);
            set_dr(ins->ea[1].reg, state.instruction.result, ins->sz);
        STEP(3)
INS_END

INS(ASL_dr_dr)
        STEP0
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(1)
            u32 cnt = regs.D[ins->ea[0].reg] & 63;
            start_wait((ins->sz == 4 ? 4 : 2) + cnt * 2, S_exec);
        STEP(2)
            u32 cnt = regs.D[ins->ea[0].reg] & 63;
            state.instruction.result = ASL(regs.D[ins->ea[1].reg], cnt, ins->sz);
            set_dr(ins->ea[1].reg, state.instruction.result, ins->sz);
INS_END

INS(ASL_ea)
        STEP0
            start_read_operands(0, ins->sz, S_exec, 0, false, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.instruction.result = ASL(state.op[0].val, 1, ins->sz);
            start_prefetch(1, 1, S_exec);
        STEP(2)
            start_write_operand(0, 0, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
INS_END

INS(ASR_qimm_dr)
        STEP0
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(1)
            start_wait((ins->sz == 4 ? 4 : 2) + ins->ea[0].reg * 2, S_exec);
        STEP(2)
            state.instruction.result = ASR(regs.D[ins->ea[1].reg], ins->ea[0].reg, ins->sz);
            set_dr(ins->ea[1].reg, state.instruction.result, ins->sz);
        STEP(3)
INS_END

INS(ASR_dr_dr)
        STEP0
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(1)
            u32 cnt = regs.D[ins->ea[0].reg] & 63;
            start_wait((ins->sz == 4 ? 4 : 2) + cnt * 2, S_exec);
        STEP(2)
            u32 cnt = regs.D[ins->ea[0].reg] & 63;
            state.instruction.result = ASR(regs.D[ins->ea[1].reg], cnt, ins->sz);
            set_dr(ins->ea[1].reg, state.instruction.result, ins->sz);
INS_END

INS(ASR_ea)
        STEP0
            start_read_operands(0, ins->sz, S_exec, 0, NO_HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.instruction.result = ASR(state.op[0].val, 1, ins->sz);
            start_prefetch(1, 1, S_exec);
        STEP(2)
            start_write_operand(0, 0, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
INS_END

INS(BCC)
        STEP0
            state.instruction.result = condition(ins->ea[0].reg);
            regs.IPC -= 4;
            sample_interrupts();
            start_wait(state.instruction.result ? 2 : 4, S_exec);
        STEP(1)
            if (!state.instruction.result) {
                regs.IPC += (ins->ea[1].reg == 0) ? 0 : 2;
                start_prefetch((ins->ea[1].reg == 0) ? 2 : 1, 1, S_exec);
            } else {
                u32 cnt = SIGNe8to32(ins->ea[1].reg);
                if (cnt == 0) cnt = SIGNe16to32(regs.IRC);
                regs.PC -= 2;
                //regs.IPC -= 2;
                regs.IPC += 2;
                regs.PC += cnt;
                start_prefetch(2, 1, S_exec);
            }
        STEP(2)
INS_END

INS(BCHG_dr_ea)
        STEP0
            // test then invert a bit. size 4 or 1
            start_read_operands(0, ins->sz, S_exec, 0, NO_HOLD, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.op[0].val &= bitsize[ins->sz] - 1;
            state.instruction.result = state.op[1].val;
            regs.SR.Z = (state.instruction.result & (1 << state.op[0].val)) == 0;
            state.instruction.result ^= 1 << state.op[0].val;
            start_prefetch(1, 1, S_exec);
        STEP(2)
            start_write_operand(0, 1, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
            u32 numcycles = 0;
            if ((ins->sz == 4) && (ins->ea[1].kind == AM_data_register_direct)) {
                if (ins->ea[0].kind == AM_data_register_direct) numcycles = 2;
                else numcycles = 4;
            }
            if (ins->ea[0].kind == AM_immediate) {
                numcycles = 4;
            }
            if (state.op[0].val >= 16) {
                numcycles = 4;
            }
            if (numcycles) start_wait(numcycles, S_exec);
        STEP(4)
INS_END

INS(BCHG_ea)
        STEP0
            state.op[1].val = regs.IRC;
            start_prefetch(1, 1, S_exec);
        STEP(1)
            start_read_operands(0, ins->sz, S_exec, 0, HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(2)
            sample_interrupts();
            state.instruction.result = state.op[0].val;
            state.op[0].val = state.op[1].val & (bitsize[ins->sz] - 1);
            regs.SR.Z = (state.instruction.result & (1 << state.op[0].val)) == 0;
            state.instruction.result ^= 1 << state.op[0].val;
            start_prefetch(1, 1, S_exec);
        STEP(3)
            start_write_operand(0, 0, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(4)
            u32 nomc = 0;
            if (state.op[0].val >= 16) nomc = 2;
            if ((ins->sz == 4) && (ins->ea[0].kind == AM_data_register_direct)) {
                //nomc += regs.SR.Z ? 4 : 2;
                nomc += 2;
            }
            if (nomc > 4) nomc = 4;
            if (nomc)
                start_wait(nomc, S_exec);
        STEP(5)
INS_END

INS(BCLR_dr_ea)
        STEP0
            start_read_operands(0, ins->sz, S_exec, 0, NO_HOLD, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.op[0].val &= bitsize[ins->sz] - 1;
            state.instruction.result = state.op[1].val;
            regs.SR.Z = (state.instruction.result & (1 << state.op[0].val)) == 0;
            state.instruction.result &= (~(1 << state.op[0].val)) & clip32[ins->sz];
            start_prefetch(1, 1, S_exec);
        STEP(2)
            start_write_operand(0, 1, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
            u32 numcycles = 0;
            if (ins->ea[1].kind == AM_immediate) {
                numcycles = 2;
            }
            if (ins->ea[1].kind == AM_data_register_direct) {
               //numcycles = ins->ea[0].kind == AM_data_register_direct ? 2 : 4;
                numcycles = 4;
            }
            if (ins->ea[0].kind == AM_immediate) {
                numcycles = 4;
            }
            if (state.op[0].val >= 16) {
                numcycles = numcycles ? 6 : 4;
            }
            if (numcycles) start_wait(numcycles, S_exec);
        STEP(4)
INS_END

INS(BCLR_ea)
        STEP0
            state.op[1].val = regs.IRC;
            start_prefetch(1, 1, S_exec);
        STEP(1)
            start_read_operands(0, ins->sz, S_exec, 0, HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(2)
            sample_interrupts();
            state.instruction.result = state.op[0].val;
            state.op[0].val = state.op[1].val & (bitsize[ins->sz] - 1);
            regs.SR.Z = (state.instruction.result & (1 << state.op[0].val)) == 0;
            state.instruction.result &= (~(1 << state.op[0].val)) & clip32[ins->sz];
            start_prefetch(1, 1, S_exec);
        STEP(3)
            start_write_operand(0, 0, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(4)
            u32 nomc = 0;
            if (state.op[0].val >= 16) nomc = 2;
            if ((ins->sz == 4) && (ins->ea[0].kind == AM_data_register_direct)) {
                //if (ins->ea[1].kind == AM_data_register_direct) nomc += 2;
                //else nomc += 4;
                nomc += 4;
            }
            if (nomc > 6) nomc = 6;
            if (nomc)
                start_wait(nomc, S_exec);
        STEP(5)
INS_END

INS(BRA)
        STEP0
            sample_interrupts();
            start_wait(2, S_exec);
        STEP(1)
            u32 cnt = SIGNe8to32(ins->ea[0].reg);
            if (cnt == 0) cnt = SIGNe16to32(regs.IRC);
            regs.PC -= 2;
            regs.IPC -= 2;
            regs.PC += cnt;
            start_prefetch(2, 1, S_exec);
        STEP(2)
INS_END

INS(BSET_dr_ea)
        STEP0
            start_read_operands(0, ins->sz, S_exec, 0, NO_HOLD, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.op[0].val &= bitsize[ins->sz] - 1;
            state.instruction.result = state.op[1].val;
            regs.SR.Z = (state.instruction.result & (1 << state.op[0].val)) == 0;
            state.instruction.result |= 1 << state.op[0].val;
            start_prefetch(1, 1, S_exec);
        STEP(2)
            start_write_operand(0, 1, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
            u32 numcycles = 0;
            if (ins->ea[1].kind == AM_immediate) {
                numcycles = 2;
            }
            if (ins->ea[1].kind == AM_data_register_direct) {
                if (ins->ea[0].kind == AM_data_register_direct) numcycles = 2;
                //else numcycles = 4;
                else numcycles = 4;
            }
            if (ins->ea[0].kind == AM_immediate) {
                numcycles = 4;
            }
            if (state.op[0].val >= 16) {
                numcycles = 4;
            }
            if (numcycles) start_wait(numcycles, S_exec);
        STEP(4)
INS_END

INS(BSET_ea)
        STEP0
            state.op[1].val = regs.IRC;
            start_prefetch(1, 1, S_exec);
        STEP(1)
            start_read_operands(0, ins->sz, S_exec, 0, HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(2)
            sample_interrupts();
            state.instruction.result = state.op[0].val;
            state.op[0].val = state.op[1].val & (bitsize[ins->sz] - 1);
            regs.SR.Z = (state.instruction.result & (1 << state.op[0].val)) == 0;
            state.instruction.result |= 1 << state.op[0].val;
            start_prefetch(1, 1, S_exec);
        STEP(3)
            start_write_operand(0, 0, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(4)
            u32 nomc = 0;
            if (state.op[0].val >= 16) nomc = 2;
            if ((ins->sz == 4) && (ins->ea[0].kind == AM_data_register_direct)) {
                if (ins->ea[1].kind == AM_data_register_direct) nomc += 2;
                else nomc += 4;
            }
            if (nomc > 4) nomc = 4;
            if (nomc)
                start_wait(nomc, S_exec);
        STEP(5)
INS_END

INS(BSR)
        STEP0
            sample_interrupts();
            start_wait(2, S_exec);
        STEP(1)
            regs.A[7] -= 4;
            if (ins->ea[0].reg != 0) {
                start_write(regs.A[7], regs.PC - 2, 4, MAKE_FC(0), M68K_RW_ORDER_NORMAL,
                                 S_exec);
            } else {
                start_write(regs.A[7], regs.PC, 4, MAKE_FC(0), M68K_RW_ORDER_NORMAL, S_exec);
            }
            regs.PC -= 2;
        STEP(2)
            i32 offset;
            if (ins->ea[0].reg != 0) {
                offset = SIGNe8to32(ins->ea[0].reg);
                //offset = SIGNe8to32(offset);
            } else {
                offset = SIGNe16to32(regs.IRC);
            }

            regs.PC += offset;
            regs.IPC = regs.PC;
            start_prefetch(2, 1, S_exec);
        STEP(3)
INS_END

INS(BTST_dr_ea)
        STEP0
            start_read_operands(0, ins->sz, S_exec, 0, NO_HOLD, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.op[0].val &= bitsize[ins->sz] - 1;
            state.instruction.result = state.op[1].val;
            regs.SR.Z = (state.instruction.result & (1 << state.op[0].val)) == 0;
            start_prefetch(1, 1, S_exec);
        STEP(2)
            u32 numcycles = 0;
            if (ins->ea[1].kind == AM_immediate) {
                numcycles = 2;
            }
            if ((ins->sz == 4) && (ins->ea[1].kind == AM_data_register_direct)) {
                //if (ins->ea[0].kind == AM_data_register_direct) numcycles = 2;
                //else numcycles = 4;
                numcycles = 2;
            }
            if (ins->ea[0].kind == AM_immediate) {
                numcycles = 4;
            }
            if (state.op[0].val >= 16) {
                numcycles = numcycles ? numcycles : 2;
            }
            if (numcycles) start_wait(numcycles, S_exec);
        STEP(3)
INS_END

INS(BTST_ea)
        STEP0
            state.op[1].val = regs.IRC;
            start_prefetch(1, 1, S_exec);
        STEP(1)
            start_read_operands(0, ins->sz, S_exec, 0, HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(2)
            sample_interrupts();
            state.instruction.result = state.op[0].val;
            state.op[0].val = state.op[1].val & (bitsize[ins->sz] - 1);
            //if (dbg.trace_on) dbg_printf("\nTEST: %02x, BIT: %d", state.instruction.result, state.op[1].val);
            regs.SR.Z = (state.instruction.result & (1 << state.op[0].val)) == 0;
            //dbg_printf("\nBIT: %d result: %d", state.op[0].val, (state.instruction.result & (1 << state.op[0].val)) == 0);
            //if (dbg.trace_on) dbg_printf("\nZ set to %d", regs.SR.Z);
            start_prefetch(1, 1, S_exec);
        STEP(3)
            u32 nomc = 0;
            if (state.op[0].val >= 16) nomc = 2;
            if ((ins->sz == 4) && (ins->ea[0].kind == AM_data_register_direct)) {
                //if (ins->ea[1].kind == AM_data_register_direct) nomc = 2;
                //else nomc = 4;
                nomc = 2;
            }
            if (nomc > 4) nomc = 4;
            if (nomc)
                start_wait(nomc, S_exec);
        STEP(5)
INS_END

INS(CHK)
        STEP0
            start_read_operands(0, 2, S_exec, 0, NO_HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            if (ins->ea[0].kind == AM_address_register_indirect) start_wait(2, S_exec);
        STEP(2)
            u16 a = state.op[1].val;
            u16 b = state.op[0].val;
            u32 r = b - a;
            regs.SR.u = regs.SR.u & 0xFFF0;
            if (!(r & 0xffff))
                regs.SR.Z = 1;
            if (r & 0x8000)
                regs.SR.N = 1;
            if (r & 0x10000)
                regs.SR.C = 1;
            if (((b & (~a) & ~r) | ((~b) & a & r)) & 0x8000)
                regs.SR.V = 1;

            u32 m_t = !!(!(regs.SR.N || regs.SR.V));

            u16 er = 0xFFFF & a;
            regs.SR.u = (regs.SR.u & 0xFFF0); // keep X
            if (!er)
                regs.SR.Z = 1;
            if (er & 0x8000)
                regs.SR.N = 1;
            regs.SR.C = regs.SR.V = 0;

            if (!m_t) {
                i32 num = 8;
                if (ins->ea[0].kind == AM_address_register_indirect) num = 6;
                start_group2_exception(IV_chk_instruction, num, regs.PC);
                return;
            }
        STEP(3)
            if (regs.SR.N) {
                i32 num = 10;
                if (ins->ea[0].kind == AM_address_register_indirect) num = 8;
                start_group2_exception(IV_chk_instruction, num, regs.PC);
                return;
            }
        STEP(4)
            u32 num = 6;
            if (ins->ea[0].kind == AM_address_register_indirect) num = 4;
            start_wait(num, S_exec);
        STEP(5)
            start_prefetch(1, 1, S_exec);
        STEP(6)
INS_END

INS(CLR)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            start_read_operands(0, ins->sz, S_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            start_prefetch(1, 1, S_exec);
        STEP(2)
            sample_interrupts();
            state.instruction.result = 0;
            start_write_operand(0, 0, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
            regs.SR.C = regs.SR.V = regs.SR.N = 0;
            regs.SR.Z = 1;
            if ((ins->sz == 4) && ((ins->ea[0].kind == AM_data_register_direct) ||
                                   (ins->ea[0].kind == AM_address_register_direct)))
                start_wait(2, S_exec);
        STEP(4)
INS_END

INS(CMP)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            start_read_operands(0, ins->sz, S_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            CMP(sgn32(state.op[0].val, ins->sz), sgn32(state.op[1].val, ins->sz), ins->sz);
            start_prefetch(1, 1, S_exec);
        STEP(2)
            if (ins->sz == 4) start_wait(2, S_exec);
        STEP(3)
INS_END

INS(CMPA)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            start_read_operands(0, ins->sz, S_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            // Ignore size for operand 2
            sample_interrupts();
            CMP(sgn32(state.op[0].val, ins->sz), regs.A[ins->ea[1].reg], 4);
            start_prefetch(1, 1, S_exec);
        STEP(2)
            start_wait(2, S_exec);
        STEP(3)
INS_END

INS(CMPI)
        STEP0
            if (ins->sz == 2) regs.IPC -= 2;
            //regs.IPC -= 2;
            STARTI(0, 1);
        STEP(2)
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(3)
            CMP(sgn32(state.instruction.result, ins->sz), sgn32(state.op[0].val, ins->sz), ins->sz);
            if ((ins->sz == 4) && (ins->ea[0].kind == AM_data_register_direct)) start_wait(2, S_exec);
        STEP(4)
INS_END

INS(CMPM)
        STEP0
            // reg0+, reg1+
            u32 rn = ins->ea[0].reg;
            start_read(regs.A[rn], ins->sz, MAKE_FC(0), 0, S_exec);
            if (ins->sz != 4) {
                regs.A[ins->ea[0].reg] += ins->sz;
                if ((ins->sz == 1) && (ins->ea[0].reg == 7)) regs.A[7]++;
            }
            else regs.A[ins->ea[0].reg] += 2;
        STEP(1)
            if (ins->sz == 4) regs.A[ins->ea[0].reg] += 2;
            state.op[0].val = state.bus_cycle.data;
            u32 rn = ins->ea[1].reg;
            start_read(regs.A[rn], ins->sz, MAKE_FC(0), 0, S_exec);
        STEP(2)
            state.op[1].val = state.bus_cycle.data;
            u32 rn = ins->ea[1].reg;
            regs.A[rn] += ins->sz;
            if ((ins->sz == 1) && (rn == 7)) regs.A[rn]++;

            sample_interrupts();
            CMP(sgn32(state.op[0].val, ins->sz), sgn32(state.op[1].val, ins->sz), ins->sz);

            start_prefetch(1, 1, S_exec);
        STEP(3)
INS_END

INS(DBCC)
        STEP0
          // idle(2);
            sample_interrupts();
            start_wait(2, S_exec);
        STEP(1)
            regs.PC -= 2;
            state.instruction.temp = !condition(ins->ea[0].reg);

            if (state.instruction.temp) {
                //   auto disp = sign<Word>(r.irc);
                state.instruction.temp2 = sgn32(regs.IRC, 2);
                //  r.pc += disp;
                regs.PC += state.instruction.temp2;
                //  prefetch();
                start_prefetch(1, 1, S_exec);
            }
            else {
                //  idle(2);
                start_wait(2, S_exec);
            }
        STEP(2)
            if (state.instruction.temp) {
                //  n16 result = read<Word>(with);
                start_read_operands(0, ins->sz, S_exec, 0, NO_HOLD, NO_REVERSE, MAKE_FC(0));
            }
            else {
                //  r.pc += 2;
                regs.PC += 2;
            }
        STEP(3)
            if (state.instruction.temp) {
                //  write<Word>(with, result - 1);
                state.instruction.result = (state.op[1].val - 1) & 0xFFFF;
                start_write_operand(0, 1, S_exec, ALLOW_REVERSE, NO_REVERSE);
                //state.instruction.result = (state.op[1].val + 1) & 0xFFFF;
            }
            else {
            }
        STEP(4)
            if (state.instruction.temp) {
                if (state.instruction.result != 0xFFFF) { // Take branch
                    //    // branch taken
                    //    prefetch();
                    start_prefetch(1, 1, S_exec);
                } else {
                    //    // branch not taken
                    //    r.pc -= disp;
                    regs.PC -= state.instruction.temp2;
                }
            }
            else {
            }
        STEP(5)
            if (state.instruction.temp) {
                if (state.instruction.result != 0xFFFF) { // Take branch
                    //    return;
                    state.instruction.done = 1;
                    return;
                }
            }
            //prefetch();
            //prefetch();
            start_prefetch(2, 1, S_exec);
        STEP(6)
INS_END

INS(DIVS)
        STEP0
            start_read_operands(0, 2, S_exec, 0, NO_HOLD, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            i32 divisor = sgn32(state.op[0].val, 2);
            i32 dividend =static_cast<i32>(regs.D[ins->ea[1].reg]);

            if (divisor == 0) {
                start_group2_exception(IV_zero_divide, 4, regs.PC - 4);
                regs.SR.u &= 0xFFF0; // Clear ZNVC
                break;
            }

            i32 result = dividend / divisor;
            i32 quotient = dividend % divisor;

            regs.SR.C = regs.SR.Z = 0;
            u32 ticks = 8;
            if (dividend < 0)
                ticks += 2;
           u32 divi32 = static_cast<u32>(abs(dividend));
           u32 divo32 = static_cast<u32>(abs(divisor)) << 16;
           if ((divi32 >= divo32) && (divisor != -32768)) {
               // Overflow (detected before calculation)
               start_wait(ticks+4, S_exec);
               regs.SR.V = regs.SR.N = 1;
               state.instruction.result = dividend;
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

            state.instruction.result = (static_cast<u32>(static_cast<u16>(quotient)) << 16) | (result & 0xFFFF);
            if ((result > 32767) || (result < -32768)) {
                regs.SR.V = 1;
                regs.SR.N = 1;
                state.instruction.result = dividend;
                start_wait(ticks+0, S_exec);
                break;
            }
            start_wait(ticks, S_exec);

            regs.SR.Z = result == 0;
            regs.SR.N = !!(result & 0x8000);
            regs.SR.V = 0;
        STEP(2)
            start_prefetch(1, 1, S_exec);
            regs.D[ins->ea[1].reg] = state.instruction.result;
        STEP(3)
INS_END

export_INS(DIVU)
        STEP0
            start_read_operands(0, 2, S_exec, 0, NO_HOLD, NO_REVERSE, MAKE_FC(0));
            state.instruction.temp = 0;
            state.instruction.temp2 = 0;
        STEP(1)
            sample_interrupts();
            u32 divisor = state.op[0].val;
            u32 dividend = regs.D[ins->ea[1].reg];
            if (divisor == 0) {
                start_group2_exception(IV_zero_divide, 4, regs.PC - 4);
                break;
            }
            u32 result = dividend / divisor;
            u32 quotient = dividend % divisor;

            u32 ticks = 0;

            regs.SR.C = regs.SR.Z = 0;
            ticks += 6;
            state.instruction.result = (quotient << 16) | result;
            if (result > 0xFFFF) {
                regs.SR.V = 1;
                regs.SR.N = 1;
                start_wait(6, S_exec);
                state.instruction.result = dividend;
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
            start_wait(ticks, S_exec);
            regs.SR.Z = result == 0;
            regs.SR.N = !!(result & 0x8000);
            regs.SR.V = 0;
        STEP(2)
            regs.D[ins->ea[1].reg] = state.instruction.result;
            start_prefetch(1, 1, S_exec);
        STEP(3)
INS_END


INS(EOR)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            start_read_operands(0, ins->sz, S_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(2)
            state.instruction.result = EOR(state.op[0].val, state.op[1].val, ins->sz);
            start_write_operand(0, 1, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
            if ((ins->sz == 4) && (ins->ea[1].kind == AM_data_register_direct))
                start_wait(4, S_exec);
        STEP(4)
INS_END

INS(EORI)
        STEP0
            if (ins->sz == 2) regs.IPC -= 2;
            STARTI(0,1);
        STEP(2)
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(3)
            state.instruction.result = EOR(state.instruction.result, state.op[0].val, ins->sz);
            start_write_operand(0, 0, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(4)
            if ((ins->sz == 4) && ((ins->ea[0].kind == AM_data_register_direct) ||
                                   (ins->ea[0].kind == AM_address_register_direct))) {
                start_wait(4, S_exec);
            }
        STEP(5)
INS_END

INS(EORI_TO_CCR)
        STEP0
            start_prefetch(1, 1, S_exec);
        STEP(1)
            sample_interrupts();
            u32 v = regs.SR.u & 0x1F;
            v ^= regs.IR & 0x1F;
            regs.SR.u = (regs.SR.u & 0xFFE0) | v;
            start_wait(8, S_exec);
        STEP(3)
            start_read(regs.PC - 2, 2, MAKE_FC(1), M68K_RW_ORDER_NORMAL, S_exec);
        STEP(4)
            start_prefetch(1, 1, S_exec);
        STEP(5)
INS_END

INS(EORI_TO_SR)
        STEP0
            sample_interrupts();
            if (!regs.SR.S) {
                start_group2_exception(IV_privilege_violation, 4, regs.PC - 2);
                return;
            }
            if (ins->ea[0].kind == 0) regs.IPC -= 2;
            start_prefetch(1, 1, S_exec);
        STEP(1)
            u32 data = (regs.IR & 0xA71F) ^ regs.SR.u;
            set_SR(data, 1);
            start_wait(8, S_exec);
        STEP(2)
            start_read(regs.PC - 2, 2, MAKE_FC(1), 0, S_exec);
        STEP(3)
            start_prefetch(1, 1, S_exec);
        STEP(4)
INS_END


INS(EXG)
        STEP0
            sample_interrupts();
            state.op[0].val = get_r(&ins->ea[0], ins->sz);
            set_r(&ins->ea[0], get_r(&ins->ea[1], ins->sz), ins->sz);
            set_r(&ins->ea[1], state.op[0].val, ins->sz);
            start_prefetch(1, 1, S_exec);
        STEP(1)
            start_wait(2, S_exec);
        STEP(2)
INS_END

INS(EXT)
        STEP0
            start_read_operands(0, ins->sz >> 1, S_exec, 0, NO_HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(2)
            state.instruction.result = sgn32(state.op[0].val, ins->sz >> 1);
            start_write_operand(0, 0, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
            regs.SR.C = regs.SR.V = 0;
            regs.SR.Z = (state.instruction.result & clip32[ins->sz]) == 0;
            regs.SR.N = !!(state.instruction.result & msb32[ins->sz]);
INS_END

INS(ILLEGAL)
        STEP0
            BADINS;
INS_END

INS(JMP)
        STEP0
            sample_interrupts();
            u32 k = ins->ea[0].kind;
            if ((k == AM_address_register_indirect_with_index) || (k == AM_program_counter_with_index))
                start_wait(6, S_exec);
            if ((k == AM_address_register_indirect_with_displacement) |
                (k == AM_program_counter_with_displacement) || (k == AM_absolute_short_data))
                start_wait(2, S_exec);
        STEP(1)
            state.op[0].ext_words = AM_ext_words(ins->ea[0].kind, 4);
            state.instruction.temp = (regs.PC - 4) + (state.op[0].ext_words << 2);
            switch (state.op[0].ext_words) {
                case 0: {
                    regs.PC -= 2;
                    state.instruction.temp = regs.PC;
                    break;
                }
                case 1: {
                    state.instruction.temp = regs.PC;
                    break;
                }
                case 2: {
                    start_read(regs.PC, 2, MAKE_FC(1), 0, S_exec);
                    state.instruction.temp = regs.PC + 2;
                    regs.PC += 2;
                    break;
                }
                default:
                    assert(1 == 2);
            }
            // #1 M68k(0000)  458504  jmp     (a2)     needs =0
            // #3 M68k(0000)  032f6e  jmp     $4ef8.w  needs -2
            regs.IPC = regs.PC;
            switch(ins->ea[0].kind) {
                case AM_address_register_indirect_with_displacement:
                case AM_address_register_indirect_with_index:
                case AM_program_counter_with_displacement:
                case AM_program_counter_with_index:
                case AM_absolute_short_data:
                    regs.IPC -= 2; break;
                case AM_absolute_long_data:
                    regs.IPC -= 4; break;
                default:
                    break;
            }
        STEP(2)
            regs.PC += state.op[0].ext_words << 1;
            // If the operand had two words, assemble from IRC and bus_cycle.data
            u32 prefetch = 0;
            if (state.op[0].ext_words == 2) {
                prefetch = (regs.IRC << 16) | state.bus_cycle.data;
            } else if (state.op[0].ext_words == 1) {// Else if it had 1 assemble from IRC
                prefetch = regs.IRC;
            }
            // 2, 5, 6 and 7 0-3  . addr reg ind, ind with index, ind with displ.
            // abs short, abs long, pc rel index, pc rel displ.
            // all require another read.
            state.instruction.result = read_ea_addr(0, 4, NO_HOLD, prefetch);
            start_read(state.instruction.result, 2, MAKE_FC(1), NO_REVERSE, S_exec);
            state.instruction.result += 2;
        STEP(3)
            regs.IR = regs.IRC;
            regs.IRC = state.bus_cycle.data;
            regs.PC = state.instruction.result;
            start_prefetch(1, 1, S_exec);
            //start_push(state.instruction.temp, 4, S_exec, 0);
        STEP(5)
INS_END

INS(JSR)
        STEP0
            sample_interrupts();
            u32 k = ins->ea[0].kind;
            if ((k == AM_address_register_indirect_with_index) || (k == AM_program_counter_with_index))
                start_wait(6, S_exec);
            if ((k == AM_address_register_indirect_with_displacement) |
                (k == AM_program_counter_with_displacement) || (k == AM_absolute_short_data))
                start_wait(2, S_exec);
        STEP(1)
            state.op[0].ext_words = AM_ext_words(ins->ea[0].kind, 4);
            state.instruction.temp = (regs.PC - 4) + (state.op[0].ext_words << 2);
            switch (state.op[0].ext_words) {
                case 0: {
                    regs.PC -= 2;
                    state.instruction.temp = regs.PC;
                    break;
                }
                case 1: {
                    state.instruction.temp = regs.PC;
                    break;
                }
                case 2: {
                    start_read(regs.PC, 2, MAKE_FC(1), 0, S_exec);
                    state.instruction.temp = regs.PC + 2;
                    regs.PC += 2;
                    break;
                }
                default:
                    assert(1 == 2);
            }
            regs.IPC = regs.PC;
        STEP(2)
            regs.PC += state.op[0].ext_words << 1;
            // If the operand had two words, assemble from IRC and bus_cycle.data
            u32 prefetch = 0;
            if (state.op[0].ext_words == 2) {
                prefetch = (regs.IRC << 16) | state.bus_cycle.data;
            } else if (state.op[0].ext_words == 1) {// Else if it had 1 assemble from IRC
                prefetch = regs.IRC;
            }
            state.instruction.result = read_ea_addr(0, 4, NO_HOLD, prefetch);
            start_read(state.instruction.result, 2, MAKE_FC(1), NO_REVERSE, S_exec);
            state.instruction.result += 2;
        STEP(3)
            regs.IR = regs.IRC;
            regs.IRC = state.bus_cycle.data;
            start_push(state.instruction.temp, 4, S_exec, 0);
        STEP(4)
            regs.PC = state.instruction.result;
            start_prefetch(1, 1, S_exec);
        STEP(5)
INS_END

INS(LEA)
       STEP0
            start_read_operand_for_ea(0, ins->sz, S_exec, 0, HOLD, ALLOW_REVERSE);
        STEP(1)
            sample_interrupts();
            state.op[0].addr = read_ea_addr(0, ins->sz,
                                                       NO_HOLD, state.op[0].ext_words);
            state.instruction.result = state.op[0].addr;
            start_write_operand(0, 1, S_exec, NO_REVERSE, NO_REVERSE);
        STEP(2)
            if ((ins->ea[0].kind == AM_address_register_indirect_with_index) || (ins->ea[0].kind == AM_program_counter_with_index))
                start_wait(2, S_exec);
        STEP(3)
            start_prefetch(1, 1, S_exec);
        STEP(4)
INS_END

INS(LINK)
        STEP0
            state.op[1].val = regs.IRC;
            state.op[1].addr = regs.A[7];
            start_prefetch(1, 1, S_exec);
            sample_interrupts();
        STEP(1)
            start_push(regs.A[ins->ea[0].reg], 4, S_exec, NO_REVERSE);
            regs.A[ins->ea[0].reg] = regs.A[7];
            regs.A[7] = regs.A[7] + sgn32(state.op[1].val, 2);
        STEP(2)
            start_prefetch(1, 1, S_exec);
        STEP(3)
INS_END

INS(LSL_qimm_dr)
        STEP0
            start_prefetch(1, 1, S_exec);
            sample_interrupts();
        STEP(1)
            start_wait((ins->sz == 4 ? 4 : 2) + ins->ea[0].reg * 2, S_exec);
        STEP(2)
            state.instruction.result = LSL(regs.D[ins->ea[1].reg], ins->ea[0].reg, ins->sz);
            set_dr(ins->ea[1].reg, state.instruction.result, ins->sz);
INS_END


INS(LSL_dr_dr)
        STEP0
            start_prefetch(1, 1, S_exec);
            sample_interrupts();
        STEP(1)
            u32 cnt = regs.D[ins->ea[0].reg] & 63;
            start_wait((ins->sz == 4 ? 4 : 2) + cnt * 2, S_exec);
        STEP(2)
            u32 cnt = regs.D[ins->ea[0].reg] & 63;
            state.instruction.result = LSL(regs.D[ins->ea[1].reg], cnt, ins->sz);
            set_dr(ins->ea[1].reg, state.instruction.result, ins->sz);
INS_END

INS(LSL_ea)
        STEP0
            start_read_operands(0, ins->sz, S_exec, 0, NO_HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.instruction.result = LSL(state.op[0].val, 1, ins->sz);
            start_prefetch(1, 1, S_exec);
        STEP(2)
            start_write_operand(0, 0, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
INS_END


INS(LSR_qimm_dr)
        STEP0
            start_prefetch(1, 1, S_exec);
            sample_interrupts();
        STEP(1)
            start_wait((ins->sz == 4 ? 4 : 2) + ins->ea[0].reg * 2, S_exec);
        STEP(2)
            state.instruction.result = LSR(regs.D[ins->ea[1].reg], ins->ea[0].reg, ins->sz);
            set_dr(ins->ea[1].reg, state.instruction.result, ins->sz);
INS_END


INS(LSR_dr_dr)
        STEP0
            start_prefetch(1, 1, S_exec);
            sample_interrupts();
        STEP(1)
            u32 cnt = regs.D[ins->ea[0].reg] & 63;
            start_wait((ins->sz == 4 ? 4 : 2) + cnt * 2, S_exec);
        STEP(2)
            u32 cnt = regs.D[ins->ea[0].reg] & 63;
            state.instruction.result = LSR(regs.D[ins->ea[1].reg], cnt, ins->sz);
            set_dr(ins->ea[1].reg, state.instruction.result, ins->sz);
INS_END

INS(LSR_ea)
        STEP0
            start_read_operands(0, ins->sz, S_exec, 0, NO_HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.instruction.result = LSR(state.op[0].val, 1, ins->sz);
            start_prefetch(1, 1, S_exec);
        STEP(2)
            start_write_operand(0, 0, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
INS_END

void core::correct_IPC_MOVE_l_pf0(u32 opnum)
{
    switch(ins->ea[opnum].kind) {
        case AM_address_register_indirect:
        case AM_address_register_indirect_with_predecrement:
            regs.IPC -= 2; break;
        case AM_absolute_short_data:
            regs.IPC += 2; break;
        case AM_address_register_indirect_with_displacement:
            if (ins->ea[opnum ^ 1].kind == AM_address_register_indirect_with_postincrement)
                regs.IPC += 2;
            break;
        case AM_address_register_indirect_with_index:
            regs.IPC += 4; break;
        default:
            break;
    }
}

export_INS(MOVE)
        STEP0
            u32 k = ins->ea[0].kind;
            //u32 k2 = ins->ea[1].kind;
            u32 numcycles = 0;
            switch(k) {
                case AM_address_register_indirect_with_index:
                case AM_program_counter_with_index:
                    numcycles = 2;
                    break;
                default:
            }
            if (numcycles) start_wait(2, S_exec);
        STEP(1)
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            // 1 ext word, 2 reads       1 read
            // 042 MOVE.l (d8, A6, Xn), -(A4) 2936  needs hold
            start_read_operands(0, ins->sz, S_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
            if ((ins->sz == 4) && (ins->ea[1].kind == AM_address_register_indirect_with_predecrement))
                state.op[1].hold = HOLD;
            if ((ins->sz == 4) && (ins->ea[0].kind == AM_address_register_indirect_with_postincrement))
                state.op[0].hold = HOLD;
            if ((ins->sz == 2) && (ins->ea[0].kind == AM_address_register_indirect_with_predecrement))
                state.op[1].hold = NO_HOLD;
            state.current = S_exec;
            // Do prefetch 0
            if (state.operands.state[OS_prefetch1]) {
                state.operands.TCU = 0;
                read_operands_prefetch(0);
                state.bus_cycle.next_state = S_exec;
                regs.IPC -= 4;
            }
        STEP(2)
            // finish prefecth 0
            state.op[0].t = state.operands.state[OS_prefetch1];
            state.op[1].t2 = 0;
            if (state.op[0].t) {
                if (!state.operands.state[OS_read1]) regs.IPC += 4;
                read_operands_prefetch(0);
                state.operands.TCU = 0;
                state.bus_cycle.next_state = S_exec;
            }

            // Do pause if needed, in between p0 and r0
            u32 k = ins->ea[0].kind;
            //u32 k2 = ins->ea[1].kind;
            if (k == AM_address_register_indirect_with_predecrement) {
                start_wait(2, S_exec);
            }
        STEP(3)
            // Do read 0
            if (state.operands.state[OS_read1]) {
                u32 k = ins->ea[0].kind;
                u32 k2 = ins->ea[1].kind;
                switch(k) {
                    case AM_address_register_indirect_with_postincrement:
                        if (k2 == AM_address_register_indirect_with_postincrement)
                            regs.IPC += 2;
                        break;
                    case AM_address_register_indirect:
                        // 011 MOVE.w (A1), (d8, A7, Xn)
                        if (k2 != AM_data_register_direct)
                            regs.IPC -= 2;
                        if (k2 == AM_address_register_indirect_with_postincrement)
                            regs.IPC += 2;
                        break;
                    case AM_address_register_indirect_with_predecrement:
                        if (k2 == AM_address_register_indirect_with_postincrement)
                            regs.IPC += 2;
                        break;
                    case AM_program_counter_with_displacement:
                    case AM_address_register_indirect_with_displacement:
                        switch(k2) {
                            case AM_data_register_direct:
                                regs.IPC += 4;
                                break;
                            case AM_address_register_indirect_with_postincrement:
                                regs.IPC += 2;
                                break;
                            default:
                        }
                        break;
                    case AM_absolute_short_data:
                        switch(k2) {
                            case AM_address_register_indirect:
                            case AM_address_register_indirect_with_index:
                            case AM_data_register_direct:
                            case AM_address_register_indirect_with_displacement:
                            case AM_address_register_indirect_with_predecrement:
                                regs.IPC += 2;
                                break;
                            case AM_address_register_indirect_with_postincrement:
                                regs.IPC += 4;
                                break;
                            default:
                        }
                        break;
                    case AM_address_register_indirect_with_index:
                        switch(k2) {
                            case AM_data_register_direct:
                                regs.IPC += 4;
                                break;
                            case AM_address_register_indirect_with_postincrement:
                                regs.IPC += 2;
                                break;
                            default:
                        }
                        break;
                    case AM_absolute_long_data:
                        switch(k2) {
                            case AM_data_register_direct:
                            case AM_address_register_indirect_with_postincrement:
                                regs.IPC += 4;
                                break;
                            case AM_address_register_indirect:
                            case AM_address_register_indirect_with_index:
                            case AM_address_register_indirect_with_displacement:
                            case AM_address_register_indirect_with_predecrement:
                                regs.IPC += 2;
                                break;
                        default:
                        }
                        break;
                    case AM_program_counter_with_index:
                        switch(k2) {
                            case AM_address_register_indirect_with_postincrement:
                                regs.IPC += 2;
                                break;
                            case AM_data_register_direct:
                                regs.IPC += 4;
                                break;
                        default:
                        }
                        break;
                    default:
                }
                read_operands_read(0, true);
                state.bus_cycle.next_state = S_exec;
                if (ins->sz == 4) correct_IPC_MOVE_l_pf0(0);
            }
        STEP(4)
            // Finish read 0
            if (state.operands.state[OS_read1]) {
                if (state.op[0].t) regs.IPC += 4;
                read_operands_read(0, true);
                state.bus_cycle.next_state = S_exec;
            }

            // Potentially wait, in between r0 and p1
            if (ins->ea[1].kind == AM_address_register_indirect_with_index) start_wait(2, S_exec);
        STEP(5)
            state.instruction.result = state.op[0].val;
            // Do prefetch 1
            if (ins->sz == 2) {
                regs.SR.Z = (clip32[ins->sz] & state.instruction.result) == 0;
                regs.SR.N = !!(state.instruction.result & msb32[ins->sz]);
            }
        STEP(6)
            if (state.operands.state[OS_prefetch2]) {
                read_operands_prefetch(1);
                if ((ins->ea[1].kind == AM_absolute_long_data) && (ins->ea[0].kind != AM_address_register_direct) && (ins->ea[0].kind != AM_data_register_direct) && (ins->ea[0].kind != AM_immediate)) {
                    state.current = S_read16;
                    state.op[1].t2 = 1;
                }
                state.bus_cycle.next_state = S_exec;
            }
        STEP(7)
            if (state.operands.state[OS_prefetch2]) {
                if (state.op[1].t2) {
                    regs.IR = regs.IRC;
                    regs.IRC = state.bus_cycle.data & 0xFFFF;
                    state.op[1].addr = ((regs.IR << 16) | regs.IRC);
                    regs.PC += 2;
                }
                else {
                    read_operands_prefetch(1);
                }
                state.bus_cycle.next_state = S_exec;
            }
        STEP(8)
            switch(ins->ea[1].kind) {
                case AM_absolute_long_data:
                    if (!state.op[1].t2) {
                        state.op[1].addr = read_ea_addr(1, ins->sz, NO_HOLD, state.op[1].ext_words);
                    }
                    FALLTHROUGH;
                case AM_imm16:
                case AM_imm32:
                case AM_immediate:
                    state.operands.state[OS_read2] = 0;
                    break;
                    break;
                default:
                    break;
            }
            if (state.operands.state[OS_read2]) {
                state.op[1].hold = 1;
                state.operands.TCU = 0;
                // 023 MOVE.w (xxx).w, (A3)  needs -2
                read_operands_read(1, false);
                state.bus_cycle.next_state = S_exec;
                state.current = S_exec;
            } else {
                state.current = S_exec;
            }
            regs.SR.C = regs.SR.V = 0;
            switch(ins->ea[1].kind) {
                case AM_address_register_indirect:
                default:
                    state.instruction.temp = NO_REVERSE;
                    state.op[0].addr = NO_REVERSE;
                    break;
            }
            state.op[1].t = 0;
            switch(ins->ea[1].kind) {
                case AM_address_register_indirect_with_predecrement:
                    state.op[1].t = 1;
                    break;
                default:
                    break;
            }
            if (state.op[1].t) {
                start_prefetch(1, 1, S_exec);
            }
            else {
                if (ins->sz <= 2) {
                    u32 k = ins->ea[0].kind;
                    u32 k2 = ins->ea[1].kind;
                    switch(k) {
                        case AM_program_counter_with_displacement:
                            if (k2 == AM_address_register_indirect_with_displacement)
                                regs.IPC -= 2;
                            break;
                        case AM_program_counter_with_index:
                            switch(k2) {
                                case AM_address_register_indirect_with_index:
                                case AM_address_register_indirect_with_displacement:
                                    regs.IPC -= 2;
                                    break;
                                default:
                            }
                            break;
                        case AM_address_register_indirect_with_postincrement:
                            switch(k2) {
                                case AM_address_register_indirect:
                                case AM_address_register_indirect_with_postincrement:
                                    regs.IPC += 2;
                                    break;
                                case AM_absolute_long_data:
                                    regs.IPC -= 2;
                                    break;
                                default:
                            }
                            break;
                        case AM_address_register_indirect_with_index:
                            // 088 MOVE.w (d8, A3, Xn), (A6)+ -2
                            // 045 MOVE.w (d8, A5, Xn), (d16, A3) -2
                            switch(k2) {
                                case AM_data_register_direct:
                                    regs.IPC += 2;
                                    break;
                                case AM_address_register_indirect_with_displacement:
                                case AM_address_register_indirect_with_index:
                                case AM_absolute_short_data:
                                    regs.IPC -= 2;
                                    break;
                                default:
                            }
                            break;
                        case AM_data_register_direct:
                            /*switch(k2) {
                                case AM_absolute_short_data:
                                    regs.IPC -= 2;
                                    break;
                            }*/
                            switch(k2) {
                                case AM_address_register_indirect:
                                case AM_address_register_indirect_with_postincrement:
                                case AM_address_register_indirect_with_displacement:
                                case AM_address_register_indirect_with_index:
                                    regs.IPC += 2;
                                    break;
                                default:
                            }
                            break;
                        case AM_address_register_direct:
                            switch(k2) {
                                case AM_address_register_indirect_with_displacement:
                                case AM_address_register_indirect_with_index:
                                case AM_address_register_indirect_with_postincrement:
                                case AM_address_register_indirect:
                                    regs.IPC += 2;
                                    break;
                                default:
                            }
                            break;
                        case AM_address_register_indirect_with_displacement:
                            switch(k2) {
                                case AM_address_register_indirect_with_index:
                                case AM_address_register_indirect_with_displacement:
                                    regs.IPC -= 2;
                                    break;
                                case AM_absolute_long_data:
                                    regs.IPC -= 4;
                                    break;
                                default:
                            }
                            break;
                        case AM_address_register_indirect:
                            switch(k2) {
                                case AM_address_register_indirect_with_postincrement:
                                case AM_address_register_indirect:
                                    regs.IPC += 2;
                                    break;
                                case AM_absolute_long_data:
                                    regs.IPC -= 2;
                                    break;
                                default:
                            }
                            break;
                        case AM_absolute_short_data:
                            switch(k2) {
                                case AM_address_register_indirect_with_postincrement:
                                case AM_absolute_short_data:
                                    regs.IPC -= 2;
                                    break;
                                case AM_address_register_indirect_with_displacement:
                                case AM_address_register_indirect_with_index:
                                    regs.IPC -= 4;
                                    break;
                                default:
                            }
                            break;
                        case AM_absolute_long_data:
                            // 093 MOVE.w (xxx).l, (A3)
                            switch(k2) {
                                case AM_address_register_indirect:
                                case AM_absolute_short_data:
                                    regs.IPC -= 2;
                                    break;
                                case AM_address_register_indirect_with_index:
                                case AM_address_register_indirect_with_displacement:
                                    regs.IPC -= 4;
                                    break;
                                default:
                            }
                            break;
                        case AM_address_register_indirect_with_predecrement:
                            switch(k2) {
                                case AM_absolute_long_data:
                                    regs.IPC -= 4;
                                    break;
                                case AM_address_register_indirect_with_displacement:
                                case AM_address_register_indirect_with_index:
                                case AM_absolute_short_data:
                                    regs.IPC -= 2;
                                    break;
                                default:
                            }
                            break;
                        case AM_immediate:
                            switch(k2) {
                                case AM_address_register_indirect_with_postincrement:
                                    regs.IPC += 2;
                                    break;
                                case AM_absolute_short_data:
                                case AM_address_register_indirect_with_index:
                                    regs.IPC -= 2;
                                    break;
                                default:
                            }
                            break;
                        default:
                    }
                }
                sample_interrupts();
                start_write_operand(0, 1, S_exec, state.instruction.temp, state.op[0].addr);
            }
        STEP(9)
            if (state.op[1].t) {
                u32 commit = 0;
                // 019 MOVE.l -(A3), (xxx).l needs commit 0
                if (ins->sz <= 2) commit = 1;

                // 027 MOVE.w D6, -(A3) needs +0
                // 037 MOVE.w (d16, A6), -(A0) needs +0
                // 038 MOVE.w (A0)+, -(A4) needs +2
                if (ins->sz <= 2) {
                    u32 k = ins->ea[0].kind;
                    u32 k2 = ins->ea[1].kind;
                    //if (k2 == AM_address_register_indirect_with_predecrement)
                    //    regs.IPC += 2;
                    switch (k) {
                        case AM_address_register_indirect:
                            regs.IPC += 2;
                            break;
                        case AM_address_register_indirect_with_postincrement:
                            if (k2 == AM_address_register_indirect_with_predecrement)
                                regs.IPC += 2;
                            break;
                        case AM_absolute_short_data:
                        case AM_absolute_long_data:
                            if (k2 == AM_address_register_indirect_with_predecrement)
                                regs.IPC -= 2;
                            break;
                        default:
                    }
                }
                sample_interrupts();
                start_write_operand(commit, 1, S_exec, ALLOW_REVERSE, NO_FORCE);
            }
            else {
                regs.SR.Z = (clip32[ins->sz] & state.instruction.result) == 0;
                regs.SR.N = !!(state.instruction.result & msb32[ins->sz]);
                finalize_ea(1);
                if (state.op[1].t2)
                    start_prefetch(2, 1, S_exec);
                else
                    start_prefetch(1, 1, S_exec);
            }
        STEP(10)
            if (state.op[1].t)
                finalize_ea(1);

            if ((ins->ea[1].kind == AM_address_register_indirect_with_postincrement) || (ins->ea[1].kind == AM_address_register_indirect_with_predecrement)) {
                regs.A[ins->ea[1].reg] = state.op[1].new_val;
            }
            regs.SR.Z = (clip32[ins->sz] & state.instruction.result) == 0;
            regs.SR.N = !!(state.instruction.result & msb32[ins->sz]);
INS_END

INS(MOVEA)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            //if (ins->sz == 4)
            // 000 MOVEA.l (d16, Ax), Ax
            // FC(0) for operand read
            //
            // 046 MOVEA.l (d8, PC, Xn), Ax
            // FC(1) for operand read
            start_read_operands(0, ins->sz, S_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            u32 v = state.op[0].val;
            if (ins->sz == 2) v = SIGNe16to32(v);
            if (ins->sz == 1) v = SIGNe8to32(v);
            regs.A[ins->ea[1].reg] = v;
            //set_ar(ins->ea[1].reg, v, ins->sz);
            start_prefetch(1, 1, S_exec);
        STEP(3)
INS_END

INS(MOVEM_TO_MEM)
        STEP0
            start_prefetch(1, 1, S_exec);
        STEP(1)
            sample_interrupts();
            state.op[1].val = regs.IR;
            state.op[1].addr = 0;
            state.instruction.result = 200;
            u32 waits = 0;
            if (ins->ea[0].kind == AM_address_register_indirect_with_index)
                waits = 2;
            start_read_operand_for_ea(0, ins->sz, S_exec, waits, HOLD, ALLOW_REVERSE);
            state.operands.state[OS_pause1] = 0;
        STEP(2)
            state.op[0].val = read_ea_addr(0, 4, HOLD, state.op[0].ext_words);
            if (ins->ea[0].kind == AM_address_register_indirect_with_predecrement) state.op[0].val += ins->sz;
            if (ins->ea[0].kind == AM_address_register_indirect_with_postincrement) state.op[0].val -= ins->sz;
            if ((ins->sz == 4) && (ins->ea[0].kind == AM_address_register_indirect_with_predecrement)) state.op[0].val -= ins->sz;
        STEP(3)
            // Loop here up to 16 times..
            regs.IPC = regs.PC;
            if (state.op[1].addr < 16) {
                // If our value is not 0, we will have a new value for a register
                u32 index = ins->ea[0].kind == AM_address_register_indirect_with_predecrement ? 15 - state.op[1].addr : state.op[1].addr;
                // Loop!
                u32 a = state.op[1].addr;
                state.instruction.TCU--;
                state.op[1].addr++;

                // If this bit isn't set, don't do anything else
                if (!(state.op[1].val & (1 << a))) {
                    break;
                }

                // Start a read
                u32 do_reverse = M68K_RW_ORDER_NORMAL;
                u32 kk = ins->ea[0].kind;
                if (kk == AM_address_register_indirect_with_predecrement) do_reverse = M68K_RW_ORDER_REVERSE;
                u32 v = (index < 8) ? regs.D[index] : regs.A[index - 8];
                start_write(state.op[0].val, v, ins->sz, MAKE_FC(0), do_reverse, S_exec);
                if (ins->ea[0].kind == AM_address_register_indirect_with_predecrement) state.op[0].val -= ins->sz;
                if (ins->ea[0].kind != AM_address_register_indirect_with_predecrement) state.op[0].val += ins->sz;
            }
        STEP(5)
            if (ins->ea[0].kind == AM_address_register_indirect_with_predecrement) state.op[0].val += ins->sz;
            if (ins->ea[0].kind == AM_address_register_indirect_with_predecrement) set_ar(ins->ea[0].reg, state.op[0].val, 4);
            if (ins->ea[0].kind == AM_address_register_indirect_with_postincrement) set_ar(ins->ea[0].reg, state.op[0].val, 4);
            start_prefetch(1, 1, S_exec);
        STEP(6)
INS_END

export_INS(MOVEM_TO_REG)
        STEP0
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(1)
            state.op[1].val = regs.IR;
            state.op[1].addr = 0;
            state.instruction.result = 200;
            /*
             * Simulate loop with addr.
             * Read memory, stick into register.
             * If we're not loop 0, then we have something for the old register
             */
            //u32 waits = 0;
            //if (ins->ea[0].kind == AM_address_register_indirect_with_index)
            //    waits = 2;
            start_read_operand_for_ea(0, ins->sz, S_exec, 0, HOLD, ALLOW_REVERSE);
        STEP(2)
            state.op[0].val = read_ea_addr(0, 4, true, state.op[0].ext_words);
            regs.IPC = regs.PC;
        STEP(3)
        // Loop here up to 16 times..
            if (state.op[1].addr < 16) {
                // If our value is not 0, we will have a new value for a register
                u32 index = ins->ea[0].kind == AM_address_register_indirect_with_predecrement ? 15 - state.op[1].addr : state.op[1].addr;
                if (state.instruction.result != 200) {
                    if (state.instruction.result < 8) set_dr(state.instruction.result, sgn32(state.bus_cycle.data, ins->sz), 4);
                    else set_ar(state.instruction.result - 8, sgn32(state.bus_cycle.data, ins->sz), 4);
                    state.instruction.result = 200;
                }

                // Loop!
                u32 a = state.op[1].addr;
                state.instruction.TCU--;
                state.op[1].addr++;

                // If this bit isn't set, don't do anything else
                if (!(state.op[1].val & (1 << a))) {
                    break;
                }

                // Start a read
                state.instruction.result = index;
                if (ins->ea[0].kind == AM_address_register_indirect_with_predecrement) state.op[0].val -= ins->sz;
                u32 fc = MAKE_FC(0);
                if ((ins->ea[0].kind == AM_program_counter_with_index) || (ins->ea[0].kind == AM_program_counter_with_displacement))
                    fc = MAKE_FC(1);
                start_read(state.op[0].val, ins->sz, fc, M68K_RW_ORDER_NORMAL, S_exec);
                if (ins->ea[0].kind != AM_address_register_indirect_with_predecrement) state.op[0].val += ins->sz;
            }
        STEP(4)
            // Finish last read, if it's there
            if (state.instruction.result != 200) {
                if (state.instruction.result < 8) set_dr(state.instruction.result, sgn32(state.bus_cycle.data, ins->sz), 4);
                else set_ar(state.instruction.result-8, sgn32(state.bus_cycle.data, ins->sz), 4);
                state.instruction.result = 200;
            }
            if (ins->ea[0].kind == AM_address_register_indirect_with_predecrement) {
                state.op[0].val -= 2;
            }
            u32 fc = MAKE_FC(0);
            if ((ins->ea[0].kind == AM_program_counter_with_index) || (ins->ea[0].kind == AM_program_counter_with_displacement))
                fc = MAKE_FC(1);
            start_read(state.op[0].val, 2, fc, 0, S_exec);
        STEP(5)
            if (ins->ea[0].kind == AM_address_register_indirect_with_predecrement) set_ar(ins->ea[0].reg, state.op[0].val, 4);
            if (ins->ea[0].kind == AM_address_register_indirect_with_postincrement) set_ar(ins->ea[0].reg, state.op[0].val, 4);
            start_prefetch(1, 1, S_exec);
        STEP(6)
INS_END

INS(MOVEP_dr_ea)
    STEP0
        start_read_operand_for_ea(0, ins->sz, S_exec, 0, HOLD, NO_REVERSE);
    STEP(1)
        sample_interrupts();
        state.op[0].val = regs.D[ins->ea[0].reg]; // data
        state.op[1].val = read_ea_addr(1, ins->sz, false, state.op[1].ext_words);
        state.op[1].addr = ins->sz * 8; // shift
        state.operands.cur_op_num = 0; // i
    STEP(2) // loop
        state.op[1].addr -= 8;
        start_write(state.op[1].val, (state.op[0].val >> state.op[1].addr) & 0xFF, 1, MAKE_FC(0), M68K_RW_ORDER_NORMAL, S_exec);
        state.op[1].val += 2;
        state.operands.cur_op_num++;
        if (state.operands.cur_op_num < ins->sz) {
            state.instruction.TCU--;
        }
    STEP(3)
        start_prefetch(1, 1, S_exec);
    STEP(4)
INS_END

INS(MOVEP_ea_dr)
    STEP0
        start_read_operand_for_ea(0, ins->sz, S_exec, 0, HOLD, ALLOW_REVERSE);
    STEP(1)
        sample_interrupts();
        state.operands.cur_op_num = 0; // i
        state.op[0].addr = ins->sz * 8;// shift
        state.op[0].val = read_ea_addr(0, ins->sz, false, state.op[0].ext_words);
    STEP(2)
        if (state.operands.cur_op_num > 0)
            regs.D[ins->ea[1].reg] |= state.bus_cycle.data << state.op[0].addr;
        state.op[0].addr -= 8;
        regs.D[ins->ea[1].reg] &= ~(0xff << state.op[0].addr);
        start_read(state.op[0].val, 1, MAKE_FC(0), M68K_RW_ORDER_NORMAL, S_exec);
        state.op[0].val += 2;
        state.operands.cur_op_num++;
        if (state.operands.cur_op_num < ins->sz) state.instruction.TCU--;
    STEP(3)
        regs.D[ins->ea[1].reg] |= state.bus_cycle.data << state.op[0].addr;
        start_prefetch(1, 1, S_exec);
    STEP(4)
INS_END

INS_NOSWITCH(MOVEP)
    if (ins->ea[0].kind == AM_data_register_direct) return ins_MOVEP_dr_ea();
    else return ins_MOVEP_ea_dr();
INS_END_NOSWITCH

INS(MOVEQ)
        STEP0
            regs.D[ins->ea[1].reg] = SIGNe8to32(ins->ea[0].reg);
            regs.SR.C = regs.SR.V = 0;
            regs.SR.Z = regs.D[ins->ea[1].reg] == 0;
            regs.SR.N = !!(regs.D[ins->ea[1].reg] & 0x80000000);
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(1)
INS_END

INS(MOVE_FROM_SR)
        STEP0
            start_read_operands(0, ins->sz, S_exec, 0, NO_HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(2)
            state.instruction.result = get_SR();
            if (ins->ea[0].kind == AM_data_register_direct) start_wait(2, S_exec);
        STEP(3)
            start_write_operand(0, 0, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(4)
INS_END

INS(MOVE_FROM_USP)
        STEP0
            sample_interrupts();
            if (!regs.SR.S) {
                start_group2_exception(IV_privilege_violation, 4, regs.PC - 2);
                return;
            }
            if (ins->ea[0].kind == 0)
                regs.IPC -= 2;
            start_read_operands(0, ins->sz, S_exec, 0, NO_HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(2)
            state.instruction.result = regs.ASP;
            if (ins->ea[0].kind == AM_data_register_direct) start_wait(2, S_exec);
        STEP(3)
            start_write_operand(0, 0, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(4)
INS_END

INS(MOVE_TO_CCR)
        STEP0
            start_read_operands(0, 2, S_exec, 0, NO_HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            regs.IPC -= 2;
            regs.PC -= 2;
            u32 data = (regs.SR.u & 0xFFE0) | (state.op[0].val & 0x001F);
            set_SR(data, 0);
            start_wait(4, S_exec);
        STEP(2)
            start_prefetch(2, 1, S_exec);
        STEP(3)
INS_END

INS(MOVE_TO_SR)
        STEP0
            if (!regs.SR.S) {
                start_group2_exception(IV_privilege_violation, 4, regs.PC-2);
                return;
            }
            start_read_operands(0, 2, S_exec, 0, NO_HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            regs.IPC -= 2;
            regs.PC -= 2;
            start_wait(4, S_exec);
        STEP(2)
            u32 data = (state.op[0].val & 0xA71F);
            set_SR(data, 1);
            start_prefetch(2, 1, S_exec);
        STEP(3)
INS_END

INS(MOVE_TO_USP)
        STEP0
            sample_interrupts();
            if (!regs.SR.S) {
                start_group2_exception(IV_privilege_violation, 4, regs.PC - 2);
                return;
            }
            start_read_operands(0, 4, S_exec, 0, NO_HOLD, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            // We're in supervisor, so we'll always be ASP
            regs.ASP = state.op[0].val;
            start_prefetch(1, 1, S_exec);
        STEP(2)
INS_END

INS(MULS)
        STEP0
            start_read_operands(0, 2, S_exec, 0, NO_HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.instruction.result = static_cast<i16>(state.op[0].val) * static_cast<i16>(state.op[1].val);
            start_prefetch(1, 1, S_exec);
        STEP(2)
            u32 bc = 0;
            // Get transitions 0->1
            u16 n = ((state.op[0].val & 0xFFFF) << 1) ^ state.op[0].val;
            while (n) { // Count 1's algorithm
                bc += n & 1;
                n >>= 1;
            }
            u32 cycles = 34 + bc * 2;
            start_wait(cycles, S_exec);
        STEP(3)
            set_dr(ins->ea[1].reg, state.instruction.result, 4);
            regs.SR.C = 0;
            regs.SR.V = 0;
            regs.SR.Z = (state.instruction.result & 0xFFFFFFFF) == 0;
            regs.SR.N = !!(state.instruction.result & 0x80000000);
INS_END

INS(MULU)
        STEP0
            start_read_operands(0, 2, S_exec, 0, NO_HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.instruction.result = static_cast<u16>(state.op[0].val) * static_cast<u16>(state.op[1].val);
            start_prefetch(1, 1, S_exec);
        STEP(2)
            u32 bc = 0;
            // Get transitions 0->1
            //u16 n = ((state.op[0].val & 0xFFFF) << 1) ^ state.op[0].val;
            u16 n = state.op[0].val;
            while (n) { // Count 1's algorithm
                bc += n & 1;
                n >>= 1;
            }
            u32 cycles = 34 + bc * 2;
            start_wait(cycles, S_exec);
        STEP(3)
            set_dr(ins->ea[1].reg, state.instruction.result, 4);
            regs.SR.C = 0;
            regs.SR.V = 0;
            regs.SR.Z = (state.instruction.result & 0xFFFFFFFF) == 0;
            regs.SR.N = !!(state.instruction.result & 0x80000000);

INS_END

INS(NBCD)
        STEP0
            start_read_operands(0, 1, S_exec, 0, HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            u32 source = state.op[0].val;
            u32 target = 0u;
            u32 result = target - source - regs.SR.X;
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
            state.instruction.result = result;

            regs.SR.C = regs.SR.X = c;
            regs.SR.V = v;

            start_prefetch(1, 1, S_exec);
        STEP(2)
            start_write_operand(0, 0, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
            if ((ins->ea[0].kind == AM_data_register_direct) || (ins->ea[0].kind == AM_address_register_direct))
                start_wait(2, S_exec);
        STEP(4)
            regs.SR.Z = (0xFF & state.instruction.result) ? 0 : regs.SR.Z;
            regs.SR.N = !!(0x80 & state.instruction.result);
INS_END

INS(NEG)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            start_read_operands(0, ins->sz, S_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.instruction.result = SUB(state.op[0].val, 0, ins->sz, 0, 1);
            start_prefetch(1, 1, S_exec);
        STEP(2)
            if ((ins->sz == 4) && ((ins->ea[0].kind == AM_address_register_direct) || (ins->ea[0].kind == AM_data_register_direct))) {
                start_wait(2, S_exec);
            }
        STEP(3)
            start_write_operand(0, 0, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(4)
INS_END

INS(NEGX)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            start_read_operands(0, ins->sz, S_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(2)
            state.instruction.result = SUB(state.op[0].val, 0, ins->sz, 1, 1);
            start_write_operand(0, 0, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
            if (ins->sz == 4) {
                if ((ins->ea[0].kind == AM_data_register_direct) ||
                    (ins->ea[0].kind == AM_address_register_direct)) {
                    start_wait(2, S_exec);
                }
            }
        STEP(4)
INS_END

INS(NOP)
        STEP0
            start_prefetch(1, 1, S_exec);
        STEP(1)
INS_END

INS(NOT)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            start_read_operands(0, ins->sz, S_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.instruction.result = (~state.op[0].val) & clip32[ins->sz];
            start_prefetch(1, 1, S_exec);
        STEP(2)
            start_write_operand(0, 0, S_exec, ALLOW_REVERSE, NO_FORCE);
            DELAY_IF_REGWRITE(ins->ea[0].kind, 2);
            regs.SR.C = regs.SR.V = 0;
            regs.SR.Z = state.instruction.result == 0;
            regs.SR.N = !!(state.instruction.result & msb32[ins->sz]);
        STEP(3)
INS_END

INS(OR)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            start_read_operands(0, ins->sz, S_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.instruction.result = OR(state.op[0].val, state.op[1].val, ins->sz);
            start_prefetch(1, 1, S_exec);
        STEP(2)
            start_write_operand(0, 1, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
            if ((ins->sz == 4) && (ins->ea[1].kind == AM_data_register_direct)) {
                if ((ins->ea[0].kind == AM_data_register_direct) || (ins->ea[0].kind == AM_immediate))
                    start_wait(4, S_exec);
                else
                    start_wait(2, S_exec);
            }
        STEP(4)

INS_END

INS(ORI)
        STEP0
            if (ins->sz == 2) regs.IPC -= 2;
            STARTI(0,1);
        STEP(2)
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(3)
            state.instruction.result = OR(state.instruction.result, state.op[0].val, ins->sz);
            start_write_operand(0, 0, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(4)
            if ((ins->sz == 4) && (ins->ea[0].kind == AM_data_register_direct))
                start_wait(4, S_exec);
        STEP(5)
INS_END

INS(ORI_TO_CCR)
        STEP0
            start_prefetch(1, 1, S_exec);
        STEP(1)
            sample_interrupts();
            u32 v = regs.SR.u & 0x1F;
            v |= (regs.IR & 0x1F);
            regs.SR.u = (regs.SR.u & 0xFFE0) | v;
            start_wait(8, S_exec);
        STEP(3)
            start_read(regs.PC-2, 2, MAKE_FC(1), M68K_RW_ORDER_NORMAL, S_exec);
        STEP(4)
            start_prefetch(1, 1, S_exec);
        STEP(5)
INS_END

INS(ORI_TO_SR)
        STEP0
            sample_interrupts();
            if (!regs.SR.S) {
                start_group2_exception(IV_privilege_violation, 4, regs.PC - 2);
                return;
            }
            if (ins->ea[0].kind == 0)
                regs.IPC -= 2;
            start_prefetch(1, 1, S_exec);
        STEP(1)
            u32 data = regs.IR | get_SR();
            set_SR(data, 1);
            start_wait(8, S_exec);
        STEP(2)
            start_read(regs.PC - 2, 2, MAKE_FC(1), 0, S_exec);
        STEP(3)
            start_prefetch(1, 1, S_exec);
        STEP(4)
INS_END

INS(PEA)
        STEP0
            start_read_operand_for_ea(0, ins->sz, S_exec, 0, HOLD, ALLOW_REVERSE);
        STEP(1)
            sample_interrupts();
            state.op[0].addr = read_ea_addr(0, ins->sz, NO_HOLD, state.op[0].ext_words);
            state.instruction.result = state.op[0].addr;
            if ((ins->ea[0].kind == AM_address_register_indirect_with_index) ||
                        (ins->ea[0].kind == AM_program_counter_with_index))
                    start_wait(2, S_exec);
        STEP(2)
            if ((ins->ea[0].kind == AM_absolute_short_data) || (ins->ea[0].kind == AM_absolute_long_data)) {
                start_push(state.instruction.result, 4, S_exec, 0);
            }
            else {
                start_prefetch(1, 1, S_exec);
            }
        STEP(3)
            if ((ins->ea[0].kind == AM_absolute_short_data) || (ins->ea[0].kind == AM_absolute_long_data)) {
                start_prefetch(1, 1, S_exec);
            }
            else {
                start_push(state.instruction.result, 4, S_exec, 0);
            }
        STEP(4)
    INS_END

INS(RESET)
        STEP0
            sample_interrupts();
            if (!regs.SR.S) {
                start_group2_exception(IV_privilege_violation, 4, regs.PC - 2);
                return;
            }
            pins.RESET = 1;
            start_wait(128, S_exec);
        STEP(1)
            pins.RESET = 0;
            start_prefetch(1, 1, S_exec);
        STEP(2)
INS_END

INS(ROL_qimm_dr)
        STEP0
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(1)
            start_wait((ins->sz == 4 ? 4 : 2) + ins->ea[0].reg * 2, S_exec);
        STEP(2)
            state.instruction.result = ROL(regs.D[ins->ea[1].reg], ins->ea[0].reg, ins->sz);
            set_dr(ins->ea[1].reg, state.instruction.result, ins->sz);
        STEP(3)
INS_END

INS(ROL_dr_dr)
        STEP0
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(1)
            u32 cnt = regs.D[ins->ea[0].reg] & 63;
            start_wait((ins->sz == 4 ? 4 : 2) + cnt * 2, S_exec);
        STEP(2)
            u32 cnt = regs.D[ins->ea[0].reg] & 63;
            state.instruction.result = ROL(regs.D[ins->ea[1].reg], cnt, ins->sz);
            set_dr(ins->ea[1].reg, state.instruction.result, ins->sz);
INS_END

INS(ROL_ea)
        STEP0
            start_read_operands(0, ins->sz, S_exec, 0, NO_HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.instruction.result = ROL(state.op[0].val, 1, ins->sz);
            start_prefetch(1, 1, S_exec);
        STEP(2)
            start_write_operand(0, 0, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
INS_END

INS(ROR_qimm_dr)
        STEP0
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(1)
            start_wait((ins->sz == 4 ? 4 : 2) + ins->ea[0].reg * 2, S_exec);
        STEP(2)
            state.instruction.result = ROR(regs.D[ins->ea[1].reg], ins->ea[0].reg, ins->sz);
            set_dr(ins->ea[1].reg, state.instruction.result, ins->sz);
        STEP(3)
INS_END

INS(ROR_dr_dr)
        STEP0
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(1)
            u32 cnt = regs.D[ins->ea[0].reg] & 63;
            start_wait((ins->sz == 4 ? 4 : 2) + cnt * 2, S_exec);
        STEP(2)
            u32 cnt = regs.D[ins->ea[0].reg] & 63;
            state.instruction.result = ROR(regs.D[ins->ea[1].reg], cnt, ins->sz);
            set_dr(ins->ea[1].reg, state.instruction.result, ins->sz);
INS_END

INS(ROR_ea)
        STEP0
            start_read_operands(0, ins->sz, S_exec, 0, NO_HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.instruction.result = ROR(state.op[0].val, 1, ins->sz);
            start_prefetch(1, 1, S_exec);
        STEP(2)
            start_write_operand(0, 0, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
INS_END

INS(ROXL_qimm_dr)
        STEP0
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(1)
            start_wait((ins->sz == 4 ? 4 : 2) + ins->ea[0].reg * 2, S_exec);
        STEP(2)
            state.instruction.result = ROXL(regs.D[ins->ea[1].reg], ins->ea[0].reg, ins->sz);
            set_dr(ins->ea[1].reg, state.instruction.result, ins->sz);
        STEP(3)
INS_END

INS(ROXL_dr_dr)
        STEP0
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(1)
            u32 cnt = regs.D[ins->ea[0].reg] & 63;
            start_wait((ins->sz == 4 ? 4 : 2) + cnt * 2, S_exec);
        STEP(2)
            u32 cnt = regs.D[ins->ea[0].reg] & 63;
            state.instruction.result = ROXL(regs.D[ins->ea[1].reg], cnt, ins->sz);
            set_dr(ins->ea[1].reg, state.instruction.result, ins->sz);
INS_END

INS(ROXL_ea)
        STEP0
            start_read_operands(0, ins->sz, S_exec, 0, NO_HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.instruction.result = ROXL(state.op[0].val, 1, ins->sz);
            start_prefetch(1, 1, S_exec);
        STEP(2)
            start_write_operand(0, 0, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
INS_END

INS(ROXR_qimm_dr)
        STEP0
            start_prefetch(1, 1, S_exec);
            sample_interrupts();
        STEP(1)
            start_wait((ins->sz == 4 ? 4 : 2) + ins->ea[0].reg * 2, S_exec);
        STEP(2)
            state.instruction.result = ROXR(regs.D[ins->ea[1].reg], ins->ea[0].reg, ins->sz);
            set_dr(ins->ea[1].reg, state.instruction.result, ins->sz);
        STEP(3)
INS_END

INS(ROXR_dr_dr)
        STEP0
            start_prefetch(1, 1, S_exec);
            sample_interrupts();
        STEP(1)
            u32 cnt = regs.D[ins->ea[0].reg] & 63;
            start_wait((ins->sz == 4 ? 4 : 2) + cnt * 2, S_exec);
        STEP(2)
            u32 cnt = regs.D[ins->ea[0].reg] & 63;
            state.instruction.result = ROXR(regs.D[ins->ea[1].reg], cnt, ins->sz);
            set_dr(ins->ea[1].reg, state.instruction.result, ins->sz);
INS_END

INS(ROXR_ea)
        STEP0
            start_read_operands(0, ins->sz, S_exec, 0, NO_HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.instruction.result = ROXR(state.op[0].val, 1, ins->sz);
            start_prefetch(1, 1, S_exec);
        STEP(2)
            start_write_operand(0, 0, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
INS_END

INS(RTE)
        STEP0
            sample_interrupts();
            if (!regs.SR.S) {
                start_group2_exception(IV_privilege_violation, 4, regs.PC-2);
                return;
            }
            state.internal_interrupt_level = 0; // We can get all? interrupts again!
            state.instruction.result = get_SSP();
            inc_SSP(6);
            start_read(state.instruction.result+0, 2, MAKE_FC(0), M68K_RW_ORDER_NORMAL, S_exec);
        STEP(1)
            state.op[1].val = MAKE_FC(0);
            set_SR(state.bus_cycle.data, 1);
            start_read(state.instruction.result+2, 2, state.op[1].val, M68K_RW_ORDER_NORMAL, S_exec);
        STEP(2)
            state.op[0].val = state.bus_cycle.data << 16;
            start_read(state.instruction.result+4, 2, state.op[1].val, M68K_RW_ORDER_NORMAL, S_exec);
        STEP(3)
            state.op[0].val |= state.bus_cycle.data;
            regs.IPC -= 2;
            start_read(state.op[0].val, 2, MAKE_FC(1), M68K_RW_ORDER_NORMAL, S_exec);
        STEP(4)
            regs.IR = regs.IRC;
            regs.IRC = state.bus_cycle.data;
            start_read(state.op[0].val+2, 2, MAKE_FC(1), M68K_RW_ORDER_NORMAL, S_exec);
        STEP(5)
            regs.IR = regs.IRC;
            regs.IRC = state.bus_cycle.data;
            regs.PC = state.op[0].val+4;
INS_END

INS(RTR)
        STEP0
            regs.IPC -= 2;
            sample_interrupts();
            start_pop(2, MAKE_FC(0), S_exec);
        STEP(1)
            regs.SR.u = (regs.SR.u & 0xFFE0) | (state.bus_cycle.data & 0x1F);
            start_pop(4, MAKE_FC(0), S_exec);
        STEP(2)
            regs.PC = state.bus_cycle.data;
            start_prefetch(2, 1, S_exec);
        STEP(3)
INS_END

INS(RTS)
        STEP0
            regs.IPC -= 2;
            sample_interrupts();
            start_pop(4, MAKE_FC(0), S_exec);
        STEP(1)
            regs.PC = state.bus_cycle.data;
            start_prefetch(2, 1, S_exec);
        STEP(2)
INS_END

INS(SBCD)
        STEP0
            start_read_operands(1, ins->sz, S_exec, 0, NO_HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(2)
            // Thanks CLK for logic
            u8 rhs = state.op[0].val;
            u8 lhs = state.op[1].val;
            u8 extend = regs.SR.X;
            int unadjusted_result = lhs - rhs - extend;
            const int top = (lhs & 0xf0) - (rhs & 0xf0) - (0x60 & (unadjusted_result >> 4));
            int result = (lhs & 0xf) - (rhs & 0xf) - extend;
            const int low_adjustment = 0x06 & (result >> 4);
            regs.SR.X = regs.SR.C = !!((unadjusted_result - low_adjustment) & 0x300);
            result = result + top - low_adjustment;

            regs.SR.Z = (result & 0xFF) ? 0 : regs.SR.Z;
            regs.SR.N = !!(result & 0x80);
            regs.SR.V = !!(unadjusted_result & ~result & 0x80);
            state.instruction.result = result;
            start_write_operand(0, 1, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
            if (ins->ea[0].kind == AM_data_register_direct)
                start_wait(2, S_exec);
        STEP(4)
INS_END

INS(SCC)
        STEP0
            start_read_operands(0, ins->sz, S_exec, 0, NO_HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(2)
            if (!condition(ins->ea[0].reg)) {
                state.instruction.result = 0;
            }
            else {
                state.instruction.result = clip32[ins->sz] & 0xFFFFFFFF;
                if (ins->ea[1].kind == AM_data_register_direct) start_wait(2, S_exec);
            }
        STEP(3)
            start_write_operand(0, 1, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(4)
INS_END

INS(STOP)
        STEP0
            printf("\nSTOP!");
            if (!regs.SR.S) {
                start_group2_exception(IV_privilege_violation, 4, regs.PC-2);
                return;
            }
            state.instruction.result = regs.IRC;
            start_prefetch(1, 1, S_exec);
        STEP(1)
            set_SR(state.instruction.result, 0);
            state.current = S_stopped;
            state.stopped.next_state = S_exec;
            sample_interrupts();
        STEP(2)
            if (state.current == S_stopped) return;
            start_prefetch(1, 1, S_exec);
        STEP(3)

INS_END

INS(SUB)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            if ((ins->sz == 2) && (ins->ea[0].kind == AM_data_register_direct) && (ins->ea[1].kind == AM_address_register_indirect_with_predecrement))
                hold = 0;
            start_read_operands(0, ins->sz, S_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.instruction.result = SUB(state.op[0].val, state.op[1].val, ins->sz, 0, 1);
            start_prefetch(1, 1, S_exec);
        STEP(2)
            start_write_operand(0, 1, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
            if (ins->sz == 4) {
                if (ins->ea[1].kind == AM_data_register_direct) {
                    switch (ins->ea[0].kind) {
                        case AM_data_register_direct:
                        case AM_address_register_direct:
                        case AM_immediate:
                            start_wait(4, S_exec);
                            break;
                        default:
                            start_wait(2, S_exec);
                    }
                }
            } else {
            }
        STEP(4)
INS_END

INS(SUBA)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            start_read_operands(0, ins->sz, S_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            u32 a = sgn32(state.op[0].val, ins->sz);
            u32 b = regs.A[ins->ea[1].reg];
            state.instruction.result = b - a;
            start_prefetch(1, 1, S_exec);
        STEP(2)
            regs.A[ins->ea[1].reg] = state.instruction.result;
            if ((ins->sz != 4) || (ins->ea[0].kind == AM_data_register_direct) || (ins->ea[0].kind == AM_address_register_direct) || (ins->ea[0].kind == AM_immediate))
                start_wait(4, S_exec);
            else
                start_wait(2, S_exec);
        STEP(3)
INS_END

INS(SUBI_21)
        STEP0
            if (ins->sz == 2) regs.IPC -= 2;
            STARTI(1, 0);
        STEP(2)
            sample_interrupts();
        STEP(3)
INS_END

INS_NOSWITCH(SUBI)
    if ((ins->sz <= 2) && (state.instruction.TCU < 2)) return ins_SUBI_21();
    switch(state.instruction.TCU) {
        STEP0
            STARTI(0, 1);
        STEP(2)
            sample_interrupts();
            start_prefetch(1, 1, S_exec);
        STEP(3)
            state.instruction.result = SUB(state.instruction.result, state.op[0].val, ins->sz,
                                                 0, 1);
            start_write_operand(0, 0, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(4)
            if ((ins->sz == 4) && (ins->ea[0].kind == AM_data_register_direct))
                start_wait(4, S_exec);
        STEP(5)
INS_END

INS(SUBQ)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            start_read_operands(0, ins->sz, S_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.instruction.result = SUB(state.op[0].val, state.op[1].val, ins->sz, 0, 1);
            start_prefetch(1, 1, S_exec);
        STEP(2)
            /*if (ins->ea[1].kind == AM_address_register_indirect_with_predecrement) {
                state.instruction.result += ins->sz;
            }*/
            start_write_operand(0, 1, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
            if ((ins->ea[1].kind == AM_data_register_direct) && (ins->sz == 4)) {
                if (ins->ea[0].kind == AM_address_register_direct)
                    start_wait(6, S_exec);
                else
                    start_wait(4, S_exec);
            }
        STEP(4)
INS_END

INS(SUBQ_ar)
        STEP0
            sample_interrupts();
            state.instruction.result = regs.A[ins->ea[1].reg] - ins->ea[0].reg;
            start_prefetch(1, 1, S_exec);
        STEP(1)
            set_ar(ins->ea[1].reg, state.instruction.result, 4);
            start_wait(4, S_exec);
        STEP(2)
INS_END

INS(SUBX_sz4_predec)
        STEP0
            start_read_operands(1, ins->sz, S_exec, 0, HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.instruction.result = SUB(state.op[0].val, state.op[1].val, ins->sz, 1, 1);
            regs.A[ins->ea[1].reg] += 4;
        STEP(2)
            regs.A[ins->ea[1].reg] -= 2;
            start_write(regs.A[ins->ea[1].reg], state.instruction.result & 0xFFFF, 2, MAKE_FC(0), M68K_RW_ORDER_NORMAL, S_exec);
        STEP(3)
            state.instruction.prefetch++;
            start_prefetch(1, 1, S_exec); // all prefetches are done before writes
        STEP(4)
            regs.A[ins->ea[1].reg] -= 2;
            start_write(regs.A[ins->ea[1].reg], state.instruction.result >> 16, 2, MAKE_FC(0), M68K_RW_ORDER_NORMAL, S_exec);
        STEP(5)
INS_END

INS(SUBX_sz4_nopredec)
        STEP0
            start_read_operands(1, ins->sz, S_exec, 0, NO_HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.instruction.result = SUB(state.op[0].val, state.op[1].val, ins->sz, 1, 1);
            start_prefetch(1, 1, S_exec); // all prefetches are done before writes
        STEP(2)
            start_write_operand(0, 1, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
            start_wait(4, S_exec);
        STEP(4)
INS_END

INS(SUBX_sz1_2)
        STEP0
            start_read_operands(1, ins->sz, S_exec, 0, NO_HOLD, ALLOW_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            state.instruction.result = SUB(state.op[0].val, state.op[1].val, ins->sz, 1, 1);
            start_prefetch(1, 1, S_exec); // all prefetches are done before writes
        STEP(2)
            start_write_operand(0, 1, S_exec, ALLOW_REVERSE, NO_FORCE);
        STEP(3)
INS_END

INS_NOSWITCH(SUBX)
    if (ins->sz == 4) {
        u32 u = (ins->operand_mode == OM_ea_ea) || (ins->operand_mode == OM_r_ea);
        if (u && (ins->ea[1].kind == AM_address_register_indirect_with_predecrement)) {
            return ins_SUBX_sz4_predec();
        }
        else {
            return ins_SUBX_sz4_nopredec();
        }
    } else {
        return ins_SUBX_sz1_2();
    }
INS_END_NOSWITCH


INS(SWAP)
        STEP0
            u32 a = regs.D[ins->ea[0].reg];
            a = (a << 16) | (a >> 16);
            regs.D[ins->ea[0].reg] = a;
            regs.SR.C = regs.SR.V = 0;
            regs.SR.Z = a == 0;
            regs.SR.N = !!(a & 0x80000000);
            start_prefetch(1, 1, S_exec);
            sample_interrupts();
        STEP(1)
INS_END

INS(TRAP)
        STEP0
            start_group2_exception(ins->ea[0].reg + 32, 4, regs.PC);
        STEP(1)
            sample_interrupts();
INS_END

INS(TRAPV)
        STEP0
            start_prefetch(1, 1, S_exec);
            sample_interrupts();
        STEP(1)
            if (regs.SR.V) {
                start_group2_exception(IV_trapv_instruction, 0, regs.PC-2);
                return;
            }
        STEP(2)
INS_END

INS(TST)
        STEP0
            HOLD_WORD_POSTINC_PREDEC;
            HOLDL_l_predec_no;
            start_read_operands(0, ins->sz, S_exec, 0, hold, NO_REVERSE, MAKE_FC(0));
        STEP(1)
            sample_interrupts();
            regs.SR.C = regs.SR.V = 0;
            u32 v = clip32[ins->sz] & state.op[0].val;
            regs.SR.Z = v == 0;
            if (ins->sz == 1) v = SIGNe8to32(v);
            else if (ins->sz == 2) v = SIGNe16to32(v);
            regs.SR.N = v >= msb32[ins->sz];
            start_prefetch(1, 1, S_exec);
        STEP(2)
INS_END

INS(UNLK)
        STEP0
            sample_interrupts();
            state.op[0].addr = regs.A[7];
            state.instruction.result = regs.A[ins->ea[0].reg];
            start_read(state.instruction.result, 4, MAKE_FC(0), NO_REVERSE, S_exec);
        STEP(1)
            regs.A[7] = state.instruction.result + 4;
            regs.A[ins->ea[0].reg] = state.bus_cycle.data;
            start_prefetch(1, 1, S_exec);
        STEP(2)
INS_END



static u32 disasm_BAD(ins_t &m_ins, jsm_debug_read_trace &rt, jsm_string &out)
{
    printf("\nERROR UNIMPLEMENTED DISASSEMBLY %04x", m_ins.opcode);
    out.sprintf("UNIMPLEMENTED DISASSEMBLY %s", __func__);
    return 0;
}

INS(NOINS)
    STEP0
        printf("\nERROR UNIMPLEMENTED M68K INSTRUCTION %04x at PC %06x cyc:%lld", ins->opcode, regs.PC, *trace.cycles);
        assert(1==0);
        //dbg_break("UNIMPLEMENTED INSTRUCTION", *trace.cycles);
        start_wait(2, S_exec);
    STEP(1)
INS_END

#undef M68KINS
#undef INS_END
#undef STEP0
#undef STEP

static int already_decoded = 0;

#define MF(x) &core::ins_##x
#define MD(x) &disasm_##x

struct m68k_str_ret
{
    u32 t_max, s_max, ea_mode, ea_reg, size_max, rm;

};

static void transform_ea(EA *ea)
{
    if (ea->kind == 7) {
        if (ea->reg == 1) ea->kind = AM_absolute_long_data;
        else if (ea->reg == 2) ea->kind = AM_program_counter_with_displacement;
        else if (ea->reg == 3) ea->kind = AM_program_counter_with_index;
        else if (ea->reg == 4) ea->kind = AM_immediate;
    }
}

static void bind_opcode(const char* inpt, u32 sz, ins_func exec_func, disassemble_t disasm_func, u32 op_or, EA *ea1, EA *ea2, u32 variant, enum operand_modes operand_mode)
{
    u32 orer = 0x8000;
    u32 out = 0;

    for (const char *ptr = inpt; orer != 0; ptr++) {
        if (*ptr == ' ') continue;
        if (*ptr == '1') out |= orer;
        orer >>= 1;
    }

    out |= op_or;

    assert(out < 65536);

    if (operand_mode == OM_ea) {
        if ((ea1->kind == AM_address_register_direct) || (ea1->kind == AM_data_register_direct)) {
            operand_mode = OM_r;
        }
    }
    else if (operand_mode == OM_ea_r) {
        if ((ea1->kind == AM_address_register_direct) || (ea1->kind == AM_data_register_direct)) {
            operand_mode = OM_r_r;
        }
    }
    else if (operand_mode == OM_r_ea) {
        if ((ea2->kind == AM_address_register_direct) || (ea2->kind == AM_data_register_direct)) {
            operand_mode = OM_r_r;
        }
    }
    else if (operand_mode == OM_ea_ea) {
        if (((ea1->kind == AM_address_register_direct) || (ea1->kind == AM_data_register_direct)) && ((ea2->kind == AM_address_register_direct) || (ea2->kind == AM_data_register_direct))) {
            operand_mode = OM_r_r;
        }
        else {
            if ((ea1->kind == AM_address_register_direct) || (ea1->kind == AM_data_register_direct))
                operand_mode = OM_r_ea;
            else {
                if ((ea2->kind == AM_address_register_direct) || (ea2->kind == AM_data_register_direct)) {
                    operand_mode = OM_ea_r;
                }
            }
        }
    }
    else if (operand_mode == OM_qimm_ea) {
        if ((ea2->kind == AM_address_register_direct) || (ea2->kind == AM_data_register_direct)) {
            operand_mode = OM_qimm_r;
        }
    }


    if (operand_mode == OM_ea) {
        transform_ea(ea1);
    }
    if (operand_mode == OM_r_ea) {
        transform_ea(ea2);
    }
    if (operand_mode == OM_ea_r) {
        transform_ea(ea1);
    }
    if (operand_mode == OM_ea_ea) {
        transform_ea(ea1);
        transform_ea(ea2);
    }
    if (operand_mode == OM_qimm_ea) {
        transform_ea(ea2);
    }
    ins_t *ins = &decoded[out];
    ins->opcode = out;
    ins->disasm = disasm_func;
    ins->exec = exec_func;
    ins->sz = sz;
    ins->variant = variant;
    ins->operand_mode = operand_mode;
    if (ea1)
        memcpy(&ins->ea[0], ea1, sizeof(EA));
    if (ea2)
        memcpy(&ins->ea[1], ea2, sizeof(EA));
}

static EA mk_ea(u32 mode, u32 reg) {
    EA a{};
    a.kind = static_cast<address_modes>(mode);
    a.reg = reg;
    return a;
}
#undef BADINS

void do_decode() {
    if (already_decoded) return;
    already_decoded = 1;
    for (u32 i = 0; i < 65536; i++) {
        ins_t &d = decoded[i];
        d.opcode = i;
        d.exec = MF(NOINS);
        d.disasm = MD(BADINS);
        d.variant = 0;
    }

    EA op1{}, op2{};

    // NOLINTNEXTLINE(bugprone-suspicious-include)
    #include "generated/generated_decodes.cpp"
}
}