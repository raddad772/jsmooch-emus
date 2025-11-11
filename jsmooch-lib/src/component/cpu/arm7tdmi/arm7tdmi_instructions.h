//
// Created by . on 12/4/24.
//

#ifndef JSMOOCH_EMUS_ARM7TDMI_INSTRUCTIONS_H
#define JSMOOCH_EMUS_ARM7TDMI_INSTRUCTIONS_H

#include "helpers/int.h"

struct ARM7TDMI;
void ARM7TDMI_ins_MUL_MLA(ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_MULL_MLAL(ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_SWP(ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_LDRH_STRH(ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_LDRSB_LDRSH(ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_MRS(ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_MSR_reg(ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_MSR_imm(ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_BX(ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_data_proc_immediate_shift(ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_data_proc_register_shift(ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_undefined_instruction(ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_data_proc_immediate(ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_LDR_STR_immediate_offset(ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_LDR_STR_register_offset(ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_LDM_STM(ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_B_BL(ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_STC_LDC(ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_CDP(ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_MCR_MRC(ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_SWI(ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_INVALID(ARM7TDMI *, u32 opcode);


#endif //JSMOOCH_EMUS_ARM7TDMI_INSTRUCTIONS_H
