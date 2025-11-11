//
// Created by . on 1/18/25.
//

#ifndef JSMOOCH_EMUS_ARM946ES_INSTRUCTIONS_H
#define JSMOOCH_EMUS_ARM946ES_INSTRUCTIONS_H

#include "helpers/int.h"

struct ARM946ES;
void ARM946ES_ins_MUL_MLA(ARM946ES *, u32 opcode);
void ARM946ES_ins_MULL_MLAL(ARM946ES *, u32 opcode);
void ARM946ES_ins_SWP(ARM946ES *, u32 opcode);
void ARM946ES_ins_LDRH_STRH(ARM946ES *, u32 opcode);
void ARM946ES_ins_LDRSB_LDRSH(ARM946ES *, u32 opcode);
void ARM946ES_ins_MRS(ARM946ES *, u32 opcode);
void ARM946ES_ins_MSR_reg(ARM946ES *, u32 opcode);
void ARM946ES_ins_MSR_imm(ARM946ES *, u32 opcode);
void ARM946ES_ins_BX(ARM946ES *, u32 opcode);
void ARM946ES_ins_data_proc_immediate_shift(ARM946ES *, u32 opcode);
void ARM946ES_ins_data_proc_register_shift(ARM946ES *, u32 opcode);
void ARM946ES_ins_undefined_instruction(ARM946ES *, u32 opcode);
void ARM946ES_ins_data_proc_immediate(ARM946ES *, u32 opcode);
void ARM946ES_ins_LDR_STR_immediate_offset(ARM946ES *, u32 opcode);
void ARM946ES_ins_LDR_STR_register_offset(ARM946ES *, u32 opcode);
void ARM946ES_ins_LDM_STM(ARM946ES *, u32 opcode);
void ARM946ES_ins_STC_LDC(ARM946ES *, u32 opcode);
void ARM946ES_ins_CDP(ARM946ES *, u32 opcode);
void ARM946ES_ins_MCR_MRC(ARM946ES *, u32 opcode);
void ARM946ES_ins_SWI(ARM946ES *, u32 opcode);
void ARM946ES_ins_INVALID(ARM946ES *, u32 opcode);
void ARM946ES_ins_PLD(ARM946ES *, u32 opcode);
void ARM946ES_ins_SMLAxy(ARM946ES *, u32 opcode);
void ARM946ES_ins_SMLAWy(ARM946ES *, u32 opcode);
void ARM946ES_ins_SMULWy(ARM946ES *, u32 opcode);
void ARM946ES_ins_SMLALxy(ARM946ES *, u32 opcode);
void ARM946ES_ins_SMULxy(ARM946ES *, u32 opcode);
void ARM946ES_ins_LDRD_STRD(ARM946ES *, u32 opcode);
void ARM946ES_ins_CLZ(ARM946ES *, u32 opcode);
void ARM946ES_ins_BLX_reg(ARM946ES *, u32 opcode);
void ARM946ES_ins_QADD_QSUB_QDADD_QDSUB(ARM946ES *, u32 opcode);
void ARM946ES_ins_BKPT(ARM946ES *, u32 opcode);
void ARM946ES_ins_B_BL(ARM946ES *, u32 opcode);
void ARM946ES_ins_BLX(ARM946ES *, u32 opcode);

#endif //JSMOOCH_EMUS_ARM946ES_INSTRUCTIONS_H
