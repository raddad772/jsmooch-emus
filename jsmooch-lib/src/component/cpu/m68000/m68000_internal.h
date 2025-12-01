//
// Created by . on 5/15/24.
//

#pragma once
#include "helpers/int.h"

namespace M68k {

enum address_modes {
    AM_data_register_direct = 0,
    AM_address_register_direct = 1,
    AM_address_register_indirect = 2,
    AM_address_register_indirect_with_postincrement = 3,
    AM_address_register_indirect_with_predecrement = 4,
    AM_address_register_indirect_with_displacement = 5,
    AM_address_register_indirect_with_index = 6,
    AM_absolute_short_data = 7,
    AM_absolute_long_data = 8,
    AM_program_counter_with_displacement = 9,
    AM_program_counter_with_index = 10,
    AM_quick_immediate = 11,
    AM_imm16 = 51,
    AM_imm32 = 52,
    AM_immediate = 53,
};

enum operand_modes {
    OM_none = 0,
    OM_r = 1,
    OM_r_r = 2,
    OM_ea_r = 3,
    OM_r_ea = 4,
    OM_ea = 5,
    OM_ea_ea = 6,
    OM_qimm = 7,
    OM_qimm_qimm = 8,
    OM_qimm_r = 9,
    OM_qimm_ea = 10,
    OM_imm16 = 11
};

struct EA {
    address_modes kind;
    u32 reg;
};

#define M68K_RW_ORDER_NORMAL 0
#define M68K_RW_ORDER_REVERSE 1

#define MAKE_FC(is_program) ((this->regs.SR.S ? 4 : 0) | ((is_program) ? 2 : 1))
u32 AM_ext_words(address_modes am, u32 sz);
}