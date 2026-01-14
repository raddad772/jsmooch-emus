#pragma once

// Not even going to ask VS why this is necessary rn
#include "helpers/int.h"

namespace SM83 {

enum SM83_MN {
    sNONE = 0,
    ADC_di_da = 1,
    ADC_di_di = 2,
    ADC_di_ind = 3,
    ADD_di_da = 4,
    ADD_di_di = 5,
    ADD16_di_di = 6,
    ADD_di_ind = 7,
    ADD_di_rel = 8,
    AND_di_da = 9,
    AND_di_di = 10,
    AND_di_ind = 11,
    BIT_idx_di = 12,
    BIT_idx_ind = 13,
    CALL_cond_addr = 14,
    CCF = 15,
    CP_di_da = 16,
    CP_di_di = 17,
    CP_di_ind = 18,
    CPL = 19,
    DAA = 20,
    DEC_di = 21,
    DEC16_di = 22,
    DEC_ind = 23,
    DI = 24,
    EI = 25,
    HALT = 26,
    INC_di = 27,
    INC16_di = 28,
    INC_ind = 29,
    JP_cond_addr = 30,
    JP_di = 31,
    JR_cond_rel = 32,
    LD_addr_di = 33,
    LD16_addr_di = 34,
    LD_di_addr = 35,
    LD_di_da = 36,
    LD16_di_da = 37,
    LD_di_di = 38,
    LD16_di_di = 39,
    LD_di_di_rel = 40,
    LD_di_ind = 41,
    LD_di_ind_dec = 42,
    LD_di_ind_inc = 43,
    LD_ind_da = 44,
    LD_ind_di = 45,
    LD_ind_dec_di = 46,
    LD_ind_inc_di = 47,
    LDH_addr_di = 48,
    LDH_di_addr = 49,
    LDH_di_ind = 50,
    LDH_ind_di = 51,
    NOP = 52,
    OR_di_da = 53,
    OR_di_di = 54,
    OR_di_ind = 55,
    POP_di = 56,
    POP_di_AF = 57,
    PUSH_di = 58,
    RES_idx_di = 59,
    RES_idx_ind = 60,
    RET = 61,
    RET_cond = 62,
    RETI = 63,
    RL_di = 64,
    RL_ind = 65,
    RLA = 66,
    RLC_di = 67,
    RLC_ind = 68,
    RLCA = 69,
    RR_di = 70,
    RR_ind = 71,
    RRA = 72,
    RRC_di = 73,
    RRC_ind = 74,
    RRCA = 75,
    RST_imp = 76,
    SBC_di_da = 77,
    SBC_di_di = 78,
    SBC_di_ind = 79,
    SCF = 80,
    SET_idx_di = 81,
    SET_idx_ind = 82,
    SLA_di = 83,
    SLA_ind = 84,
    SRA_di = 85,
    SRA_ind = 86,
    SRL_di = 87,
    SRL_ind = 88,
    SUB_di_da = 89,
    SUB_di_di = 90,
    SUB_di_ind = 91,
    SWAP_di = 92,
    SWAP_ind = 93,
    STOP = 94,
    XOR_di_da = 95,
    XOR_di_di = 96,
    XOR_di_ind = 97,
    RESET = 98,
    S_IRQ = 99,
};

static constexpr u32 INS_RESET = 0x101;
static constexpr u32 INS_IRQ = 0x100;
static constexpr u32 INS_HALT = 0x76;
static constexpr u32 INS_DECODE = 0x102;
static constexpr u32 INS_MAX_OPCODE = 0x101;


struct regs;
struct pins;

typedef void (*ins_func)(regs &rgs, pins &pins);

struct opcode_info {
    u32 opcode;
    SM83_MN ins;
};

struct opcode_function {
    u8 opcode;
    SM83_MN ins;
    ins_func exec_func;
};
extern ins_func decoded_opcodes[0x202];

}
