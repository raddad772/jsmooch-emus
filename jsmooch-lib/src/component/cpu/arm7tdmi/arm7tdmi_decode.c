//
// Created by . on 12/4/24.
//

#include "arm7tdmi.h"
#include "arm7tdmi_decode.h"
#include "arm7tdmi_instructions.h"
#include "thumb_instructions.h"

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
    ARM_undefined_instruction,
    ARM_data_proc_immediate,
    ARM_LDR_STR_immediate_offset,
    ARM_LDR_STR_register_offset,
    ARM_LDM_STM,
    ARM_B_BL,
    ARM_STC_LDC,
    ARM_CDP,
    ARM_MCR_MRC,
    ARM_SWI,
    ARM_INVALID
};

static u16 doBITS(u16 val, u16 hi, u16 lo)
{
    u16 shift = lo;
    u16 mask = (1 << ((hi - lo) + 1)) - 1;
    return (val >> shift) & mask;
}

void decode_thumb(u16 opc, struct thumb_instruction *ins)
{
#define OBIT(x) ((opc >> (x)) & 1)
#define BITS(hi,lo) (doBITS(opc, hi, lo))
    ins->opcode = opc;
    ins->func = &ARM7TDMI_THUMB_ins_INVALID;
    if ((opc & 0b1111100000000000) == 0b0001100000000000) { // ADD_SUB
        ins->func = &ARM7TDMI_THUMB_ins_ADD_SUB;
        ins->I = OBIT(10); // 0 = register, 1 = immediate
        ins->sub_opcode = OBIT(9); // 0= add, 1 = sub
        if (ins->I)
            ins->Rn = BITS(8,6);
        else
            ins->imm = BITS(8, 6);
        ins->Rs = BITS(5, 3);
        ins->Rd = BITS(2, 0);
    }
    else if ((opc & 0b1110000000000000) == 0b0000000000000000) { // LSL, LSR, etc.
        ins->func = &ARM7TDMI_THUMB_ins_LSL_LSR_ASR;
        ins->sub_opcode = BITS(12, 11);
        ins->imm = BITS(10, 6);
        ins->Rs = BITS(5, 3);
        ins->Rd = BITS(2, 0);
    }
    else if ((opc & 0b1110000000000000) == 0b0010000000000000) { // MOV, CMP, ADD, SUB
        ins->func = &ARM7TDMI_THUMB_ins_MOV_CMP_ADD_SUB;
        ins->sub_opcode = BITS(12, 11);
        ins->Rd = BITS(10, 8);
        ins->imm = BITS(7, 0);
    }
    else if ((opc & 0b1111110000000000) == 0b0100000000000000) { // data proc
        ins->func = &ARM7TDMI_THUMB_ins_data_proc;
        ins->sub_opcode = BITS(9, 6);
        ins->Rs = BITS(5, 3);
        ins->Rd = BITS(2, 0);
    }
    else if ((opc & 0b1111111100000000) == 0b0100011100000000) { // BX
        ins->func = &ARM7TDMI_THUMB_ins_BX;
        ins->Rd = OBIT(7) << 3;
        ins->Rs = OBIT(6) << 3;
        ins->Rs |= BITS(5,3);
        ins->Rd |= BITS(2, 0);
    }
    else if ((opc & 0b1111110000000000) == 0b0100010000000000) { // ADD, CMP, MOV hi
        ins->func = &ARM7TDMI_THUMB_ins_ADD_CMP_MOV_hi;
        ins->Rd = OBIT(7) << 3;
        ins->Rs = OBIT(6) << 3;
        ins->Rs |= BITS(5,3);
        ins->Rd |= BITS(2, 0);
        ins->sub_opcode = BITS(9, 8);
    }
    else if ((opc & 0b1111100000000000) == 0b0100100000000000) { // 01001.....
        ins->func = &ARM7TDMI_THUMB_ins_LDR_PC_relative;
        ins->Rd = BITS(10, 8);
        ins->imm = BITS(7, 0);
    }
    else if ((opc & 0b1111011000000000) == 0b0101001000000000) { // 0101.01...
        ins->func = &ARM7TDMI_THUMB_ins_LDRH_STRH_reg_offset;
        ins->L = OBIT(11);
        ins->Ro = BITS(8, 6);
        ins->Rb = BITS(5, 3);
        ins->Rd = BITS(2, 0);
    }
    else if ((opc & 0b1111011000000000) == 0b0101011000000000) { // 0101.11...
        ins->func = &ARM7TDMI_THUMB_ins_LDRSH_LDRSB_reg_offset;
        ins->B = OBIT(10); // 0=byte, 1=halfword. oops!
        ins->Ro = BITS(8, 6);
        ins->Rb = BITS(5, 3);
        ins->Rd = BITS(2, 0);
    }
    else if ((opc & 0b1111011000000000) == 0b0101000000000000) { // 0101.00...
        ins->func = &ARM7TDMI_THUMB_ins_LDR_STR_reg_offset;
        ins->L = OBIT(11);
        ins->Ro = BITS(8, 6);
        ins->Rb = BITS(5, 3);
        ins->Rd = BITS(2, 0);
    }
    else if ((opc & 0b1111011000000000) == 0b0101010000000000) { // 0101.10...
        ins->func = &ARM7TDMI_THUMB_ins_LDRB_STRB_reg_offset;
        ins->L = OBIT(10); // 0=STR, 1=LDR
        ins->Ro = BITS(8, 6);
        ins->Rb = BITS(5, 3);
        ins->Rd = BITS(2, 0);
    }
    else if ((opc & 0b1111000000000000) == 0b0110000000000000) { // 0110......
        ins->func = &ARM7TDMI_THUMB_ins_LDR_STR_imm_offset;
        ins->L = OBIT(11);
        ins->imm = BITS(10, 6);
        ins->Rb = BITS(5, 3);
        ins->Rd = BITS(2, 0);
    }
    else if ((opc & 0b1111000000000000) == 0b0111000000000000) { // 0111......
        ins->func = &ARM7TDMI_THUMB_ins_LDRB_STRB_imm_offset;
        ins->L = OBIT(11);
        ins->imm = BITS(10, 6);
        ins->Rb = BITS(5, 3);
        ins->Rd = BITS(2, 0);
    }
    else if ((opc & 0b1111000000000000) == 0b1000000000000000) { // 1000......
        ins->func = &ARM7TDMI_THUMB_ins_LDRH_STRH_imm_offset;
        ins->L = OBIT(11);
        ins->imm = BITS(10, 6);
        ins->Rb = BITS(5, 3);
        ins->Rd = BITS(2, 0);
    }
    else if ((opc & 0b1111000000000000) == 0b1001000000000000) { // 1001......
        ins->func = &ARM7TDMI_THUMB_ins_LDR_STR_SP_relative;
        ins->L = OBIT(11);
        ins->Rd = BITS(10, 8);
        ins->imm = BITS(7, 0);
    }
    else if ((opc & 0b1111000000000000) == 0b1010000000000000) { // 1010......
        ins->func = &ARM7TDMI_THUMB_ins_ADD_SP_or_PC;
        ins->SP = OBIT(11);
        ins->Rd = BITS(10, 8);
        ins->imm = BITS(7, 0);
    }
    else if ((opc & 0b1111111100000000) == 0b1011000000000000) { // 10110000..
        ins->func = &ARM7TDMI_THUMB_ins_ADD_SUB_SP;
        ins->S = OBIT(7);
        ins->imm = BITS(6, 0);
    }
    else if ((opc & 0b1111011000000000) == 0b1011010000000000) { // 1011.10...
        ins->func = &ARM7TDMI_THUMB_ins_PUSH_POP;
        ins->sub_opcode = OBIT(11);
        ins->PC_LR = OBIT(8);
        ins->rlist = BITS(7, 0);
    }
    else if ((opc & 0b1111000000000000) == 0b1100000000000000) { // 1100......
        ins->func = &ARM7TDMI_THUMB_ins_LDM_STM;
        ins->sub_opcode = OBIT(11);
        ins->Rb = BITS(10, 8);
        ins->rlist = BITS(7, 0);
    }
    else if ((opc & 0b1111111100000000) == 0b1101111100000000) { // 11011111..
        ins->func = &ARM7TDMI_THUMB_ins_SWI;
        ins->comment = BITS(7, 0);
    }
    else if ((opc & 0b1111111100000000) == 0b1101111000000000) { // 11011110..
        ins->func = &ARM7TDMI_THUMB_ins_UNDEFINED_BCC;
        ins->sub_opcode = BITS(11, 8);
        ins->imm = BITS(7, 0);
    }
    else if ((opc & 0b1111000000000000) == 0b1101000000000000) { // 1101......
        ins->func = &ARM7TDMI_THUMB_ins_BCC;
        ins->sub_opcode = BITS(11, 8);
        ins->imm = BITS(7, 0);
    }
    else if ((opc & 0b1111100000000000) == 0b1110000000000000) { // 11100.....
        ins->func = &ARM7TDMI_THUMB_ins_B;
        ins->imm = BITS(10, 0);
    }
    else if ((opc & 0b1111100000000000) == 0b1111000000000000) { // 11110.....
        ins->func = &ARM7TDMI_THUMB_ins_BL_BLX_prefix;
        ins->imm = BITS(11, 0);
        ins->imm = (i16)(i32)SIGNe11to32(ins->imm); // now SHL 11...
    }
    else if ((opc & 0b1111100000000000) == 0b1111100000000000) { // 11111.....
        ins->func = &ARM7TDMI_THUMB_ins_BL_suffix;
        ins->imm = BITS(10, 0);
    }
}

enum ARM_ins_kind decode_arm(u32 opc)
{
    if ((opc & 0b111111001111) == 0b000000001001) // 000'000.. 1001  MUL, MLA
        return ARM_MUL_MLA;
        //000'01... 1001  MULL, MLAL
    else if ((opc & 0b111110001111) == 0b000010001001)
        return ARM_MULL_MLAL;
        //000'10.00 1001  SWP
    else if ((opc & 0b111110111111) == 0b000100001001)
        return ARM_SWP;
        //000'..... 1011  LDRH, STRH
    else if ((opc & 0b111000001111) == 0b000000001011)
        return ARM_LDRH_STRH;
        //000'....1 11.1  LDRSB, LDRSH
    else if ((opc & 0b111000011101) == 0b000000011101)
        return ARM_LDRSB_LDRSH;
        //000'10.00 0000  MRS
    else if ((opc & 0b111110111111) == 0b000100000000)
        return ARM_MRS;
        //000'10.10 0000  MSR (register)
    else if ((opc & 0b111110111111) == 0b000100100000)
        return ARM_MSR_reg;
        // 001'10.10 ....  MSR (immediate)
    else if ((opc & 0b111110110000) == 0b001100100000)
        return ARM_MSR_imm;
    //000'10010 0001  BX
    else if (opc == 0b000100100001)
        return ARM_BX;
    //000'..... ...0  Data Processing (immediate shift)
    else if ((opc & 0b111000000001) == 0b000000000000)
        return ARM_data_proc_immediate_shift;
    //000'..... 0..1  Data Processing (register shift)
    else if ((opc & 0b111000001001) == 0b000000000001)
        return ARM_data_proc_register_shift;
    //001'10.00 ....  Undefined instructions in Data Processing
    else if ((opc & 0b111110110000) == 0b001100000000)
        return ARM_undefined_instruction;
    //001'..... ....  Data Processing (immediate value)
    else if ((opc & 0b111000000000) == 0b001000000000)
        return ARM_data_proc_immediate;
    //010'..... ....  LDR, STR (immediate offset)
    else if ((opc & 0b111000000000) == 0b010000000000)
        return ARM_LDR_STR_immediate_offset;
    // 011'..... ...0  LDR, STR (register offset)
    else if ((opc & 0b111000000001) == 0b011000000000)
        return ARM_LDR_STR_register_offset;
    //100'..... ....  LDM, STM
    else if ((opc & 0b111000000000) == 0b100000000000)
        return ARM_LDM_STM;
    //101'..... ....  B, BL
    else if ((opc & 0b111000000000) == 0b101000000000)
        return ARM_B_BL;
    //110'..... ....  STC, LDC
    else if ((opc & 0b111000000000) == 0b110000000000)
        return ARM_STC_LDC;
    //111'0.... ...0  CDP
    else if ((opc & 0b111100000001) == 0b111000000000)
        return ARM_CDP;
    //111'0.... ...1  MCR, MRC
    else if ((opc & 0b111100000001) == 0b111000000001)
        return ARM_MCR_MRC;
    //111'1.... ....  SWI
    else if ((opc & 0b111100000000) == 0b111100000000)
        return ARM_SWI;
    return ARM_INVALID;
}


void ARM7TDMI_fill_arm_table(struct ARM7TDMI *this)
{
    for (u32 opc = 0; opc < 4096; opc++) {
        struct ARM7_ins *ins = &this->opcode_table_arm[opc];
        enum ARM_ins_kind k = decode_arm(opc);
        switch(k) {
#define I(x) case ARM_##x: ins->exec = &ARM7TDMI_ins_##x; break;
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
