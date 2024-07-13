//
// Created by . on 5/15/24.
//

#ifndef JSMOOCH_EMUS_M68000_INSTRUCTIONS_H
#define JSMOOCH_EMUS_M68000_INSTRUCTIONS_H

#include "helpers/int.h"
#include "helpers/debug.h"
#include "m68000_internal.h"

struct M68k;
struct M68k_ins_t;

typedef void (*M68k_ins_func)(struct M68k* this, struct M68k_ins_t *ins);
typedef void (*M68k_disassemble_t)(struct M68k_ins_t *ins, u32 PC, struct jsm_debug_read_trace *rt, struct jsm_string *out);

struct M68k_ins_t {
    u16 opcode;
    u32 sz;
    u32 variant;
    M68k_ins_func exec;
    M68k_disassemble_t disasm;
    enum M68k_operand_modes operand_mode;
    struct M68k_EA ea[2];
};

struct M68k;

void do_M68k_decode(); // call this before using core

extern struct M68k_ins_t M68k_decoded[65536];
void M68k_ins_RESET_POWER(struct M68k* this, struct M68k_ins_t *ins);
void M68k_ins_DIVU(struct M68k* this, struct M68k_ins_t *ins);
void M68k_ins_MOVEM_TO_REG(struct M68k* this, struct M68k_ins_t *ins);
void M68k_ins_MOVE(struct M68k* this, struct M68k_ins_t *ins);

#endif //JSMOOCH_EMUS_M68000_INSTRUCTIONS_H
