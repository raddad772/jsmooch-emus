//
// Created by . on 12/4/24.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "arm7tdmi.h"
#include "arm7tdmi_decode.h"
#include "armv4_disassembler.h"
#include "thumb_disassembler.h"

//#define TRACE

#define PC R[15]
#define LR R[14]
#define SP R[13]


static u32 fetch_ins(struct ARM7TDMI *this, u32 sz) {
    u32 r = ARM7TDMI_fetch_ins(this, this->regs.PC, sz, this->pipeline.access);
    return r;
}


void ARM7TDMI_init(struct ARM7TDMI *this, u32 *waitstates)
{
    //dbg.trace_on = 1;
    memset(this, 0, sizeof(*this));
    ARM7TDMI_fill_arm_table(this);
    this->waitstates = waitstates;
    for (u32 i = 0; i < 16; i++) {
        this->regmap[i] = &this->regs.R[i];
    }
    for (u32 i = 0; i < 65536; i++) {
        decode_thumb(i, &this->opcode_table_thumb[i]);
    }
    jsm_string_init(&this->trace.str, 100);
    jsm_string_init(&this->trace.str2, 100);

    DBG_EVENT_VIEW_INIT;
    DBG_TRACE_VIEW_INIT;
}

void ARM7TDMI_delete(struct ARM7TDMI *this)
{

}

void ARM7TDMI_reset(struct ARM7TDMI *this)
{
    this->pipeline.flushed = 0;

    this->regs.CPSR.F = 1;
    this->regs.CPSR.mode = ARM7_supervisor;
    this->regs.SPSR_svc = this->regs.CPSR.u;
    this->regs.CPSR.T = 0;
    this->regs.CPSR.I = 1;
    *this->regmap[14] = 0;
    *this->regmap[15] = 0;
    ARM7TDMI_reload_pipeline(this);

}

void ARM7TDMI_setup_tracing(struct ARM7TDMI* this, struct jsm_debug_read_trace *strct, u64 *trace_cycle_pointer, i32 source_id)
{
    this->trace.strct.read_trace_m68k = strct->read_trace_m68k;
    this->trace.strct.ptr = strct->ptr;
    this->trace.strct.read_trace = strct->read_trace;
    this->trace.ok = 1;
    this->trace.cycles = trace_cycle_pointer;
    this->trace.source_id = source_id;
}

void ARM7TDMI_disassemble_entry(struct ARM7TDMI *this, struct disassembly_entry* entry)
{
    u16 IR = this->trace.strct.read_trace_m68k(this->trace.strct.ptr, entry->addr, 1, 1);
    u16 opcode = IR;
    jsm_string_quickempty(&entry->dasm);
    jsm_string_quickempty(&entry->context);
    assert(1==0);
}

static void do_IRQ(struct ARM7TDMI* this)
{
    if (this->regs.CPSR.T) {
        fetch_ins(this, 2);
    }
    else {
        fetch_ins(this, 4);
    };


    this->regs.SPSR_irq = this->regs.CPSR.u;
    //printf("\nDO IRQ! CURRENT PC:%08x T:%d cyc:%lld", this->regs.PC, this->regs.CPSR.T, *this->trace.cycles);
    this->regs.CPSR.mode = ARM7_irq;
    ARM7TDMI_fill_regmap(this);
    this->regs.CPSR.I = 1;

    u32 *r14 = this->regmap[14];
    if (this->regs.CPSR.T) {
        this->regs.CPSR.T = 0;
        *r14 = this->regs.PC;
    }
    else {
        *r14 = this->regs.PC - 4;
    }

    this->regs.PC = 0x00000018;
    ARM7TDMI_reload_pipeline(this);
}

static void do_FIQ(struct ARM7TDMI *this)
{
    this->regs.SPSR_irq = this->regs.CPSR.u;
    this->regs.CPSR.mode = ARM7_fiq;
    printf("\nFIQ! CURRENT PC:%08x T:%d cyc:%lld", this->regs.PC, this->regs.CPSR.T, *this->trace.cycles);
    this->regs.CPSR.mode = ARM7_fiq;
    ARM7TDMI_fill_regmap(this);
    this->regs.CPSR.I = 1;

    u32 *r14 = this->regmap[14];
    if (this->regs.CPSR.T) {
        this->regs.CPSR.T = 0;
        *r14 = this->regs.PC;
    }
    else {
        *r14 = this->regs.PC - 4;
    }

    this->regs.PC = 0x0000001C;
    ARM7TDMI_reload_pipeline(this);
}

static struct jsm_string arryo;
static u32 did_yo = 0;

static void bad_trace(struct ARM7TDMI *this, u32 r, u32 sz)
{
    if (!did_yo) {
        jsm_string_init(&arryo, 250);
        did_yo = 1;
    }
    if (sz == 2) {
        ARM7TDMI_thumb_disassemble(r, &arryo, -1, NULL);
        printf("\nFetch THUMB opcode from %08x: %04x: %s", this->regs.PC - 4, r, arryo.ptr);
    }
    else {
        ARMv4_disassemble(r, &arryo, -1, NULL);
        printf("\nFetch ARM opcode from %08x: %08x: %s", this->regs.PC - 8, r, arryo.ptr);
    }

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


void ARM7TDMI_reload_pipeline(struct ARM7TDMI* this)
{
    this->pipeline.flushed = 0;
    if (this->regs.CPSR.T) {
        this->pipeline.access = ARM7P_code | ARM7P_nonsequential;
        this->pipeline.opcode[0] = fetch_ins(this, 2) & 0xFFFF;
        this->pipeline.addr[0] = this->regs.PC;
        this->regs.PC += 2;
        this->pipeline.access = ARM7P_code | ARM7P_sequential;
        this->pipeline.opcode[1] = fetch_ins(this, 2) & 0xFFFF;
        this->pipeline.addr[1] = this->regs.PC;
        this->regs.PC += 2;
    }
    else {
        this->pipeline.access = ARM7P_code | ARM7P_nonsequential;
        this->pipeline.opcode[0] = fetch_ins(this, 4);
        this->pipeline.addr[0] = this->regs.PC;
        this->regs.PC += 4;
        this->pipeline.access = ARM7P_code | ARM7P_sequential;
        this->pipeline.opcode[1] = fetch_ins(this, 4);
        this->pipeline.addr[1] = this->regs.PC;
        this->regs.PC += 4;
    }
}

static void print_context(struct ARM7TDMI *this, struct ARMctxt *ct, struct jsm_string *out)
{
    jsm_string_quickempty(out);
    u32 needs_commaspace = 0;
    for (u32 i = 0; i < 15; i++) {
        if (ct->regs & (1 << i)) {
            if (needs_commaspace) {
                jsm_string_sprintf(out, ", ");
            }
            needs_commaspace = 1;
            jsm_string_sprintf(out, "R%d:%08x", i, *this->regmap[i]);
        }
    }
}

static void armv4_trace_format(struct ARM7TDMI *this, u32 opcode, u32 addr, u32 T)
{
    struct ARMctxt ct;
    ct.regs = 0;
    if (T) {
        ARM7TDMI_thumb_disassemble(opcode, &this->trace.str, (i64) addr, &ct);
    }
    else {
        ARMv4_disassemble(opcode, &this->trace.str, (i64) addr, &ct);
    }
    print_context(this, &ct, &this->trace.str2);
#ifdef TRACE
    printf("\nARM7: %08x  %s  /  %s    / %08x", addr, this->trace.str.ptr, this->trace.str2.ptr, this->regs.R[15]);
#else

    u64 tc;
    if (!this->trace.cycles) tc = 0;
    else tc = *this->trace.cycles;
    tc += *this->waitstates;
    struct trace_view *tv = this->dbg.tvptr;
    trace_view_startline(tv, this->trace.source_id);
    if (T) {
        trace_view_printf(tv, 0, "THUMB7");
        trace_view_printf(tv, 3, "%04x", opcode);
    }
    else {
        trace_view_printf(tv, 0, "ARM7");
        trace_view_printf(tv, 3, "%08x", opcode);
    }
    trace_view_printf(tv, 1, "%lld", tc);
    trace_view_printf(tv, 2, "%08x", addr);
    trace_view_printf(tv, 4, "%s", this->trace.str.ptr);
    trace_view_printf(tv, 5, "%s", this->trace.str2.ptr);
    trace_view_endline(tv);
#endif
}

static void decode_and_exec_thumb(struct ARM7TDMI *this, u32 opcode, u32 opcode_addr)
{
#ifdef TRACE
    armv4_trace_format(this, opcode, opcode_addr, 1);
#else
    if (dbg.trace_on) armv4_trace_format(this, opcode, opcode_addr, 1);
#endif
    struct thumb_instruction *ins = &this->opcode_table_thumb[opcode];
    ins->func(this, ins);
    if (this->pipeline.flushed)
        ARM7TDMI_reload_pipeline(this);
}

static void decode_and_exec_arm(struct ARM7TDMI *this, u32 opcode, u32 opcode_addr)
{
    // bits 27-0 and 7-4
#ifdef TRACE
    armv4_trace_format(this, opcode, opcode_addr, 0);
#else
    if (dbg.trace_on) armv4_trace_format(this, opcode, opcode_addr, 0);
#endif
    u32 decode = ((opcode >> 4) & 15) | ((opcode >> 16) & 0xFF0);
    this->arm7_ins = &this->opcode_table_arm[decode];
    this->arm7_ins->exec(this, opcode);
}

void ARM7TDMI_idle(struct ARM7TDMI*this, u32 num)
{
    (*this->waitstates) += num;
}

void ARM7TDMI_run(struct ARM7TDMI*this)
{
    if (this->regs.IRQ_line && !this->regs.CPSR.I) {
        do_IRQ(this);
    }

    u32 opcode = this->pipeline.opcode[0];
    u32 opcode_addr = this->pipeline.addr[0];
    this->pipeline.opcode[0] = this->pipeline.opcode[1];
    this->pipeline.addr[0] = this->pipeline.addr[1];
    this->regs.PC &= 0xFFFFFFFE;
    //if (this->regs.PC == 0x0800a648) dbg_break("PC==0800a649", *this->trace.cycles);

    if (this->regs.CPSR.T) { // THUMB mode!
        this->pipeline.opcode[1] = fetch_ins(this, 2);
        this->pipeline.addr[1] = this->regs.PC;
        decode_and_exec_thumb(this, opcode, opcode_addr);
        if (this->pipeline.flushed)
            ARM7TDMI_reload_pipeline(this);
    }
    else {
        this->pipeline.opcode[1] = fetch_ins(this, 4);
        this->pipeline.addr[1] = this->regs.PC;
        if (condition_passes(&this->regs, (int)(opcode >> 28))) {
            decode_and_exec_arm(this, opcode, opcode_addr);
            if (this->pipeline.flushed)
                ARM7TDMI_reload_pipeline(this);
        }
        else {
#ifdef TRACE
            armv4_trace_format(this, opcode, opcode_addr, 0);
#else
            if (dbg.trace_on) armv4_trace_format(this, opcode, opcode_addr, 0);
#endif
            this->pipeline.access = ARM7P_code | ARM7P_sequential;
            this->regs.PC += 4;
        }
    }
}

void ARM7TDMI_flush_pipeline(struct ARM7TDMI *this)
{
    //assert(1==2);
    this->pipeline.flushed = 1;
}

u32 ARM7TDMI_fetch_ins(struct ARM7TDMI *this, u32 addr, u32 sz, u32 access)
{
    if (sz == 2) addr &= 0xFFFFFFFE;
    else addr &= 0xFFFFFFFC;
    u32 v = this->fetch_ins(this->fetch_ptr, addr, sz, access);
    return v;
}

static const u32 masksz[5] = { 0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF };

u32 ARM7TDMI_read(struct ARM7TDMI *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    u32 v = this->read(this->read_ptr, addr, sz, access, has_effect) & masksz[sz];
    return v;
}

void ARM7TDMI_write(struct ARM7TDMI *this, u32 addr, u32 sz, u32 access, u32 val)
{
    this->write(this->write_ptr, addr, sz, access, val);
}
