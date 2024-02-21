//
// Created by Dave on 2/10/2024.
//

#ifndef JSMOOCH_EMUS_SH4_INTERPRETER_OPCODES_H
#define JSMOOCH_EMUS_SH4_INTERPRETER_OPCODES_H

struct SH4_ins_t;

struct SH4;
typedef void (*SH4_ins_func)(struct SH4* sh4, struct SH4_ins_t *ins);
struct SH4_ins_t {
    u16 opcode;
    u32 Rm;
    u32 Rn;
    i32 disp;
    u32 imm;
    SH4_ins_func exec;
    u32 decoded;
};

void do_sh4_decode();
struct sh4_str_ret
{
    u32 n_max, m_max, d_max, i_max;
    u32 n_shift, m_shift, d_shift, i_shift;
    u32 n_mask, m_mask, d_mask, i_mask;
    u32 mask;
};
struct SH4;

extern struct SH4_ins_t SH4_decoded[65536];
extern char SH4_disassembled[65536][30];
extern char SH4_mnemonic[65536][30];
void SH4_interrupt_IRL(struct SH4* this, u32 level);

#endif //JSMOOCH_EMUS_SH4_INTERPRETER_OPCODES_H
