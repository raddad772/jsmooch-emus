//
// Created by . on 12/4/24.
//

#pragma once

namespace ARM7TDMI {
#include "helpers/int.h"

struct core;
void ins_MUL_MLA(core *, u32 opcode);
void ins_MULL_MLAL(core *, u32 opcode);
void ins_SWP(core *, u32 opcode);
void ins_LDRH_STRH(core *, u32 opcode);
void ins_LDRSB_LDRSH(core *, u32 opcode);
void ins_MRS(core *, u32 opcode);
void ins_MSR_reg(core *, u32 opcode);
void ins_MSR_imm(core *, u32 opcode);
void ins_BX(core *, u32 opcode);
void ins_data_proc_immediate_shift(core *, u32 opcode);
void ins_data_proc_register_shift(core *, u32 opcode);
void ins_undefined_instruction(core *, u32 opcode);
void ins_data_proc_immediate(core *, u32 opcode);
void ins_LDR_STR_immediate_offset(core *, u32 opcode);
void ins_LDR_STR_register_offset(core *, u32 opcode);
void ins_LDM_STM(core *, u32 opcode);
void ins_B_BL(core *, u32 opcode);
void ins_STC_LDC(core *, u32 opcode);
void ins_CDP(core *, u32 opcode);
void ins_MCR_MRC(core *, u32 opcode);
void ins_SWI(core *, u32 opcode);
void ins_INVALID(core *, u32 opcode);

}
