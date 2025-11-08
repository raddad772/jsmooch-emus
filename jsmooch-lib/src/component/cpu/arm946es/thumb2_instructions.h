//
// Created by . on 1/18/25.
//

#ifndef JSMOOCH_EMUS_THUMB2_INSTRUCTIONS_H
#define JSMOOCH_EMUS_THUMB2_INSTRUCTIONS_H

#include "helpers/int.h"
struct ARM946ES;

struct thumb2_instruction;
struct thumb2_instruction {
    void (*func)(struct ARM946ES *, struct thumb2_instruction *);
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

void ARM946ES_THUMB_ins_INVALID(struct ARM946ES *, struct thumb2_instruction *ins);
void ARM946ES_THUMB_ins_ADD_SUB(struct ARM946ES *, struct thumb2_instruction *ins);
void ARM946ES_THUMB_ins_LSL_LSR_ASR(struct ARM946ES *, struct thumb2_instruction *ins);
void ARM946ES_THUMB_ins_MOV_CMP_ADD_SUB(struct ARM946ES *, struct thumb2_instruction *ins);
void ARM946ES_THUMB_ins_data_proc(struct ARM946ES *, struct thumb2_instruction *ins);
void ARM946ES_THUMB_ins_BX_BLX(struct ARM946ES *, struct thumb2_instruction *ins);
void ARM946ES_THUMB_ins_ADD_CMP_MOV_hi(struct ARM946ES *, struct thumb2_instruction *ins);
void ARM946ES_THUMB_ins_LDR_PC_relative(struct ARM946ES *, struct thumb2_instruction *ins);
void ARM946ES_THUMB_ins_LDRH_STRH_reg_offset(struct ARM946ES *, struct thumb2_instruction *ins);
void ARM946ES_THUMB_ins_LDRSH_LDRSB_reg_offset(struct ARM946ES *, struct thumb2_instruction *ins);
void ARM946ES_THUMB_ins_LDR_STR_reg_offset(struct ARM946ES *, struct thumb2_instruction *ins);
void ARM946ES_THUMB_ins_LDRB_STRB_reg_offset(struct ARM946ES *, struct thumb2_instruction *ins);
void ARM946ES_THUMB_ins_LDR_STR_imm_offset(struct ARM946ES *, struct thumb2_instruction *ins);
void ARM946ES_THUMB_ins_LDRB_STRB_imm_offset(struct ARM946ES *, struct thumb2_instruction *ins);
void ARM946ES_THUMB_ins_LDRH_STRH_imm_offset(struct ARM946ES *, struct thumb2_instruction *ins);
void ARM946ES_THUMB_ins_LDR_STR_SP_relative(struct ARM946ES *, struct thumb2_instruction *ins);
void ARM946ES_THUMB_ins_ADD_SP_or_PC(struct ARM946ES *, struct thumb2_instruction *ins);
void ARM946ES_THUMB_ins_ADD_SUB_SP(struct ARM946ES *, struct thumb2_instruction *ins);
void ARM946ES_THUMB_ins_PUSH_POP(struct ARM946ES *, struct thumb2_instruction *ins);
void ARM946ES_THUMB_ins_LDM_STM(struct ARM946ES *, struct thumb2_instruction *ins);
void ARM946ES_THUMB_ins_SWI(struct ARM946ES *, struct thumb2_instruction *ins);
void ARM946ES_THUMB_ins_UNDEFINED_BCC(struct ARM946ES *, struct thumb2_instruction *ins);
void ARM946ES_THUMB_ins_BCC(struct ARM946ES *, struct thumb2_instruction *ins);
void ARM946ES_THUMB_ins_B(struct ARM946ES *, struct thumb2_instruction *ins);
void ARM946ES_THUMB_ins_BL_BLX_prefix(struct ARM946ES *, struct thumb2_instruction *ins);
void ARM946ES_THUMB_ins_BL_suffix(struct ARM946ES *, struct thumb2_instruction *ins);
void ARM946ES_THUMB_ins_BKPT(struct ARM946ES *, struct thumb2_instruction *ins);
void ARM946ES_THUMB_ins_BLX_suffix(struct ARM946ES *, struct thumb2_instruction *ins);

#endif //JSMOOCH_EMUS_THUMB2_INSTRUCTIONS_H
