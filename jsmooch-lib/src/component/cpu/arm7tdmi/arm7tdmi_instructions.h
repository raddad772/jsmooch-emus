//
// Created by . on 12/4/24.
//

#ifndef JSMOOCH_EMUS_ARM7TDMI_INSTRUCTIONS_H
#define JSMOOCH_EMUS_ARM7TDMI_INSTRUCTIONS_H

#include "helpers/int.h"

struct ARM7TDMI;
void ARM7TDMI_ins_MUL_MLA(struct ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_MULL_MLAL(struct ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_SWP(struct ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_LDRH_STRH(struct ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_LDRSB_LDRSH(struct ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_MRS(struct ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_MSR_reg(struct ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_MSR_imm(struct ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_BX(struct ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_data_proc_immediate_shift(struct ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_data_proc_register_shift(struct ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_undefined_instruction(struct ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_data_processing_immediate(struct ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_LDR_STR_immediate_offset(struct ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_LDR_STR_register_offset(struct ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_LDM_STM(struct ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_B_BL(struct ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_STC_LDC(struct ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_CDP(struct ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_MCR_MRC(struct ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_SWI(struct ARM7TDMI *, u32 opcode);
void ARM7TDMI_ins_INVALID(struct ARM7TDMI *, u32 opcode);


#endif //JSMOOCH_EMUS_ARM7TDMI_INSTRUCTIONS_H
