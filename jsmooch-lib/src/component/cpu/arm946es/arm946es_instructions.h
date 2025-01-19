//
// Created by . on 1/18/25.
//

#ifndef JSMOOCH_EMUS_ARM946ES_INSTRUCTIONS_H
#define JSMOOCH_EMUS_ARM946ES_INSTRUCTIONS_H

#include "helpers/int.h"

struct ARM946ES;
void ARM946ES_ins_MUL_MLA(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_MULL_MLAL(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_SWP(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_LDRH_STRH(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_LDRSB_LDRSH(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_MRS(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_MSR_reg(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_MSR_imm(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_BX(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_data_proc_immediate_shift(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_data_proc_register_shift(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_undefined_instruction(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_data_proc_immediate(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_LDR_STR_immediate_offset(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_LDR_STR_register_offset(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_LDM_STM(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_B_BL(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_STC_LDC(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_CDP(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_MCR_MRC(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_SWI(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_INVALID(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_PLD(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_SMLAxy(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_SMLAWy(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_SMULWy(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_SMLALxy(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_SMULxy(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_LDRD_STRD(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_CLZ(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_BLX_reg(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_QADD_QSUB_QDADD_QDSUB(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_BKPT(struct ARM946ES *, u32 opcode);
void ARM946ES_ins_B_BL_BLX(struct ARM946ES *, u32 opcode);

#endif //JSMOOCH_EMUS_ARM946ES_INSTRUCTIONS_H
