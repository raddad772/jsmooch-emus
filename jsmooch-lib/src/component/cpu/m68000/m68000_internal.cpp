//
// Created by . on 6/20/24.
//
#include <cstdio>
#include <cstdlib>
#include <cassert>

#include "helpers/cvec.h"
#include "m68000.h"
#include "m68000_instructions.h"
#include "m68000_internal.h"

#define ADDRAND1 { pins.LDS = (state.bus_cycle.addr & 1); pins.UDS = pins.LDS ^ 1; }
#define ALLOW_REVERSE 1
#define NO_REVERSE 0

namespace M68k {
static constexpr u32 clip32[5] = { 0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF};

u32 core::get_dr(u32 num, u32 sz) const {
    return regs.D[num] & clip32[sz];
}

void core::adjust_IPC(u32 opnum, u32 sz)
{
    u32 ex_words = state.op[opnum].ext_words;
    u32 kind0 = ins->ea[0].kind;
    //u32 kind1 = ins->ea[1].kind;
    u32 kk = ins->ea[opnum].kind;
    switch(ins->operand_mode) {
        case OM_none:
            regs.IPC -= 2;
            break;
        case OM_ea_r:
            switch(kk) {
                case AM_absolute_short_data: // 1 ext word
                    break;
                case AM_address_register_indirect_with_predecrement: // 0 ext word
                    if (ins->sz == 4) regs.IPC -= 2;
                    break;
                case AM_absolute_long_data: // 2 ext word, 0 read
                    regs.IPC += 2; break;
                case AM_address_register_indirect_with_index:
                default:
                    regs.IPC -= 2; break;
            }
            break;
        case OM_qimm_ea:
        case OM_r_ea:
            if (sz == 4) {
                switch (kk) {
                    case AM_absolute_short_data:
                        break;
                    case AM_absolute_long_data:
                        regs.IPC += 2;
                        break;
                    default:
                        regs.IPC -= 2;
                        break;
                }
            }
            else {
                switch(kk) {
                    case AM_address_register_indirect_with_displacement:
                    case AM_address_register_indirect_with_index:
                    case AM_address_register_indirect_with_postincrement:
                    case AM_address_register_indirect:
                        regs.IPC -= 2;
                        break;
                    case AM_absolute_long_data:
                        regs.IPC += 2;
                        break;
                    default:
                }
            }
            break;
        case OM_ea:
            if (sz == 4) {
                switch (kind0) {
                    case AM_absolute_long_data:
                        regs.IPC += 2;
                        break;
                    case AM_address_register_indirect:
                    case AM_address_register_indirect_with_postincrement:
                    case AM_address_register_indirect_with_displacement:
                    case AM_program_counter_with_displacement:
                    case AM_program_counter_with_index:
                    case AM_address_register_indirect_with_index:
                        regs.IPC -= 2;
                        break;
                    case AM_address_register_indirect_with_predecrement:
                        if (ins->sz == 4) regs.IPC -= 2;
                        break;
                    default:
                }
            }
            else {
                switch(kind0) {
                    case AM_absolute_long_data:
                        regs.IPC += 2; break;
                    case AM_address_register_indirect_with_postincrement:
                    case AM_address_register_indirect_with_displacement:
                    case AM_address_register_indirect_with_index:
                    case AM_address_register_indirect:
                    case AM_program_counter_with_displacement:
                    case AM_program_counter_with_index:
                        regs.IPC -= 2; break;
                    default:
                }
            }
            break;
        case OM_ea_ea: {
            //u32 mk = opnum == 0 ? kind0 : kind1;
            if (kk == AM_address_register_indirect_with_postincrement)
                regs.IPC -= 2;
            else
                regs.IPC += ex_words << 1;
            break; }
        default:
            regs.IPC += ex_words << 1;
            break;
    }
}

u32 core::get_ar(u32 num, u32 sz) const {
    u32 v = regs.A[num];
    switch(sz) {
        case 1:
            return static_cast<u32>(static_cast<i32>(static_cast<i8>(v)));
        case 2:
            return static_cast<u32>(static_cast<i32>(static_cast<i16>(v)));
        case 4:
            return static_cast<u32>(static_cast<i32>(v));
        nodefault
    }
}

bool core::am_in_group0_or_1() const {
    // TODO: detect group 1?
    return state.bus_cycle.next_state == S_exc_group0;
}

void core::bus_cycle_read()
{
    switch(state.bus_cycle.TCU & 3) {
        case 0:
            pins.FC = state.bus_cycle.FC;
            state.bus_cycle.done = 0;
            if ((state.bus_cycle.TCU == 0) && (state.current == S_read32) && (state.bus_cycle.reversed)) {
                state.bus_cycle.addr += 2;
            }
            if (state.current == S_read8) {
                ADDRAND1;
            }
            else {
                pins.LDS = pins.UDS = 1;
            }
            pins.RW = 0;
            pins.Addr = state.bus_cycle.addr & 0xFFFFFE;
            break;
        case 1:
#ifdef M68K_TESTING
            pins.AS = 1;
#else
            pins.AS = !state.bus_cycle.addr_error;
#endif
            break;
        case 2:
#ifndef M68K_TESTING
            if (!state.bus_cycle.addr_error) {
#endif
                state.bus_cycle.TCU--;
                if (pins.DTACK) // take out wait state
                    state.bus_cycle.TCU++;
                else if (pins.VPA) {// TODO: fix VPA E-cycle sync
#ifndef M68K_E_CLOCK
                    state.bus_cycle.TCU++;
#else
                    if ((state.e_clock_count == 3) && (!pins.VMA)) { // The only place VMA can latch
                        pins.VMA = 1;
                    }
                    if ((state.e_clock_count == 9) && (pins.VMA)) { // Final cycle can happen next
                        state.bus_cycle.TCU++;
                    }
#endif
                }
#ifndef M68K_TESTING
            }
#endif
            break;
        case 3: // latch data and de-assert pins
            pins.DTACK = 0;
            pins.VPA = 0;
            pins.VMA = 0;
            pins.AS = 0;
            if (state.current == S_read32) {
                if (state.bus_cycle.addr_error) {
                    start_group0_exception(IV_address_error, 7, am_in_group0_or_1(), state.bus_cycle.addr);
                    return;
                }
                if (state.bus_cycle.reversed) {
                    if (state.bus_cycle.TCU == 3) { // We have another go-round left!
                        state.bus_cycle.data = pins.D;
                        state.bus_cycle.addr = (state.bus_cycle.addr - 2) & 0xFFFFFFFF;
                    } else state.bus_cycle.data |= (pins.D << 16);
                }
                else {
                    if (state.bus_cycle.TCU == 3) { // We have another go-round left!
                        state.bus_cycle.data = pins.D << 16;
                        state.bus_cycle.addr = (state.bus_cycle.addr + 2) & 0xFFFFFFFF;
                    } else state.bus_cycle.data |= pins.D;
                }
            }
            else if (state.current == S_read16) {
                state.bus_cycle.data = pins.D;
                if (state.bus_cycle.addr_error) {
                    start_group0_exception(IV_address_error, 7, am_in_group0_or_1(), state.bus_cycle.addr);
                    return;
                }
            }
            else { // 8
                if (pins.LDS) state.bus_cycle.data = pins.D & 0xFF;
                else state.bus_cycle.data = pins.D >> 8;
            }
            pins.LDS = pins.UDS = 0;
            if ((state.current != S_read32) || (state.bus_cycle.TCU == 7)) {
                state.bus_cycle.done = 1;

                state.bus_cycle.TCU = -1;
                state.current = state.bus_cycle.next_state;
            }
            break;
        nodefault
    }
    state.bus_cycle.TCU++;
}

void core::start_group0_exception(u32 vector_number, i32 wait_cycles, bool was_in_group0_or_1, u32 addr)
{
    //M68k_start_wait(1, S_exc_group0);
    //state.current = S_exc_group0;
    state.exception.group0.vector = vector_number;
    state.exception.group0.TCU = 0;
    state.exception.group0.normal_state = !was_in_group0_or_1;
    state.exception.group0.addr = addr;
    state.current = S_exc_group0;
    if (wait_cycles > 0) {
        start_wait(wait_cycles + 1, S_exc_group0);
    }
    // SR cooy
    // supervisor enter
    // tracing off
}

void core::start_group12_exception(u32 vector_number, i32 wait_cycles, u32 PC, u32 groupnum)
{
    state.current = S_exc_group12;
    state.exception.group12.TCU = 0;
    state.exception.group12.vector = vector_number;
    state.exception.group12.PC = PC - 2;
    state.exception.group12.group = groupnum;

    if (wait_cycles > 0)
        start_wait(wait_cycles, S_exc_group12);
}

// Bus cycle for acknowledging interrupt
void core::bus_cycle_iaq()
{
    switch(state.bus_cycle_iaq.TCU) {
        case 0:
            state.bus_cycle_iaq.autovectored = 0;
            pins.DTACK = 0;
            pins.UDS = pins.LDS = 0;
            pins.FC = 7;
            pins.RW = 0;
            pins.AS = pins.DTACK = 0;
            pins.VMA = pins.VPA = 0;
            break;
        case 1:
            pins.AS = 1;
            state.bus_cycle_iaq.ilevel = pins.IPL;
            // Set A1-3 to ilevel
            pins.Addr = state.bus_cycle_iaq.ilevel << 1;
            break;
        case 2:
            state.bus_cycle_iaq.TCU--;
            if (pins.DTACK) // take out wait state
                state.bus_cycle_iaq.TCU++;
            else if (pins.VPA) {
#ifndef M68K_E_CLOCK
                state.bus_cycle_iaq.TCU++;
                state.bus_cycle_iaq.vector_number = 0x18 + state.bus_cycle_iaq.ilevel;
                state.bus_cycle_iaq.autovectored = 1;
#else
                if ((state.e_clock_count == 3) && (!pins.VMA)) { // The only place VMA can latch
                    pins.VMA = 1;
                }
                if ((state.e_clock_count == 9) && (pins.VMA)) { // Final cycle can happen next
                    state.bus_cycle_iaq.vector_number = 0x18 + state.bus_cycle_iaq.ilevel;
                    state.bus_cycle_iaq.autovectored = 1;
                    state.bus_cycle_iaq.TCU++;
                }
#endif
            }
            break;
        case 3:
            pins.FC = 0;
            pins.AS = 0;
            pins.DTACK = 0;
            pins.VPA = 0;
            pins.VMA = 0;
            state.current = S_exc_interrupt;
            break;
        nodefault
    }
    state.bus_cycle_iaq.TCU++;
}

void core::start_group2_exception(u32 vector_number, i32 wait_cycles, u32 PC)
{
    start_group12_exception(vector_number, wait_cycles, PC, 2);
}

void core::start_group1_exception(u32 vector_number, i32 wait_cycles, u32 PC)
{
    start_group12_exception(vector_number, wait_cycles, PC, 1);
}

// this is duplicated in m68000_opcodes.c
#define ADDRAND1 { pins.LDS = (state.bus_cycle.addr & 1); pins.UDS = pins.LDS ^ 1; }

void core::bus_cycle_write()
{
    switch(state.bus_cycle.TCU & 3) {
        case 0: {
            pins.FC = state.bus_cycle.FC;
            pins.RW = 0;
            state.bus_cycle.done = 0;
            if ((state.bus_cycle.TCU == 0) && (state.current == S_write32) && (state.bus_cycle.reversed)) {
                // Do high-word first
                state.bus_cycle.addr += 2;
            }
            pins.Addr = state.bus_cycle.addr & 0xFFFFFE;
            break; }
        case 1: {
#ifdef M68K_TESTING
            pins.AS = 1;
#else
            pins.AS = !state.bus_cycle.addr_error;
#endif
            pins.RW = 1;
            // technically this happens at rising edge of next cycle, buuuut, we're doing it here
            if (state.current == S_write32) {
                if (state.bus_cycle.reversed) {
                    if (state.bus_cycle.TCU == 1) { // first write
                        pins.D = state.bus_cycle.data & 0xFFFF;
                        state.bus_cycle.addr = (state.bus_cycle.addr - 2) & 0xFFFFFFFF;
                    } else pins.D = state.bus_cycle.data >> 16; // second write
                }
                else {
                    if (state.bus_cycle.TCU == 1) { // first write
                        pins.D = state.bus_cycle.data >> 16;
                        state.bus_cycle.addr = (state.bus_cycle.addr + 2) & 0xFFFFFFFF;
                    } else pins.D = state.bus_cycle.data & 0xFFFF; // second write
                }
                pins.LDS = pins.UDS = 1;
            }
            else if (state.current == S_write16) {
                pins.LDS = pins.UDS = 1;
                pins.D = state.bus_cycle.data & 0XFFFF;
            }
            else {
                ADDRAND1;
                if (pins.LDS) pins.D = state.bus_cycle.data;
                else pins.D = state.bus_cycle.data << 8;
            }
            break; }
        case 2: {
            // This is annoying
#ifndef M68K_TESTING
            if (!state.bus_cycle.addr_error) {
#endif
                state.bus_cycle.TCU--;
                if (pins.DTACK) // take out wait state
                    state.bus_cycle.TCU++;
                else if (pins.VPA) {// TODO: fix VPA E-cycle sync
#ifndef M68K_E_CLOCK
                    state.bus_cycle.TCU++;
#else
                    if ((state.e_clock_count == 3) && (!pins.VMA)) { // The only place VMA can latch
                        pins.VMA = 1;
                    }
                    if ((state.e_clock_count == 9) && (pins.VMA)) { // Final cycle can happen next
                        state.bus_cycle.TCU++;
                    }
#endif
                }
#ifndef M68K_TESTING
            }
#endif
            break; }
        case 3: {// latch data and de-assert pins
            pins.DTACK = 0;
            pins.AS = 0;
            pins.LDS = pins.UDS = 0;
            pins.VPA = pins.VMA = 0;
            if (state.current != S_write8) {
                if (state.bus_cycle.addr_error) {
                    u32 addr = state.bus_cycle.original_addr;
                    if ((state.current == S_write32) && (state.bus_cycle.reversed)) {
                        addr += 2;
                    }
                    start_group0_exception(IV_address_error, 7, am_in_group0_or_1(), addr);
                    return;
                }
            }
            if ((state.current != S_write32) || (state.bus_cycle.TCU == 7)) {
                state.bus_cycle.done = 1;
                state.bus_cycle.TCU = -1;
                state.current = state.bus_cycle.next_state;
            }
            break; }
        nodefault
    }
    state.bus_cycle.TCU++;
}

void core::start_prefetch(u32 num, u32 is_program, states next_state)
{
    state.current = S_prefetch;
    state.prefetch.num = static_cast<i32>(num);
    state.prefetch.TCU = 0;
    state.prefetch.FC = MAKE_FC(is_program);
    state.prefetch.next_state = next_state;
}

void core::start_read(u32 addr, u32 sz, u32 FC, u32 reversed, states next_state)
{
    switch(sz) {
        case 1:
            state.current = S_read8;
            break;
        case 2:
            state.current = S_read16;
            break;
        case 4:
            state.current = S_read32;
            break;
        nodefault
    }
    state.bus_cycle.reversed = reversed;
    state.bus_cycle.TCU = 0;
    state.bus_cycle.addr = addr;
    state.bus_cycle.func = &core::bus_cycle_read;
    state.bus_cycle.addr_error = (addr & 1) && (sz != 1);
    state.bus_cycle.next_state = next_state;
    state.bus_cycle.e_phase = 0;
    state.bus_cycle.FC = FC;
}

void core::start_write(u32 addr, u32 val, u32 sz, u32 FC, u32 reversed, states next_state)
{
    switch(sz) {
        case 1:
            state.current = S_write8;
            break;
        case 2:
            state.current = S_write16;
            break;
        case 4:
            state.current = S_write32;
            break;
        nodefault
    }
    state.bus_cycle.TCU = 0;
    state.bus_cycle.original_addr = addr;
    state.bus_cycle.addr = addr;
    state.bus_cycle.func = &core::bus_cycle_write;
    state.bus_cycle.addr_error = (addr & 1) && (sz != 1);
    state.bus_cycle.data = val;
    state.bus_cycle.FC = FC;
    state.bus_cycle.e_phase = 0;
    state.bus_cycle.reversed = reversed;
    state.bus_cycle.next_state = next_state;
}

void core::set_dr(u32 num, u32 result, u32 sz)
{
    switch(sz) {
        case 1:
            regs.D[num] = (regs.D[num] & 0xFFFFFF00) | (result & 0xFF);
            return;
        case 2:
            regs.D[num] = (regs.D[num] & 0xFFFF0000) | (result & 0xFFFF);
            return;
        case 4:
            regs.D[num] = result;
            return;
        nodefault
    }
}

void core::set_ar(u32 num, u32 result, u32 sz)
{
    switch(sz) {
        case 1:
            regs.A[num] = static_cast<u32>(static_cast<i32>(static_cast<i8>(result & 0xFF)));
            return;
        case 2:
            regs.A[num] = static_cast<u32>(static_cast<i32>(result & 0xFFFF));
            return;
        case 4:
            regs.A[num] = result;
            return;
        nodefault
    }
}

u32 core::read_ea_addr(uint32 opnum, u32 sz, bool hold, u32 prefetch)
{
    u32 v, c;
    i64 a, b;
    EA *ea = opnum == 0 ? &ins->ea[0] : &ins->ea[1];
    switch(ea->kind) {
        case AM_address_register_indirect: // 0 ext word
            return regs.A[ea->reg];
        case AM_address_register_indirect_with_postincrement: // 0 ext word
            v = regs.A[ea->reg] + sz;
            c = regs.A[ea->reg];
            if ((ea->reg == 7) && (sz == 1)) v++;
            state.op[opnum].new_val = v;
            if (!hold) {
                regs.A[ea->reg] = v;
            }
            else {
                state.op[opnum].held = 1;
            }
            return c;
        case AM_address_register_indirect_with_predecrement: // 0 ext word
            v = regs.A[ea->reg] - sz;
            if ((ea->reg == 7) && (sz == 1)) v--;
            state.op[opnum].new_val = v;
            if (!hold)
                regs.A[ea->reg] = v;
            else {
                state.op[opnum].held = 1;
            }
            return v;
        case AM_address_register_indirect_with_displacement: // 1 ext word
            v = static_cast<u32>(static_cast<i64>(regs.A[ea->reg]) + static_cast<i64>(static_cast<i16>(prefetch & 0xFFFF)));
            return v;
        case AM_address_register_indirect_with_index: // 1 ext word
            v = regs.A[ea->reg];
            a = SIGNe8to64(prefetch);
            b = prefetch & 0x8000 ? regs.A[(prefetch >> 12) & 7] : regs.D[(prefetch >> 12) & 7];
            if (!(prefetch & 0x800))
                b = static_cast<u32>(static_cast<i32>(static_cast<i16>(b & 0xFFFF)));
            v = ((v + a + b) & 0xFFFFFFFF);
            return v;
        case AM_absolute_short_data: // 1 ext word
            // Sign-extend 16-bit prefetch word, and use that
            return SIGNe16to32(prefetch);
        case AM_absolute_long_data: // 2 ext word
            return prefetch;
        case AM_program_counter_with_displacement: // 1 ext word
            v = SIGNe16to32(prefetch);
            return regs.PC + v - 4;
        case AM_program_counter_with_index: // 1 ext word
            v = regs.PC;
            a = SIGNe8to64(prefetch);
            b = prefetch & 0x8000 ? regs.A[(prefetch >> 12) & 7] : regs.D[(prefetch >> 12) & 7];
            if (!(prefetch & 0x800))
                b = static_cast<u32>(static_cast<i32>(static_cast<i16>(b & 0xFFFF)));
            return ((v + a + b) - 4) & 0xFFFFFFFF;
        nodefault
    }
}

u32 core::write_ea_addr(const EA *ea, const bool commit, const u32 opnum)
{
    switch(ea->kind) {
        case AM_address_register_indirect_with_postincrement:
        case AM_address_register_indirect_with_predecrement:
            if (commit) // meaning, commit
                regs.A[ea->reg] = state.op[opnum].new_val;
            return state.op[opnum].addr;
        case AM_address_register_indirect:
        case AM_address_register_indirect_with_displacement:
        case AM_address_register_indirect_with_index:
        case AM_absolute_short_data:
        case AM_absolute_long_data:
        case AM_program_counter_with_displacement:
        case AM_program_counter_with_index:
            return state.op[opnum].addr;
        nodefault
    }
}

void core::start_write_operand(u32 commit, u32 op_num, states next_state, bool allow_reverse, bool force_reverse)
{
    state.operands.TCU = 0;
    state.operands.cur_op_num = op_num;
    EA *ea = (op_num == 0) ? &ins->ea[0] : &ins->ea[1];
    state.operands.ea = ea;
    state.current = S_write_operand;
    switch(state.operands.ea->kind) {
        case AM_data_register_direct:
            set_dr(ea->reg, state.instruction.result, ins->sz);
            break;
        case AM_address_register_direct:
            set_ar(ea->reg, state.instruction.result, ins->sz);
            break;
        default: {
            const u32 addr = write_ea_addr((op_num==0) ? &ins->ea[0] : &ins->ea[1], commit, op_num);
            const u32 reverse = force_reverse ? 1 : state.op[op_num].reversed & allow_reverse;
            start_write(addr, state.instruction.result, ins->sz, MAKE_FC(0), reverse, next_state);
            return; }
    }
    state.current = next_state;
}

u32 AM_ext_words(address_modes am, u32 sz)
{
    switch(am) {
        case AM_data_register_direct:
        case AM_address_register_direct:
        case AM_address_register_indirect:
        case AM_address_register_indirect_with_postincrement:
        case AM_address_register_indirect_with_predecrement:
        case AM_quick_immediate:
            return 0;
        case AM_address_register_indirect_with_displacement:
        case AM_address_register_indirect_with_index:
        case AM_absolute_short_data:
        case AM_program_counter_with_displacement:
        case AM_program_counter_with_index:
        case AM_imm16:
            return 1;
        case AM_absolute_long_data:
            return 2;
        case AM_immediate:
            return (sz >> 1) ? sz >> 1 : 1;
        nodefault
    }
}

void core::eval_ea_wait(u32 num_ea, u32 opnum, u32 sz)
{
    u32 prefetchnum = opnum == 0 ? OS_prefetch1 : OS_prefetch2;
    u32 pausenum = opnum == 0 ? OS_pause1 : OS_pause2;
    EA *ea = &ins->ea[opnum];

    // # ext words
    state.op[opnum].ext_words = AM_ext_words(ea->kind, sz);

    // Reverse our read/write 32-bit order...
    state.op[opnum].reversed = 1;

    // Prefetch if >0 ext words
    state.operands.state[prefetchnum] = state.op[opnum].ext_words != 0;
    if ((ea->kind == AM_program_counter_with_index) ||
        (ea->kind == AM_address_register_indirect_with_index)) {
        state.operands.state[pausenum] = 1;
    }
    if ((opnum == 0) && ((ea->kind == AM_address_register_indirect_with_predecrement))) {
        state.operands.state[pausenum] = 1;
    }
    if (opnum == 1) {
        if ((ea->kind == AM_address_register_indirect_with_predecrement) && (num_ea == 2) &&
            (!state.operands.fast)) {
            state.operands.state[pausenum] = 1;
        } else if ((num_ea == 1) && (ea->kind == AM_address_register_indirect_with_predecrement))
            state.operands.state[pausenum] = 1;
    }
}

void core::swap_ASP()
{
    u32 x = regs.A[7];
    regs.A[7] = regs.ASP;
    regs.ASP = x;
}

static u32 is_immediate(address_modes am)
{
    switch(am) {
        case AM_imm16:
        case AM_imm32:
        case AM_immediate:
            return 1;
        default:
            return 0;
    }
}

void core::start_read_operand_for_ea(const u32 fast, const u32 sz, const states next_state, const u32 wait_states, const bool hold, const bool allow_reverse)
{
    start_read_operands(fast, sz, next_state, wait_states, hold, allow_reverse, MAKE_FC(0));
    state.operands.state[OS_read1] = state.operands.state[OS_read2] = 0;
}


u32 core::get_r(const EA *ea, const u32 sz) const {
    u32 v;
    switch(ea->kind) {
        case AM_data_register_direct:
            v = get_dr(ea->reg, sz);
            return v;
        case AM_address_register_direct:
            v = get_ar(ea->reg, sz);
            return v;
        case AM_quick_immediate:
        case AM_immediate:
            v = ea->reg;
            break;
        nodefault
    }
    switch(sz) {
        case 1:
            return v & 0xFF;
        case 2:
            return v & 0xFFFF;
        case 4:
            return v;
        nodefault
    }
}

void core::start_read_operands(u32 fast, u32 sz, states next_state, u32 wait_states, bool hold, bool allow_reverse, bool read_fc)
{
    if (wait_states > 0) start_wait(wait_states, S_read_operands);
    else state.current = S_read_operands;
    state.operands.allow_reverse = allow_reverse;
    state.operands.TCU = 0;
    state.operands.read_fc = read_fc;
    state.operands.state[OS_read1] = state.operands.state[OS_read2] = state.operands.state[OS_prefetch1] = state.operands.state[OS_prefetch2] = state.operands.state[OS_pause1] = state.operands.state[OS_pause2] = 0;
    state.operands.fast = fast;
    state.operands.sz = sz;
    state.op[0].ext_words = state.op[1].ext_words = 0;
    state.op[0].reversed = state.op[1].reversed = 0;
    state.op[0].held = state.op[1].held = 0;
    state.op[0].hold = state.op[1].hold = hold;
    state.op[0].new_val = state.op[1].new_val = 0;
    state.operands.next_state = next_state;
    u32 no_fetches = 0;
    auto *m_ins = ins;
    switch(m_ins->operand_mode) {
        case OM_none:
            no_fetches = 1;
            adjust_IPC(0, sz);
            break;
        case OM_qimm:
            state.op[0].val = m_ins->ea[0].reg;
            no_fetches = 1;
            break;
        case OM_qimm_qimm:
            state.op[0].val = m_ins->ea[0].reg;
            state.op[1].val = m_ins->ea[1].reg;
            no_fetches = 1;
            break;
        case OM_qimm_r:
            state.op[0].val = m_ins->ea[0].reg;
            state.op[1].val = get_r(&m_ins->ea[1], sz);
            no_fetches = 1;
            break;
        case OM_r:
            state.op[0].val = get_r(&m_ins->ea[0], sz);
            no_fetches = 1;
            adjust_IPC(0, sz);
            break;
        case OM_r_r:
            state.op[0].val = get_r(&m_ins->ea[0], sz);
            state.op[1].val = get_r(&m_ins->ea[1], sz);
            no_fetches = 1;
            break;
        case OM_ea_r:
            state.operands.state[OS_prefetch1] = 1;
            if (!is_immediate(m_ins->ea[0].kind)) state.operands.state[OS_read1] = 1;
            state.op[1].val = get_r(&m_ins->ea[1], sz);
            eval_ea_wait(1, 0, sz);
            if (!state.operands.state[OS_prefetch1]) {
                adjust_IPC(0, sz);
            }
            break;
        case OM_r_ea:
            state.op[0].val = get_r(&m_ins->ea[0], sz);
            if (!is_immediate(m_ins->ea[1].kind)) state.operands.state[OS_read2] = 1;
            state.operands.state[OS_prefetch2] = 1;
            eval_ea_wait(1, 1, sz);
            if (!state.operands.state[OS_prefetch2]) {
                adjust_IPC(1, sz);
            }
            break;
        case OM_ea: {
            if (!is_immediate(m_ins->ea[0].kind)) state.operands.state[OS_read1] = 1;
            state.operands.state[OS_prefetch1] = 1;
            eval_ea_wait(1, 0, sz);
            if (!state.operands.state[OS_prefetch1]) {
                adjust_IPC(0, sz);
            }
            break; }
        case OM_ea_ea:
            if (!is_immediate(m_ins->ea[0].kind)) state.operands.state[OS_read1] = 1;
            if (!is_immediate(m_ins->ea[1].kind)) state.operands.state[OS_read2] = 1;
            eval_ea_wait(2, 0, sz);
            eval_ea_wait(2, 1, sz);
            if (!state.operands.state[OS_prefetch1]) {
                adjust_IPC(0, sz);
            }
            if (!state.operands.state[OS_prefetch2]) {
                adjust_IPC(1, sz);
            }
            break;
        case OM_qimm_ea:
            if (!is_immediate(m_ins->ea[1].kind)) state.operands.state[OS_read2] = 1;
            //state.operands.state[OS_prefetch2] = 1;
            state.op[0].val = m_ins->ea[0].reg;
            eval_ea_wait(1, 1, sz);
            if (!state.operands.state[OS_prefetch2]) {
                adjust_IPC(1, sz);
            }
            break;
        case OM_imm16:
            state.operands.state[OS_read1] = 0;
            state.operands.state[OS_prefetch1] = 1;
            eval_ea_wait(1, 0, sz);
        nodefault
    }
    if (no_fetches) {
        if (!wait_states) state.current = next_state;
    }
}

void core::start_wait(u32 num, states state_after) {
    if (num == 0) {
        state.current = state_after;
        return;
    }
    state.current = S_wait_cycles;
    state.wait_cycles.cycles_left = static_cast<i32>(num);
    state.wait_cycles.next_state = state_after;
}

u32 core::inc_SSP(u32 num) {
    u32 *r = regs.SR.S ? &regs.A[7] : &regs.ASP;
    *r += num;
    return *r;
}

u32 core::dec_SSP(u32 num) {
    u32 *r = regs.SR.S ? &regs.A[7] : &regs.ASP;
    *r -= num;
    return *r;
}

u32 core::get_SSP() const {
    return regs.SR.S ? regs.A[7] : regs.ASP;
}

void core::set_SSP(u32 to) {
    if (regs.SR.S) regs.A[7] = to;
    else regs.ASP = to;
}

void core::prefetch()
{
    if (state.prefetch.TCU == 1) { // We just completed a cycle of prefetch
        regs.PC += 2;
        regs.IR = regs.IRC;
        regs.IRC = state.bus_cycle.data;
        state.prefetch.num--;
        state.prefetch.TCU = 0;
    }
    if (state.prefetch.num <= 0) {
        state.current = state.prefetch.next_state;
        return;
    }
    start_read(regs.PC, 2, state.prefetch.FC, M68K_RW_ORDER_NORMAL, S_prefetch);
    state.prefetch.TCU = 1;
}

void core::finalize_ea(u32 opnum)
{
    if (!state.op[opnum].held) return;
    EA *ea = opnum == 0 ? &ins->ea[0] : &ins->ea[1];
    switch(ea->kind) {
        case AM_address_register_indirect_with_predecrement:
        case AM_address_register_indirect_with_postincrement:
            regs.A[ea->reg] = state.op[opnum].new_val;
            break;
        nodefault
    }
    state.op[opnum].held = 0;
}

void core::read_operands_prefetch(u32 opnum)
{
    // read 16 or 32 bits.
    // move them into
    if (state.operands.TCU == 1) {
        u32 v;
        //u32 snum = opnum == 0 ? OS_read1 : OS_read2;
        finalize_ea(opnum);

        if (state.op[opnum].ext_words) regs.PC += (state.op[opnum].ext_words << 1); // Add to PC

        if (state.op[opnum].ext_words == 1) { // 1 prefetch word
            regs.IR = regs.IRC;
            regs.IRC = state.bus_cycle.data & 0xFFFF;
            v = regs.IR;
        } else {
            assert(state.op[opnum].ext_words == 2);
            v = (regs.IRC << 16);
            regs.IR = state.bus_cycle.data >> 16;
            regs.IRC = state.bus_cycle.data & 0xFFFF;
            v |= regs.IR;
        }
        state.op[opnum].ext_words = v;  // Set the variable with the result
        state.operands.state[(opnum == 0) ? OS_prefetch1 : OS_prefetch2] = 0;
        state.operands.TCU = 0;
        u32 s = state.operands.state[(opnum == 0) ? OS_read1 : OS_read2];
        if (state.operands.state[s] == 0) {
            state.op[opnum].val = v;
        }
        return;
    }
    start_read(regs.PC, state.op[opnum].ext_words << 1, MAKE_FC(1), M68K_RW_ORDER_NORMAL, S_read_operands);
    adjust_IPC(opnum, ins->sz);
    state.operands.TCU = 1;
}

void core::read_operands_read(u32 opnum, bool commit)
{
    // get the EA
    if (state.operands.TCU == 1) { // read is done
        if (commit) finalize_ea(opnum);
        state.op[opnum].val = state.bus_cycle.data;
        state.operands.state[(opnum == 0) ? OS_read1 : OS_read2] = 0;
        state.operands.TCU = 0;
        return;
    }
    // setup read
    state.op[opnum].addr = read_ea_addr(opnum, state.operands.sz,
                                                   state.op[opnum].hold,
                                                   state.op[opnum].ext_words);
    // 001 MOVEA.l (d8, A7, Xn), A6 2c77
    //      wants: FC(0
    u32 rfc = MAKE_FC(0);
    if ((ins->sz == 4) && (ins->ea[opnum].kind == AM_address_register_indirect_with_index))
        rfc = MAKE_FC(0);
    if ((ins->ea[opnum].kind == AM_program_counter_with_index) || (ins->ea[opnum].kind == AM_program_counter_with_displacement))
        rfc = MAKE_FC(1);

    start_read(state.op[opnum].addr, ins->sz, rfc, state.op[opnum].reversed & state.operands.allow_reverse, S_read_operands);
    state.operands.TCU = 1;
}

void core::read_operands() {
    i32 m_state = -1;

    for (u32 i = 0; i < 6; i++) {
        if (state.operands.state[i] > 0) {
            m_state = static_cast<i32>(i);
            break;
        }
    }
    if (m_state == -1) {
        state.current = state.operands.next_state;
        return;
    }
    u32 r;
    switch(m_state) {
        case OS_read1:
            return read_operands_read(0, true);
        case OS_read2:
            return read_operands_read(1, true);
        case OS_prefetch1:
            return read_operands_prefetch(0);
        case OS_prefetch2:
            return read_operands_prefetch(1);
        case OS_pause1:
        case OS_pause2:
            r = (state.operands.state[OS_pause1] > 0) ? OS_pause1 : OS_pause2;
            state.operands.state[r] = 0;
            start_wait(2, S_read_operands);
            return;
        nodefault
    }
}

void core::sample_interrupts()
{
    if (!state.nmi) {
        if (pins.IPL <= regs.SR.I) return;
        if (pins.IPL == 0) return;
    }
    state.nmi = 0;
    state.exception.interrupt.on_next_instruction = 1;
    //dbg_printf("\nSET ONI cyc:%lld  IPL:%d   I:%d   %d", *trace.cycles, pins.IPL, regs.SR.I, state.nmi);
}

#define EXC_WRITE(val, n, desc) start_write(state.exception.group0.base_addr+n, val, 2, FC_supervisor_data, M68K_RW_ORDER_NORMAL, S_exc_group0)

void core::exc_interrupt()
{
    switch(state.exception.interrupt.TCU) {
        case 0:
            dec_SSP(6);
            state.exception.interrupt.base_addr = get_SSP();
            state.exception.interrupt.SR = get_SR();
            state.exception.interrupt.PC = regs.PC - 4;
            set_SR((state.exception.interrupt.SR & 0x5FFF) | 0x2000, 1);
            if (regs.SR.I < state.exception.interrupt.new_I) regs.SR.I = state.exception.interrupt.new_I;
            start_wait(6, S_exc_interrupt);
            break;
        case 1: // write SR
            start_write(state.exception.interrupt.base_addr, state.exception.interrupt.SR, 2, MAKE_FC(0), M68K_RW_ORDER_NORMAL, S_exc_interrupt);
            break;
        case 2: // do interrupt acknowledge cycle
            state.current = S_bus_cycle_iaq;
            state.bus_cycle_iaq.TCU = 0;
            state.bus_cycle.e_phase = 0;
            break;
        case 3:
            state.internal_interrupt_level = state.bus_cycle_iaq.ilevel;
            start_wait(4, S_exc_interrupt);
            for (auto h : iack_handlers) {
                h.handler(h.ptr);
            }
            break;
        case 4: // PC HI
            start_write(state.exception.interrupt.base_addr + 4, state.exception.interrupt.PC & 0xFFFF, 2, MAKE_FC(0), M68K_RW_ORDER_NORMAL, S_exc_interrupt);
            break;
        case 5: // PC LO
            start_write(state.exception.interrupt.base_addr + 2, state.exception.interrupt.PC >> 16, 2, MAKE_FC(0), M68K_RW_ORDER_NORMAL, S_exc_interrupt);
            break;
        case 6: // Read vector
            start_read(state.bus_cycle_iaq.vector_number << 2, 4, MAKE_FC(0), M68K_RW_ORDER_NORMAL, S_exc_interrupt);
            break;
        case 7: // Prefetch
            regs.PC = state.bus_cycle.data;
            start_prefetch(1, 1, S_exc_interrupt);
            break;
        case 8: // Idle
            start_wait(2, S_exc_interrupt);
            break;
        case 9: // Final prefetch before start executing
            start_prefetch(1, 1, S_decode);
            break;
        nodefault;
    }
    state.exception.interrupt.TCU++;
}

void core::exc_group12() {
    switch(state.exception.group12.TCU) {
        case 0: {
            // SR
            // PCH
            // PCL
            dec_SSP(6);
            state.exception.group12.base_addr = get_SSP();
            state.exception.group12.SR = get_SR();
            set_SR((state.exception.group12.SR & 0x5FFF) | 0x2000, 1);
            start_write(state.exception.group12.base_addr + 4, state.exception.group12.PC & 0xFFFF, 2, MAKE_FC(0), M68K_RW_ORDER_NORMAL, S_exc_group12);
            break; }
        case 1:
            start_write(state.exception.group12.base_addr, state.exception.group12.SR, 2, MAKE_FC(0), M68K_RW_ORDER_NORMAL, S_exc_group12);
            break;
        case 2:
            start_write(state.exception.group12.base_addr + 2, state.exception.group12.PC >> 16, 2, MAKE_FC(0), M68K_RW_ORDER_NORMAL, S_exc_group12);
            break;
        case 3:
            start_read(state.exception.group12.vector << 2, 4, MAKE_FC(0), M68K_RW_ORDER_NORMAL, S_exc_group12);
            break;
        case 4:
            regs.PC = state.bus_cycle.data;
            start_prefetch(1, 1, S_exc_group12);
            break;
        case 5:
            start_wait(2, S_exc_group12);
            break;
        case 6:
            start_prefetch(1, 1, S_decode);
            break;
            nodefault
        }
    state.exception.group12.TCU++;
}

void core::exc_group0()
{
    switch(state.exception.group0.TCU) {
        case 0: { // Save variables and write first word (FC, I/N, R/W). set S, unset T. write PC lo
            //state.exception.group0.addr = state.bus_cycle.addr;
            state.exception.group0.IR = regs.IRD;
            state.exception.group0.PC = regs.IPC; // TODO: should be in vicinity even if there's a jump somehow
            state.exception.group0.SR = get_SR();
            //set_SR((get_SR() | 0x2000) & 0x7FFF); // set bit 13 (S)
            set_SR((get_SR() & 0b0000011100011111) | 0x2000, 1);
            dec_SSP(14);
            state.exception.group0.base_addr = get_SSP();
            state.exception.group0.first_word = (pins.FC) | ((state.exception.group0.normal_state ^ 1) << 3) | ((pins.RW ^ 1) << 4);
            state.exception.group0.first_word |= (regs.IRD & 0xFFE0);
            // 2046   +12 PC lo
            // 2042   +8  SR
            // 2044   +10 PC hi
            // 2040   +6  IR
            // 2038   +4  access lo
            // 2034   +0  first word
            // 2036   +2  access hi
            //
            EXC_WRITE(state.exception.group0.PC & 0xFFFF, 12, "PC LO");
            break; }
        case 1: { // write SR
            EXC_WRITE(state.exception.group0.SR, 8, "SR");
            break; }
        case 2: { // write PC hi
            EXC_WRITE(state.exception.group0.PC >> 16, 10, "PC HI");
            break; }
        case 3: { // write IR
            EXC_WRITE(state.exception.group0.IR, 6, "IR");
            break; }
        case 4: { // write addr lo
            EXC_WRITE(state.exception.group0.addr & 0xFFFF, 4, "ADDR LO");
            break; }
        case 5: { // write first word
            EXC_WRITE(state.exception.group0.first_word, 0, "FIRST WORD");
            break; }
        case 6: { // write addr hi
            EXC_WRITE(state.exception.group0.addr >> 16, 2, "ADDR HI");
            break; }
        case 7: { // read vector
            start_read(state.exception.group0.vector << 2, 4, FC_supervisor_data, M68K_RW_ORDER_NORMAL, S_exc_group0);
            break; }
        case 8: {
            //start_wait(2, S_exc_group0);
            break; }
        case 9: { // Load prefetches and go!
            regs.PC = state.bus_cycle.data;
            start_prefetch(1, 1, S_exc_group0);
            break; }
        case 10:
            start_wait(2, S_exc_group0);
            break;
        case 11:
            start_prefetch(1, 1, S_decode);
            break;
        default: NOGOHERE;
    }
    state.exception.group0.TCU++;
}

static i32 sgn32(u32 num, u32 sz) {
    switch(sz) {
        case 1:
            return static_cast<i8>(num);
        case 2:
            return static_cast<i16>(num);
        case 4:
            return static_cast<i32>(num);
        default:
            NOGOHERE;
    }
}

void core::set_r(const EA *ea, const u32 val, const u32 sz)
{
    u32 v;
    switch(ea->kind) {
        case AM_data_register_direct:
            v = clip32[sz] & val;
            regs.D[ea->reg] = v;
            return;
        case AM_address_register_direct:
            v = static_cast<u32>(sgn32(clip32[sz] & val, sz));
            regs.A[ea->reg] = v;
            return;
        case AM_quick_immediate:
        case AM_immediate:
            //v = ea->reg;
            //break;
        nodefault
    }
}

u32 core::serialize_func() const
{
    if (state.bus_cycle.func == &core::bus_cycle_read)
        return 1;
    if (state.bus_cycle.func == &core::bus_cycle_write)
        return 2;
    return 0;
}

void core::deserialize_func(u32 v)
{
    if (v == 1)
        state.bus_cycle.func = &core::bus_cycle_read;
    else if (v == 2)
        state.bus_cycle.func = &core::bus_cycle_write;
}

}