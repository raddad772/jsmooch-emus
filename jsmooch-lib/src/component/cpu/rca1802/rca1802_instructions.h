#pragma once

#include "helpers/int.h"

namespace RCA1802 {
struct core;

#define I(name) void ins_##name(core *core);

I(LDN)
I(LDA)
I(INC)
I(DEC)
I(GLO)
I(GHI)
I(PLO)
I(PHI)
I(SEP)
I(SEX)
I(IRX)
I(OUT)
I(INP)
I(LDXA)
I(STXA)
I(SHRC_RSHR)
I(SHLC_RSHL)
I(ADC)
I(SDB)
I(STXD)
I(SMB)
I(MARK)
I(REQ)
I(SEQ)
I(RET)
I(DIS)
I(OR)
I(AND)
I(XOR)
I(ADD)
I(SD)
I(SHR)
I(SM)
I(SHL)
I(SHORT_BRANCH)
I(LONG_BRANCH)
I(IDLE)
I(none)
#undef I
}