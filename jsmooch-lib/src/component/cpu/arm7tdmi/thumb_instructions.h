//
// Created by . on 12/18/24.
//

#ifndef JSMOOCH_EMUS_THUMB_INSTRUCTIONS_H
#define JSMOOCH_EMUS_THUMB_INSTRUCTIONS_H
/*
        THUMB_ADD_SUB,
        THUMB_LSL_LSR_ASR,
        THUMB_MOV_CMP_ADD_SUB,
        THUMB_data_proc,
        THUMB_BX,
        THUMB_ADD_CMP_MOV_hi,
        THUMB_LDR_PC_relative,
        THUMB_LDRH_STRH_reg_offset,
        THUMB_LDRSH_LDRSB_reg_offset,
        THUMB_LDR_STR_reg_offset,
        THUMB_LDRB_STRB_reg_offset,
        THUMB_LDR_STR_imm_offset,
        THUMB_LDRB_STRB_imm_offset,
        THUMB_LDRH_STRH_imm_offset,
        THUMB_LDR_STR_SP_relative,
        THUMB_ADD_SP_or_PC,
        THUMB_ADD_SUB_SP,
        THUMB_PUSH_POP,
        THUMB_LDM_STM,
        THUMB_SWI,
        THUMB_UNDEFINED_BCC,
        THUMB_BCC,
        THUMB_B,
        THUMB_BL_BLX_prefix,
        THUMB_BL_suffix
 */
#include "helpers/int.h"
struct ARM7TDMI;

struct thumb_instruction;
struct thumb_instruction {
    void (*func)(ARM7TDMI *, thumb_instruction *);
    u16 sub_opcode;
    u16 opcode;
    u16 Rd, Rs;
    union {
        u16 Ro;
        u16 Rm;
    };
    union {
        u16 Rn;
        u16 Rb;
    };
    union {
        u16 L;
        u16 I;
        u16 B;
        u16 SP;
        u16 S;
        u16 PC_LR;
    };
    union {
        u32 imm;
        u16 rlist;
        u16 comment;
    };
};

/*
        THUMB_ADD_SUB,
        THUMB_LSL_LSR_ASR,
        THUMB_MOV_CMP_ADD_SUB,
        THUMB_data_proc,
        THUMB_BX,
        THUMB_ADD_CMP_MOV_hi,
        THUMB_LDR_PC_relative,
        THUMB_LDRH_STRH_reg_offset,
        THUMB_LDRSH_LDRSB_reg_offset,
        THUMB_LDR_STR_reg_offset,
        THUMB_LDRB_STRB_reg_offset,
        THUMB_LDR_STR_imm_offset,
        THUMB_LDRB_STRB_imm_offset,
        THUMB_LDRH_STRH_imm_offset,
        THUMB_LDR_STR_SP_relative,
        THUMB_ADD_SP_or_PC,
        THUMB_ADD_SUB_SP,
        THUMB_PUSH_POP,
        THUMB_LDM_STM,
        THUMB_SWI,
        THUMB_UNDEFINED_BCC,
        THUMB_BCC,
        THUMB_B,
        THUMB_BL_BLX_prefix,
        THUMB_BL_suffix
 */
void ARM7TDMI_THUMB_ins_INVALID(ARM7TDMI *, thumb_instruction *ins);
void ARM7TDMI_THUMB_ins_ADD_SUB(ARM7TDMI *, thumb_instruction *ins);
void ARM7TDMI_THUMB_ins_LSL_LSR_ASR(ARM7TDMI *, thumb_instruction *ins);
void ARM7TDMI_THUMB_ins_MOV_CMP_ADD_SUB(ARM7TDMI *, thumb_instruction *ins);
void ARM7TDMI_THUMB_ins_data_proc(ARM7TDMI *, thumb_instruction *ins);
void ARM7TDMI_THUMB_ins_BX(ARM7TDMI *, thumb_instruction *ins);
void ARM7TDMI_THUMB_ins_ADD_CMP_MOV_hi(ARM7TDMI *, thumb_instruction *ins);
void ARM7TDMI_THUMB_ins_LDR_PC_relative(ARM7TDMI *, thumb_instruction *ins);
void ARM7TDMI_THUMB_ins_LDRH_STRH_reg_offset(ARM7TDMI *, thumb_instruction *ins);
void ARM7TDMI_THUMB_ins_LDRSH_LDRSB_reg_offset(ARM7TDMI *, thumb_instruction *ins);
void ARM7TDMI_THUMB_ins_LDR_STR_reg_offset(ARM7TDMI *, thumb_instruction *ins);
void ARM7TDMI_THUMB_ins_LDRB_STRB_reg_offset(ARM7TDMI *, thumb_instruction *ins);
void ARM7TDMI_THUMB_ins_LDR_STR_imm_offset(ARM7TDMI *, thumb_instruction *ins);
void ARM7TDMI_THUMB_ins_LDRB_STRB_imm_offset(ARM7TDMI *, thumb_instruction *ins);
void ARM7TDMI_THUMB_ins_LDRH_STRH_imm_offset(ARM7TDMI *, thumb_instruction *ins);
void ARM7TDMI_THUMB_ins_LDR_STR_SP_relative(ARM7TDMI *, thumb_instruction *ins);
void ARM7TDMI_THUMB_ins_ADD_SP_or_PC(ARM7TDMI *, thumb_instruction *ins);
void ARM7TDMI_THUMB_ins_ADD_SUB_SP(ARM7TDMI *, thumb_instruction *ins);
void ARM7TDMI_THUMB_ins_PUSH_POP(ARM7TDMI *, thumb_instruction *ins);
void ARM7TDMI_THUMB_ins_LDM_STM(ARM7TDMI *, thumb_instruction *ins);
void ARM7TDMI_THUMB_ins_SWI(ARM7TDMI *, thumb_instruction *ins);
void ARM7TDMI_THUMB_ins_UNDEFINED_BCC(ARM7TDMI *, thumb_instruction *ins);
void ARM7TDMI_THUMB_ins_BCC(ARM7TDMI *, thumb_instruction *ins);
void ARM7TDMI_THUMB_ins_B(ARM7TDMI *, thumb_instruction *ins);
void ARM7TDMI_THUMB_ins_BL_BLX_prefix(ARM7TDMI *, thumb_instruction *ins);
void ARM7TDMI_THUMB_ins_BL_suffix(ARM7TDMI *, thumb_instruction *ins);

#endif //JSMOOCH_EMUS_THUMB_INSTRUCTIONS_H
