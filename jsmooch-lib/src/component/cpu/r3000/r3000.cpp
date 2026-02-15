//
// Created by . on 2/11/25.
//
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cstdio> // printf
#include "helpers/debug.h"

#include "r3000_instructions.h"
#include "r3000.h"
#include "r3000_disassembler.h"
namespace R3000 {
static constexpr char reg_alias_arr[33][12] = {
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

pipeline_item *pipeline::push()
{
    if (num_items == 2) return &empty_item;
    num_items++;
    switch(num_items) {
        case 1:
            return &item0;
        case 2:
            return &item1;
        default:
            NOGOHERE;
    }
}

void pipeline_item::clear()
{
    target = -1;
    value = 0;
    new_PC = 0xFFFFFFFF;
}

void pipeline::clear()
{
    item0.clear();
    item1.clear();
    current.clear();
    num_items = 0;
}

pipeline_item *pipeline::move_forward()
{
    if (num_items == 0) return &empty_item;
    current = item0;
    item0 = item1;

    item1.clear();
    num_items--;

    return &current;
}

void core::do_decode_table() {
    for (u32 op1 = 0; op1 < 0x3F; op1++) {
        OPCODE *mo = &decode_table[op1];
        insfunc o = &core::fNA;
        MN m = MN_NA;
        i32 a = 0;
        switch (op1) {
            case 0: {// SPECIAL
                for (u32 op2 = 0; op2 < 0x3F; op2++) {
                    a = 0;
                    switch (op2) {
                        case 0: // SLL
                            m = MN_J;
                            o = &core::fSLL;
                            break;
                        case 0x02: // SRL
                            m = MN_SRL;
                            o = &core::fSRL;
                            break;
                        case 0x03: // SRA
                            m = MN_SRA;
                            o = &core::fSRA;
                            break;
                        case 0x4: // SLLV
                            m = MN_SLLV;
                            o = &core::fSLLV;
                            break;
                        case 0x06: // SRLV
                            m = MN_SRLV;
                            o = &core::fSRLV;
                            break;
                        case 0x07: // SRAV
                            m = MN_SRAV;
                            o = &core::fSRAV;
                            break;
                        case 0x08: // JR
                            m = MN_JR;
                            o = &core::fJR;
                            break;
                        case 0x09: // JALR
                            m = MN_JALR;
                            o = &core::fJALR;
                            break;
                        case 0x0C: // SYSCALL
                            m = MN_SYSCALL;
                            o = &core::fSYSCALL;
                            break;
                        case 0x0D: // BREAK
                            m = MN_BREAK;
                            o = &core::fBREAK;
                            break;
                        case 0x10: // MFHI
                            m = MN_MFHI;
                            o = &core::fMFHI;
                            break;
                        case 0x11: // MTHI
                            m = MN_MTHI;
                            o = &core::fMTHI;
                            break;
                        case 0x12: // MFLO
                            m = MN_MFLO;
                            o = &core::fMFLO;
                            break;
                        case 0x13: // MTLO
                            m = MN_MTLO;
                            o = &core::fMTLO;
                            break;
                        case 0x18: // MULT
                            m = MN_MULT;
                            o = &core::fMULT;
                            break;
                        case 0x19: // MULTU
                            m = MN_MULTU;
                            o = &core::fMULTU;
                            break;
                        case 0x1A: // DIV
                            m = MN_DIV;
                            o = &core::fDIV;
                            break;
                        case 0x1B: // DIVU
                            m = MN_DIVU;
                            o = &core::fDIVU;
                            break;
                        case 0x20: // ADD
                            m = MN_ADD;
                            o = &core::fADD;
                            break;
                        case 0x21: // ADDU
                            m = MN_ADDU;
                            o = &core::fADDU;
                            break;
                        case 0x22: // SUB
                            m = MN_SUB;
                            o = &core::fSUB;
                            break;
                        case 0x23: // SUBU
                            m = MN_SUBU;
                            o = &core::fSUBU;
                            break;
                        case 0x24: // AND
                            m = MN_AND;
                            o = &core::fAND;
                            break;
                        case 0x25: // OR
                            m = MN_OR;
                            o = &core::fOR;
                            break;
                        case 0x26: // XOR
                            m = MN_XOR;
                            o = &core::fXOR;
                            break;
                        case 0x27: // NOR
                            m = MN_NOR;
                            o = &core::fNOR;
                            break;
                        case 0x2A: // SLT
                            m = MN_SLT;
                            o = &core::fSLT;
                            break;
                        case 0x2B: // SLTU
                            m = MN_SLTU;
                            o = &core::fSLTU;
                            break;
                        default:
                            m = MN_NA;
                            o = &core::fNA;
                            break;
                    }
                    mo = &decode_table[op2 + 0x3F];
                    mo->func = o;
                    mo->opcode = op2;
                    mo->mn = m;
                    mo->arg = a;
                }
                continue;
            }
            case 0x01: // BcondZ
                m = MN_BcondZ;
                o = &core::fBcondZ;
                break;
            case 0x02: // J
                m = MN_J;
                o = &core::fJ;
                break;
            case 0x03: // JAL
                m = MN_JAL;
                o = &core::fJAL;
                break;
            case 0x04: // BEQ
                m = MN_BEQ;
                o = &core::fBEQ;
                break;
            case 0x05: // BNE
                m = MN_BNE;
                o = &core::fBNE;
                break;
            case 0x06: // BLEZ
                m = MN_BLEZ;
                o = &core::fBLEZ;
                break;
            case 0x07: // BGTZ
                m = MN_BGTZ;
                o = &core::fBGTZ;
                break;
            case 0x08: // ADDI
                m = MN_ADDI;
                o = &core::fADDI;
                break;
            case 0x09: // ADDIU
                m = MN_ADDIU;
                o = &core::fADDIU;
                break;
            case 0x0A: // SLTI
                m = MN_SLTI;
                o = &core::fSLTI;
                break;
            case 0x0B: // SLTIU
                m = MN_SLTIU;
                o = &core::fSLTIU;
                break;
            case 0x0C: // ANDI
                m = MN_ANDI;
                o = &core::fANDI;
                break;
            case 0x0D: // ORI
                m = MN_ORI;
                o = &core::fORI;
                break;
            case 0x0E: // XORI
                m = MN_XORI;
                o = &core::fXORI;
                break;
            case 0x0F: // LUI
                m = MN_LUI;
                o = &core::fLUI;
                break;
            case 0x13: // COP3
            case 0x12: // COP2
            case 0x11: // COP1
            case 0x10: // COP0
                m = MN_COPx;
                o = &core::fCOP;
                a = (op1 - 0x10);
                break;
            case 0x20: // LB
                m = MN_LB;
                o = &core::fLB;
                break;
            case 0x21: // LH
                m = MN_LH;
                o = &core::fLH;
                break;
            case 0x22: // LWL
                m = MN_LWL;
                o = &core::fLWL;
                break;
            case 0x23: // LW
                m = MN_LW;
                o = &core::fLW;
                break;
            case 0x24: // LBU
                m = MN_LBU;
                o = &core::fLBU;
                break;
            case 0x25: // LHU
                m = MN_LHU;
                o = &core::fLHU;
                break;
            case 0x26: // LWR
                m = MN_LWR;
                o = &core::fLWR;
                break;
            case 0x28: // SB
                m = MN_SB;
                o = &core::fSB;
                break;
            case 0x29: // SH
                m = MN_SH;
                o = &core::fSH;
                break;
            case 0x2A: // SWL
                m = MN_SWL;
                o = &core::fSWL;
                break;
            case 0x2B: // SW
                m = MN_SW;
                o = &core::fSW;
                break;
            case 0x2E: // SWR
                m = MN_SWR;
                o = &core::fSWR;
                break;
            case 0x33: // LWC3
            case 0x32: // LWC2
            case 0x31: // LWC1
            case 0x30: // LWC0
                m = MN_LWCx;
                o = &core::fLWC;
                a = op1 - 0x30;
                break;
            case 0x3B: // SWC3
            case 0x3A: // SWC2
            case 0x39: // SWC1
            case 0x38: // SWC0
                m = MN_SWCx;
                o = &core::fSWC;
                a = op1 - 0x38;
                break;
        }
        mo->opcode = op1;
        mo->mn = m;
        mo->func = o;
        mo->arg = a;
    }
}

core::core(u64 *master_clock_in, u64 *waitstates_in, scheduler_t *scheduler_in, IRQ_multiplexer_b *IRQ_multiplexer_in) :
    clock(master_clock_in),
    scheduler(scheduler_in),
    waitstates(waitstates_in)
{
    io.I_STAT = IRQ_multiplexer_in;

    do_decode_table();

    pipe.clear();
}

void core::setup_tracing(jsm_debug_read_trace &strct, u64 *trace_cycle_pointer, i32 source_id)
{
    trace.strct.read_trace_m68k = strct.read_trace_m68k;
    trace.strct.ptr = strct.ptr;
    trace.strct.read_trace = strct.read_trace;
    trace.ok = true;
    trace.source_id = source_id;
}

void core::reset()
{
    pipe.clear();
    regs.PC = 0xBFC00000;
}

void core::add_to_console(u32 ch)
{
    /*if (dbg.console) {
        console_view_add_char(dbg.console, ch);
    }*/
    if (ch == '\n' || (console.cur - console.ptr) >= (console.allocated_len-1)) {
        printf("\n(CONSOLE) %s", console.ptr);
        console.quickempty();
    } else
        console.sprintf("%c", ch);
    //printf("%c", ch);
}

void core::delay_slots(pipeline_item &which)
{
    // Load delay slot from instruction before this one
    if (which.target > 0) {// R0 stays 0
        regs.R[which.target] = which.value;
        //printf("\nDelay slot %s set to %08x", reg_alias_arr[which.target], which.value);
        which.target = -1;
    }

    // Branch delay slot
    if (which.new_PC != 0xFFFFFFFF) {
        regs.PC = which.new_PC;
        if ((regs.PC & 0x1FFFFFFF) == 0x6BA0) {
            dbg_break("WOOPSIE", 0);
        }
        if ((regs.PC == 0xA0 && regs.R[9] == 0x3C) || (regs.PC == 0xB0 && regs.R[9] == 0x3D)) {
            if (regs.R[9] == 0x3D) {
                add_to_console(regs.R[4]);
            }
        }
        which.new_PC = 0xFFFFFFFF;
    }

}

void core::flush_pipe()
{
    delay_slots(pipe.current);
    delay_slots(pipe.item0);
    delay_slots(pipe.item1);
    pipe.move_forward();
    pipe.move_forward();
}

void core::exception(u32 code, u32 branch_delay, u32 cop0)
{
    //if (code == 4) dbg_break("Address Unaligned Error!", *clock);
    //else if (code == 0) {
    //            dbg_break("IRQ!", *clock);
    //}
        //printf("\nCurrent PC at exception: %08x  Current instruction address:%08x", regs.PC, pipe.current.addr);
    //printf("\nCurrent PC at exception: %08x  Current instruction address:%08x", regs.PC, pipe.current.addr);

    u32 mycode = code;
    code <<= 2;
    u32 vector = 0x80000080;
    if (regs.COP0[RCR_SR] & 0x400000) {
        vector = 0xBFC00180;
    }
    u32 raddr;
    if (!branch_delay) {
        raddr = regs.PC;
        //printf("\nNOT BRANCH DELAY? SO DO %08x", raddr);
    }
    else
    {
        raddr = regs.PC - 4;
        code |= 0x80000000;
        //printf("\nBRANCH DELAY!? SO DO %08x", raddr);
        //dbg_break("BRANCH DELAY", *clock);
    }
    regs.COP0[RCR_EPC] = raddr;
    flush_pipe();

    if (cop0)
        vector -= 0x40;

    dbglog_exception(mycode, vector, raddr, branch_delay);
    regs.PC = vector;
    regs.COP0[RCR_Cause] = code;
    u32 lstat = regs.COP0[RCR_SR];
    regs.COP0[RCR_SR] = (lstat & 0xFFFFFFC0) | ((lstat & 0x0F) << 2);
}

void core::decode(u32 IR, pipeline_item &current)
{
    u32 p1 = (IR & 0xFC000000) >> 26;

    if (p1 == 0) {
        current.op = &decode_table[0x3F + (IR & 0x3F)];
    }
    else {
        current.op = &decode_table[p1];
    }
}

void core::fetch_and_decode()
{
    if (regs.PC & 3) {
        exception(4, 0, 0);
    }
    u32 IR = read(read_ptr, regs.PC, 4, 1);
    auto &current = *pipe.push();
    decode(IR, current);
    current.opcode = IR;
    current.addr = regs.PC;
    regs.PC += 4;
}

void core::print_context(ctxt &ct, jsm_string &out)
{
    out.quickempty();
    u32 needs_commaspace = 0;
    for (u32 i = 1; i < 32; i++) {
        if (ct.regs & (1 << i)) {
            if (needs_commaspace) {
                out.sprintf(", ");
            }
            needs_commaspace = 1;
            out.sprintf("%s:%08x", reg_alias_arr[i], regs.R[i]);
        }
    }
    if (pipe.current.op->func == &core::fSYSCALL) out.sprintf("\nr4:%08x", regs.R[4]);
}

void core::lycoder_trace_format(jsm_string &out)
{
    ctxt ct{};
    ct.cop = 0;
    ct.regs = 0;
    ct.gte = 0;
    dbg_printf("\n%08x: %08x cyc:%lld", pipe.current.addr, pipe.current.opcode, *clock);
    R3000_disassemble(pipe.current.opcode, out, pipe.current.addr, &ct);
    dbg_printf("     %s", out.ptr);
    out.quickempty();
    print_context(ct, out);
    if ((out.cur - out.ptr) > 1) {
        dbg_printf("             \t; %s", out.ptr);
    }
}

void core::dbglog_exception(u32 code, u32 vector, u32 raddr, bool branch_delay) {
    if (!dbg.dvptr) return;
    if (dbg.dvptr->ids_enabled[trace.exception_id]) {
        if (dbg.dvptr->id_break[trace.exception_id]) dbg_break("Exception", *clock);
        trace.str.quickempty();
        trace.str.sprintf("Exception. Code:%d Vector:%08x ReturnAddr:%08x Delay:%d", code, vector, raddr, branch_delay);
        dbg.dvptr->add_printf(trace.exception_id, *clock, DBGLS_TRACE, "%s", trace.str.ptr);
    }
}


    void core::trace_format() {
    bool do_dbglog = false;
    if (dbg.dvptr) {
        do_dbglog = dbg.dvptr->ids_enabled[dbg.dv_id];
    }
    if (do_dbglog) {
        ctxt ct{};
        ct.cop = 0;
        ct.regs = 0;
        ct.gte = 0;
        //dbg_printf("\n%08x: %08x cyc:%lld", pipe.current.addr, pipe.current.opcode, *clock);
        R3000_disassemble(pipe.current.opcode, trace.str, pipe.current.addr, &ct);
        //if (pipe.current.addr == 0x80059ddc) dbg_break("SysBad instruction or whatever", *clock);
        trace.str2.quickempty();
        print_context(ct, trace.str2);
        dbglog_view *dv = dbg.dvptr;
        dv->add_printf(dbg.dv_id, *clock, DBGLS_TRACE, "%08x  %s", pipe.current.addr, trace.str.ptr);
        dv->extra_printf("%s", trace.str2.ptr);
    }
}


void core::trace_format_console(jsm_string &out)
{
    ctxt ct{};
    ct.cop = 0;
    ct.regs = 0;
    ct.gte = 0;
    //dbg_printf("\n%08x: %08x cyc:%lld", pipe.current.addr, pipe.current.opcode, *clock);
    R3000_disassemble(pipe.current.opcode, out, pipe.current.addr, &ct);
    //if (pipe.current.addr == 0xbfc0d8e8) dbg_break("SysBad instruction or whatever", *clock);
    printf("\n%08x  (%08x)   %s", pipe.current.addr, pipe.current.opcode, out.ptr);
    out.quickempty();
    print_context(ct, out);
    if ((out.cur - out.ptr) > 1) {
        printf("           \t%s", out.ptr);
    }
}

void core::check_IRQ()
{
    if (pins.IRQ && (regs.COP0[12] & 0x400) && (regs.COP0[12] & 1) && pipe.item0.new_PC == 0xFFFFFFFF) {
        //printf("\nDO IRQ!");
        regs.PC -= 4;
        exception(0, pipe.item0.new_PC != 0xFFFFFFFF, 0);
        //dbg_enable_trace();
        //dbg_break("YO", 0);
    }
}

void core::cycle(i32 howmany)
{
    i64 cycles_left = howmany;
    assert(regs.R[0] == 0);
    while(cycles_left > 0) {
        if (pipe.num_items < 1)
            fetch_and_decode();
        auto *current = pipe.move_forward();

#ifdef LYCODER
        //lycoder_trace_format(&trace.str);
#else
        if (::dbg.trace_on && ::dbg.traces.r3000.instruction) {
            trace_format_console(trace.str);
        }
#endif
        trace_format();
        (this->*current->op->func)(current->opcode, current->op);

        delay_slots(*current);

        current->clear();

        fetch_and_decode();

        *waitstates = 2;
        cycles_left -= *waitstates;
        (*clock) += *(waitstates);
        (*waitstates) = 0;

        if (::dbg.do_break) break;
    }
}

void core::update_I_STAT()
{
    pins.IRQ = (io.I_STAT->IF & io.I_MASK) != 0;
}

void core::write_reg(u32 addr, u8 sz, u32 val)
{
    u32 l3 = addr & 3;
    addr &= 0xFFFFFFFC;
    val <<= l3 * 8;
    switch(addr) {
        case 0x1F801070: // I_STAT write
            //printf("\nwrite I_STAT %04x current:%04llx", val, io.I_STAT->IF);
            io.I_STAT->mask(val);
            update_I_STAT();
            //printf("\nnew I_STAT: %04llx", io.I_STAT->IF);
            return;
        case 0x1F801074: // I_MASK write
            io.I_MASK = val & 0xFFFF07FF;
            //printf("\nwrite I_MASK %04x", val);
            update_I_STAT();
            return;
    }
    printf("\nUnhandled CPU write %08x (%d): %08x", addr, sz, val);
}

u32 core::read_reg(u32 addr, u8 sz)
{
    u32 l3 = addr & 3;
    addr &= 0xFFFFFFFC;
    i64 v = -1;
    switch(addr) {
        case 0x1F801070: // I_STAT read
            v = io.I_STAT->IF;
            break;
        case 0x1F801074: // I_MASK read
            v = io.I_MASK;
            break;
    }
    if (v == -1) {
        printf("\nUnhandled CPU read %08x", addr);
        static constexpr u32 mask[5] = {0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF};
        return mask[sz];
    }
    return v >> (l3 * 8);
}

void core::idle(u32 howlong)
{
    (*waitstates) += howlong;
}
}
