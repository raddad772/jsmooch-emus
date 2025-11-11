//
// Created by . on 1/18/25.
//

#include <stdio.h>

#include "arm946es.h"
#include "arm946es_decode.h"
#include "arm946es_instructions.h"

enum ARM_ins_kind {
    ARM_MUL_MLA,
    ARM_MULL_MLAL,
    ARM_SWP,
    ARM_LDRH_STRH,
    ARM_LDRSB_LDRSH,
    ARM_MRS,
    ARM_MSR_reg,
    ARM_MSR_imm,
    ARM_BX,
    ARM_data_proc_immediate_shift,
    ARM_data_proc_register_shift,
    ARM_data_proc_undefined,
    ARM_undefined_instruction,
    ARM_data_proc_immediate,
    ARM_LDR_STR_immediate_offset,
    ARM_LDR_STR_register_offset,
    ARM_LDM_STM,
    ARM_STC_LDC,
    ARM_CDP,
    ARM_MCR_MRC,
    ARM_SWI,
    ARM_INVALID,
    ARM_PLD,
    ARM_SMLAxy,
    ARM_SMLAWy,
    ARM_SMULWy,
    ARM_SMLALxy,
    ARM_SMULxy,
    ARM_LDRD_STRD,
    ARM_CLZ,
    ARM_BLX_reg,
    ARM_QADD_QSUB_QDADD_QDSUB,
    ARM_BKPT,
    ARM_B_BL,
    ARM_BLX
};

#define OB(a,b) (0b ## a ## b)
#define OPCd(n,a,b) if ((opc & a) == b) return ARM_##n

static enum ARM_ins_kind decode_arm(u32 opc)
{
    OPCd(MUL_MLA,                   OB(11111100,1111), OB(00000000,1001)); // .... 000'000.. 1001  MUL, MLA
    OPCd(MULL_MLAL,                 OB(11111000,1111), OB(00001000,1001)); // .... 000'01... 1001  MULL, MLAL
    OPCd(SMLAxy,                    OB(11111111,1001), OB(00010000,1000)); // .... 000'10000 1..0  SMLAxy
    OPCd(SMLAWy,                    OB(11111111,1011), OB(00010010,1000)); // .... 000'10010 1.00  SMLAWy
    OPCd(SMULWy,                    OB(11111111,1011), OB(00010010,1010)); // .... 000'10010 1.10  SMULWy
    OPCd(SMLALxy,                   OB(11111111,1001), OB(00010100,1000)); // .... 000'10100 1..0  SMLALxy
    OPCd(SMULxy,                    OB(11111111,1001), OB(00010110,1000)); // .... 000'10110 1..0  SMULxy
    OPCd(SWP,                       OB(11111011,1111), OB(00010000,1001)); // .... 000'10.00 1001  SWP
    OPCd(LDRH_STRH,                 OB(11100000,1111), OB(00000000,1011)); // .... 000'..... 1011  LDRH, STRH
    OPCd(LDRD_STRD,                 OB(11100001,1101), OB(00000000,1101)); // .... 000'....0 11.1  LDRD, STRD
    OPCd(LDRSB_LDRSH,               OB(11100001,1101), OB(00000001,1101)); // .... 000'....1 11.1  LDRSB, LDRSH
    OPCd(MRS,                       OB(11111011,1111), OB(00010000,0000)); // .... 000'10.00 0000  MRS
    OPCd(MSR_reg,                   OB(11111011,1111), OB(00010010,0000)); // .... 000'10.10 0000  MSR (register)
    OPCd(MSR_imm,                   OB(11111011,0000), OB(00110010,0000)); // .... 001'10.10 ....  MSR (immediate)
    OPCd(BX,                        OB(11111111,1111), OB(00010010,0001)); // .... 000'10010 0001  BX
    OPCd(CLZ,                       OB(11111111,1111), OB(00010110,0001)); // .... 000'10110 0001  CLZ
    OPCd(BLX_reg,                   OB(11111111,1111), OB(00010010,0011)); // .... 000'10010 0011  BLX (register)
    OPCd(QADD_QSUB_QDADD_QDSUB,     OB(11111001,1111), OB(00010000,0101)); // .... 000'10..0 0101  QADD, QSUB, QDADD, QDSUB
    OPCd(BKPT,                      OB(11111111,1111), OB(00010010,0111)); // .... 000'10010 0111  BKPT
    OPCd(data_proc_immediate_shift, OB(11100000,0001), OB(00000000,0000)); // .... 000'..... ...0  Data Processing (immediate shift)
    OPCd(data_proc_register_shift,  OB(11100000,1001), OB(00000000,0001)); // .... 000'..... 0..1  Data Processing (register shift)
    OPCd(data_proc_undefined,       OB(11111011,0000), OB(00110000,0000)); // .... 001'10.00 ....  Undefined instructions in Data Processing
    OPCd(data_proc_immediate,       OB(11100000,0000), OB(00100000,0000)); // .... 001'..... ....  Data Processing (immediate value)
    OPCd(LDR_STR_immediate_offset,  OB(11100000,0000), OB(01000000,0000)); // .... 010'..... ....  LDR, STR (immediate offset)
    OPCd(LDR_STR_register_offset,   OB(11100000,0001), OB(01100000,0000)); // .... 011'..... ...0  LDR, STR (register offset)
    OPCd(LDM_STM,                   OB(11100000,0000), OB(10000000,0000)); // .... 100'..... ....  LDM, STM
    OPCd(B_BL,                      OB(11100000,0000), OB(10100000,0000)); // .... 101'..... ....  B, BL, BLX
    OPCd(STC_LDC,                   OB(11100000,0000), OB(11000000,0000)); // .... 110'..... ....  STC, LDC
    OPCd(CDP,                       OB(11110000,0001), OB(11100000,0000)); // .... 111'0.... ...0  CDP
    OPCd(MCR_MRC,                   OB(11110000,0001), OB(11100000,0001)); // .... 111'0.... ...1  MCR, MRC
    OPCd(SWI,                       OB(11110000,0000), OB(11110000,0000)); // .... 111'1.... ....  SWI
    return ARM_INVALID;
}



static enum ARM_ins_kind decode_arm_never(u32 opc)
{
    OPCd(PLD,                       OB(11010111,0000), OB(01010101,0000)); // 1111 01.'1.101 ....  PLD
    OPCd(undefined_instruction,     OB(10000000,0000), OB(00000000,0000)); // 1111 0..'..... ....  Undefined
    OPCd(undefined_instruction,     OB(11100000,0000), OB(10000000,0000)); // 1111 100'..... ....  Undefined
    OPCd(undefined_instruction,     OB(11110000,0000), OB(11110000,0000)); // 1111 111'1.... ....  Undefined
    OPCd(BLX,                       OB(11100000,0000), OB(10100000,0000)); // .... 101'..... ....  B, BL, BLX
    OPCd(MCR_MRC,                   OB(11110000,0001), OB(11100000,0001)); // .... 111'0.... ...1  MCR, MRC
    return ARM_INVALID;
}
#undef OPCd
#undef OB


void ARM946ES_fill_arm_table(ARM946ES *this)
{
    for (u32 opc = 0; opc < 4096; opc++) {
        struct arm9_ins *ins = &this->opcode_table_arm[opc];
        enum ARM_ins_kind k = decode_arm(opc);
        switch(k) {
#define I(x) case ARM_##x: ins->exec = &ARM946ES_ins_##x; break;
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
            I(STC_LDC)
            I(CDP)
            I(MCR_MRC)
            I(BKPT)
            I(SWI)
            I(INVALID)
            I(PLD)
            I(SMLAxy)
            I(SMLAWy)
            I(SMULWy)
            I(SMLALxy)
            I(SMULxy)
            I(LDRD_STRD)
            I(CLZ)
            I(BLX_reg)
            I(QADD_QSUB_QDADD_QDSUB)
            I(B_BL)
            default:
                ins->exec = &ARM946ES_ins_INVALID;
                break;

        }
        // Now do the same but for the NEVER table
        ins = &this->opcode_table_arm_never[opc];
        k = decode_arm_never(opc);
#undef I
#define I(x) case ARM_##x: ins->exec = &ARM946ES_ins_##x; ins->valid = 1; break;

        switch(k) {
            I(PLD);
            I(undefined_instruction);
            I(BLX);
            I(MCR_MRC);
            case ARM_INVALID:
                ins->valid = 0;
                break;
        }
#undef I
    }
}
static u16 doBITS(u16 val, u16 hi, u16 lo)
{
    u16 shift = lo;
    u16 mask = (1 << ((hi - lo) + 1)) - 1;
    return (val >> shift) & mask;
}

void decode_thumb2(u16 opc, thumb2_instruction *ins)
{
#define OBIT(x) ((opc >> (x)) & 1)
#define BITS(hi,lo) (doBITS(opc, hi, lo))
    ins->opcode = opc;
    ins->func = &ARM946ES_THUMB_ins_INVALID;
    if ((opc & 0b1111100000000000) == 0b0001100000000000) { // ADD_SUB
        ins->func = &ARM946ES_THUMB_ins_ADD_SUB;
        ins->I = OBIT(10); // 0 = register, 1 = immediate
        ins->sub_opcode = OBIT(9); // 0= add, 1 = sub
        if (!ins->I)
            ins->Rn = BITS(8,6);
        else
            ins->imm = BITS(8, 6);
        ins->Rs = BITS(5, 3);
        ins->Rd = BITS(2, 0);
    }
    else if ((opc & 0b1110000000000000) == 0b0000000000000000) { // LSL, LSR, etc.
        ins->func = &ARM946ES_THUMB_ins_LSL_LSR_ASR;
        ins->sub_opcode = BITS(12, 11);
        ins->imm = BITS(10, 6);
        ins->Rs = BITS(5, 3);
        ins->Rd = BITS(2, 0);
    }
    else if ((opc & 0b1110000000000000) == 0b0010000000000000) { // MOV, CMP, ADD, SUB
        ins->func = &ARM946ES_THUMB_ins_MOV_CMP_ADD_SUB;
        ins->sub_opcode = BITS(12, 11);
        ins->Rd = BITS(10, 8);
        ins->imm = BITS(7, 0);
    }
    else if ((opc & 0b1111110000000000) == 0b0100000000000000) { // data proc
        ins->func = &ARM946ES_THUMB_ins_data_proc;
        ins->sub_opcode = BITS(9, 6);
        ins->Rs = BITS(5, 3);
        ins->Rd = BITS(2, 0);
    }
    else if ((opc & 0b1111111100000000) == 0b0100011100000000) { // BX
        ins->func = &ARM946ES_THUMB_ins_BX_BLX;
        // for BX, MSBd must be zero
        // for BLX, MSBd must be one
        //ins->Rd = OBIT(7) << 3;
        ins->Rs = OBIT(6) << 3;
        ins->Rs |= BITS(5,3);
        ins->Rd = BITS(2, 0);
        ins->sub_opcode = OBIT(7); // 0=BX, 1=BLX
    }
    else if ((opc & 0b1111110000000000) == 0b0100010000000000) { // ADD, CMP, MOV hi
        ins->func = &ARM946ES_THUMB_ins_ADD_CMP_MOV_hi;
        ins->Rd = OBIT(7) << 3;
        ins->Rs = OBIT(6) << 3;
        ins->Rs |= BITS(5,3);
        ins->Rd |= BITS(2, 0);
        ins->sub_opcode = BITS(9, 8);
    }
    else if ((opc & 0b1111100000000000) == 0b0100100000000000) { // 01001.....
        ins->func = &ARM946ES_THUMB_ins_LDR_PC_relative;
        ins->Rd = BITS(10, 8);
        ins->imm = BITS(7, 0);
        ins->imm <<= 2;
    }
    else if ((opc & 0b1111011000000000) == 0b0101001000000000) { // 0101.01...
        ins->func = &ARM946ES_THUMB_ins_LDRH_STRH_reg_offset;
        ins->L = OBIT(11);
        ins->Ro = BITS(8, 6);
        ins->Rb = BITS(5, 3);
        ins->Rd = BITS(2, 0);
    }
    else if ((opc & 0b1111011000000000) == 0b0101011000000000) { // 0101.11...
        ins->func = &ARM946ES_THUMB_ins_LDRSH_LDRSB_reg_offset;
        ins->B = !OBIT(11); // 0=byte, 1=halfword. oops!
        ins->Ro = BITS(8, 6);
        ins->Rb = BITS(5, 3);
        ins->Rd = BITS(2, 0);
    }
    else if ((opc & 0b1111011000000000) == 0b0101000000000000) { // 0101.00...
        ins->func = &ARM946ES_THUMB_ins_LDR_STR_reg_offset;
        ins->L = OBIT(11);
        ins->Ro = BITS(8, 6);
        ins->Rb = BITS(5, 3);
        ins->Rd = BITS(2, 0);
    }
    else if ((opc & 0b1111011000000000) == 0b0101010000000000) { // 0101.10...
        ins->func = &ARM946ES_THUMB_ins_LDRB_STRB_reg_offset;
        ins->L = OBIT(11); // 0=STR, 1=LDR
        ins->Ro = BITS(8, 6);
        ins->Rb = BITS(5, 3);
        ins->Rd = BITS(2, 0);
    }
    else if ((opc & 0b1111000000000000) == 0b0110000000000000) { // 0110......
        ins->func = &ARM946ES_THUMB_ins_LDR_STR_imm_offset;
        ins->L = OBIT(11);
        ins->imm = BITS(10, 6);
        ins->imm <<= 2;
        ins->Rb = BITS(5, 3);
        ins->Rd = BITS(2, 0);
    }
    else if ((opc & 0b1111000000000000) == 0b0111000000000000) { // 0111......
        ins->func = &ARM946ES_THUMB_ins_LDRB_STRB_imm_offset;
        ins->L = OBIT(11);
        ins->imm = BITS(10, 6);
        ins->Rb = BITS(5, 3);
        ins->Rd = BITS(2, 0);
    }
    else if ((opc & 0b1111000000000000) == 0b1000000000000000) { // 1000......
        ins->func = &ARM946ES_THUMB_ins_LDRH_STRH_imm_offset;
        ins->L = OBIT(11);
        ins->imm = BITS(10, 6);
        ins->imm <<= 1;
        ins->Rb = BITS(5, 3);
        ins->Rd = BITS(2, 0);
    }
    else if ((opc & 0b1111000000000000) == 0b1001000000000000) { // 1001......
        ins->func = &ARM946ES_THUMB_ins_LDR_STR_SP_relative;
        ins->L = OBIT(11);
        ins->Rd = BITS(10, 8);
        ins->imm = BITS(7, 0);
        ins->imm <<= 2;
    }
    else if ((opc & 0b1111000000000000) == 0b1010000000000000) { // 1010......
        ins->func = &ARM946ES_THUMB_ins_ADD_SP_or_PC;
        ins->SP = OBIT(11);
        ins->Rd = BITS(10, 8);
        ins->imm = BITS(7, 0);
        ins->imm <<= 2;
    }
    else if ((opc & 0b1111111100000000) == 0b1011000000000000) { // 10110000..
        ins->func = &ARM946ES_THUMB_ins_ADD_SUB_SP;
        ins->S = OBIT(7);
        ins->imm = BITS(6, 0);
        ins->imm <<= 2;
    }
    else if ((opc & 0b1111011000000000) == 0b1011010000000000) { // 1011.10...
        ins->func = &ARM946ES_THUMB_ins_PUSH_POP;
        ins->sub_opcode = OBIT(11);
        ins->PC_LR = OBIT(8);
        ins->rlist = BITS(7, 0);
    }
    else if ((opc & 0b1111111100000000) == 0b1011111000000000) {// 1011 1110..  BKPT
        ins->func = &ARM946ES_THUMB_ins_BKPT;
    }
    else if ((opc & 0b1111000000000000) == 0b1100000000000000) { // 1100......
        ins->func = &ARM946ES_THUMB_ins_LDM_STM;
        ins->sub_opcode = OBIT(11);
        ins->Rb = BITS(10, 8);
        ins->rlist = BITS(7, 0);
    }
    else if ((opc & 0b1111111100000000) == 0b1101111100000000) { // 11011111..
        ins->func = &ARM946ES_THUMB_ins_SWI;
        ins->comment = BITS(7, 0);
    }
    else if ((opc & 0b1111111100000000) == 0b1101111000000000) { // 11011110..
        ins->func = &ARM946ES_THUMB_ins_UNDEFINED_BCC;
        ins->sub_opcode = BITS(11, 8);
        ins->imm = BITS(7, 0);
        ins->imm = SIGNe8to32(ins->imm);
        ins->imm <<= 1;
    }
    else if ((opc & 0b1111000000000000) == 0b1101000000000000) { // 1101......
        ins->func = &ARM946ES_THUMB_ins_BCC;
        ins->sub_opcode = BITS(11, 8);
        ins->imm = BITS(7, 0);
        ins->imm = SIGNe8to32(ins->imm);
        ins->imm <<= 1;
    }
    else if ((opc & 0b1111100000000000) == 0b1110000000000000) { // 11100.....
        ins->func = &ARM946ES_THUMB_ins_B;
        ins->imm = BITS(10, 0);
        ins->imm = SIGNe11to32(ins->imm);
        ins->imm <<= 1;
    }
    else if ((opc & 0b1111100000000000) == 0b1110100000000000) { // 11101.....  BLX suffix
        ins->func = &ARM946ES_THUMB_ins_BLX_suffix;
        ins->imm = BITS(10, 0);
        ins->imm <<= 1;
    }
    else if ((opc & 0b1111100000000000) == 0b1111000000000000) { // 11110.....
        ins->func = &ARM946ES_THUMB_ins_BL_BLX_prefix;
        ins->imm = BITS(11, 0);
        ins->imm = (i32)SIGNe11to32(ins->imm); // now SHL 11...
        ins->imm <<= 12;
    }
    else if ((opc & 0b1111100000000000) == 0b1111100000000000) { // 11111.....
        ins->func = &ARM946ES_THUMB_ins_BL_suffix;
        ins->imm = BITS(10, 0);
        ins->imm <<= 1;
    }
}

