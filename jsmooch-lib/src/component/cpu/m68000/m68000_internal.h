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
    M68k_AM_immediate = 53
};

struct M68k_DR {
    u32 num;
};

struct M68k_AR {
    u32 num;
};

struct M68k_EA {
    enum M68k_address_modes kind;
    u32 ea_mode, ea_register;
    u32 extension_words;
};

#endif //JSMOOCH_EMUS_M68000_INTERNAL_H
