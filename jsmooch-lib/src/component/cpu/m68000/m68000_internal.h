//
// Created by . on 5/15/24.
//

#ifndef JSMOOCH_EMUS_M68000_INTERNAL_H
#define JSMOOCH_EMUS_M68000_INTERNAL_H

#include "helpers/int.h"

enum M68k_address_modes {
    M68k_AM_data_register_direct = 0,
    M68k_AM_address_register_direct = 1,
    M68k_AM_address_register_indirect = 2,
    M68k_AM_address_register_indirect_with_postincrement = 3,
    M68k_AM_address_register_indirect_with_predecrement = 4,
    M68k_AM_address_register_indirect_with_displacement = 5,
    M68k_AM_address_register_indirect_with_index = 6,
    M68k_AM_absolute_short_data = 7,
    M68k_AM_absolute_long_data = 8,
    M68k_AM_program_counter_with_displacement = 9,
    M68k_AM_program_counter_with_index = 10,
    M68k_AM_quick_immediate,
    M68k_AM_implied,
    M68k_AM_none = 50,
    M68k_AM_imm16 = 51,
    M68k_AM_imm32 = 52,
    M68k_AM_immediate = 53,
    M68k_AM_bad = 54,
};

enum M68k_operand_modes {
    M68k_OM_none = 0,
    M68k_OM_r = 1,
    M68k_OM_r_r = 2,
    M68k_OM_ea_r = 3,
    M68k_OM_r_ea = 4,
    M68k_OM_ea = 5,
    M68k_OM_ea_ea = 6,
    M68k_OM_qimm = 7,
    M68k_OM_qimm_qimm = 8,
    M68k_OM_qimm_r = 9,
    M68k_OM_qimm_ea = 10,
    M68k_OM_imm16 = 11
};

struct M68k_EA {
    enum M68k_address_modes kind;
    u32 reg;
};

#define M68K_RW_ORDER_NORMAL 0
#define M68K_RW_ORDER_REVERSE 1

struct M68k;
void M68k_start_read(struct M68k* this, u32 addr, u32 sz, u32 FC, u32 reversed, u32 next_state);
void M68k_start_write(struct M68k* this, u32 addr, u32 val, u32 sz, u32 FC, u32 reversed, u32 next_state);
void M68k_start_prefetch(struct M68k* this, u32 num, u32 is_program, u32 next_state);
void M68k_start_read_operands(struct M68k* this, u32 fast, u32 sz, u32 next_state, u32 wait_states, u32 hold, u32 allow_reverse);
void M68k_start_read_operand_for_ea(struct M68k* this, u32 fast, u32 sz, u32 next_state, u32 wait_states, u32 hold, u32 allow_reverse);
void M68k_start_write_operand(struct M68k* this, u32 commit, u32 op_num, u32 next_state, u32 allow_reverse, u32 force_reverse);
u32 M68k_AM_ext_words(enum M68k_address_modes am, u32 sz);
void M68k_start_wait(struct M68k* this, u32 num, u32 state_after);
u32 M68k_read_ea_addr(struct M68k* this, uint32 opnum, u32 sz, u32 hold, u32 prefetch);
void M68k_start_group0_exception(struct M68k* this, u32 vector_number, i32 wait_cycles, u32 was_in_group0_or_1, u32 addr);
void M68k_start_group1_exception(struct M68k* this, u32 vector_number, i32 wait_cycles);
void M68k_start_group2_exception(struct M68k* this, u32 vector_number, i32 wait_cycles, u32 PC);
u32 M68k_inc_SSP(struct M68k* this, u32 num);
u32 M68k_dec_SSP(struct M68k* this, u32 num);
u32 M68k_get_SSP(struct M68k* this);
void M68k_set_ar(struct M68k* this, u32 num, u32 result, u32 sz);
void M68k_set_dr(struct M68k* this, u32 num, u32 result, u32 sz);
void M68k_swap_ASP(struct M68k* this);
void M68k_exc_group1(struct M68k* this);
void M68k_exc_group2(struct M68k* this);
void M68k_exc_group0(struct M68k* this);
void M68k_prefetch(struct M68k* this);
void M68k_read_operands_read(struct M68k* this, u32 opnum);
void M68k_read_operands(struct M68k* this);
u32 M68k_get_r(struct M68k* this, struct M68k_EA *ea, u32 sz);
void M68k_set_r(struct M68k* this, struct M68k_EA *ea, u32 val, u32 sz);
void M68k_finalize_ea(struct M68k* this, u32 opnum);
void M68k_read_operands_prefetch(struct M68k* this, u32 opnum);

#define MAKE_FC(is_program) ((this->regs.SR.S << 2) | ((is_program) ? 1 : 2))


#endif //JSMOOCH_EMUS_M68000_INTERNAL_H
