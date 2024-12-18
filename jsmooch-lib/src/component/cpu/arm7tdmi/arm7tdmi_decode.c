//
// Created by . on 12/4/24.
//

#include "arm7tdmi.h"
#include "arm7tdmi_decode.h"
#include "arm7tdmi_instructions.h"

enum ARM_ins_kind {
    MUL_MLA,
    MULL_MLAL,
    SWP,
    LDRH_STRH,
    LDRSB_LDRSH,
    MRS,
    MSR_reg,
    MSR_imm,
    BX,
    data_proc_immediate_shift,
    data_proc_register_shift,
    undefined_instruction,
    data_proc_immediate,
    LDR_STR_immediate_offset,
    LDR_STR_register_offset,
    LDM_STM,
    B_BL,
    STC_LDC,
    CDP,
    MCR_MRC,
    SWI,
    INVALID
};

enum ARM_ins_kind decode_arm(u32 opc)
{
    if ((opc & 0b111111001111) == 0b000000001001) // 000'000.. 1001  MUL, MLA
        return MUL_MLA;
        //000'01... 1001  MULL, MLAL
    else if ((opc & 0b111110001111) == 0b000010001001)
        return MULL_MLAL;
        //000'10.00 1001  SWP
    else if ((opc & 0b111110111111) == 0b000100001001)
        return SWP;
        //000'..... 1011  LDRH, STRH
    else if ((opc & 0b111000001111) == 0b000000001011)
        return LDRH_STRH;
        //000'....1 11.1  LDRSB, LDRSH
    else if ((opc & 0b111000011101) == 0b000000011101)
        return LDRSB_LDRSH;
        //000'10.00 0000  MRS
    else if ((opc & 0b111110111111) == 0b000100000000)
        return MRS;
        //000'10.10 0000  MSR (register)
    else if ((opc & 0b111110111111) == 0b000100100000)
        return MSR_reg;
        // 001'10.10 ....  MSR (immediate)
    else if ((opc & 0b111110110000) == 0b001100100000)
        return MSR_imm;
    //000'10010 0001  BX
    else if (opc == 0b000100100001)
        return BX;
    //000'..... ...0  Data Processing (immediate shift)
    else if ((opc & 0b111000000001) == 0b000000000000)
        return data_proc_immediate_shift;
    //000'..... 0..1  Data Processing (register shift)
    else if ((opc & 0b111000001001) == 0b000000000001)
        return data_proc_register_shift;
    //001'10.00 ....  Undefined instructions in Data Processing
    else if ((opc & 0b111110110000) == 0b001100000000)
        return undefined_instruction;
    //001'..... ....  Data Processing (immediate value)
    else if ((opc & 0b111000000000) == 0b001000000000)
        return data_proc_immediate;
    //010'..... ....  LDR, STR (immediate offset)
    else if ((opc & 0b111000000000) == 0b010000000000)
        return LDR_STR_immediate_offset;
    // 011'..... ...0  LDR, STR (register offset)
    else if ((opc & 0b111000000001) == 0b011000000000)
        return LDR_STR_register_offset;
    //100'..... ....  LDM, STM
    else if ((opc & 0b111000000000) == 0b100000000000)
        return LDM_STM;
    //101'..... ....  B, BL
    else if ((opc & 0b111000000000) == 0b101000000000)
        return B_BL;
    //110'..... ....  STC, LDC
    else if ((opc & 0b111000000000) == 0b110000000000)
        return STC_LDC;
    //111'0.... ...0  CDP
    else if ((opc & 0b111100000001) == 0b111000000000)
        return CDP;
    //111'0.... ...1  MCR, MRC
    else if ((opc & 0b111100000001) == 0b111000000001)
        return MCR_MRC;
    //111'1.... ....  SWI
    else if ((opc & 0b111100000000) == 0b111100000000)
        return SWI;
    return INVALID;
}


void ARM7TDMI_fill_arm_table(struct ARM7TDMI *this)
{
    for (u32 opc = 0; opc < 4096; opc++) {
        struct ARM7_ins *ins = &this->opcode_table_arm[opc];
        enum ARM_ins_kind k = decode_arm(opc);
        switch(k) {
#define I(x) case x: ins->exec = &ARM7TDMI_ins_##x; break;
            I(MUL_MLA)
            I(MULL_MLAL)
            I(SWP)
            I(LDRH_STRH)
            I(LDRSB_LDRSH)
            I(MRS)
            I(MSR_reg)
            I(MSR_imm)
            I(BX)
            I(data_proc_immediate_shift)
            I(data_proc_register_shift)
            I(undefined_instruction)
            I(data_proc_immediate)
            I(LDR_STR_immediate_offset)
            I(LDR_STR_register_offset)
            I(LDM_STM)
            I(B_BL)
            I(STC_LDC)
            I(CDP)
            I(MCR_MRC)
            I(SWI)
            I(INVALID)
            default:
                assert(1==2);
                break;
#undef I
        }
    }
}
