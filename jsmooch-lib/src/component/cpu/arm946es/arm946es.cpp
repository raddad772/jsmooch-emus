//
// Created by . on 1/18/25.
//
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "arm946es.h"
#include "arm946es_decode.h"
#include "armv5_disassembler.h"
#include "arm946es_instructions.h"
#include "thumb2_disassembler.h"

#define PC R[15]

//#define TRACE
#define TRACEOFF
static constexpr u32 masksz[5] = { 0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF };
static constexpr u32 maskalign[5] = {0, 0xFFFFFFFF, 0xFFFFFFFE, 0, 0xFFFFFFFC};

namespace ARM946ES {
    u32 core::fetch_ins(u8 sz) {
        u32 r = do_fetch_ins(regs.PC, sz, pipeline.access);
        return r;
    }

core::core(scheduler_t *scheduler_in, u64 *master_clock_in, u64 *waitstates_in) :
        scheduler{scheduler_in}, waitstates{waitstates_in}, master_clock{master_clock_in}
{
    //dbg.trace_on = 1;
    fill_arm_table();
    for (u32 i = 0; i < 16; i++) {
        regmap[i] = &regs.R[i];
    }
    for (u32 i = 0; i < 65536; i++) {
        decode_thumb2(i, &opcode_table_thumb[i]);
    }
    DBG_TRACE_VIEW_INIT;
}

void core::reset()
{
    pipeline.flushed = false;

    regs.CPSR.F = 1;
    regs.CPSR.mode = M_supervisor;
    regs.SPSR_svc = regs.CPSR.u;
    regs.CPSR.T = 0;
    regs.CPSR.I = 1;
    *regmap[14] = 0;
    *regmap[15] = 0;
    regs.R[15] = 0xFFFF0000;
    reload_pipeline();

    regs.EBR = 0xFFFF0000;

}

void core::setup_tracing(jsm_debug_read_trace *strct, u64 *trace_cycle_pointer, i32 source_id)
{
    trace.strct.read_trace_m68k = strct->read_trace_m68k;
    trace.strct.ptr = strct->ptr;
    trace.strct.read_trace = strct->read_trace;
    trace.ok = true;
    trace.cycles = trace_cycle_pointer;
    trace.source_id = source_id;
}

void core::do_IRQ()
{
    halted = false;
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

    regs.PC = regs.EBR | 0x00000018;
    reload_pipeline();
}
void core::do_FIQ()
{
    regs.SPSR_irq = regs.CPSR.u;
    regs.CPSR.mode = M_fiq;
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

    regs.PC = regs.EBR | 0x0000001C;
    reload_pipeline();
}

bool core::condition_passes(const condition_codes which) const {
#define flag(x) (regs.CPSR. x)
    switch(which) {
        case CC_AL:    return true;
        case CC_NV:    return false;
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
            return false;
    }
#undef flag
}

void core::reload_pipeline()
{
    /*if (regs.PC == 0xFFFF07CE) {
        printf("\nYO!");
        dbg_break("arm9 ffff07ce", 0);
    }*/
    pipeline.flushed = false;
    if (regs.CPSR.T) {
        pipeline.access = ARM9P_code | ARM9P_nonsequential;
        pipeline.opcode[0] = fetch_ins(2) & 0xFFFF;
        pipeline.addr[0] = regs.PC;
        regs.PC += 2;
        pipeline.access = ARM9P_code | ARM9P_sequential;
        pipeline.opcode[1] = fetch_ins(2) & 0xFFFF;
        pipeline.addr[1] = regs.PC;
        regs.PC += 2;
    }
    else {
        pipeline.access = ARM9P_code | ARM9P_nonsequential;
        pipeline.opcode[0] = fetch_ins(4);
        pipeline.addr[0] = regs.PC;
        regs.PC += 4;
        pipeline.access = ARM9P_code | ARM9P_sequential;
        pipeline.opcode[1] = fetch_ins(4);
        pipeline.addr[1] = regs.PC;
        regs.PC += 4;
    }
}

void core::print_context(ARMctxt *ct, jsm_string *out, bool taken) const {
    out->quickempty();
    bool needs_commaspace = false;
    if (!taken) out->sprintf("NT.");
    for (u32 i = 0; i < 16; i++) {
        if (ct->regs & (1 << i)) {
            if (needs_commaspace) {
                out->sprintf(", ");
            }
            needs_commaspace = true;
            out->sprintf("R%d:%08x", i, *regmap[i]);
        }
    }
}


void core::armv5_trace_format(u32 opcode, u32 addr, bool T, bool taken)
{
    bool do_dbglog = false;
    if (dbg.dvptr) {
        do_dbglog = dbg.dvptr->ids_enabled[dbg.dv_id];
    }
    u32 do_tracething = (dbg.tvptr && ::dbg.trace_on && ::dbg.traces.arm946es.instruction);
    if (do_dbglog || do_tracething) {
        ARMctxt ct;
        ct.regs = 0;
        if (T) {
            thumb_disassemble(opcode, &trace.str, (i64) addr, &ct);
        } else {
            ARMv5_disassemble(opcode, &trace.str, (i64) addr, &ct);
        }
        print_context(&ct, &trace.str2, taken);

        u64 tc;
        if (!trace.cycles) tc = 0;
        else tc = *trace.cycles;
        tc += *waitstates;

        if (do_dbglog) {
            dbglog_view *dv = dbg.dvptr;
            dv->add_printf(dbg.dv_id, tc, DBGLS_TRACE, "%08x  %s", addr, trace.str.ptr);
            dv->extra_printf("%s", trace.str2.ptr);
        }

        if (do_tracething) {
            trace_view *tv = dbg.tvptr;
            tv->startline(trace.source_id);
            if (T) {
                tv->printf(0, "THUMB9");
                tv->printf(3, "%04x", opcode);
            } else {
                tv->printf(0, "ARM9");
                tv->printf(3, "%08x", opcode);
            }
            tv->printf(1, "%lld", tc);
            tv->printf(2, "%08x", addr);
            tv->printf(4, "%s", trace.str.ptr);
            tv->printf(5, "%s", trace.str2.ptr);
            tv->endline();
        }
    }
}

void core::decode_and_exec_thumb(u32 opcode, u32 opcode_addr)
{
    armv5_trace_format(opcode, opcode_addr, true, true);
    const thumb2_instruction &ins = opcode_table_thumb[opcode];
    (this->*ins.func)(ins);
    if (pipeline.flushed)
        reload_pipeline();
}

void core::decode_and_exec_arm(u32 opcode, u32 opcode_addr)
{
    // bits 27-0 and 7-4
#ifdef TRACE
    armv5_trace_format(opcode, opcode_addr, 0, 1);
#else
#ifndef TRACEOFF
    armv5_trace_format(opcode, opcode_addr, false, true);
#endif
#endif
    u32 decode = ((opcode >> 4) & 15) | ((opcode >> 16) & 0xFF0);
    arm_ins = &opcode_table_arm[decode];
    (this->*arm_ins->exec)(opcode);
}

void core::sch_check_irq(void *ptr, u64 key, u64 timecode, u32 jitter)
{
    auto *th = static_cast<core *>(ptr);
    if (th->regs.IRQ_line && !th->regs.CPSR.I) {
        th->do_IRQ();
    }
}

void core::IRQcheck(bool do_sched) {
    if (do_sched) {
        scheduler->add_next(0, this, &sch_check_irq, nullptr);
        return;
    }
    if (regs.IRQ_line && !regs.CPSR.I) {
        do_IRQ();
    }
}

void core::schedule_IRQ_check()
{
    if (scheduler && !sch_irq_sch) {
        scheduler->add_next(0, this, &sch_check_irq, &sch_irq_sch);
    }
}

void core::run_noIRQcheck()
{
    if (halted) {
        (*waitstates)++;
        return;
    }
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
            reload_pipeline();
    }
    else {
        pipeline.opcode[1] = fetch_ins(4);
        pipeline.addr[1] = regs.PC;
        if (condition_passes(static_cast<condition_codes>(opcode >> 28))) {
            decode_and_exec_arm(opcode, opcode_addr);
            if (pipeline.flushed)
                reload_pipeline();
        }
        else {
            // check for PLD and 4 undefined's
            u32 execed = 0;
            if ((opcode >> 28) == 15) {
#ifdef TRACE
                armv5_trace_format(opcode, opcode_addr, 0, 1);
#else
#ifndef TRACEOFF
                armv5_trace_format(opcode, opcode_addr, false, true);
#endif
#endif
                u32 decode = ((opcode >> 4) & 15) | ((opcode >> 16) & 0xFF0);
                arm_ins = &opcode_table_arm_never[decode];
                if (arm_ins->valid) {
                    (this->*arm_ins->exec)(opcode);
                    if (pipeline.flushed)
                        reload_pipeline();
                    execed = 1;
                }
            }

            if (!execed) {
#ifdef TRACE
                armv5_trace_format(opcode, opcode_addr, 0, 0);
#else
#ifndef TRACEOFF
                armv5_trace_format(opcode, opcode_addr, false, false);
#endif
#endif
                pipeline.access = ARM9P_code | ARM9P_sequential;
                regs.PC += 4;
            }
        }
    }
}

void core::idle(u32 num)
{
    *waitstates += num;
}

u32 core::do_fetch_ins(u32 addr, u8 sz, u8 access)
{
    addr &= maskalign[sz];
    if (addr_in_itcm(addr) && cp15.regs.control.itcm_enable && !cp15.regs.control.itcm_load_mode) {
        return read_itcm(addr, sz);
    }
    u32 v = fetch_ins_func(fetch_ptr, addr, sz, access);
    return v;
}

u32 core::read(u32 addr, u8 sz, u8 access, bool has_effect) {
    u32 v;
    addr &= maskalign[sz];

    if (addr_in_itcm(addr) && cp15.regs.control.itcm_enable && !cp15.regs.control.itcm_load_mode) {
        return read_itcm(addr, sz);
    }
    if (!(access & ARM9P_code) && addr_in_dtcm(addr) && cp15.regs.control.dtcm_enable && !cp15.regs.control.dtcm_load_mode) {
        return read_dtcm(addr, sz);
    }

    v = read_func(read_ptr, addr, sz, access, has_effect) & masksz[sz];
    return v;
}

void core::write(u32 addr, u8 sz, u8 access, u32 val)
{
    addr &= maskalign[sz];
    if (addr_in_itcm(addr) && cp15.regs.control.itcm_enable) {
        write_itcm(addr, sz, val);
        return;
    }
    if (!(access & ARM9P_code) && addr_in_dtcm(addr) && cp15.regs.control.dtcm_enable) {
        write_dtcm(addr, sz, val);
        return;
    }

    write_func(write_ptr, addr, sz, access, val);
}
}