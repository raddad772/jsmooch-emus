//
// Created by . on 12/4/24.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "arm7tdmi.h"
#include "arm7tdmi_decode.h"

#define PC R[15]
#define LR R[14]
#define SP R[13]

void ARM7TDMI_init(struct ARM7TDMI *this)
{
    memset(this, 0, sizeof(*this));
    ARM7TDMI_fill_arm_table(this);
    for (u32 i = 0; i < 16; i++) {
        this->regmap[i] = &this->regs.R[i];
    }
    for (u32 i = 0; i < 65536; i++) {
        decode_thumb(i, &this->opcode_table_thumb[i]);
    }
}

void ARM7TDMI_delete(struct ARM7TDMI *this)
{

}

void ARM7TDMI_reset(struct ARM7TDMI *this)
{
    this->regs.CPSR.u = 0;
    this->regs.CPSR.mode = ARM7_user;

}

void ARM7TDMI_setup_tracing(struct ARM7TDMI* this, struct jsm_debug_read_trace *strct, u64 *trace_cycle_pointer)
{
    this->trace.strct.read_trace_m68k = strct->read_trace_m68k;
    this->trace.strct.ptr = strct->ptr;
    this->trace.strct.read_trace = strct->read_trace;
    this->trace.ok = 1;
    this->trace.cycles = trace_cycle_pointer;
}

void ARM7TDMI_disassemble_entry(struct ARM7TDMI *this, struct disassembly_entry* entry)
{
    u16 IR = this->trace.strct.read_trace_m68k(this->trace.strct.ptr, entry->addr, 1, 1);
    u16 opcode = IR;
    jsm_string_quickempty(&entry->dasm);
    jsm_string_quickempty(&entry->context);
    /*struct M68k_ins_t *ins = &M68k_decoded[opcode];
    u32 mPC = entry->addr+2;
    ins->disasm(ins, &mPC, &this->trace.strct, &entry->dasm);
    entry->ins_size_bytes = mPC - entry->addr;
    struct M68k_ins_t *t =  &M68k_decoded[IR & 0xFFFF];
    pprint_ea(this, t, 0, &entry->context);
    pprint_ea(this, t, 1, &entry->context);*/
}

static void do_IRQ(struct ARM7TDMI* this)
{
    this->regs.R_irq[1] = this->regs.PC - 4;
    this->regs.SPSR_irq = this->regs.CPSR.u;
    this->regs.CPSR.T = 0;
    this->regs.CPSR.mode = ARM7_irq;
    ARM7TDMI_fill_regmap(this);
    this->regs.CPSR.I = 1;
    this->regs.PC = 0x00000018;
    ARM7TDMI_flush_pipeline(this);
}

static void do_FIQ(struct ARM7TDMI *this)
{
    this->regs.R_fiq[1] = this->regs.PC - 4;
    this->regs.SPSR_fiq = this->regs.CPSR.u;
    this->regs.CPSR.T = 0;
    this->regs.CPSR.mode = ARM7_fiq;
    ARM7TDMI_fill_regmap(this);
    this->regs.CPSR.I = 1;
    this->regs.CPSR.F = 1;
    this->regs.PC = 0x0000001C;
    ARM7TDMI_flush_pipeline(this);
}

static u32 fetch_ins(struct ARM7TDMI *this, u32 sz) {
    this->cycles_executed++;
    return this->fetch_ins(this->fetch_ptr, this->regs.PC, sz, this->pipeline.access);
}

static int condition_passes(struct ARM7TDMI_regs *this, int which) {
#define flag(x) (this->CPSR. x)
    switch(which) {
        case ARM7CC_AL:    return 1;
        case ARM7CC_NV:    return 0;
        case ARM7CC_EQ:    return flag(Z) == 1;
        case ARM7CC_NE:    return flag(Z) != 1;
        case ARM7CC_CS_HS: return flag(C) == 1;
        case ARM7CC_CC_LO: return flag(C) == 0;
        case ARM7CC_MI:    return flag(N) == 1;
        case ARM7CC_PL:    return flag(N) == 0;
        case ARM7CC_VS:    return flag(V) == 1;
        case ARM7CC_VC:    return flag(V) == 0;
        case ARM7CC_HI:    return (flag(C) == 1) && (flag(Z) == 0);
        case ARM7CC_LS:    return (flag(C) == 0) || (flag(Z) == 1);
        case ARM7CC_GE:    return flag(N) == flag(V);
        case ARM7CC_LT:    return flag(N) != flag(V);
        case ARM7CC_GT:    return (flag(Z) == 0) && (flag(N) == flag(V));
        case ARM7CC_LE:    return (flag(Z) == 1) || (flag(N) != flag(V));
        default:
            NOGOHERE;
    }
#undef flag
}


static void reload_pipeline(struct ARM7TDMI* this)
{
    this->pipeline.flushed = 0;
    if (this->regs.CPSR.T) {
        printf("\nTHUMBFLUSH!");
        this->pipeline.access = ARM7P_code | ARM7P_nonsequential;
        this->pipeline.opcode[0] = fetch_ins(this, 2) & 0xFFFF;
        this->regs.PC += 2;
        this->pipeline.access = ARM7P_code | ARM7P_sequential;
        this->pipeline.opcode[1] = fetch_ins(this, 2) & 0xFFFF;
        this->regs.PC += 2;
    }
    else {
        this->pipeline.access = ARM7P_code | ARM7P_nonsequential;
        this->pipeline.opcode[0] = fetch_ins(this, 4);
        this->regs.PC += 4;
        this->pipeline.access = ARM7P_code | ARM7P_sequential;
        this->pipeline.opcode[1] = fetch_ins(this, 4);
        this->regs.PC += 4;
    }
}

static void decode_and_exec_thumb(struct ARM7TDMI *this, u32 opcode)
{
    struct thumb_instruction *ins = &this->opcode_table_thumb[opcode];
    ins->func(this, ins);
    if (this->pipeline.flushed)
        reload_pipeline(this);
}

static void decode_and_exec_arm(struct ARM7TDMI *this, u32 opcode)
{
    // bits 27-0 and 7-4
    u32 decode = ((opcode >> 4) & 15) | ((opcode >> 16) & 0xFF0);
    this->last_arm7_opcode = opcode;
    this->arm7_ins = &this->opcode_table_arm[decode];
    this->arm7_ins->exec(this, opcode);
}

void ARM7TDMI_cycle(struct ARM7TDMI*this, i32 num)
{
    /*this->cycles_executed -= (i32)num;
    printf("\nCALL CYCLE! %d", this->cycles_executed);
    while(this->cycles_executed < 0) {*/
    for (u32 i = 0; i < num; i++) {
        if (dbg.trace_on) printf("\nCYCLE!");
        if (this->regs.IRQ_line) {
            do_IRQ(this);
        }

        u32 opcode = this->pipeline.opcode[0];
        this->pipeline.opcode[0] = this->pipeline.opcode[1];
        this->regs.PC &= 0xFFFFFFFE;

        if (this->regs.CPSR.T) { // THUMB mode!
            this->pipeline.opcode[1] = fetch_ins(this, 2);
            decode_and_exec_thumb(this, opcode);
        }
        else {
            this->pipeline.opcode[1] = fetch_ins(this, 4);
            if (condition_passes(&this->regs, (int)(opcode >> 28))) {
                decode_and_exec_arm(this, opcode);
                if (this->pipeline.flushed)
                    reload_pipeline(this);
            }
            else {
                this->pipeline.access = ARM7P_code | ARM7P_sequential;
                this->regs.PC += 4;
            }
        }
    }
}

void ARM7TDMI_flush_pipeline(struct ARM7TDMI *this)
{
    //assert(1==2);
    printf("\nFLUSH THE PIPE!@ PC:%08x", this->regs.PC);
    this->pipeline.flushed = 1;
}