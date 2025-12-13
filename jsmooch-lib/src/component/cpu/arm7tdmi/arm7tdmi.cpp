//
// Created by . on 12/4/24.
//

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>

#include "arm7tdmi.h"
#include "arm7tdmi_decode.h"
#include "armv4_disassembler.h"
#include "thumb_disassembler.h"

//#define TRACE

#define PC R[15]
#define LR R[14]
#define SP R[13]

namespace ARM7TDMI {

static constexpr u32 maskalign[5] = {0, 0xFFFFFFFF, 0xFFFFFFFE, 0, 0xFFFFFFFC};

u32 core::fetch_ins(u32 sz) {
    u32 r = go_fetch_ins(regs.PC, sz, pipeline.access);
    return r;
}

void core::do_IRQ()
{
    if (regs.CPSR.T) {
        fetch_ins(2);
    }
    else {
        fetch_ins(4);
    };

    regs.SPSR_irq = regs.CPSR.u;
    //printf("\nDO IRQ! CURRENT PC:%08x T:%d cyc:%lld", regs.PC, regs.CPSR.T, *trace.cycles);
    regs.CPSR.mode = M_irq;
    fill_regmap();
    regs.CPSR.I = 1;

    u32 *r14 = regmap[14];
    if (regs.CPSR.T) {
        regs.CPSR.T = 0;
        *r14 = regs.PC;
    }
    else {
        *r14 = regs.PC - 4;
    }

    regs.PC = 0x00000018;
    reload_pipeline();
}

void core::sch_check_irq(void *ptr, u64 key, u64 timecode, u32 jitter)
{
    auto *th = static_cast<core *>(ptr);
    if (th->regs.IRQ_line && !th->regs.CPSR.I) {
        th->do_IRQ();
    }
}

void core::schedule_IRQ_check()
{
    if (scheduler && !sch_irq_sch) {
        scheduler->add_next(0, this, &sch_check_irq, nullptr);
    }
}

core::core(u64 *master_clock_in, u64 *waitstates_in, scheduler_t *scheduler_in) :
    master_clock(master_clock_in), waitstates(waitstates_in), scheduler(scheduler_in)
{
    fill_arm_table();
    for (u32 i = 0; i < 16; i++) {
        regmap[i] = &regs.R[i];
    }
    for (u32 i = 0; i < 65536; i++) {
        decode_thumb(i, &opcode_table_thumb[i]);
    }
    DBG_TRACE_VIEW_INIT;
}

void core::reset()
{
    pipeline.flushed = 0;

    regs.CPSR.F = 1;
    regs.CPSR.mode = M_supervisor;
    regs.SPSR_svc = regs.CPSR.u;
    regs.CPSR.T = 0;
    regs.CPSR.I = 1;
    *regmap[14] = 0;
    *regmap[15] = 0;
    reload_pipeline();

}

void core::setup_tracing(jsm_debug_read_trace *strct, u64 *trace_cycle_pointer, i32 source_id)
{
    trace.strct.read_trace_m68k = strct->read_trace_m68k;
    trace.strct.ptr = strct->ptr;
    trace.strct.read_trace = strct->read_trace;
    trace.ok = 1;
    trace.cycles = trace_cycle_pointer;
    trace.source_id = source_id;
}

void core::disassemble_entry(disassembly_entry& entry)
{
    u16 IR = trace.strct.read_trace_m68k(trace.strct.ptr, entry.addr, 1, 1);
    u16 opcode = IR;
    entry.dasm.quickempty();
    entry.context.quickempty();
    assert(1==0);
}

void core::do_FIQ()
{
    regs.SPSR_irq = regs.CPSR.u;
    regs.CPSR.mode = M_fiq;
    printf("\nFIQ! CURRENT PC:%08x T:%d cyc:%lld", regs.PC, regs.CPSR.T, *trace.cycles);
    regs.CPSR.mode = M_fiq;
    fill_regmap();
    regs.CPSR.I = 1;

    u32 *r14 = regmap[14];
    if (regs.CPSR.T) {
        regs.CPSR.T = 0;
        *r14 = regs.PC;
    }
    else {
        *r14 = regs.PC - 4;
    }

    regs.PC = 0x0000001C;
    reload_pipeline();
}

static jsm_string arryo{250};

void core::bad_trace(u32 r, u32 sz)
{
    if (sz == 2) {
        thumb_disassemble(r, &arryo, -1, nullptr);
        printf("\nFetch THUMB opcode from %08x: %04x: %s", regs.PC - 4, r, arryo.ptr);
    }
    else {
        ARMv4_disassemble(r, &arryo, -1, nullptr);
        printf("\nFetch ARM opcode from %08x: %08x: %s", regs.PC - 8, r, arryo.ptr);
    }

}

int regs::condition_passes(int which) const {
#define flag(x) (CPSR. x)
    switch(which) {
        case CC_AL:    return 1;
        case CC_NV:    return 0;
        case CC_EQ:    return flag(Z) == 1;
        case CC_NE:    return flag(Z) != 1;
        case CC_CS_HS: return flag(C) == 1;
        case CC_CC_LO: return flag(C) == 0;
        case CC_MI:    return flag(N) == 1;
        case CC_PL:    return flag(N) == 0;
        case CC_VS:    return flag(V) == 1;
        case CC_VC:    return flag(V) == 0;
        case CC_HI:    return (flag(C) == 1) && (flag(Z) == 0);
        case CC_LS:    return (flag(C) == 0) || (flag(Z) == 1);
        case CC_GE:    return flag(N) == flag(V);
        case CC_LT:    return flag(N) != flag(V);
        case CC_GT:    return (flag(Z) == 0) && (flag(N) == flag(V));
        case CC_LE:    return (flag(Z) == 1) || (flag(N) != flag(V));
        default:
            NOGOHERE;
            return 0;
    }
#undef flag
}


void core::reload_pipeline()
{
    pipeline.flushed = 0;
    if (regs.CPSR.T) {
        pipeline.access = ARM7P_code | ARM7P_nonsequential;
        pipeline.opcode[0] = fetch_ins(2) & 0xFFFF;
        pipeline.addr[0] = regs.PC;
        regs.PC += 2;
        pipeline.access = ARM7P_code | ARM7P_sequential;
        pipeline.opcode[1] = fetch_ins(2) & 0xFFFF;
        pipeline.addr[1] = regs.PC;
        regs.PC += 2;
    }
    else {
        pipeline.access = ARM7P_code | ARM7P_nonsequential;
        pipeline.opcode[0] = fetch_ins(4);
        pipeline.addr[0] = regs.PC;
        regs.PC += 4;
        pipeline.access = ARM7P_code | ARM7P_sequential;
        pipeline.opcode[1] = fetch_ins(4);
        pipeline.addr[1] = regs.PC;
        regs.PC += 4;
    }
}

void core::print_context(ARMctxt *ct, jsm_string *out)
{
    out->quickempty();
    u32 needs_commaspace = 0;
    for (u32 i = 0; i < 15; i++) {
        if (ct->regs & (1 << i)) {
            if (needs_commaspace) {
                out->sprintf(", ");
            }
            needs_commaspace = 1;
            out->sprintf("R%d:%08x", i, *regmap[i]);
        }
    }
}

void core::armv4_trace_format(u32 opcode, u32 addr, u32 T)
{
#ifdef TRACE
    ARMctxt ct;
        ct.regs = 0;
        if (T) {
            ARM7TDMI_thumb_disassemble(opcode, &trace.str, (i64) addr, &ct);
        } else {
            ARMv4_disassemble(opcode, &trace.str, (i64) addr, &ct);
        }
        print_context(&ct, &trace.str2);
    printf("\nARM7: %08x  %s  /  %s    / %08x", addr, trace.str.ptr, trace.str2.ptr, regs.R[15]);
#else
    u32 do_dbglog = 0;
    if (trace.dbglog.view) {
        do_dbglog = trace.dbglog.view->ids_enabled[trace.dbglog.id];
        //if (trace.dbglog.view->ids_enabled[trace.dbglog.id]) printf("\nID ENABLED? %d", do_dbglog);
    }
    u32 do_tracething = (dbg.tvptr && ::dbg.trace_on && ::dbg.traces.arm7tdmi.instruction);

    if (do_dbglog || do_tracething) {
        ARMctxt ct;
        ct.regs = 0;
        if (T) {
            thumb_disassemble(opcode, &trace.str, (i64) addr, &ct);
        } else {
            ARMv4_disassemble(opcode, &trace.str, (i64) addr, &ct);
        }
        print_context(&ct, &trace.str2);

        u64 tc;
        if (!trace.cycles) tc = 0;
        else tc = *trace.cycles;
        tc += *waitstates;

        if (do_dbglog) {
            dbglog_view *dv = trace.dbglog.view;
            dv->add_printf(trace.dbglog.id, tc, DBGLS_TRACE, "%08x  %s", addr, trace.str.ptr);
            dv->extra_printf("%s", trace.str2.ptr);
        }

        if (do_tracething && dbg.tvptr) {
            trace_view *tv = dbg.tvptr;
            tv->startline(trace.source_id);
            if (T) {
                tv->printf(0, "THUMB7");
                tv->printf(3, "%04x", opcode);
            } else {
                tv->printf(0, "ARM7");
                tv->printf(3, "%08x", opcode);
            }
            tv->printf(1, "%lld", tc);
            tv->printf(2, "%08x", addr);
            tv->printf(4, "%s", trace.str.ptr);
            tv->printf(5, "%s", trace.str2.ptr);
            tv->endline();
        }
    }
#endif
}

void core::decode_and_exec_thumb(u32 opcode, u32 opcode_addr)
{
#ifdef TRACE
    armv4_trace_format(opcode, opcode_addr, 1);
#else
    armv4_trace_format(opcode, opcode_addr, 1);
#endif
    thumb_instruction *ins = &opcode_table_thumb[opcode];
    ins->func(this, ins);
    if (pipeline.flushed)
        reload_pipeline();
}

void core::decode_and_exec_arm(u32 opcode, u32 opcode_addr)
{
    // bits 27-0 and 7-4
#ifdef TRACE
    armv4_trace_format(opcode, opcode_addr, 0);
#else
    armv4_trace_format(opcode, opcode_addr, 0);
#endif
    u32 decode = ((opcode >> 4) & 15) | ((opcode >> 16) & 0xFF0);
    arm7_ins = &opcode_table_arm[decode];
    arm7_ins->exec(this, opcode);
}

void core::idle(u32 num)
{
    (*waitstates) += num;
}

void core::IRQcheck(bool do_sched)
{
    if (do_sched) {
        scheduler->add_next(0, this, &sch_check_irq, nullptr);
        return;
    }
    if (regs.IRQ_line && !regs.CPSR.I) {
        do_IRQ();
    }
}

void core::run_noIRQcheck()
{
    u32 opcode = pipeline.opcode[0];
    u32 opcode_addr = pipeline.addr[0];
    pipeline.opcode[0] = pipeline.opcode[1];
    pipeline.addr[0] = pipeline.addr[1];
    regs.PC &= 0xFFFFFFFE;
    //if (regs.PC == 0x0800a648) dbg_break("PC==0800a649", *trace.cycles);

    if (regs.CPSR.T) { // THUMB mode!
        pipeline.opcode[1] = fetch_ins(2);
        pipeline.addr[1] = regs.PC;
        decode_and_exec_thumb(opcode, opcode_addr);
        if (pipeline.flushed)
            core::reload_pipeline();
    }
    else {
        pipeline.opcode[1] = fetch_ins(4);
        pipeline.addr[1] = regs.PC;
        if (regs.condition_passes(static_cast<int>(opcode >> 28))) {
            decode_and_exec_arm(opcode, opcode_addr);
            if (pipeline.flushed)
                reload_pipeline();
        }
        else {
#ifdef TRACE
            armv4_trace_format(opcode, opcode_addr, 0);
#else
            armv4_trace_format(opcode, opcode_addr, 0);
#endif
            pipeline.access = ARM7P_code | ARM7P_sequential;
            regs.PC += 4;
        }
    }
}

void core::flush_pipeline()
{
    //assert(1==2);
    pipeline.flushed = 1;
}

u32 core::go_fetch_ins(u32 addr, u32 sz, u32 access)
{
    //addr &= maskalign[sz];
    u32 v = (*this->read_ins)(read_ins_ptr, addr, sz, access);
    return v;
}

static constexpr u32 masksz[5] = { 0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF };

u32 core::read(u32 addr, u32 sz, u32 access, u32 has_effect)
{
    //addr &= maskalign[sz];
    u32 v = read_data(read_data_ptr, addr, sz, access, has_effect) & masksz[sz];
    return v;
}

void core::write(u32 addr, u32 sz, u32 access, u32 val)
{
    //addr &= maskalign[sz];
    write_data(write_data_ptr, addr, sz, access, val);
}
}