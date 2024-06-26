//
// Created by RadDad772 on 4/14/24.
//

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "helpers/debug.h"
#include "m68000.h"
#include "m68000_instructions.h"
#include "m68000_internal.h"
#include "m68000_disassembler.h"

void M68k_disasm_RESET(struct M68k_ins_t *ins, u32 PC, struct jsm_debug_read_trace *rt, struct jsm_string *out);
void M68k_init(struct M68k* this)
{
    memset(this, 0, sizeof(struct M68k));
    do_M68k_decode();
    this->SPEC_RESET.sz = 4;
    this->SPEC_RESET.operand_mode = M68k_OM_none;
    this->SPEC_RESET.disasm = &M68k_disasm_RESET;
    this->SPEC_RESET.exec = &M68k_ins_RESET;
    jsm_string_init(&this->trace.str, 100);
}

void M68k_setup_tracing(struct M68k* this, struct jsm_debug_read_trace *strct)
{
    this->trace.strct.read_trace_m68k = strct->read_trace_m68k;
    this->trace.strct.ptr = strct->ptr;
    this->trace.strct.read_trace = strct->read_trace;
    this->trace.ok = 1;
}

void M68k_delete(struct M68k* this)
{
    jsm_string_delete(&this->trace.str);
}

// big-endian processor. so bytes are Hi, Lo
// in a 16-bit read, the Upper will be addr 0, the Lower will be addr 1

// addr = 0, UDS = 1, LDS = 0
// addr = 1, UDS = 0, LDS = 1


static void M68k_decode(struct M68k* this)
{
    u32 IRD = this->regs.IR;
    this->regs.IRD = IRD;
    struct M68k_ins_t *ins = &M68k_decoded[IRD & 0xFFFF];
    this->state.instruction.TCU = 0;
    this->ins = ins;
    this->state.instruction.done = 0;
    this->state.instruction.prefetch = 1; // 1 prefetches are needed currently
}

#define SIGNe8to32(x) ((((x) >> 7) * 0xFFFFFF00) | ((x) & 0xFF))

u32 M68k_read_ea_addr(struct M68k* this, struct M68k_EA *ea, u32 sz, u32 hold, u32 prefetch)
{
    u32 v;
    i64 a, b;
    switch(ea->kind) {
        case M68k_AM_address_register_indirect: // 0 ext word
            return this->regs.A[ea->reg];
        case M68k_AM_address_register_indirect_with_postincrement: // 0 ext word
            v = this->regs.A[ea->reg];
            this->regs.A[ea->reg] += sz;
            if ((ea->reg == 7) && (sz == 1)) this->regs.A[7]++;
            return v;
        case M68k_AM_address_register_indirect_with_predecrement: // 0 ext word
            this->regs.A[ea->reg] -= sz;
            if ((ea->reg == 7) && (sz == 1)) this->regs.A[7]--;
            return this->regs.A[ea->reg];
        case M68k_AM_address_register_indirect_with_displacement: // 1 ext word
            v = (u32)((i64)this->regs.A[ea->reg] + (i64)(i16)(prefetch & 0xFFFF));
            return v;
        case M68k_AM_address_register_indirect_with_index: // 1 ext word
            v = this->regs.A[ea->reg];
            a = SIGNe8to64(prefetch);
            b = prefetch & 0x8000 ? this->regs.A[(prefetch >> 12) & 7] : this->regs.D[(prefetch >> 12) & 7];
            if (!(prefetch & 0x800))
                b = (u32)(i32)(i16)(b & 0xFFFF);
            return (v + a + b) & 0xFFFFFFFF;
        case M68k_AM_absolute_short_data: // 1 ext word
            // Sign-extend 16-bit prefetch word, and use that
            return SIGNe16to32(prefetch);
        case M68k_AM_absolute_long_data: // 2 ext word
            return prefetch;
        case M68k_AM_program_counter_with_displacement: // 1 ext word
            v = SIGNe16to32(prefetch);
            return this->regs.PC + v - 4;
        case M68k_AM_program_counter_with_index: // 1 ext word
            v = this->regs.PC;
            a = SIGNe8to64(prefetch);
            b = prefetch & 0x8000 ? this->regs.A[(prefetch >> 12) & 7] : this->regs.D[(prefetch >> 12) & 7];
            if (!(prefetch & 0x800))
                b = (u32)(i32)(i16)(b & 0xFFFF);
            return ((v + a + b) - 4) & 0xFFFFFFFF;
        default:
            printf("\nUNKNOWN EA ADDR %d!", ea->kind);
            assert(1==0);
            return 0;
    }
}


// so read_operands has got an interesting job to do.
// it must read as many extension words (0-4) as there are for the operands through prefetch,
// then calculate addresses and act on them.

// so it'll do
// pref1
// pref2
// wait1
// read1
// wait2
// read2


void M68k_set_SR(struct M68k* this, u32 val)
{
    u32 new_s = (val & 0x2000) >> 13;
    // CCR
    this->regs.SR.u = (this->regs.SR.u & 0xFFE0) | (val & 0x1F);

    if (new_s != this->regs.SR.S) {
        M68k_swap_ASP(this);
    }
    this->regs.SR.S = new_s;

    this->regs.SR.I = (val >> 8) & 7;
    this->regs.SR.T = (val >> 15) & 1;
}

u32  M68k_get_SR(struct M68k* this) {
    return this->regs.SR.u;
}


static void M68k_prefetch(struct M68k* this)
{
    if (this->state.prefetch.TCU == 1) { // We just completed a cycle of prefetch
        this->regs.IR = this->regs.IRC;
        this->regs.IRC = this->state.bus_cycle.data;
        this->state.prefetch.num--;
        this->state.prefetch.TCU = 0;
    }
    if (this->state.prefetch.num <= 0) {
        this->state.current = this->state.prefetch.next_state;
        return;
    }
    M68k_start_read(this, this->regs.PC, 2, this->state.prefetch.FC, M68kS_prefetch);
    this->regs.PC += 2;
    this->state.prefetch.TCU = 1;
}

static void M68k_read_operands_prefetch(struct M68k* this, u32 opnum)
{
    // read 16 or 32 bits.
    // move them into
    if (this->state.operands.TCU == 1) {
        u32 v;
        if (this->state.op[opnum].ext_words == 1) { // 1 prefetch word
            this->regs.IR = this->regs.IRC;
            this->regs.IRC = this->state.bus_cycle.data & 0xFFFF;
            v = this->regs.IR;
        } else {
            assert(this->state.op[opnum].ext_words == 2);
            v = (this->regs.IRC << 16);
            this->regs.IR = this->state.bus_cycle.data >> 16;
            this->regs.IRC = this->state.bus_cycle.data & 0xFFFF;
            v |= this->regs.IR;
        }
        this->state.op[opnum].ext_words = v;  // Set the variable with the result
        this->state.operands.state[(opnum == 0) ? M68kOS_prefetch1 : M68kOS_prefetch2] = 0;
        this->state.operands.TCU = 0;
        u32 s = this->state.operands.state[(opnum == 0) ? M68kOS_read1 : M68kOS_read2];
        if (this->state.operands.state[s] == 0) {
            this->state.op[opnum].val = v;
        }
        return;
    }
    M68k_start_read(this, this->regs.PC, this->state.op[opnum].ext_words << 1, MAKE_FC(1), M68kS_read_operands);
    this->regs.PC += (this->state.op[opnum].ext_words << 1); // Add to PC
    this->state.operands.TCU = 1;
}

static void M68k_read_operands_read(struct M68k* this, u32 opnum)
{
    // get the EA
    if (this->state.operands.TCU == 1) { // read is done
        this->state.op[opnum].val = this->state.bus_cycle.data;
        this->state.operands.state[(opnum == 0) ? M68kOS_read1 : M68kOS_read2] = 0;
        this->state.operands.TCU = 0;
        return;
    }

    // setup read
    this->state.op[opnum].addr = M68k_read_ea_addr(this, opnum == 0 ? &this->ins->ea1 : &this->ins->ea2, this->ins->sz,
                                                    (opnum == 1) ? this->state.operands.hold : 0,
                                                   this->state.op[opnum].ext_words);
    M68k_start_read(this, this->state.op[opnum].addr, this->ins->sz, MAKE_FC(1), M68kS_read_operands);
    this->state.operands.TCU = 1;
}

static void M68k_read_operands(struct M68k* this) {
    i32 state = -1;
    for (u32 i = 0; i < 6; i++) {
        if (this->state.operands.state[i] > 0) {
            state = (i32)i;
            break;
        }
    }
    if (state == -1) {
        this->state.current = this->state.operands.next_state;
        return;
    }
    u32 r;
    switch(state) {
        case M68kOS_read1:
            return M68k_read_operands_read(this, 0);
        case M68kOS_read2:
            return M68k_read_operands_read(this, 1);
        case M68kOS_prefetch1:
            return M68k_read_operands_prefetch(this, 0);
        case M68kOS_prefetch2:
            return M68k_read_operands_prefetch(this, 1);
        case M68kOS_pause1:
        case M68kOS_pause2:
            r = (this->state.operands.state[M68kOS_pause1] > 0) ? M68kOS_pause1 : M68kOS_pause2;
            this->state.operands.TCU++;
            if (this->state.operands.TCU == 3) {
                this->state.operands.TCU = 0;
                this->state.operands.state[r] = 0;
            }
            return;
        default:
            assert(1==0);
    }
}

#define EXC_WRITE(val, n, desc) M68k_start_write(this, this->state.exception.group0.base_addr+n, val, 2, M68k_FC_supervisor_data, M68kS_exc_group0)

static void M68k_exc_group1(struct M68k* this)
{
    switch(this->state.exception.group1.TCU) {
        case 0: {
                // SR
                // PCH
                // PCL
                M68k_dec_SSP(this, 6);
                this->state.exception.group1.base_addr = M68k_get_SSP(this);
                this->state.exception.group1.PC = this->regs.PC-2;
                this->state.exception.group1.SR = M68k_get_SR(this);
                M68k_start_write(this, this->state.exception.group1.base_addr+4, this->state.exception.group1.PC & 0xFFFF, 2, MAKE_FC(0), M68kS_exc_group1);
                break;
            case 1:
                M68k_start_write(this, this->state.exception.group1.base_addr, this->state.exception.group1.SR, 2, MAKE_FC(0), M68kS_exc_group1);
                break;
            case 2:
                M68k_start_write(this, this->state.exception.group1.base_addr+2, this->state.exception.group1.PC >> 16, 2, MAKE_FC(0), M68kS_exc_group1);
                break;
            case 3:
                M68k_start_read(this, this->state.exception.group1.vector << 2, 4, MAKE_FC(0), M68kS_exc_group1);
                break;
            case 4:
                this->regs.PC = this->state.bus_cycle.data;
                M68k_start_prefetch(this, 2, 1, M68kS_decode);
                break;
        }
    }
    this->state.exception.group1.TCU++;
}

static void M68k_exc_group2(struct M68k* this)
{
    switch(this->state.exception.group2.TCU) {
        case 0: {
            // SR
            // PCH
            // PCL
            M68k_dec_SSP(this, 6);
            this->state.exception.group2.base_addr = M68k_get_SSP(this);
            this->state.exception.group2.SR = M68k_get_SR(this);
            M68k_start_write(this, this->state.exception.group2.base_addr+4, this->state.exception.group2.PC & 0xFFFF, 2, MAKE_FC(0), M68kS_exc_group2);
            break;
        case 1:
            M68k_start_write(this, this->state.exception.group2.base_addr, this->state.exception.group2.SR, 2, MAKE_FC(0), M68kS_exc_group2);
            break;
        case 2:
            M68k_start_write(this, this->state.exception.group2.base_addr+2, this->state.exception.group2.PC >> 16, 2, MAKE_FC(0), M68kS_exc_group2);
            break;
        case 3:
            M68k_start_read(this, this->state.exception.group2.vector << 2, 4, MAKE_FC(0), M68kS_exc_group2);
            break;
        case 4:
            this->regs.PC = this->state.bus_cycle.data;
            M68k_start_prefetch(this, 2, 1, M68kS_decode);
            break;
        }
    }
    this->state.exception.group2.TCU++;
}

static void M68k_exc_group0(struct M68k* this)
{
    switch(this->state.exception.group0.TCU) {
        case 0: { // Save variables and write first word (FC, I/N, R/W). set S, unset T. write PC lo
            this->state.exception.group0.addr = this->state.bus_cycle.addr;
            this->state.exception.group0.IR = this->regs.IRD;
            this->state.exception.group0.SR = M68k_get_SR(this);
            this->state.exception.group0.PC = this->regs.PC - 4; // TODO: should be in vicinity even if there's a jump somehow
            M68k_dec_SSP(this, 14);
            this->state.exception.group0.base_addr = M68k_get_SSP(this);
            this->state.exception.group0.first_word = (this->pins.FC) | ((this->state.exception.group0.normal_state ^ 1) << 3) | ((this->pins.RW ^ 1) << 4);
            this->state.exception.group0.first_word |= (this->regs.IRD & 0xFFE0);
            M68k_set_SR(this, (M68k_get_SR(this) | 0x2000) & 0x7FFF); // set bit 13 (S)
            // 2046   +12 PC lo
            // 2042   +8  SR
            // 2044   +10 PC hi
            // 2040   +6  IR
            // 2038   +4  access lo
            // 2034   +0  first word
            // 2036   +2  access hi
            //
            EXC_WRITE(this->state.exception.group0.PC & 0xFFFF, 12, "PC LO");
            break; }
        case 1: { // write SR
            EXC_WRITE(this->state.exception.group0.SR, 8, "SR");
            break; }
        case 2: { // write PC hi
            EXC_WRITE(this->state.exception.group0.PC >> 16, 10, "PC HI");
            break; }
        case 3: { // write IR
            EXC_WRITE(this->state.exception.group0.IR, 6, "IR");
            break; }
        case 4: { // write addr lo
            EXC_WRITE(this->state.exception.group0.addr & 0xFFFF, 4, "ADDR LO");
            break; }
        case 5: { // write first word
            EXC_WRITE(this->state.exception.group0.first_word, 0, "FIRST WORD");
            break; }
        case 6: { // write addr hi
            EXC_WRITE(this->state.exception.group0.addr >> 16, 2, "ADDR HI");
            break; }
        case 7: { // read vector
            M68k_start_read(this, this->state.exception.group0.vector << 2, 4, M68k_FC_supervisor_data, M68kS_exc_group0);
            break; }
        case 8: {
            //M68k_start_wait(this, 2, M68kS_exc_group0);
            break; }
        case 9: { // Load prefetches and go!
            this->regs.PC = this->state.bus_cycle.data;
            M68k_start_prefetch(this, 2, 1, M68kS_decode);
            break; }
    }
    this->state.exception.group0.TCU++;
}

static void M68k_trace_format(struct M68k* this)
{
    jsm_string_quickempty(&this->trace.str);
    M68k_disassemble(this->regs.PC-2, this->regs.IR, &this->trace.strct, &this->trace.str);
    printf("\nM68k(%04llu)  %06x  %s", this->trace_cycles, this->regs.PC-4, this->trace.str.ptr);
}

void M68k_cycle(struct M68k* this)
{
    //printf("\n\nNew cycle %d", this->trace_cycles);
    u32 quit = 0;
    while (!quit) {
        // only functions that cause "work" (i.e. cycles to pass) cause a quit.
        // so waiting for cycles, or doing bus transactions.
        switch(this->state.current) {
            case M68kS_exc_group0: {
                M68k_exc_group0(this);
                break; }
            case M68kS_exc_group1: {
                M68k_exc_group1(this);
                break; }
            case M68kS_exc_group2: {
                M68k_exc_group2(this);
                break; }
            case M68kS_decode: {
                if (this->state.exception.group1_pending) {
                    M68k_start_group1_exception(this, this->state.exception.group1.vector, this->state.exception.group1.wait_cycles);
                    continue;
                }
                this->state.exception.group1_pending = 0;
                if (this->testing && this->trace_cycles > 0) {
                    this->ins_decoded = 1;
                    return;
                }
                M68k_decode(this);
                this->ins_decoded = 1;
                this->state.current = M68kS_exec;
                if (dbg.trace_on && this->trace.ok) {
                    M68k_trace_format(this);
                }
                break; }
            case M68kS_exec: {
                this->ins->exec(this, this->ins);
                if (this->state.instruction.done) {
                    this->state.current = M68kS_decode;
                }
                break; }
            case M68kS_prefetch: {
                M68k_prefetch(this);
                break; }
            case M68kS_read_operands: {
                M68k_read_operands(this);
                break; }
            case M68kS_read8:
            case M68kS_read16:
            case M68kS_read32:
            case M68kS_write8:
            case M68kS_write16:
            case M68kS_write32: {
                this->state.bus_cycle.func(this);
                quit = 1;
                break; }
            case M68kS_wait_cycles: { // exit on end of waiting cycles
                if (this->state.wait_cycles.cycles_left <= 0) {
                    this->state.current = this->state.wait_cycles.next_state;
                }
                else {
                    quit = 1;
                    this->state.wait_cycles.cycles_left--;
                }
                break; }
            default:
                assert(1==0);
        }
    }
    this->trace_cycles++;
}

void M68k_reset(struct M68k* this)
{
    this->state.current = M68kS_exec;
    this->state.instruction.done = 0;
    this->ins = &this->SPEC_RESET;
    this->state.instruction.TCU = 0;
    this->state.instruction.prefetch = 0;
}