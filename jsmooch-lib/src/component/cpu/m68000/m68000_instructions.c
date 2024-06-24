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
            M68k_start_read(this, 0, 2, M68k_FC_supervisor_program, M68kS_exec);
        STEP(1)
            // First word will be
            this->state.instruction.result = this->state.bus_cycle.data << 16;
            M68k_start_read(this, 2, 2, M68k_FC_supervisor_program, M68kS_exec);
        STEP(2)
            this->regs.ASP = this->state.instruction.result | this->state.bus_cycle.data;
            this->state.instruction.result = 0;
            M68k_start_read(this, 4, 2, M68k_FC_supervisor_program, M68kS_exec);
        STEP(3)
            this->state.instruction.result = this->state.bus_cycle.data << 16;
            M68k_start_read(this, 6, 2, M68k_FC_supervisor_program, M68kS_exec);
        STEP(4)
            this->regs.PC = this->state.instruction.result | this->state.bus_cycle.data;
            this->regs.SR.I = 7;
            // Start filling prefetch queue
            M68k_start_read(this, this->regs.PC, 2, M68k_FC_supervisor_program, M68kS_exec);
            this->regs.PC += 2;
        STEP(5)
            this->regs.IRC = this->state.bus_cycle.data;
            this->regs.IR = this->regs.IRC;
            M68k_start_read(this, this->regs.PC, 2, M68k_FC_supervisor_program, M68kS_exec);
            this->regs.PC += 2;
        STEP(6)
            this->regs.IRC = this->state.bus_cycle.data;
            this->regs.IRD = this->regs.IR;
INS_END

// IRD holds currently-executing instruction,
// IR holds currently-decoding instruction,
// IRC holds last pre-fetched word

#define BADINS { printf("\nUNIMPLEMENTED INSTRUCTION! %s", __func__); }

M68KINS(TAS)
        STEP0
            BADINS;
INS_END


M68KINS(ABCD)
        STEP0
            BADINS;
INS_END

M68KINS(ADD)
        STEP0
            BADINS;
INS_END

M68KINS(ADDA)
        STEP0
            BADINS;
INS_END

M68KINS(ADDI)
        STEP0
            BADINS;
INS_END

M68KINS(ADDQ)
        STEP0
            BADINS;
INS_END

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

    this->regs.SR.C = (carries ^ overflow) & msb32[sz];
    this->regs.SR.V = overflow & msb32[sz];
    this->regs.SR.Z = (clip32[sz] & result) ? 0 : (extend ? this->regs.SR.Z : 1);
    this->regs.SR.N = sgn32(result,sz) < 0;
    this->regs.SR.X = this->regs.SR.C;

    return clip32[sz] & result;
}

M68KINS(ADDX_sz4_predec)
        STEP0
            M68k_start_read_operands(this, 1, 1, M68kS_exec);
        STEP(1)
            this->state.instruction.result = ADD(this, this->state.op[0].val, this->state.op[1].val, ins->sz, 1);
        STEP(2)
            this->regs.A[ins->ea2.reg] -= 2;
            M68k_start_write(this, this->regs.A[ins->ea2.reg], this->state.instruction.result & 0xFFFF, 2, MAKE_FC(0), M68kS_exec);
        STEP(3)
            this->state.instruction.prefetch++;
            M68k_start_prefetch(this, 1, M68kS_exec); // all prefetches are done before writes
        STEP(4)
            this->regs.A[ins->ea2.reg] -= 2;
            M68k_start_write(this, this->regs.A[ins->ea2.reg], this->state.instruction.result >> 16, 2, MAKE_FC(0), M68kS_exec);
        STEP(5)
INS_END

M68KINS(ADDX_sz4_nopredec)
        STEP0
            M68k_start_read_operands(this, 1, 1, M68kS_exec);
        STEP(1)
            this->state.instruction.result = ADD(this, this->state.op[0].val, this->state.op[1].val, ins->sz, 1);
            M68k_start_prefetch(this, 1, M68kS_exec); // all prefetches are done before writes
        STEP(2)
            M68k_start_write_operand(this, 0, 1, M68kS_exec);
        STEP(3)
            M68k_start_wait(this, 4, M68kS_exec);
        STEP(4)
INS_END


M68KINS(ADDX_sz1_2)
        STEP0
            M68k_start_read_operands(this, 1, 1, M68kS_exec);
        STEP(1)
            this->state.instruction.result = ADD(this, this->state.op[0].val, this->state.op[1].val, ins->sz, 1);
            M68k_start_prefetch(this, 1, M68kS_exec); // all prefetches are done before writes
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
            BADINS;
INS_END

M68KINS(ANDI)
        STEP0
            BADINS;
INS_END

M68KINS(ANDI_TO_CCR)
        STEP0
            BADINS;
INS_END

M68KINS(ANDI_TO_SR)
        STEP0
            BADINS;
INS_END

M68KINS(ASL)
        STEP0
            BADINS;
INS_END

M68KINS(ASR)
        STEP0
            BADINS;
INS_END

M68KINS(BCC)
        STEP0
            BADINS;
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
            BADINS;
INS_END

M68KINS(BSET)
        STEP0
            BADINS;
INS_END

M68KINS(BSR)
        STEP0
            BADINS;
INS_END

M68KINS(BTST)
        STEP0
            BADINS;
INS_END

M68KINS(CHK)
        STEP0
            BADINS;
INS_END

M68KINS(CLR)
        STEP0
            BADINS;
INS_END

M68KINS(CMP)
        STEP0
            BADINS;
INS_END

M68KINS(CMPA)
        STEP0
            BADINS;
INS_END

M68KINS(CMPI)
        STEP0
            BADINS;
INS_END

M68KINS(CMPM)
        STEP0
            BADINS;
INS_END

M68KINS(DBCC)
        STEP0
            BADINS;
INS_END

M68KINS(DIVS)
        STEP0
            BADINS;
INS_END

M68KINS(DIVU)
        STEP0
            BADINS;
INS_END

M68KINS(EOR)
        STEP0
            BADINS;
INS_END

M68KINS(EORI)
        STEP0
            BADINS;
INS_END

M68KINS(EORI_TO_CCR)
        STEP0
            BADINS;
INS_END

M68KINS(EORI_TO_SR)
        STEP0
            BADINS;
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
            BADINS;
INS_END

M68KINS(LEA)
        STEP0
            BADINS;
INS_END

M68KINS(LINK)
        STEP0
            BADINS;
INS_END

M68KINS(LSL)
        STEP0
            BADINS;
INS_END

M68KINS(LSR)
        STEP0
            BADINS;
INS_END

M68KINS(MOVE)
        STEP0
            BADINS;
INS_END

M68KINS(MOVEA)
        STEP0
            BADINS;
INS_END

M68KINS(MOVEM_TO_MEM)
        STEP0
            BADINS;
INS_END

M68KINS(MOVEM_TO_REG)
        STEP0
            BADINS;
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
            BADINS;
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
            BADINS;
INS_END

M68KINS(NEGX)
        STEP0
            BADINS;
INS_END

M68KINS(NOP)
        STEP0
            BADINS;
INS_END

M68KINS(NOT)
        STEP0
            BADINS;
INS_END

M68KINS(OR)
        STEP0
            BADINS;
INS_END

M68KINS(ORI)
        STEP0
            BADINS;
INS_END

M68KINS(ORI_TO_CCR)
        STEP0
            BADINS;
INS_END

M68KINS(ORI_TO_SR)
        STEP0
            BADINS;
INS_END

M68KINS(PEA)
        STEP0
            BADINS;
INS_END

M68KINS(ROL)
        STEP0
            BADINS;
INS_END

M68KINS(ROR)
        STEP0
            BADINS;
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
            BADINS;
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
            BADINS;
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

M68KINS(SUBX)
        STEP0
            BADINS;
INS_END

M68KINS(SWAP)
        STEP0
            BADINS;
INS_END

M68KINS(TRAP)
        STEP0
            BADINS;
INS_END

M68KINS(TST)
        STEP0
            BADINS;
INS_END

M68KINS(UNLK)
        STEP0
            BADINS;
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
