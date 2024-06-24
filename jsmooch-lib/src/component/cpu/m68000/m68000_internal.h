//
// Created by . on 5/15/24.
//

#ifndef JSMOOCH_EMUS_M68000_INTERNAL_H
#define JSMOOCH_EMUS_M68000_INTERNAL_H

#include "helpers/int.h"

enum M68k_address_modes {
    M68k_AM_data_register_direct,
    M68k_AM_address_register_direct,
    M68k_AM_address_register_indirect,
    M68k_AM_address_register_indirect_with_postincrement,
    M68k_AM_address_register_indirect_with_predecrement,
    M68k_AM_address_register_indirect_with_displacement,
    M68k_AM_address_register_indirect_with_index,
    M68k_AM_absolute_short_data,
    M68k_AM_absolute_long_data,
    M68k_AM_program_counter_with_displacement,
    M68k_AM_program_counter_with_index,
    M68k_AM_quick_immediate,
    M68k_AM_implied,
    M68k_AM_none = 50,
    M68k_AM_imm16 = 51,
    M68k_AM_imm32 = 52,
    M68k_AM_immediate = 53,
    M68k_AM_bad = 54,
};

enum M68k_operand_modes {
    M68k_OM_none,
    M68k_OM_r,
    M68k_OM_r_r,
    M68k_OM_ea_r,
    M68k_OM_r_ea,
    M68k_OM_ea,
    M68k_OM_ea_ea,
    M68k_OM_qimm,
    M68k_OM_qimm_qimm,
    M68k_OM_qimm_r,
    M68k_OM_qimm_ea,
};

struct M68k_DR {
    u32 num;
};

struct M68k_AR {
    u32 num;
};

struct M68k_EA {
    enum M68k_address_modes kind;
    u32 reg;
};

struct M68k;
void M68k_start_read(struct M68k* this, u32 addr, u32 sz, u32 FC, u32 next_state);
void M68k_start_write(struct M68k* this, u32 addr, u32 val, u32 sz, u32 FC, u32 next_state);
void M68k_start_prefetch(struct M68k* this, u32 num, u32 next_state);
void M68k_start_read_operands(struct M68k* this, u32 fast, u32 hold, u32 next_state);
void M68k_start_write_operand(struct M68k* this, u32 hold, u32 op_num, u32 next_state);
void M68k_start_wait(struct M68k* this, u32 num, u32 state_after);
u32 M68k_write_ea_addr(struct M68k* this, struct M68k_EA *ea, u32 sz, u32 hold, u32 opnum);
void M68k_transition_to_supervisor(struct M68k* this);
void M68k_transition_to_user(struct M68k* this);
void M68k_start_group0_exception(struct M68k* this, u32 vector_number, i32 wait_cycles, u32 was_in_group0_or_1);
void M68k_start_group1_exception(struct M68k* this);

#define MAKE_FC(is_program) ((this->regs.SR.S << 2) | ((is_program) ? 1 : 2))


#endif //JSMOOCH_EMUS_M68000_INTERNAL_H
