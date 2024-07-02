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
            if ((this->state.bus_cycle.TCU == 0) && (this->state.current == M68kS_read32) && (this->state.bus_cycle.reversed)) {
                this->state.bus_cycle.addr += 2;
            }
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
                if (this->state.bus_cycle.reversed) {
                    if (this->state.bus_cycle.TCU == 3) { // We have another go-round left!
                        this->state.bus_cycle.data = this->pins.D;
                        this->state.bus_cycle.addr = (this->state.bus_cycle.addr - 2) & 0xFFFFFF;
                    } else this->state.bus_cycle.data |= (this->pins.D << 16);
                }
                else {
                    if (this->state.bus_cycle.TCU == 3) { // We have another go-round left!
                        this->state.bus_cycle.data = this->pins.D << 16;
                        this->state.bus_cycle.addr = (this->state.bus_cycle.addr + 2) & 0xFFFFFF;
                    } else this->state.bus_cycle.data |= this->pins.D;
                }
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
            if ((this->state.bus_cycle.TCU == 0) && (this->state.current == M68kS_write32) && (this->state.bus_cycle.reversed)) {
                // Do high-word first
                this->state.bus_cycle.addr += 2;
            }
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
                if (this->state.bus_cycle.reversed) {
                    if (this->state.bus_cycle.TCU == 1) { // first write
                        this->pins.D = this->state.bus_cycle.data & 0xFFFF;
                        this->state.bus_cycle.addr = (this->state.bus_cycle.addr - 2) & 0xFFFFFE;
                    } else this->pins.D = this->state.bus_cycle.data >> 16; // second write
                }
                else {
                    if (this->state.bus_cycle.TCU == 1) { // first write
                        this->pins.D = this->state.bus_cycle.data >> 16;
                        this->state.bus_cycle.addr = (this->state.bus_cycle.addr + 2) & 0xFFFFFE;
                    } else this->pins.D = this->state.bus_cycle.data & 0xFFFF; // second write
                }
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

void M68k_start_read(struct M68k* this, u32 addr, u32 sz, u32 FC, u32 reversed, u32 next_state)
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
    this->state.bus_cycle.reversed = reversed;
    this->state.bus_cycle.TCU = 0;
    this->state.bus_cycle.addr = addr;
    this->state.bus_cycle.func = &M68k_bus_cycle_read;
    this->state.bus_cycle.next_state = next_state;
    this->state.bus_cycle.FC = FC;
}

void M68k_start_write(struct M68k* this, u32 addr, u32 val, u32 sz, u32 FC, u32 reversed, u32 next_state)
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
    this->state.bus_cycle.reversed = reversed;
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

u32 M68k_read_ea_addr(struct M68k* this, struct M68k_EA *ea, u32 sz, u32 hold, u32 prefetch)
{
    u32 v, c;
    i64 a, b;
    switch(ea->kind) {
        case M68k_AM_address_register_indirect: // 0 ext word
            return this->regs.A[ea->reg];
        case M68k_AM_address_register_indirect_with_postincrement: // 0 ext word
            v = this->regs.A[ea->reg] + sz;
            c = this->regs.A[ea->reg];
            if ((ea->reg == 7) && (sz == 1)) v++;
            if (!hold) this->regs.A[ea->reg] = v;
            return c;
        case M68k_AM_address_register_indirect_with_predecrement: // 0 ext word
            v = this->regs.A[ea->reg] - sz;
            if ((ea->reg == 7) && (sz == 1)) v--;
            if (!hold) this->regs.A[ea->reg] = v;
            return v;
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

static u32 M68k_write_ea_addr(struct M68k* this, struct M68k_EA *ea, u32 sz, u32 hold, u32 opnum)
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
    this->state.operands.TCU = 0;
    this->state.operands.cur_op_num = op_num;
    struct M68k_EA *ea = (op_num == 0) ? &this->ins->ea1 : &this->ins->ea2;
    this->state.operands.ea = ea;
    this->state.current = M68kS_write_operand;
    u32 no_work = 0;
    u32 addr;
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
    M68k_start_write(this, addr, this->state.instruction.result, this->ins->sz, MAKE_FC(0), this->state.op[op_num].reversed, next_state);
}

u32 M68k_AM_ext_words(enum M68k_address_modes am, u32 sz)
{
    switch(am) {
        case M68k_AM_data_register_direct:
        case M68k_AM_address_register_direct:
        case M68k_AM_address_register_indirect:
        case M68k_AM_address_register_indirect_with_postincrement:
        case M68k_AM_address_register_indirect_with_predecrement:
        case M68k_AM_quick_immediate:
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

static void eval_ea_wait(struct M68k* this, u32 num_ea, u32 opnum, u32 sz)
{
    u32 prefetchnum = opnum == 0 ? M68kOS_prefetch1 : M68kOS_prefetch2;
    u32 pausenum = opnum == 0 ? M68kOS_pause1 : M68kOS_pause2;
    struct M68k_EA *ea = opnum == 0 ? &this->ins->ea1 : &this->ins->ea2;

    // # ext words
    this->state.op[opnum].ext_words = M68k_AM_ext_words(ea->kind, sz);

    // Reverse our read/write 32-bit order if this is a predecrement
    this->state.op[opnum].reversed = ea->kind == M68k_AM_address_register_indirect_with_predecrement && this->state.operands.can_reverse;

    // Prefetch if >0 ext words
    this->state.operands.state[prefetchnum] = this->state.op[opnum].ext_words != 0;
    if ((ea->kind == M68k_AM_program_counter_with_index) ||
        (ea->kind == M68k_AM_address_register_indirect_with_index)) {
        this->state.operands.state[pausenum] = 1;
    }
    if (opnum == 1) {
        if ((ea->kind == M68k_AM_address_register_indirect_with_predecrement) && (num_ea == 2) &&
            (!this->state.operands.fast)) {
            this->state.operands.state[pausenum] = 1;
        } else if ((num_ea == 1) && (ea->kind == M68k_AM_address_register_indirect_with_predecrement))
            this->state.operands.state[pausenum] = 1;
    }
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

void M68k_start_read_operand_for_ea(struct M68k* this, u32 fast, u32 sz, u32 next_state)
{
    M68k_start_read_operands(this, fast, sz, next_state);
    this->state.operands.state[M68kOS_read1] = this->state.operands.state[M68kOS_read2] = 0;
}

void M68k_start_read_operands(struct M68k* this, u32 fast, u32 sz, u32 next_state)
{
    this->state.current = M68kS_read_operands;
    this->state.operands.TCU = 0;
    this->state.operands.state[M68kOS_read1] = this->state.operands.state[M68kOS_read2] = this->state.operands.state[M68kOS_prefetch1] = this->state.operands.state[M68kOS_prefetch2] = this->state.operands.state[M68kOS_pause1] = this->state.operands.state[M68kOS_pause2] = 0;
    this->state.operands.fast = fast;
    this->state.operands.sz = sz;
    this->state.op[0].ext_words = this->state.op[1].ext_words = 0;
    this->state.op[0].reversed = this->state.op[1].reversed = 0;
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
            this->state.op[1].val = get_r(this, &ins->ea2, sz);
            no_fetches = 1;
            break;
        case M68k_OM_r:
            this->state.op[0].val = get_r(this, &ins->ea1, sz);
            no_fetches = 1;
            break;
        case M68k_OM_r_r:
            this->state.op[0].val = get_r(this, &ins->ea1, sz);
            this->state.op[1].val = get_r(this, &ins->ea2, sz);
            no_fetches = 1;
            break;
        case M68k_OM_ea_r:
            this->state.operands.state[M68kOS_prefetch1] = 1;
            if (!is_immediate(this->ins->ea1.kind)) this->state.operands.state[M68kOS_read1] = 1;
            this->state.op[1].val = get_r(this, &ins->ea2, sz);
            eval_ea_wait(this, 1, 0, sz);
            break;
        case M68k_OM_r_ea:
            this->state.op[0].val = get_r(this, &ins->ea1, sz);
            if (!is_immediate(this->ins->ea2.kind)) this->state.operands.state[M68kOS_read2] = 1;
            this->state.operands.state[M68kOS_prefetch2] = 1;
            eval_ea_wait(this, 1, 1, sz);
            break;
        case M68k_OM_ea: {
            if (!is_immediate(this->ins->ea1.kind)) this->state.operands.state[M68kOS_read1] = 1;
            this->state.operands.state[M68kOS_prefetch1] = 1;
            eval_ea_wait(this, 1, 0, sz);
            break; }
        case M68k_OM_ea_ea:
            if (!is_immediate(this->ins->ea1.kind)) this->state.operands.state[M68kOS_read1] = 1;
            if (!is_immediate(this->ins->ea2.kind)) this->state.operands.state[M68kOS_read2] = 1;
            eval_ea_wait(this, 2, 0, sz);
            eval_ea_wait(this, 2, 1, sz);
            break;
        case M68k_OM_qimm_ea:
            if (!is_immediate(this->ins->ea2.kind)) this->state.operands.state[M68kOS_read2] = 1;
            //this->state.operands.state[M68kOS_prefetch2] = 1;
            this->state.op[0].val = ins->ea1.reg;
            eval_ea_wait(this, 1, 1, sz);
            break;
        case M68k_OM_imm16:
            this->state.operands.state[M68kOS_read1] = 0;
            this->state.operands.state[M68kOS_prefetch1] = 1;
            eval_ea_wait(this, 1, 0, sz);
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

u32 M68k_inc_SSP(struct M68k* this, u32 num) {
    u32 *r = this->regs.SR.S ? &this->regs.A[7] : &this->regs.ASP;
    *r += num;
    return *r;
}

u32 M68k_dec_SSP(struct M68k* this, u32 num) {
    u32 *r = this->regs.SR.S ? &this->regs.A[7] : &this->regs.ASP;
    *r -= num;
    return *r;
}

u32 M68k_get_SSP(struct M68k* this) {
    if (this->regs.SR.S) return this->regs.A[7];
    return this->regs.ASP;
}

u32 M68k_get_SR(struct M68k* this) {
    return this->regs.SR.u;
}


void M68k_prefetch(struct M68k* this)
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
    M68k_start_read(this, this->regs.PC, 2, this->state.prefetch.FC, M68K_RW_ORDER_NORMAL, M68kS_prefetch);
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
    M68k_start_read(this, this->regs.PC, this->state.op[opnum].ext_words << 1, MAKE_FC(1), M68K_RW_ORDER_NORMAL, M68kS_read_operands);
    this->regs.PC += (this->state.op[opnum].ext_words << 1); // Add to PC
    this->state.operands.TCU = 1;
}

void M68k_read_operands_read(struct M68k* this, u32 opnum)
{
    // get the EA
    if (this->state.operands.TCU == 1) { // read is done
        this->state.op[opnum].val = this->state.bus_cycle.data;
        this->state.operands.state[(opnum == 0) ? M68kOS_read1 : M68kOS_read2] = 0;
        this->state.operands.TCU = 0;
        return;
    }

    // setup read
    this->state.op[opnum].addr = M68k_read_ea_addr(this, opnum == 0 ? &this->ins->ea1 : &this->ins->ea2, this->state.operands.sz,
                                                   0,
                                                   this->state.op[opnum].ext_words);
    M68k_start_read(this, this->state.op[opnum].addr, this->ins->sz, MAKE_FC(1), M68K_RW_ORDER_NORMAL, M68kS_read_operands);
    this->state.operands.TCU = 1;
}

void M68k_read_operands(struct M68k* this) {
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

#define EXC_WRITE(val, n, desc) M68k_start_write(this, this->state.exception.group0.base_addr+n, val, 2, M68k_FC_supervisor_data, M68K_RW_ORDER_NORMAL, M68kS_exc_group0)

void M68k_exc_group1(struct M68k* this)
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
            M68k_start_write(this, this->state.exception.group1.base_addr+4, this->state.exception.group1.PC & 0xFFFF, 2, MAKE_FC(0), M68K_RW_ORDER_NORMAL, M68kS_exc_group1);
            break;
            case 1:
                M68k_start_write(this, this->state.exception.group1.base_addr, this->state.exception.group1.SR, 2, MAKE_FC(0), M68K_RW_ORDER_NORMAL, M68kS_exc_group1);
            break;
            case 2:
                M68k_start_write(this, this->state.exception.group1.base_addr+2, this->state.exception.group1.PC >> 16, 2, MAKE_FC(0), M68K_RW_ORDER_NORMAL, M68kS_exc_group1);
            break;
            case 3:
                M68k_start_read(this, this->state.exception.group1.vector << 2, 4, MAKE_FC(0), M68K_RW_ORDER_NORMAL, M68kS_exc_group1);
            break;
            case 4:
                this->regs.PC = this->state.bus_cycle.data;
            M68k_start_prefetch(this, 2, 1, M68kS_decode);
            break;
        }
    }
    this->state.exception.group1.TCU++;
}

void M68k_exc_group2(struct M68k* this)
{
    switch(this->state.exception.group2.TCU) {
        case 0: {
            // SR
            // PCH
            // PCL
            M68k_dec_SSP(this, 6);
            this->state.exception.group2.base_addr = M68k_get_SSP(this);
            this->state.exception.group2.SR = M68k_get_SR(this);
            M68k_start_write(this, this->state.exception.group2.base_addr+4, this->state.exception.group2.PC & 0xFFFF, 2, MAKE_FC(0), M68K_RW_ORDER_NORMAL, M68kS_exc_group2);
            break;
            case 1:
                M68k_start_write(this, this->state.exception.group2.base_addr, this->state.exception.group2.SR, 2, MAKE_FC(0), M68K_RW_ORDER_NORMAL, M68kS_exc_group2);
            break;
            case 2:
                M68k_start_write(this, this->state.exception.group2.base_addr+2, this->state.exception.group2.PC >> 16, 2, MAKE_FC(0), M68K_RW_ORDER_NORMAL, M68kS_exc_group2);
            break;
            case 3:
                M68k_start_read(this, this->state.exception.group2.vector << 2, 4, MAKE_FC(0), M68K_RW_ORDER_NORMAL, M68kS_exc_group2);
            break;
            case 4:
                this->regs.PC = this->state.bus_cycle.data;
            M68k_start_prefetch(this, 2, 1, M68kS_decode);
            break;
        }
    }
    this->state.exception.group2.TCU++;
}

void M68k_exc_group0(struct M68k* this)
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
            M68k_start_read(this, this->state.exception.group0.vector << 2, 4, M68k_FC_supervisor_data, M68K_RW_ORDER_NORMAL, M68kS_exc_group0);
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
