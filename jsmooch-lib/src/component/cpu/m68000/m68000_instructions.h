//
// Created by . on 5/15/24.
//

#pragma once

#include "helpers/int.h"
#include "helpers/debug.h"
#include "m68000_internal.h"

namespace M68k {
struct core;
struct ins_t;

typedef void (*M68k_ins_func)(core*, ins_t *ins);
typedef void (*M68k_disassemble_t)(ins_t *ins, u32 *PC, jsm_debug_read_trace *rt, jsm_string *out);

struct ins_t {
    u16 opcode;
    u32 sz;
    u32 variant;
    ins_func exec;
    disassemble_t disasm;
    operand_modes operand_mode;
    EA ea[2];
};

void do_M68k_decode(); // call this before using core

extern struct ins_t M68k_decoded[65536];
void M68k_ins_RESET_POWER(core*, ins_t *ins);
void M68k_ins_DIVU(core*, ins_t *ins);
void M68k_ins_MOVEM_TO_REG(core*, ins_t *ins);
void M68k_ins_MOVE(core*, ins_t *ins);
}