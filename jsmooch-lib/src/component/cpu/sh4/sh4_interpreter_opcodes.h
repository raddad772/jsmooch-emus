//
// Created by Dave on 2/10/2024.
//

#pragma once
namespace SH4 {
struct ins_t;
struct core;
typedef void (core::*ins_func)(ins_t *ins);
struct ins_t {
    u16 opcode{};
    i16 Rm{};
    i16 Rn{};
    i32 disp{};
    u32 imm{};
    ins_func exec{};
    bool decoded{};
};

struct ins_t_encoding {
    u16 mask{};
    u32 sz{};
    u32 pr{};
};

#define SH4_SZPR(sz,pr) (((sz) << 1) | (pr))

void do_sh4_decode();
struct sh4_str_ret
{
    u32 n_max{}, m_max{}, d_max{}, i_max{};
    u32 n_shift{}, m_shift{}, d_shift{}, i_shift{};
    u32 n_mask{}, m_mask{}, d_mask{}, i_mask{};
    u32 mask{};
};

// based on PR bit
// SZ << 1 | PR
#define SH4_decoded_index ((this->regs.FPSCR.SZ << 1) | this->regs.FPSCR.PR)
extern ins_t decoded[4][65536];
extern char disassembled[4][65536][30];
extern char mnemonic[4][65536][30];
extern ins_t decoded_by_encoding[65536];
}