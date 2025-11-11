//
// Created by . on 1/18/25.
//
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "arm946es.h"
#include "arm946es_decode.h"
#include "armv5_disassembler.h"
#include "arm946es_instructions.h"
#include "thumb2_disassembler.h"

#include "helpers/multisize_memaccess.c"
#define PC R[15]

//#define TRACE
static const u32 masksz[5] = { 0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF };
static const u32 maskalign[5] = {0, 0xFFFFFFFF, 0xFFFFFFFE, 0, 0xFFFFFFFC};

static u32 fetch_ins(ARM946ES *this, u32 sz) {
    u32 r = ARM946ES_fetch_ins(this, this->regs.PC, sz, this->pipeline.access);
    return r;
}

void ARM946ES_flush_pipeline(ARM946ES *this)
{
    this->pipeline.flushed = 1;
}

void ARM946ES_init(ARM946ES *this, u64 *master_clock, u64 *waitstates, scheduler_t *scheduler)
{
    //dbg.trace_on = 1;
    memset(this, 0, sizeof(*this));
    ARM946ES_fill_arm_table(this);
    this->waitstates = waitstates;
    this->scheduler = scheduler;
    this->master_clock = master_clock;
    for (u32 i = 0; i < 16; i++) {
        this->regmap[i] = &this->regs.R[i];
    }
    for (u32 i = 0; i < 65536; i++) {
        decode_thumb2(i, &this->opcode_table_thumb[i]);
    }
    jsm_string_init(&this->trace.str, 100);
    jsm_string_init(&this->trace.str2, 100);

    DBG_EVENT_VIEW_INIT;
    DBG_TRACE_VIEW_INIT;
}

void ARM946ES_reset(ARM946ES *this)
{
    this->pipeline.flushed = 0;

    this->regs.CPSR.F = 1;
    this->regs.CPSR.mode = ARM9_supervisor;
    this->regs.SPSR_svc = this->regs.CPSR.u;
    this->regs.CPSR.T = 0;
    this->regs.CPSR.I = 1;
    *this->regmap[14] = 0;
    *this->regmap[15] = 0;
    this->regs.R[15] = 0xFFFF0000;
    ARM946ES_reload_pipeline(this);

    this->regs.EBR = 0xFFFF0000;

}

void ARM946ES_setup_tracing(ARM946ES* this, jsm_debug_read_trace *strct, u64 *trace_cycle_pointer, i32 source_id)
{
    this->trace.strct.read_trace_m68k = strct->read_trace_m68k;
    this->trace.strct.ptr = strct->ptr;
    this->trace.strct.read_trace = strct->read_trace;
    this->trace.ok = 1;
    this->trace.cycles = trace_cycle_pointer;
    this->trace.source_id = source_id;
}

static void do_IRQ(ARM946ES* this)
{
    /*if (this->halted) {
        printf("\nUNHALT ARM9");
    }*/
    this->halted = 0;
    if (this->regs.CPSR.T) {
        fetch_ins(this, 2);
    }
    else {
        fetch_ins(this, 4);
    };

    this->regs.SPSR_irq = this->regs.CPSR.u;
    //printf("\nDO IRQ! CURRENT PC:%08x T:%d cyc:%lld", this->regs.PC, this->regs.CPSR.T, *this->trace.cycles);
    this->regs.CPSR.mode = ARM9_irq;
    ARM946ES_fill_regmap(this);
    this->regs.CPSR.I = 1;

    u32 *r14 = this->regmap[14];
    if (this->regs.CPSR.T) {
        this->regs.CPSR.T = 0;
        *r14 = this->regs.PC;
    }
    else {
        *r14 = this->regs.PC - 4;
    }

    this->regs.PC = this->regs.EBR | 0x00000018;
    ARM946ES_reload_pipeline(this);
}
static void do_FIQ(ARM946ES *this)
{
    this->regs.SPSR_irq = this->regs.CPSR.u;
    this->regs.CPSR.mode = ARM9_fiq;
    this->regs.CPSR.mode = ARM9_fiq;
    ARM946ES_fill_regmap(this);
    this->regs.CPSR.I = 1;

    u32 *r14 = this->regmap[14];
    if (this->regs.CPSR.T) {
        this->regs.CPSR.T = 0;
        *r14 = this->regs.PC;
    }
    else {
        *r14 = this->regs.PC - 4;
    }

    this->regs.PC = this->regs.EBR | 0x0000001C;
    ARM946ES_reload_pipeline(this);
}

static int condition_passes(ARM946ES_regs *this, int which) {
#define flag(x) (this->CPSR. x)
    switch(which) {
        case ARM9CC_AL:    return 1;
        case ARM9CC_NV:    return 0;
        case ARM9CC_EQ:    return flag(Z) == 1;
        case ARM9CC_NE:    return flag(Z) != 1;
        case ARM9CC_CS_HS: return flag(C) == 1;
        case ARM9CC_CC_LO: return flag(C) == 0;
        case ARM9CC_MI:    return flag(N) == 1;
        case ARM9CC_PL:    return flag(N) == 0;
        case ARM9CC_VS:    return flag(V) == 1;
        case ARM9CC_VC:    return flag(V) == 0;
        case ARM9CC_HI:    return (flag(C) == 1) && (flag(Z) == 0);
        case ARM9CC_LS:    return (flag(C) == 0) || (flag(Z) == 1);
        case ARM9CC_GE:    return flag(N) == flag(V);
        case ARM9CC_LT:    return flag(N) != flag(V);
        case ARM9CC_GT:    return (flag(Z) == 0) && (flag(N) == flag(V));
        case ARM9CC_LE:    return (flag(Z) == 1) || (flag(N) != flag(V));
        default:
            NOGOHERE;
            return 0;
    }
#undef flag
}

void ARM946ES_reload_pipeline(ARM946ES* this)
{
    if (this->regs.PC == 0xFFFF07CE) {
        printf("\nYO!");
        dbg_break("arm9 ffff07ce", 0);
    }
    this->pipeline.flushed = 0;
    if (this->regs.CPSR.T) {
        this->pipeline.access = ARM9P_code | ARM9P_nonsequential;
        this->pipeline.opcode[0] = fetch_ins(this, 2) & 0xFFFF;
        this->pipeline.addr[0] = this->regs.PC;
        this->regs.PC += 2;
        this->pipeline.access = ARM9P_code | ARM9P_sequential;
        this->pipeline.opcode[1] = fetch_ins(this, 2) & 0xFFFF;
        this->pipeline.addr[1] = this->regs.PC;
        this->regs.PC += 2;
    }
    else {
        this->pipeline.access = ARM9P_code | ARM9P_nonsequential;
        this->pipeline.opcode[0] = fetch_ins(this, 4);
        this->pipeline.addr[0] = this->regs.PC;
        this->regs.PC += 4;
        this->pipeline.access = ARM9P_code | ARM9P_sequential;
        this->pipeline.opcode[1] = fetch_ins(this, 4);
        this->pipeline.addr[1] = this->regs.PC;
        this->regs.PC += 4;
    }
}

static void print_context(ARM946ES *this, ARMctxt *ct, jsm_string *out, u32 taken)
{
    jsm_string_quickempty(out);
    u32 needs_commaspace = 0;
    if (!taken) jsm_string_sprintf(out, "NT.");
    for (u32 i = 0; i < 16; i++) {
        if (ct->regs & (1 << i)) {
            if (needs_commaspace) {
                jsm_string_sprintf(out, ", ");
            }
            needs_commaspace = 1;
            jsm_string_sprintf(out, "R%d:%08x", i, *this->regmap[i]);
        }
    }
}


static void armv5_trace_format(ARM946ES *this, u32 opcode, u32 addr, u32 T, u32 taken)
{
#ifdef TRACE
    printf("\nARM9: %08x  %s  /  %s    / %08x", addr, this->trace.str.ptr, this->trace.str2.ptr, this->regs.R[15]);
    struct ARMctxt ct;
    ct.regs = 0;
    if (T) {
        ARM946ES_thumb_disassemble(opcode, &this->trace.str, (i64) addr, &ct);
    }
    else {
        ARMv5_disassemble(opcode, &this->trace.str, (i64) addr, &ct);
    }
    print_context(this, &ct, &this->trace.str2, taken);
#else
    u32 do_dbglog = 0;
    if (this->dbg.dvptr) {
        do_dbglog = this->dbg.dvptr->ids_enabled[this->dbg.dv_id];
    }
    u32 do_tracething = (this->dbg.tvptr && dbg.trace_on && dbg.traces.arm946es.instruction);
    if (do_dbglog || do_tracething) {
        struct ARMctxt ct;
        ct.regs = 0;
        if (T) {
            ARM946ES_thumb_disassemble(opcode, &this->trace.str, (i64) addr, &ct);
        } else {
            ARMv5_disassemble(opcode, &this->trace.str, (i64) addr, &ct);
        }
        print_context(this, &ct, &this->trace.str2, taken);

        u64 tc;
        if (!this->trace.cycles) tc = 0;
        else tc = *this->trace.cycles;
        tc += *this->waitstates;

        if (do_dbglog) {
            struct dbglog_view *dv = this->dbg.dvptr;
            dbglog_view_add_printf(dv, this->dbg.dv_id, tc, DBGLS_TRACE, "%08x  %s", addr, this->trace.str.ptr);
            dbglog_view_extra_printf(dv, "%s", this->trace.str2.ptr);
        }

        if (do_tracething) {
            struct trace_view *tv = this->dbg.tvptr;
            trace_view_startline(tv, this->trace.source_id);
            if (T) {
                trace_view_printf(tv, 0, "THUMB9");
                trace_view_printf(tv, 3, "%04x", opcode);
            } else {
                trace_view_printf(tv, 0, "ARM9");
                trace_view_printf(tv, 3, "%08x", opcode);
            }
            trace_view_printf(tv, 1, "%lld", tc);
            trace_view_printf(tv, 2, "%08x", addr);
            trace_view_printf(tv, 4, "%s", this->trace.str.ptr);
            trace_view_printf(tv, 5, "%s", this->trace.str2.ptr);
            trace_view_endline(tv);
        }
    }
#endif
}

static void decode_and_exec_thumb(ARM946ES *this, u32 opcode, u32 opcode_addr)
{
#ifdef TRACE
    armv5_trace_format(this, opcode, opcode_addr, 1, 1);
#else
    armv5_trace_format(this, opcode, opcode_addr, 1, 1);
#endif
    struct thumb2_instruction *ins = &this->opcode_table_thumb[opcode];
    ins->func(this, ins);
    if (this->pipeline.flushed)
        ARM946ES_reload_pipeline(this);
}

static void decode_and_exec_arm(ARM946ES *this, u32 opcode, u32 opcode_addr)
{
    // bits 27-0 and 7-4
#ifdef TRACE
    armv5_trace_format(this, opcode, opcode_addr, 0, 1);
#else
    armv5_trace_format(this, opcode, opcode_addr, 0, 1);
#endif
    u32 decode = ((opcode >> 4) & 15) | ((opcode >> 16) & 0xFF0);
    this->arm9_ins = &this->opcode_table_arm[decode];
    this->arm9_ins->exec(this, opcode);
}

static void sch_check_irq(void *ptr, u64 key, u64 timecode, u32 jitter)
{
    struct ARM946ES *this = (ARM946ES *)ptr;
    if (this->regs.IRQ_line && !this->regs.CPSR.I) {
        do_IRQ(this);
    }
}

void ARM946ES_IRQcheck(ARM946ES *this, u32 do_sched) {
    if (do_sched) {
        scheduler_add_next(this->scheduler, 0, this, &sch_check_irq, NULL);
        return;
    }
    if (this->regs.IRQ_line && !this->regs.CPSR.I) {
        do_IRQ(this);
    }
}

void ARM946ES_schedule_IRQ_check(ARM946ES *this)
{
    if (this->scheduler && !this->sch_irq_sch) {
        scheduler_add_next(this->scheduler, 0, this, &sch_check_irq, &this->sch_irq_sch);
    }
}

void ARM946ES_run_noIRQcheck(ARM946ES*this)
{
    if (this->halted) {
        (*this->waitstates)++;
        return;
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
            ARM946ES_reload_pipeline(this);
    }
    else {
        this->pipeline.opcode[1] = fetch_ins(this, 4);
        this->pipeline.addr[1] = this->regs.PC;
        if (condition_passes(&this->regs, (int)(opcode >> 28))) {
            decode_and_exec_arm(this, opcode, opcode_addr);
            if (this->pipeline.flushed)
                ARM946ES_reload_pipeline(this);
        }
        else {
            // check for PLD and 4 undefined's
            u32 execed = 0;
            if ((opcode >> 28) == 15) {
#ifdef TRACE
                armv5_trace_format(this, opcode, opcode_addr, 0, 1);
#else
                armv5_trace_format(this, opcode, opcode_addr, 0, 1);
#endif
                u32 decode = ((opcode >> 4) & 15) | ((opcode >> 16) & 0xFF0);
                this->arm9_ins = &this->opcode_table_arm_never[decode];
                if (this->arm9_ins->valid) {
                    this->arm9_ins->exec(this, opcode);
                    if (this->pipeline.flushed)
                        ARM946ES_reload_pipeline(this);
                    execed = 1;
                }
            }

            if (!execed) {
#ifdef TRACE
                armv5_trace_format(this, opcode, opcode_addr, 0, 0);
#else
                armv5_trace_format(this, opcode, opcode_addr, 0, 0);
#endif
                this->pipeline.access = ARM9P_code | ARM9P_sequential;
                this->regs.PC += 4;
            }
        }
    }
}

void ARM946ES_delete(ARM946ES *this)
{

}

void ARM946ES_idle(ARM946ES*this, u32 num)
{
    *this->waitstates += num;
}

static inline u32 addr_in_itcm(ARM946ES *this, u32 addr)
{
    //printf("\ntest ADDR:%08x. ?:%d", addr, ((addr >= this->cp15.itcm.base_addr) && (addr < this->cp15.itcm.end_addr)));
    return ((addr >= this->cp15.itcm.base_addr) && (addr < this->cp15.itcm.end_addr));
}

static inline u32 read_itcm(ARM946ES *this, u32 addr, u32 sz)
{
    (*this->waitstates)++;
    u32 tcm_addr = (addr - this->cp15.itcm.base_addr) & this->cp15.itcm.mask;
    return cR[sz](this->cp15.itcm.data, tcm_addr & (ITCM_SIZE - 1));
}

u32 ARM946ES_fetch_ins(ARM946ES *this, u32 addr, u32 sz, u32 access)
{
    addr &= maskalign[sz];
    if (addr_in_itcm(this, addr) && this->cp15.regs.control.itcm_enable && !this->cp15.regs.control.itcm_load_mode) {
        return read_itcm(this, addr, sz);
    }
    u32 v = this->fetch_ins(this->fetch_ptr, addr, sz, access);
    return v;
}

static inline u32 addr_in_dtcm(ARM946ES *this, u32 addr)
{
    return ((addr >= this->cp15.dtcm.base_addr) && ((addr < this->cp15.dtcm.end_addr)));
}

static inline u32 read_dtcm(ARM946ES *this, u32 addr, u32 sz)
{
    (*this->waitstates)++;
    u32 tcm_addr = (addr - this->cp15.dtcm.base_addr) & (DTCM_SIZE - 1);
    return cR[sz](this->cp15.dtcm.data, tcm_addr & (DTCM_SIZE - 1));
}

static inline void write_dtcm(ARM946ES *this, u32 addr, u32 sz, u32 v)
{
    (*this->waitstates)++;
    u32 tcm_addr = (addr - this->cp15.dtcm.base_addr) & (this->cp15.dtcm.size - 1);
    cW[sz](this->cp15.dtcm.data, tcm_addr & (DTCM_SIZE - 1), v);
}

static inline void write_itcm(ARM946ES *this, u32 addr, u32 sz, u32 v)
{
    (*this->waitstates)++;
    u32 tcm_addr = (addr - this->cp15.itcm.base_addr) & this->cp15.itcm.mask;
    cW[sz](this->cp15.itcm.data, tcm_addr & (ITCM_SIZE - 1), v);
}

u32 ARM946ES_read(ARM946ES *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    u32 v;
    addr &= maskalign[sz];

    if (addr_in_itcm(this, addr) && this->cp15.regs.control.itcm_enable && !this->cp15.regs.control.itcm_load_mode) {
        return read_itcm(this, addr, sz);
    }
    if (!(access & ARM9P_code) && addr_in_dtcm(this, addr) && this->cp15.regs.control.dtcm_enable && !this->cp15.regs.control.dtcm_load_mode) {
        return read_dtcm(this, addr, sz);
    }

    v = this->read(this->read_ptr, addr, sz, access, has_effect) & masksz[sz];
    return v;
}

void ARM946ES_write(ARM946ES *this, u32 addr, u32 sz, u32 access, u32 val)
{
    addr &= maskalign[sz];
    if (addr_in_itcm(this, addr) && this->cp15.regs.control.itcm_enable) {
        write_itcm(this, addr, sz, val);
        return;
    }
    if (!(access & ARM9P_code) && addr_in_dtcm(this, addr) && this->cp15.regs.control.dtcm_enable) {
        write_dtcm(this, addr, sz, val);
        return;
    }

    this->write(this->write_ptr, addr, sz, access, val);
}
