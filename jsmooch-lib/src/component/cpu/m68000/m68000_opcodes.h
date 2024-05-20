//
// Created by . on 5/15/24.
//

#ifndef JSMOOCH_EMUS_M68000_OPCODES_H
#define JSMOOCH_EMUS_M68000_OPCODES_H

#include "helpers/int.h"
#include "m68000_internal.h"

struct M68k;
struct M68k_ins_t;

typedef void (*M68k_ins_func)(struct M68k* this, struct M68k_ins_t *ins);
typedef u32 (*M68k_disassemble_t)(struct M68k* this, struct M68k_ins_t *ins, char *ptr, u32 max_len);

struct M68k_ins_t {
    u16 opcode;
    u32 sz;
    M68k_ins_func exec;
    M68k_disassemble_t disasm;
    struct M68k_EA ea1, ea2;
    struct M68k_DR dr;
    struct M68k_AR ar;
};

struct M68k;


void do_M68k_decode();

extern struct M68k_ins_t M68k_decoded[65536];
extern M68k_disassemble_t M68k_disassembe[65536];
extern char M68k_mnemonic[65536][30];
void M68K_ins_RESET(struct M68k* this, struct M68k_ins_t *ins);

#endif //JSMOOCH_EMUS_M68000_OPCODES_H
