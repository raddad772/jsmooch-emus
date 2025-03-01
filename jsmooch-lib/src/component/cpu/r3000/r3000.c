//
// Created by . on 2/11/25.
//
#include <stdlib.h>
#include <string.h>
#include <printf.h>

#include "r3000_instructions.h"
#include "r3000.h"
#include "r3000_disassembler.h"

static const char reg_alias_arr[33][12] = {
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


static struct R3000_pipeline_item *pipe_push(struct R3000_pipeline *this)
{
    if (this->num_items == 2) return &this->empty_item;
    this->num_items++;
    switch(this->num_items) {
        case 1:
            return &this->item0;
        case 2:
            return &this->item1;
        default:
            NOGOHERE;
    }
}

static void pipe_item_clear(struct R3000_pipeline_item *this)
{
    this->target = -1;
    this->value = 0;
    this->new_PC = 0xFFFFFFFF;
}

static void pipe_clear(struct R3000_pipeline *this)
{
    pipe_item_clear(&this->item0);
    pipe_item_clear(&this->item1);
    pipe_item_clear(&this->current);
    this->num_items = 0;
}

struct R3000_pipeline_item *R3000_pipe_move_forward(struct R3000_pipeline *this)
{
    if (this->num_items == 0) return &this->empty_item;
    this->current = this->item0;
    this->item0 = this->item1;

    pipe_item_clear(&this->item1);
    this->num_items--;

    return &this->current;
}

static void do_decode_table(struct R3000 *this) {
    for (u32 op1 = 0; op1 < 0x3F; op1++) {
        struct R3000_opcode *mo = &this->decode_table[op1];
        R3000_ins o = &R3000_fNA;
        enum R3000_MN m = R3000_MN_NA;
        i32 a = 0;
        switch (op1) {
            case 0: {// SPECIAL
                for (u32 op2 = 0; op2 < 0x3F; op2++) {
                    a = 0;
                    switch (op2) {
                        case 0: // SLL
                            m = R3000_MN_J;
                            o = &R3000_fSLL;
                            break;
                        case 0x02: // SRL
                            m = R3000_MN_SRL;
                            o = &R3000_fSRL;
                            break;
                        case 0x03: // SRA
                            m = R3000_MN_SRA;
                            o = &R3000_fSRA;
                            break;
                        case 0x4: // SLLV
                            m = R3000_MN_SLLV;
                            o = &R3000_fSLLV;
                            break;
                        case 0x06: // SRLV
                            m = R3000_MN_SRLV;
                            o = &R3000_fSRLV;
                            break;
                        case 0x07: // SRAV
                            m = R3000_MN_SRAV;
                            o = &R3000_fSRAV;
                            break;
                        case 0x08: // JR
                            m = R3000_MN_JR;
                            o = &R3000_fJR;
                            break;
                        case 0x09: // JALR
                            m = R3000_MN_JALR;
                            o = &R3000_fJALR;
                            break;
                        case 0x0C: // SYSCALL
                            m = R3000_MN_SYSCALL;
                            o = &R3000_fSYSCALL;
                            break;
                        case 0x0D: // BREAK
                            m = R3000_MN_BREAK;
                            o = &R3000_fBREAK;
                            break;
                        case 0x10: // MFHI
                            m = R3000_MN_MFHI;
                            o = &R3000_fMFHI;
                            break;
                        case 0x11: // MTHI
                            m = R3000_MN_MTHI;
                            o = &R3000_fMTHI;
                            break;
                        case 0x12: // MFLO
                            m = R3000_MN_MFLO;
                            o = &R3000_fMFLO;
                            break;
                        case 0x13: // MTLO
                            m = R3000_MN_MTLO;
                            o = &R3000_fMTLO;
                            break;
                        case 0x18: // MULT
                            m = R3000_MN_MULT;
                            o = &R3000_fMULT;
                            break;
                        case 0x19: // MULTU
                            m = R3000_MN_MULTU;
                            o = &R3000_fMULTU;
                            break;
                        case 0x1A: // DIV
                            m = R3000_MN_DIV;
                            o = &R3000_fDIV;
                            break;
                        case 0x1B: // DIVU
                            m = R3000_MN_DIVU;
                            o = &R3000_fDIVU;
                            break;
                        case 0x20: // ADD
                            m = R3000_MN_ADD;
                            o = &R3000_fADD;
                            break;
                        case 0x21: // ADDU
                            m = R3000_MN_ADDU;
                            o = &R3000_fADDU;
                            break;
                        case 0x22: // SUB
                            m = R3000_MN_SUB;
                            o = &R3000_fSUB;
                            break;
                        case 0x23: // SUBU
                            m = R3000_MN_SUBU;
                            o = &R3000_fSUBU;
                            break;
                        case 0x24: // AND
                            m = R3000_MN_AND;
                            o = &R3000_fAND;
                            break;
                        case 0x25: // OR
                            m = R3000_MN_OR;
                            o = &R3000_fOR;
                            break;
                        case 0x26: // XOR
                            m = R3000_MN_XOR;
                            o = &R3000_fXOR;
                            break;
                        case 0x27: // NOR
                            m = R3000_MN_NOR;
                            o = &R3000_fNOR;
                            break;
                        case 0x2A: // SLT
                            m = R3000_MN_SLT;
                            o = &R3000_fSLT;
                            break;
                        case 0x2B: // SLTU
                            m = R3000_MN_SLTU;
                            o = &R3000_fSLTU;
                            break;
                        default:
                            m = R3000_MN_NA;
                            o = &R3000_fNA;
                            break;
                    }
                    mo = &this->decode_table[op2 + 0x3F];
                    mo->func = o;
                    mo->opcode = op2;
                    mo->mn = m;
                    mo->arg = a;
                }
                continue;
            }
            case 0x01: // BcondZ
                m = R3000_MN_BcondZ;
                o = &R3000_fBcondZ;
                break;
            case 0x02: // J
                m = R3000_MN_J;
                o = &R3000_fJ;
                break;
            case 0x03: // JAL
                m = R3000_MN_JAL;
                o = &R3000_fJAL;
                break;
            case 0x04: // BEQ
                m = R3000_MN_BEQ;
                o = &R3000_fBEQ;
                break;
            case 0x05: // BNE
                m = R3000_MN_BNE;
                o = &R3000_fBNE;
                break;
            case 0x06: // BLEZ
                m = R3000_MN_BLEZ;
                o = &R3000_fBLEZ;
                break;
            case 0x07: // BGTZ
                m = R3000_MN_BGTZ;
                o = &R3000_fBGTZ;
                break;
            case 0x08: // ADDI
                m = R3000_MN_ADDI;
                o = &R3000_fADDI;
                break;
            case 0x09: // ADDIU
                m = R3000_MN_ADDIU;
                o = &R3000_fADDIU;
                break;
            case 0x0A: // SLTI
                m = R3000_MN_SLTI;
                o = &R3000_fSLTI;
                break;
            case 0x0B: // SLTIU
                m = R3000_MN_SLTIU;
                o = &R3000_fSLTIU;
                break;
            case 0x0C: // ANDI
                m = R3000_MN_ANDI;
                o = &R3000_fANDI;
                break;
            case 0x0D: // ORI
                m = R3000_MN_ORI;
                o = &R3000_fORI;
                break;
            case 0x0E: // XORI
                m = R3000_MN_XORI;
                o = &R3000_fXORI;
                break;
            case 0x0F: // LUI
                m = R3000_MN_LUI;
                o = &R3000_fLUI;
                break;
            case 0x13: // COP3
            case 0x12: // COP2
            case 0x11: // COP1
            case 0x10: // COP0
                m = R3000_MN_COPx;
                o = &R3000_fCOP;
                a = (op1 - 0x10);
                break;
            case 0x20: // LB
                m = R3000_MN_LB;
                o = &R3000_fLB;
                break;
            case 0x21: // LH
                m = R3000_MN_LH;
                o = &R3000_fLH;
                break;
            case 0x22: // LWL
                m = R3000_MN_LWL;
                o = &R3000_fLWL;
                break;
            case 0x23: // LW
                m = R3000_MN_LW;
                o = &R3000_fLW;
                break;
            case 0x24: // LBU
                m = R3000_MN_LBU;
                o = &R3000_fLBU;
                break;
            case 0x25: // LHU
                m = R3000_MN_LHU;
                o = &R3000_fLHU;
                break;
            case 0x26: // LWR
                m = R3000_MN_LWR;
                o = &R3000_fLWR;
                break;
            case 0x28: // SB
                m = R3000_MN_SB;
                o = &R3000_fSB;
                break;
            case 0x29: // SH
                m = R3000_MN_SH;
                o = &R3000_fSH;
                break;
            case 0x2A: // SWL
                m = R3000_MN_SWL;
                o = &R3000_fSWL;
                break;
            case 0x2B: // SW
                m = R3000_MN_SW;
                o = &R3000_fSW;
                break;
            case 0x2E: // SWR
                m = R3000_MN_SWR;
                o = &R3000_fSWR;
                break;
            case 0x33: // LWC3
            case 0x32: // LWC2
            case 0x31: // LWC1
            case 0x30: // LWC0
                m = R3000_MN_LWCx;
                o = &R3000_fLWC;
                a = op1 - 0x30;
                break;
            case 0x3B: // SWC3
            case 0x3A: // SWC2
            case 0x39: // SWC1
            case 0x38: // SWC0
                m = R3000_MN_SWCx;
                o = &R3000_fSWC;
                a = op1 - 0x38;
                break;
        }
        mo->opcode = op1;
        mo->mn = m;
        mo->func = o;
        mo->arg = a;
    }
}

void R3000_init(struct R3000 *this, u64 *master_clock, u64 *waitstates, struct scheduler_t *scheduler, struct IRQ_multiplexer_b *IRQ_multiplexer)
{
    memset(this, 0, sizeof(*this));
    this->clock = master_clock;
    this->scheduler = scheduler;
    this->waitstates = waitstates;
    this->io.I_STAT = IRQ_multiplexer;

    do_decode_table(this);

    pipe_clear(&this->pipe);

    jsm_string_init(&this->console, 200);
    jsm_string_init(&this->trace.str, 100);

    GTE_init(&this->gte);
}

void R3000_delete(struct R3000 *this)
{
    jsm_string_delete(&this->console);
    jsm_string_delete(&this->trace.str);
}

void R3000_setup_tracing(struct R3000 *this, struct jsm_debug_read_trace *strct, u64 *trace_cycle_pointer, i32 source_id)
{
    this->trace.strct.read_trace_m68k = strct->read_trace_m68k;
    this->trace.strct.ptr = strct->ptr;
    this->trace.strct.read_trace = strct->read_trace;
    this->trace.ok = 1;
    this->trace.source_id = source_id;
}

void R3000_reset(struct R3000 *this)
{
    pipe_clear(&this->pipe);
    this->regs.PC = 0xBFC00000;
}

static void add_to_console(struct R3000 *this, u32 ch)
{
    if (ch == '\n') {
        printf("\n%s", this->console.ptr);
        jsm_string_quickempty(&this->console);
    }
    else
        jsm_string_sprintf(&this->console, "%c", ch);
}

static void delay_slots(struct R3000 *this, struct R3000_pipeline_item *which)
{
    // Load delay slot from instruction before this one
    if (which->target > 0) {// R0 stays 0
        this->regs.R[which->target] = which->value;
        //printf("\nDelay slot %s set to %08x", reg_alias_arr[which->target], which->value);
        which->target = -1;
    }

    // Branch delay slot
    if (which->new_PC != 0xFFFFFFFF) {
        this->regs.PC = which->new_PC;
        if ((this->regs.PC == 0xA0 && this->regs.R[9] == 0x3C) || (this->regs.PC == 0xB0 && this->regs.R[9] == 0x3D)) {
            if (this->regs.R[9] == 0x3D) {
                add_to_console(this, this->regs.R[4]);
            }
        }
        which->new_PC = 0xFFFFFFFF;
    }

}

void R3000_flush_pipe(struct R3000 *this)
{
    delay_slots(this, &this->pipe.current);
    delay_slots(this, &this->pipe.item0);
    delay_slots(this, &this->pipe.item1);
    R3000_pipe_move_forward(&this->pipe);
    R3000_pipe_move_forward(&this->pipe);
}

void R3000_exception(struct R3000 *this, u32 code, u32 branch_delay, u32 cop0)
{
    code <<= 2;
    u32 vector = 0x80000080;
    if (this->regs.COP0[RCR_SR] & 0x400000) {
        vector = 0xBFC00180;
    }
    u32 raddr;
    if (!branch_delay)
        raddr = this->regs.PC - 4;
    else
    {
        raddr = this->regs.PC;
        code |= 0x80000000;
    }
    this->regs.COP0[RCR_EPC] = raddr;
    R3000_flush_pipe(this);

    if (cop0)
        vector -= 0x40;

    this->regs.PC = vector;
    this->regs.COP0[RCR_Cause] = code;
    u32 lstat = this->regs.COP0[RCR_SR];
    this->regs.COP0[RCR_SR] = (lstat & 0xFFFFFFC0) | ((lstat & 0x0F) << 2);
}

static inline void decode(struct R3000 *this, u32 IR, struct R3000_pipeline_item *current)
{
    u32 p1 = (IR & 0xFC000000) >> 26;

    if (p1 == 0) {
        current->op = &this->decode_table[0x3F + (IR & 0x3F)];
    }
    else {
        current->op = &this->decode_table[p1];
    }
}

static void fetch_and_decode(struct R3000 *this)
{
    if (this->regs.PC & 3) {
        R3000_exception(this, 4, 0, 0);
    }
    u32 IR = this->read(this->read_ptr, this->regs.PC, 4, 1);
    struct R3000_pipeline_item *current = pipe_push(&this->pipe);
    decode(this, IR, current);
    current->opcode = IR;
    current->addr = this->regs.PC;
    this->regs.PC += 4;
}

static void R3000_print_context(struct R3000 *this, struct R3000ctxt *ct, struct jsm_string *out)
{
    jsm_string_quickempty(out);
    u32 needs_commaspace = 0;
    for (u32 i = 1; i < 32; i++) {
        if (ct->regs & (1 << i)) {
            if (needs_commaspace) {
                jsm_string_sprintf(out, ", ");
            }
            needs_commaspace = 1;
            jsm_string_sprintf(out, "%s:%08x", reg_alias_arr[i], this->regs.R[i]);
        }
    }
}

static void lycoder_trace_format(struct R3000 *this, struct jsm_string *out)
{
    struct R3000ctxt ct;
    ct.cop = 0;
    ct.regs = 0;
    ct.gte = 0;
    dbg_printf("\n%08x: %08x cyc:%lld", this->pipe.current.addr, this->pipe.current.opcode, *this->clock);
    R3000_disassemble(this->pipe.current.opcode, out, this->pipe.current.addr, &ct);
    dbg_printf("     %s", out->ptr);
    jsm_string_quickempty(out);
    R3000_print_context(this, &ct, out);
    if ((out->cur - out->ptr) > 1) {
        dbg_printf("             \t; %s", out->ptr);
    }
}

void R3000_trace_format(struct R3000 *this, struct jsm_string *out)
{
    struct R3000ctxt ct;
    ct.cop = 0;
    ct.regs = 0;
    ct.gte = 0;
    //dbg_printf("\n%08x: %08x cyc:%lld", this->pipe.current.addr, this->pipe.current.opcode, *this->clock);
    R3000_disassemble(this->pipe.current.opcode, out, this->pipe.current.addr, &ct);
    printf("\n%08x  (%08x)   %s", this->pipe.current.addr, this->pipe.current.opcode, out->ptr);
    jsm_string_quickempty(out);
    R3000_print_context(this, &ct, out);
    if ((out->cur - out->ptr) > 1) {
        printf("           \t%s", out->ptr);
    }
}

void R3000_check_IRQ(struct R3000 *this)
{
    if (this->pins.IRQ && (this->regs.COP0[12] & 0x400) && (this->regs.COP0[12] & 1)) {
        printf("\nDO IRQ!");
        R3000_exception(this, 0, this->pipe.item0.new_PC != 0xFFFFFFFF, 0);
    }
}

void R3000_cycle(struct R3000 *this, i32 howmany)
{
    i32 cycles_left = howmany;
    while(cycles_left > 0) {
        (*this->clock) += 2;

        if (this->pipe.num_items < 1)
            fetch_and_decode(this);
        struct R3000_pipeline_item *current = R3000_pipe_move_forward(&this->pipe);

#ifdef LYCODER
        lycoder_trace_format(this, &this->trace.str);
#else
        if (dbg.trace_on) {
            R3000_trace_format(this, &this->trace.str);
            //console.log(hex8(this->regs.PC) + ' ' + R3000_disassemble(current.opcode));
            //dbg.traces.add(D_RESOURCE_TYPES.R3000, this->clock.trace_cycles-1, this->trace_format(R3000_disassemble(current.opcode), current.addr))
        }
#endif
        current->op->func(current->opcode, current->op, this);

        delay_slots(this, current);

        pipe_item_clear(current);

        fetch_and_decode(this);

        cycles_left -= 2;
        if (dbg.do_break) break;
    }
}

void R3000_update_I_STAT(struct R3000 *this)
{
    this->pins.IRQ = (this->io.I_STAT->IF & this->io.I_MASK) != 0;
}

void R3000_write_reg(struct R3000 *this, u32 addr, u32 sz, u32 val)
{
    switch(addr) {
        case 0x1F801070: // I_STAT write
            //printf("\nwrite I_STAT %04x current:%04llx", val, this->io.I_STAT->IF);
            IRQ_multiplexer_b_mask(this->io.I_STAT, val);
            R3000_update_I_STAT(this);
            //printf("\nnew I_STAT: %04llx", this->io.I_STAT->IF);
            return;
        case 0x1F801074: // I_MASK write
            this->io.I_MASK = val;
            //printf("\nwrite I_MASK %04x", val);
            R3000_update_I_STAT(this);
            return;
    }
    printf("\nUnhandled CPU write %08x (%d): %08x", addr, sz, val);
}

u32 R3000_read_reg(struct R3000 *this, u32 addr, u32 sz)
{
    switch(addr) {
        case 0x1F801070: // I_STAT read
            return this->io.I_STAT->IF;
        case 0x1F801074: // I_MASK read
            return this->io.I_MASK;
    }
    printf("\nUnhandled CPU read %08x (%d)", addr, sz);
    static const u32 mask[5] = {0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF};
    return mask[sz];
}
