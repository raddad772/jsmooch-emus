//
// Created by . on 2/11/25.
//

#ifndef JSMOOCH_EMUS_R3000_INSTRUCTIONS_H
#define JSMOOCH_EMUS_R3000_INSTRUCTIONS_H

#include "helpers/int.h"

struct R3000;
struct R3000_opcode;

void R3000_fNA(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fSLL(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fSRL(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fSRA(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fSLLV(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fSRLV(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fSRAV(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fJR(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fJALR(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fSYSCALL(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fBREAK(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fMFHI(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fMFLO(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fMTHI(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fMTLO(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fMULT(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fMULTU(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fDIV(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fDIVU(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fADD(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fADDU(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fSUB(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fSUBU(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fAND(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fOR(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fXOR(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fNOR(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fSLT(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fSLTU(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fBcondZ(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fJ(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fJAL(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fBEQ(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fBNE(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fBLEZ(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fBGTZ(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fADDI(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fADDIU(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fSLTI(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fSLTIU(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fANDI(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fORI(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fXORI(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fLUI(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fCOP(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fLB(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fLH(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fLWL(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fLW(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fLBU(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fLHU(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fLWR(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fSB(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fSH(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fSWL(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fSW(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fSWR(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fLWC(u32 opcode, struct R3000_opcode *op, struct R3000 *core);
void R3000_fSWC(u32 opcode, struct R3000_opcode *op, struct R3000 *core);


#endif //JSMOOCH_EMUS_R3000_INSTRUCTIONS_H
