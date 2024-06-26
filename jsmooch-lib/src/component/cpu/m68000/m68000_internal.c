//
// Created by . on 6/20/24.
//
#include <stdio.h>
#include <assert.h>

#include "m68000.h"
#include "m68000_instructions.h"
#include "m68000_internal.h"

#define ADDRAND1 { this->pins.LDS = (this->state.bus_cycle.addr & 1); this->pins.UDS = this->pins.LDS ^ 1; }
static u32 clip32[5] = { 0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF};

static u32 get_dr(struct M68k* this, u32 num, u32 sz)
{
    return this->regs.D[num] & clip32[sz];
}

static u32 get_ar(struct M68k* this, u32 num, u32 sz)
{
    u32 v = this->regs.A[num];
    switch(sz) {
        case 1:
            return (u32)(i32)(i8)v;
        case 2:
            return (u32)(i32)(i16)v;
        case 4:
            return (u32)(i32)v;
    }
    assert(1==0);
}

static u32 get_r(struct M68k* this, struct M68k_EA *ea, u32 sz)
{
    u32 v;
    switch(ea->kind) {
        case M68k_AM_data_register_direct:
            return get_dr(this, ea->reg, sz);
        case M68k_AM_address_register_direct:
            return get_ar(this, ea->reg, sz);
        case M68k_AM_quick_immediate:
        case M68k_AM_immediate:
            v = ea->reg;
            break;
        default:
            assert(1==0);
            break;
    }
    switch(sz) {
        case 1:
            return v & 0xFF;
        case 2:
            return v & 0xFFFF;
        case 4:
            return v;
        default:
            assert(1==0);
    }
    return 0;
}

static u32 am_in_group0_or_1(struct M68k* this)
{
    if (this->state.bus_cycle.next_state == M68kS_exc_group0) return 1;
    // TODO: detect group 1
    return 0;
}

void M68k_bus_cycle_read(struct M68k* this)
{
    switch(this->state.bus_cycle.TCU & 3) {
        case 0:
            this->pins.FC = this->state.bus_cycle.FC;
            this->pins.RW = 0;
            this->state.bus_cycle.done = 0;
            if (this->state.current == M68kS_read8) {
                ADDRAND1;
            }
            else {
                if (this->state.bus_cycle.addr & 1) {
                    M68k_start_group0_exception(this, M68kIV_address_error, 3, am_in_group0_or_1(this));
                    return;
                }
                this->pins.LDS = this->pins.UDS = 1;
            }
            this->pins.Addr = this->state.bus_cycle.addr & 0xFFFFFE;
            //if (this->state.current == M68kS_read32) this->state.bus_cycle.addr += 2;
            break;
        case 1:
            this->pins.AS = 1;
            break;
        case 2:
            if (!this->pins.DTACK) // insert wait state
                this->state.bus_cycle.TCU--;
            break;
        case 3: // latch data and de-assert pins
            this->pins.DTACK = 0;
            this->pins.AS = 0;
            if (this->state.current == M68kS_read32) {
                if (this->state.bus_cycle.TCU == 3) { // We have another go-round left!
                    this->state.bus_cycle.data = this->pins.D << 16;
                    this->state.bus_cycle.addr = (this->state.bus_cycle.addr + 2) & 0xFFFFFF;
                }
                else this->state.bus_cycle.data |= this->pins.D;
            }

            else if (this->state.current == M68kS_read16) {
                this->state.bus_cycle.data = this->pins.D;
            }
            else {
                if (this->pins.LDS) this->state.bus_cycle.data = this->pins.D & 0xFF;
                else this->state.bus_cycle.data = this->pins.D >> 8;
            }
            this->pins.Addr = 0;
            this->pins.LDS = this->pins.UDS = 0;
            if ((this->state.current != M68kS_read32) || (this->state.bus_cycle.TCU == 7)) {
                this->state.bus_cycle.done = 1;
                this->state.bus_cycle.TCU = -1;
                this->state.current = this->state.bus_cycle.next_state;
            }
            break;
    }
    this->state.bus_cycle.TCU++;
}

void M68k_start_group0_exception(struct M68k* this, u32 vector_number, i32 wait_cycles, u32 was_in_group0_or_1)
{
    if (dbg.trace_on) printf("\nSTART GROUP0! cyc:%lld addr:%06x", this->trace_cycles, this->state.bus_cycle.addr);
    this->state.current = M68kS_exc_group0;
    this->state.exception.group0.vector = vector_number;
    this->state.exception.group0.TCU = 0;
    this->state.exception.group0.normal_state = !was_in_group0_or_1;
    if (wait_cycles > 0)
        M68k_start_wait(this, wait_cycles, M68kS_exc_group0);
    // SR cooy
    // supervisor enter
    // tracing off

}

void M68k_start_group1_exception(struct M68k* this, u32 vector_number, i32 wait_cycles)
{
    if (dbg.trace_on) printf("\nSTART GROUP1! cyc:%lld", this->trace_cycles);
    this->state.current = M68kS_exc_group1;
    this->state.exception.group1.TCU = 0;
    this->state.exception.group1.vector = vector_number;

    if (wait_cycles > 0)
        M68k_start_wait(this, wait_cycles, M68kS_exc_group1);
}

void M68k_start_group2_exception(struct M68k* this, u32 vector_number, i32 wait_cycles, u32 PC)
{
    if (dbg.trace_on) printf("\nSTART GROUP2! cyc:%lld", this->trace_cycles);
    this->state.current = M68kS_exc_group2;
    this->state.exception.group2.TCU = 0;
    this->state.exception.group2.vector = vector_number;
    this->state.exception.group2.PC = PC;

    if (wait_cycles > 0)
        M68k_start_wait(this, wait_cycles, M68kS_exc_group2);
}

// this is duplicated in m68000_opcodes.c
#define ADDRAND1 { this->pins.LDS = (this->state.bus_cycle.addr & 1); this->pins.UDS = this->pins.LDS ^ 1; }

void M68k_bus_cycle_write(struct M68k* this)
{
    switch(this->state.bus_cycle.TCU & 3) {
        case 0: {
            this->pins.FC = this->state.bus_cycle.FC;
            this->pins.RW = 0;
            this->state.bus_cycle.done = 0;
            if ((this->state.current != M68kS_write8) && ((this->state.bus_cycle.addr & 1))) {
                M68k_start_group0_exception(this, M68kIV_address_error, 0, am_in_group0_or_1(this));
                return;
            }
            this->pins.Addr = this->state.bus_cycle.addr & 0xFFFFFE;
            break; }
        case 1: {
            this->pins.AS = 1;
            this->pins.RW = 1;
            // technically this happens at rising edge of next cycle, buuuut, we're doing it here
            if (this->state.current == M68kS_write32) {
                if (this->state.bus_cycle.TCU == 1) { // first write
                    this->pins.D = this->state.bus_cycle.data >> 16;
                    this->state.bus_cycle.addr = (this->state.bus_cycle.addr + 2) & 0xFFFFFE;
                }
                else this->pins.D = this->state.bus_cycle.data & 0xFFFF; // second write
                this->pins.LDS = this->pins.UDS = 1;
            }
            else if (this->state.current == M68kS_write16) {
                this->pins.LDS = this->pins.UDS = 1;
                this->pins.D = this->state.bus_cycle.data & 0XFFFF;
            }
            else {
                ADDRAND1;
                if (this->pins.LDS) this->pins.D = this->state.bus_cycle.data;
                else this->pins.D = this->state.bus_cycle.data << 8;
            }
            break; }
        case 2: {
            // This is annoying
            if (!this->pins.DTACK) // insert wait state
                this->state.bus_cycle.TCU--;
            break; }
        case 3: {// latch data and de-assert pins
            this->pins.DTACK = 0;
            this->pins.AS = 0;
            this->pins.LDS = this->pins.UDS = 0;
            if ((this->state.current != M68kS_write32) || (this->state.bus_cycle.TCU == 7)) {
                this->state.bus_cycle.done = 1;
                this->state.bus_cycle.TCU = -1;
                this->state.current = this->state.bus_cycle.next_state;
            }
            break; }
    }
    this->state.bus_cycle.TCU++;
}

void M68k_start_prefetch(struct M68k* this, u32 num, u32 is_program, u32 next_state)
{
    this->state.current = M68kS_prefetch;
    this->state.prefetch.num = (i32)num;
    this->state.prefetch.TCU = 0;
    this->state.prefetch.FC = MAKE_FC(is_program);
    this->state.prefetch.next_state = next_state;
}

void M68k_start_read(struct M68k* this, u32 addr, u32 sz, u32 FC, u32 next_state)
{
    switch(sz) {
        case 1:
            this->state.current = M68kS_read8;
            break;
        case 2:
            this->state.current = M68kS_read16;
            break;
        case 4:
            this->state.current = M68kS_read32;
            break;
        default:
            assert(1==0);
    }
    this->state.bus_cycle.TCU = 0;
    this->state.bus_cycle.addr = addr;
    this->state.bus_cycle.func = &M68k_bus_cycle_read;
    this->state.bus_cycle.next_state = next_state;
    this->state.bus_cycle.FC = FC;
}

void M68k_start_write(struct M68k* this, u32 addr, u32 val, u32 sz, u32 FC, u32 next_state)
{
    switch(sz) {
        case 1:
            this->state.current = M68kS_write8;
            break;
        case 2:
            this->state.current = M68kS_write16;
            break;
        case 4:
            this->state.current = M68kS_write32;
            break;
        default:
            assert(1==0);
    }
    this->state.bus_cycle.TCU = 0;
    this->state.bus_cycle.addr = addr;
    this->state.bus_cycle.func = &M68k_bus_cycle_write;
    this->state.bus_cycle.data = val;
    this->state.bus_cycle.FC = FC;
    this->state.bus_cycle.next_state = next_state;
}

void M68k_set_dr(struct M68k* this, u32 num, u32 result, u32 sz)
{
    switch(sz) {
        case 1:
            this->regs.D[num] = (this->regs.D[num] & 0xFFFFFF00) | (result & 0xFF);
            return;
        case 2:
            this->regs.D[num] = (this->regs.D[num] & 0xFFFF0000) | (result & 0xFFFF);
            return;
        case 4:
            this->regs.D[num] = result;
            return;
        default:
            assert(1==0);
    }
}

void M68k_set_ar(struct M68k* this, u32 num, u32 result, u32 sz)
{
    switch(sz) {
        case 1:
            this->regs.A[num] = (u32)(i32)(i8)(result & 0xFF);
            return;
        case 2:
            this->regs.A[num] = (u32)(i32)(result & 0xFFFF);
            return;
        case 4:
            this->regs.A[num] = result;
            return;
    }
}

u32 M68k_write_ea_addr(struct M68k* this, struct M68k_EA *ea, u32 sz, u32 hold, u32 opnum)
{
    switch(ea->kind) {
        case M68k_AM_address_register_indirect:
        case M68k_AM_address_register_indirect_with_postincrement:
        case M68k_AM_address_register_indirect_with_predecrement:
        case M68k_AM_address_register_indirect_with_displacement:
        case M68k_AM_address_register_indirect_with_index:
        case M68k_AM_absolute_short_data:
        case M68k_AM_absolute_long_data:
        case M68k_AM_program_counter_with_displacement:
        case M68k_AM_program_counter_with_index:
            return this->state.op[opnum].addr;
        default:
            assert(1==0);
    }
}

void M68k_start_write_operand(struct M68k* this, u32 hold, u32 op_num, u32 next_state)
{
    this->state.operands.hold = hold;
    this->state.operands.TCU = 0;
    this->state.operands.cur_op_num = op_num;
    struct M68k_EA *ea = (op_num == 0) ? &this->ins->ea1 : &this->ins->ea2;
    this->state.operands.ea = ea;
    this->state.current = M68kS_write_operand;
    u32 no_work = 0;
    u32 addr = 0;
    switch(this->state.operands.ea->kind) {
        case M68k_AM_data_register_direct:
            M68k_set_dr(this, ea->reg, this->state.instruction.result, this->ins->sz);
            no_work = 1;
            break;
        case M68k_AM_address_register_direct:
            M68k_set_ar(this, ea->reg, this->state.instruction.result, this->ins->sz);
            no_work = 1;
            break;
        default:
            addr = M68k_write_ea_addr(this, (op_num==0) ? &this->ins->ea1 : &this->ins->ea2, this->ins->sz, hold, op_num);
            break;
    }
    if (no_work) {
        this->state.current = next_state;
        return;
    }
    M68k_start_write(this, addr, this->state.instruction.result, this->ins->sz, MAKE_FC(0), next_state);
}

static u32 ext_words(enum M68k_address_modes am, u32 sz)
{
    switch(am) {
        case M68k_AM_data_register_direct:
        case M68k_AM_address_register_direct:
        case M68k_AM_address_register_indirect:
        case M68k_AM_address_register_indirect_with_postincrement:
        case M68k_AM_address_register_indirect_with_predecrement:
            return 0;
        case M68k_AM_address_register_indirect_with_displacement:
        case M68k_AM_address_register_indirect_with_index:
        case M68k_AM_absolute_short_data:
        case M68k_AM_program_counter_with_displacement:
        case M68k_AM_program_counter_with_index:
        case M68k_AM_imm16:
            return 1;
        case M68k_AM_absolute_long_data:
            return 2;
        case M68k_AM_immediate:
            return sz >> 1;
        default:
            assert(1==0);
    }
}

static void eval_ea1_wait(struct M68k* this, u32 num_ea)
{
    struct M68k_EA *ea = &this->ins->ea1;
    this->state.op[0].ext_words = ext_words(ea->kind, this->ins->sz);
    this->state.operands.state[M68kOS_prefetch1] = this->state.op[0].ext_words != 0;
    if ((ea->kind == M68k_AM_address_register_indirect_with_predecrement) ||
            (ea->kind == M68k_AM_program_counter_with_index) ||
            (ea->kind == M68k_AM_address_register_indirect_with_index)) {
        this->state.operands.state[M68kOS_pause1] = 1;
    }
}

static void eval_ea2_wait(struct M68k* this, u32 num_ea)
{
    struct M68k_EA *ea = &this->ins->ea2;
    this->state.op[1].ext_words = ext_words(ea->kind, this->ins->sz);
    this->state.operands.state[M68kOS_prefetch2] = this->state.op[1].ext_words != 0;
    if ((ea->kind == M68k_AM_program_counter_with_index) ||
        (ea->kind == M68k_AM_address_register_indirect_with_index)) {
        this->state.operands.state[M68kOS_pause2] = 1;
    }
    if ((ea->kind == M68k_AM_address_register_indirect_with_predecrement) && (num_ea==2) && (!this->state.operands.fast)) {
        this->state.operands.state[M68kOS_pause2] = 1;
    }
    else if ((num_ea == 1) && (ea->kind == M68k_AM_address_register_indirect_with_predecrement))
        this->state.operands.state[M68kOS_pause2] = 1;
}

void M68k_swap_ASP(struct M68k* this)
{
    u32 x = this->regs.A[7];
    this->regs.A[7] = this->regs.ASP;
    this->regs.ASP = x;
}

static u32 is_immediate(enum M68k_address_modes am)
{
    switch(am) {
        case M68k_AM_imm16:
        case M68k_AM_imm32:
        case M68k_AM_immediate:
            return 1;
        default:
            return 0;
    }
}

void M68k_start_read_operands(struct M68k* this, u32 fast, u32 hold, u32 next_state)
{
    this->state.current = M68kS_read_operands;
    this->state.operands.TCU = 0;
    this->state.operands.state[M68kOS_read1] = this->state.operands.state[M68kOS_read2] = this->state.operands.state[M68kOS_prefetch1] = this->state.operands.state[M68kOS_prefetch2] = this->state.operands.state[M68kOS_pause1] = this->state.operands.state[M68kOS_pause2] = 0;
    this->state.operands.fast = fast;
    this->state.operands.hold = hold;
    this->state.operands.reverse = 1;
    this->state.op[0].ext_words = this->state.op[1].ext_words = 0;
    this->state.operands.next_state = next_state;
    u32 no_fetches = 0;
    struct M68k_ins_t *ins = this->ins;
    switch(this->ins->operand_mode) {
        case M68k_OM_none:
            no_fetches = 1;
            break;
        case M68k_OM_qimm:
            this->state.op[0].val = ins->ea1.reg;
            no_fetches = 1;
            break;
        case M68k_OM_qimm_qimm:
            this->state.op[0].val = ins->ea1.reg;
            this->state.op[1].val = ins->ea2.reg;
            no_fetches = 1;
            break;
        case M68k_OM_qimm_r:
            this->state.op[0].val = ins->ea1.reg;
            no_fetches = 1;
            break;
        case M68k_OM_r:
            this->state.op[0].val = get_r(this, &ins->ea1, ins->sz);
            no_fetches = 1;
            break;
        case M68k_OM_r_r:
            this->state.op[0].val = get_r(this, &ins->ea1, ins->sz);
            this->state.op[1].val = get_r(this, &ins->ea2, ins->sz);
            no_fetches = 1;
            break;
        case M68k_OM_ea_r:
            this->state.operands.state[M68kOS_prefetch1] = 1;
            if (!is_immediate(this->ins->ea1.kind)) this->state.operands.state[M68kOS_read1] = 1;
            this->state.op[1].val = get_r(this, &ins->ea2, ins->sz);
            eval_ea1_wait(this, 1);
            break;
        case M68k_OM_r_ea:
            this->state.op[0].val = get_r(this, &ins->ea1, ins->sz);
            if (!is_immediate(this->ins->ea2.kind)) this->state.operands.state[M68kOS_read2] = 1;
            this->state.operands.state[M68kOS_prefetch2] = 1;
            eval_ea2_wait(this, 1);
            break;
        case M68k_OM_ea: {
            if (!is_immediate(this->ins->ea1.kind)) this->state.operands.state[M68kOS_read1] = 1;
            this->state.operands.state[M68kOS_prefetch1] = 1;
            eval_ea1_wait(this, 1);
        }
            break;
        case M68k_OM_ea_ea:
            if (!is_immediate(this->ins->ea1.kind)) this->state.operands.state[M68kOS_read1] = 1;
            if (!is_immediate(this->ins->ea2.kind)) this->state.operands.state[M68kOS_read2] = 1;
            eval_ea1_wait(this, 2);
            eval_ea2_wait(this, 2);
            break;
        case M68k_OM_qimm_ea:
            this->state.operands.state[M68kOS_read2] = 1;
            this->state.operands.state[M68kOS_prefetch2] = 1;
            this->state.op[0].val = ins->ea1.reg;
            eval_ea1_wait(this, 1);
            break;
        case M68k_OM_imm16:
            this->state.operands.state[M68kOS_read1] = 0;
            this->state.operands.state[M68kOS_prefetch1] = 1;
            eval_ea1_wait(this, 1);
        default:
            assert(1==0);
    }
    if (no_fetches) {
        this->state.current = next_state;
        return;
    }
}

void M68k_start_wait(struct M68k* this, u32 num, u32 state_after) {
    this->state.current = M68kS_wait_cycles;
    this->state.wait_cycles.cycles_left = (i32)num;
    this->state.wait_cycles.next_state = state_after;
}

void M68k_inc_SSP(struct M68k* this, u32 num) {
    u32 *r = this->regs.SR.S ? &this->regs.A[7] : &this->regs.ASP;
    *r += num;
}

void M68k_dec_SSP(struct M68k* this, u32 num) {
    u32 *r = this->regs.SR.S ? &this->regs.A[7] : &this->regs.ASP;
    *r -= num;
}

u32 M68k_get_SSP(struct M68k* this) {
    if (this->regs.SR.S) return this->regs.A[7];
    return this->regs.ASP;
}

