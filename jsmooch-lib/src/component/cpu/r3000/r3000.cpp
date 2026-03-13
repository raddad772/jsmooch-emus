//
// Created by . on 2/11/25.
//
#include <cstdlib>
#include <cassert>
#include <cstdio> // printf
#include "helpers/debug.h"

#include "r3000_debugger.h"
#include "r3000_instructions.h"
#include "r3000.h"
#include "r3000_disassembler.h"
namespace R3000 {
    static constexpr i32 CYCLES_PER_INSTRUCTION = 2;
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

void core::do_decode_table() {
    for (u32 op1 = 0; op1 < 0x3F; op1++) {
        OPCODE *mo = &decode_table[op1];
        insfunc o = &core::fNA;
        i32 a = 0;
        switch (op1) {
            case 0: {// SPECIAL
                for (u32 op2 = 0; op2 < 0x3F; op2++) {
                    a = 0;
                    switch (op2) {
                        case 0: // SLL
                            o = &core::fSLL;
                            break;
                        case 0x02: // SRL
                            o = &core::fSRL;
                            break;
                        case 0x03: // SRA
                            o = &core::fSRA;
                            break;
                        case 0x4: // SLLV
                            o = &core::fSLLV;
                            break;
                        case 0x06: // SRLV
                            o = &core::fSRLV;
                            break;
                        case 0x07: // SRAV
                            o = &core::fSRAV;
                            break;
                        case 0x08: // JR
                            o = &core::fJR;
                            break;
                        case 0x09: // JALR
                            o = &core::fJALR;
                            break;
                        case 0x0C: // SYSCALL
                            o = &core::fSYSCALL;
                            break;
                        case 0x0D: // BREAK
                            o = &core::fBREAK;
                            break;
                        case 0x10: // MFHI
                            o = &core::fMFHI;
                            break;
                        case 0x11: // MTHI
                            o = &core::fMTHI;
                            break;
                        case 0x12: // MFLO
                            o = &core::fMFLO;
                            break;
                        case 0x13: // MTLO
                            o = &core::fMTLO;
                            break;
                        case 0x18: // MULT
                            o = &core::fMULT;
                            break;
                        case 0x19: // MULTU
                            o = &core::fMULTU;
                            break;
                        case 0x1A: // DIV
                            o = &core::fDIV;
                            break;
                        case 0x1B: // DIVU
                            o = &core::fDIVU;
                            break;
                        case 0x20: // ADD
                            o = &core::fADD;
                            break;
                        case 0x21: // ADDU
                            o = &core::fADDU;
                            break;
                        case 0x22: // SUB
                            o = &core::fSUB;
                            break;
                        case 0x23: // SUBU
                            o = &core::fSUBU;
                            break;
                        case 0x24: // AND
                            o = &core::fAND;
                            break;
                        case 0x25: // OR
                            o = &core::fOR;
                            break;
                        case 0x26: // XOR
                            o = &core::fXOR;
                            break;
                        case 0x27: // NOR
                            o = &core::fNOR;
                            break;
                        case 0x2A: // SLT
                            o = &core::fSLT;
                            break;
                        case 0x2B: // SLTU
                            o = &core::fSLTU;
                            break;
                        default:
                            o = &core::fNA;
                            break;
                    }
                    mo = &decode_table[op2 + 0x3F];
                    mo->func = o;
                    mo->opcode = op2;
                    mo->arg = a;
                }
                continue;
            }
            case 0x01: // BcondZ
                o = &core::fBcondZ;
                break;
            case 0x02: // J
                o = &core::fJ;
                break;
            case 0x03: // JAL
                o = &core::fJAL;
                break;
            case 0x04: // BEQ
                o = &core::fBEQ;
                break;
            case 0x05: // BNE
                o = &core::fBNE;
                break;
            case 0x06: // BLEZ
                o = &core::fBLEZ;
                break;
            case 0x07: // BGTZ
                o = &core::fBGTZ;
                break;
            case 0x08: // ADDI
                o = &core::fADDI;
                break;
            case 0x09: // ADDIU
                o = &core::fADDIU;
                break;
            case 0x0A: // SLTI
                o = &core::fSLTI;
                break;
            case 0x0B: // SLTIU
                o = &core::fSLTIU;
                break;
            case 0x0C: // ANDI
                o = &core::fANDI;
                break;
            case 0x0D: // ORI
                o = &core::fORI;
                break;
            case 0x0E: // XORI
                o = &core::fXORI;
                break;
            case 0x0F: // LUI
                o = &core::fLUI;
                break;
            case 0x13: // COP3
            case 0x12: // COP2
            case 0x11: // COP1
            case 0x10: // COP0
                o = &core::fCOP;
                a = (op1 - 0x10);
                break;
            case 0x20: // LB
                o = &core::fLB;
                break;
            case 0x21: // LH
                o = &core::fLH;
                break;
            case 0x22: // LWL
                o = &core::fLWL;
                break;
            case 0x23: // LW
                o = &core::fLW;
                break;
            case 0x24: // LBU
                o = &core::fLBU;
                break;
            case 0x25: // LHU
                o = &core::fLHU;
                break;
            case 0x26: // LWR
                o = &core::fLWR;
                break;
            case 0x28: // SB
                o = &core::fSB;
                break;
            case 0x29: // SH
                o = &core::fSH;
                break;
            case 0x2A: // SWL
                o = &core::fSWL;
                break;
            case 0x2B: // SW
                o = &core::fSW;
                break;
            case 0x2E: // SWR
                o = &core::fSWR;
                break;
            case 0x33: // LWC3
            case 0x32: // LWC2
            case 0x31: // LWC1
            case 0x30: // LWC0
                o = &core::fLWC;
                a = op1 - 0x30;
                break;
            case 0x3B: // SWC3
            case 0x3A: // SWC2
            case 0x39: // SWC1
            case 0x38: // SWC0
                o = &core::fSWC;
                a = op1 - 0x38;
                break;
        }
        mo->opcode = op1;
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
    regs.PC = 0xBFC00000;
    regs.PC_next = regs.PC + 4;
    delay.branch[0] = {};
    delay.branch[1] = {};
    delay.load[0] = {};
    delay.load[1] = {};
}

void core::add_to_console(u32 ch)
{
    /*if (dbg.console) {
        console_view_add_char(dbg.console, ch);
    }*/
    if (ch == '\n' || (console.cur - console.ptr) >= (console.allocated_len-1)) {
        printf("\n(CONSOLE) %s", console.ptr);
        dbgloglog(trace.console_log_id, DBGLS_INFO, "%s", console.ptr);
        console.quickempty();
    } else {
        console.sprintf("%c", ch);
    }
    //printf("%c", ch);
}

void core::exception(u32 code, u32 cop0)
{
    CAUSE c;
    c.u = 0;
    c.exception_code = code;
    c.IP = io.I_STAT->IF;
    u32 vector = 0x80000080;
    if (regs.COP0[RCR_SR] & 0x400000) {
        vector = 0xBFC00180;
    }
    u32 raddr = regs.PC;
    c.BT = delay.branch[0].taken;
    c.BD = delay.branch[0].slot;
    if (delay.branch[0].slot) {
        raddr -= 4;
        if (delay.branch[0].taken) {
            regs.COP0[RCR_TAR] = delay.branch[0].target;
        }
        else {
            regs.COP0[RCR_TAR] = regs.PC_next;
        }
    }
    regs.COP0[RCR_EPC] = raddr;

    if (cop0)
        vector -= 0x40;

    if(code != 6 && code !=7) {
        c.CE = (regs.IR >> 26) & 3;
    }
    else {
        c.CE = (regs.COP0[RCR_Cause] >> 28) & 3;
    }

    dbglog_exception(code, vector, raddr);
    delay.branch[0] = {};
    delay.branch[1] = {};
    regs.PC = vector;
    regs.PC_next = vector;
    regs.COP0[RCR_Cause] = c.u;
    u32 lstat = regs.COP0[RCR_SR];
    regs.COP0[RCR_SR] = (lstat & 0xFFFFFFC0) | ((lstat & 0x0F) << 2);
}

void core::decode(u32 IR)
{
    u32 p1 = (IR & 0xFC000000) >> 26;

    if (p1 == 0) {
        cur_ins = &decode_table[0x3F + (IR & 0x3F)];
    }
    else {
        cur_ins = &decode_table[p1];
    }
}

bool core::fetch_and_decode()
{
    if (regs.PC & 3) {
        exception(4, 0);
        return false;
    }
    regs.IR = fetch_ins(fetch_ins_ptr, regs.PC);
    decode(regs.IR);
    cur_ins->opcode = regs.IR;
    return true;
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
    //if (pipe.current.op->func == &core::fSYSCALL) out.sprintf("\nr4:%08x", regs.R[4]);
}

void core::dbglog_exception(u32 code, u32 vector, u32 raddr) {
    if (!dbg.dvptr) return;
    if (dbg.dvptr->ids_enabled[trace.exception_id]) {
        if (code == 0 && dbg.dvptr->id_break[trace.exception_id]) dbg_break("Exception", *clock);
        trace.str.quickempty();
        trace.str.sprintf("Exception. Code:%d Vector:%08x ReturnAddr:%08x Delay:%d", code, vector, raddr, delay.branch[0].taken);
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
        R3000_disassemble(regs.IR, trace.str, regs.PC, &ct);
        //if (pipe.current.addr == 0x80059ddc) dbg_break("SysBad instruction or whatever", *clock);
        trace.str2.quickempty();
        print_context(ct, trace.str2);
        dbglog_view *dv = dbg.dvptr;
        dv->add_printf(dbg.dv_id, *clock, DBGLS_TRACE, "%08x  %s", regs.PC, trace.str.ptr);
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
    R3000_disassemble(regs.IR, out, regs.PC, &ct);
    //if (pipe.current.addr == 0xbfc0d8e8) dbg_break("SysBad instruction or whatever", *clock);
    printf("\n%08x  (%08x)   %s", regs.PC, regs.IR, out.ptr);
    out.quickempty();
    print_context(ct, out);
    if ((out.cur - out.ptr) > 1) {
        printf("           \t%s", out.ptr);
    }
}

static bool is_gte(u32 opcode) {
    return  (opcode &0xfe000000) == 0x4a000000;
}

u32 core::peek_next_instruction() const {
    return peek_ins(peek_ins_ptr, regs.PC);
}

void core::check_IRQ()
{
    if (pins.IRQ && (regs.COP0[12] & 0x400) && (regs.COP0[12] & 1) && (!delay.branch[0].slot)) {
        u32 ni = peek_next_instruction();
        if (is_gte(ni)) {
            // Execute opcode "early" before exception!
            gte.command(ni, current_clock());
        }
        exception(0, 0);
        after_ins();
    }
}

void core::after_ins() {
    regs.PC = regs.PC_next;
    regs.PC_next += 4;

    if ((regs.PC == 0xA0 && regs.R[9] == 0x3C) || (regs.PC == 0xB0 && regs.R[9] == 0x3D)) {
        if (regs.R[9] == 0x3D) {
            add_to_console(regs.R[4]);
        }
    }

    // Delay loads
    if (delay.load[0].target != -1) {
        regs.R[delay.load[0].target] = delay.load[0].value;
    }
    regs.R[0] = 0;
    delay.load[0] = delay.load[1];
    delay.load[1] = {};

    if (delay.branch[1].taken) {
        regs.PC_next = delay.branch[1].target;
    }

    delay.branch[0] = delay.branch[1];
    delay.branch[1] = {};
    check_IRQ();
}

void core::instruction() {
    if (!fetch_and_decode()) {
        after_ins();
        return;
    };

    trace_format();
    (this->*cur_ins->func)(regs.IR, cur_ins);

    after_ins();

    *waitstates = CYCLES_PER_INSTRUCTION;
}

void core::cycle(i32 howmany)
{
    i64 cycles_left = howmany;
    assert(regs.R[0] == 0);
    while(cycles_left > 0) {
        instruction();
        (*clock) += *(waitstates);
        cycles_left -= *waitstates;
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
            dbgloglog(trace.I_STAT_write, DBGLS_INFO, "IRQs acked: %04x", val & io.I_STAT->IF);
            io.I_STAT->mask(val);
            update_I_STAT();
            return;
        case 0x1F801074: // I_MASK write
            //dbgloglog(trace.I_MASK_write, DBGLS_INFO, "I_MASK: %04x", val);
            io.I_MASK = val & 0xFFFF07FF;
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
