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
void M68k_init(struct M68k* this, u32 megadrive_bug)
{
    memset(this, 0, sizeof(struct M68k));
    do_M68k_decode();
    this->SPEC_RESET.sz = 4;
    this->SPEC_RESET.operand_mode = M68k_OM_none;
    this->SPEC_RESET.disasm = &M68k_disasm_RESET;
    this->SPEC_RESET.exec = &M68k_ins_RESET;
    this->megadrive_bug = megadrive_bug;
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
