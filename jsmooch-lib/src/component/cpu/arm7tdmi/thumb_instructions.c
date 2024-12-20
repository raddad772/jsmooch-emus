//
// Created by . on 12/18/24.
//
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "thumb_instructions.h"

#define UNIMPLEMENTED printf("\nUNIMPLEMENTED INSTRUCTION! %08x", ins->sub_opcode); assert(1==2)

void ARM7TDMI_THUMB_ins_INVALID(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_THUMB_ins_ADD_SUB(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_THUMB_ins_LSL_LSR_ASR(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_THUMB_ins_MOV_CMP_ADD_SUB(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_THUMB_ins_data_proc(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_THUMB_ins_BX(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_THUMB_ins_ADD_CMP_MOV_hi(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_THUMB_ins_LDR_PC_relative(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_THUMB_ins_LDRH_STRH_reg_offset(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_THUMB_ins_LDRSH_LDRSB_reg_offset(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_THUMB_ins_LDR_STR_reg_offset(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_THUMB_ins_LDRB_STRB_reg_offset(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_THUMB_ins_LDR_STR_imm_offset(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_THUMB_ins_LDRB_STRB_imm_offset(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_THUMB_ins_LDRH_STRH_imm_offset(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_THUMB_ins_LDR_STR_SP_relative(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_THUMB_ins_ADD_SP_or_PC(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_THUMB_ins_ADD_SUB_SP(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_THUMB_ins_PUSH_POP(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_THUMB_ins_LDM_STM(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_THUMB_ins_SWI(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_THUMB_ins_UNDEFINED_BCC(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_THUMB_ins_BCC(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_THUMB_ins_B(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_THUMB_ins_BL_BLX_prefix(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

void ARM7TDMI_THUMB_ins_BL_suffix(struct ARM7TDMI *this, struct thumb_instruction *ins)
{
    UNIMPLEMENTED;
}

