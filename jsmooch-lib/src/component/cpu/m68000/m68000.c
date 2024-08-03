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

//#define BREAKPOINT 0x40016e

void M68k_disasm_RESET_POWER(struct M68k_ins_t *ins, u32 PC, struct jsm_debug_read_trace *rt, struct jsm_string *out);
void M68k_init(struct M68k* this, u32 megadrive_bug)
{
    memset(this, 0, sizeof(struct M68k));
    do_M68k_decode();
    this->SPEC_RESET.sz = 4;
    this->SPEC_RESET.operand_mode = M68k_OM_none;
    this->SPEC_RESET.disasm = &M68k_disasm_RESET_POWER;
    this->SPEC_RESET.exec = &M68k_ins_RESET_POWER;
    this->megadrive_bug = megadrive_bug;
    //this->regs.D[0] = this->regs.A[0] = 0xFFFFFFFF;
    jsm_string_init(&this->trace.str, 1000);
}

void M68k_setup_tracing(struct M68k* this, struct jsm_debug_read_trace *strct, u64 *trace_cycle_pointer)
{
    this->trace.strct.read_trace_m68k = strct->read_trace_m68k;
    this->trace.strct.ptr = strct->ptr;
    this->trace.strct.read_trace = strct->read_trace;
    this->trace.ok = 1;
    this->trace.cycles = trace_cycle_pointer;
}

void M68k_delete(struct M68k* this)
{
    jsm_string_delete(&this->trace.str);
}

static void M68k_decode(struct M68k* this)
{
    u32 IRD = this->regs.IR;
    this->regs.IRD = IRD;
#ifdef BREAKPOINT
    if ((((this->regs.PC-4) & 0xFFFFFF) == BREAKPOINT) && (!dbg.did_breakpoint)) {
        dbg_break("M68K PC BREAKPOINT", *this->trace.cycles);
        dbg.did_breakpoint =1;
        //printf("\nBREAK FOR WAIT ON INTERRUPT!! cycle:%d", *this->trace.cycles);
    }
#endif
    struct M68k_ins_t *ins = &M68k_decoded[IRD & 0xFFFF];
    this->state.instruction.TCU = 0;
    this->ins = ins;
    this->state.instruction.done = 0;
    this->state.instruction.prefetch = 1; // 1 prefetches are needed currently
}

void M68k_set_SR(struct M68k* this, u32 val, u32 immediate_t)
{
    u32 new_s = (val & 0x2000) >> 13;
    // CCR
    //this->regs.SR.u = val & 0xA71F; //(this->regs.SR.u & 0xFFE0) | (val & 0x1F);
    this->regs.SR.u = (this->regs.SR.u & 0xFFE0) | (val & 0x1F);

    if (new_s != this->regs.SR.S) {
        M68k_swap_ASP(this);
    }
    this->regs.SR.S = new_s;

    this->regs.SR.I = (val >> 8) & 7;
    this->regs.next_SR_T = (val >> 15) & 1;
    if (immediate_t || (this->regs.next_SR_T == 0)) this->regs.SR.T = this->regs.next_SR_T;
}

static void pprint_ea(struct M68k* this, u32 opnum)
{
    struct M68k_ins_t *ins = this->ins;
    struct M68k_EA *ea = &ins->ea[opnum];
    u32 kind = 0; // 0 = NONE, 1 = addr reg, 2 = data reg, 3 = EA
    if (opnum == 0) {
        switch(ins->operand_mode) {
            case M68k_OM_none:
            case M68k_OM_qimm:
                break;
            case M68k_OM_ea_r:
            case M68k_OM_ea:
            case M68k_OM_ea_ea:
                if (ea->kind == M68k_AM_data_register_direct) kind = 2;
                else if (ea->kind == M68k_AM_address_register_indirect) kind = 1;
                else kind = 3;
                break;
            case M68k_OM_r:
            case M68k_OM_r_ea:
            case M68k_OM_r_r:
                if (ea->kind == M68k_AM_data_register_direct)
                    kind = 2;
                else
                    kind = 1;
                break;
            case M68k_OM_qimm_qimm:
            case M68k_OM_qimm_r:
            case M68k_OM_qimm_ea:
                break;
            default:
                printf("\nFORGOT DISASM1 FOR OM %d", ins->operand_mode);
                break;
        }
    }
    else {
        switch(ins->operand_mode) {
            case M68k_OM_r:
            case M68k_OM_ea:
            case M68k_OM_none:
                break;
            case M68k_OM_r_r:
            case M68k_OM_ea_r:
            case M68k_OM_qimm_r:
                if (ea->kind == M68k_AM_data_register_direct)
                    kind = 2;
                else if (ea->kind == M68k_AM_address_register_direct)
                    kind = 1;
                else
                    assert(1==2);
                break;
            case M68k_OM_r_ea:
            case M68k_OM_qimm_ea:
            case M68k_OM_ea_ea:
                if (ea->kind == M68k_AM_data_register_direct) kind = 2;
                else if (ea->kind == M68k_AM_address_register_indirect) kind = 1;
                else kind = 3;
                break;
            case M68k_OM_qimm:
            case M68k_OM_qimm_qimm:
                break;
            default:
                printf("\nFORGOT DISASM2 FOR OM %d", ins->operand_mode);
                break;
        }
    }
    if (kind == 3) {
        switch(ea->kind) {
            case M68k_AM_address_register_indirect_with_predecrement:
            case M68k_AM_address_register_indirect:
            case M68k_AM_address_register_indirect_with_postincrement:
            case M68k_AM_address_register_indirect_with_displacement:
            case M68k_AM_address_register_indirect_with_index:
            case M68k_AM_address_register_direct:
                kind = 1; // addr reg
                break;
            default:
                kind = 0;
                break;
        }
    }
    switch(kind) {
        case 0:
            break;
        case 1: // addr reg
            dbg_printf("A%d:%08x", ea->reg, this->regs.A[ea->reg]);
            break;
        case 2: // data reg
            dbg_printf("D%d:%08x", ea->reg, this->regs.D[ea->reg]);
    }
    if ((kind != 0) && (opnum == 0)) dbg_printf("  ");
}

static void M68k_trace_format(struct M68k* this)
{
    jsm_string_quickempty(&this->trace.str);
    M68k_disassemble(this->regs.PC-2, this->regs.IR, &this->trace.strct, &this->trace.str);
    u64 tc;
    if (this->trace.cycles == 0) tc = 0;
    else tc = *this->trace.cycles >> 1;
#ifdef  M68K_TESTING
    printf(DBGC_M68K "\nM68k  (%04llu)   %06x  %s" DBGC_RST, tc, this->regs.PC-4, this->trace.str.ptr);
#else
    dbg_printf(DBGC_M68K "\nM68k  (%04llu)   %06x  %s", tc, this->regs.PC-4, this->trace.str.ptr);
    dbg_seek_in_line(TRACE_BRK_POS);
    dbg_printf("SR:%04x  ", this->regs.SR.u);
    pprint_ea(this, 0);
    pprint_ea(this, 1);
    dbg_printf(DBGC_RST);
#endif
}

void M68k_set_interrupt_level(struct M68k* this, u32 val)
{
    if ((this->pins.IPL < 7) && (val == 7)) {
        this->state.nmi = 1;
    }
    this->pins.IPL = val;
}

static u32 M68k_process_interrupts(struct M68k* this)
{
    if (this->state.exception.interrupt.on_next_instruction) {
        printf("\nM68K IRQFIRE cyc:%lld", *this->trace.cycles);
        this->state.exception.interrupt.on_next_instruction = 0;
        this->state.exception.interrupt.PC = this->regs.PC - 4;
        this->state.exception.interrupt.TCU = 0;
        this->state.exception.interrupt.new_I = this->pins.IPL;
        this->state.current = M68kS_exc_interrupt;
        //printf("\nM68K IRQ FIRE! cyc:%lld", *this->trace.cycles);
        if (dbg.trace_on && dbg.traces.m68000.irq) {
            dbg_printf(DBGC_M68K "\n M68K  (%06llu)  !!!!    INTERRUPT level:%d!" DBGC_RST, *this->trace.cycles, this->state.exception.interrupt.new_I);
        }
        if (dbg.breaks.m68000.irq) {
            dbg_break("M68K IRQ FIRE", *this->trace.cycles);
        }
        return 1;
    }
    return 0;
}

static void lycoder_pprint2(struct M68k* this)
{
    // d:FFFFFFFF FFFFFFFF B6DB6DB6 DB6DB6DB 6DB6DB6D 00000000 00000000 00000000
    dbg_printf(" d:%08X %08X %08X %08X %08X %08X %08X %08X",
               this->regs.D[0], this->regs.D[1], this->regs.D[2], this->regs.D[3],
               this->regs.D[4], this->regs.D[5], this->regs.D[6], this->regs.D[7]);
    // a:00600100 004001BE 004001C0 00000000 00000004 004006F8 004000F0 0000FC00
    dbg_printf(" a:%08X %08X %08X %08X %08X %08X %08X %08X",
               this->regs.A[0], this->regs.A[1], this->regs.A[2], this->regs.A[3],
               this->regs.A[4], this->regs.A[5], this->regs.A[6], this->regs.A[7]);
    // pc:4002D0 sr:2700 asp:000000
    dbg_printf(" pc:%08X sr:%04X asp:%08X\n", this->opc, this->regs.SR.u, this->regs.ASP);
}

static void lycoder_pprint1(struct M68k* this)
{
    // cyc:14742674 a:004002D0 opc:21FC
    dbg_printf("cyc:%lld a:%08X opc:%04X", *this->trace.cycles >> 1, this->regs.PC-4, this->regs.IRD);
    this->opc = this->regs.PC - 4;
}

void M68k_cycle(struct M68k* this)
{
    u32 quit = 0;
    while (!quit) {
        // only functions that cause "work" (i.e. cycles to pass) cause a quit.
        // so waiting for cycles, or doing bus transactions.
        switch(this->state.current) {
            case M68kS_bus_cycle_iaq:
                M68k_bus_cycle_iaq(this);
                quit = 1;
                break;
            case M68kS_exc_interrupt: {
                M68k_exc_interrupt(this);
                break;
            }
            case M68kS_exc_group0: {
                M68k_exc_group0(this);
                break; }
            case M68kS_exc_group12: {
                M68k_exc_group12(this);
                break; }
            case M68kS_decode: {
#ifdef M68K_TESTING
                if (this->testing && *this->trace.cycles > 0) { // FOR TESTING JSONs
                    this->ins_decoded = 1;
                    return;
                }
#endif
                if (M68k_process_interrupts(this)) break;
                M68k_decode(this);
                // This must be done AFTER interrupt, trace, etc. processing
                this->regs.SR.T = this->regs.next_SR_T;
                this->ins_decoded = 1;
                this->state.current = M68kS_exec;
#ifdef LYCODER
                //if (this->opc != 0xFFFFFFFF) lycoder_pprint2(this);
                lycoder_pprint1(this);
                lycoder_pprint2(this);
#else
                if (dbg.trace_on && this->trace.ok && dbg.traces.m68000.instruction) {
                    M68k_trace_format(this);
                }
#endif
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
            case M68kS_stopped: {
                M68k_sample_interrupts(this);
                if (this->state.exception.interrupt.on_next_instruction)
                    this->state.current = M68kS_exec;
                else
                    quit = 1;
                break; }
            default:
                assert(1==0);
        }
    }
#ifdef M68K_E_CLOCK
    this->state.e_clock_count = (this->state.e_clock_count + 1) % 10;
#endif
}

void M68k_unstop(struct M68k* this)
{
    if (this->state.current == M68kS_stopped)
        this->state.current = this->state.stopped.next_state;
}

void M68k_reset(struct M68k* this)
{
    this->state.current = M68kS_exec;
    this->state.instruction.done = 0;
    this->state.e_clock_count = 0;
    M68k_set_SR(this, (M68k_get_SR(this) & 0x1F) | 0x2700, 1);
    printf("\nSUPERVISOR? %d", this->regs.SR.S);
    this->ins = &this->SPEC_RESET;
    this->state.instruction.TCU = 0;
    this->state.instruction.prefetch = 0;
    this->opc = 0xFFFFFFFF;
}
