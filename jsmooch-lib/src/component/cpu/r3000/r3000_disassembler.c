//
// Created by . on 2/12/25.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "r3000_disassembler.h"

static void add_r_context(struct R3000ctxt *t, u32 rnum)
{
    if (t) t->regs |= (1L << rnum);
}

static void add_PC_context(struct R3000ctxt *t, u32 rnum)
{
    t->regs |= (1L << 32);
}

static void add_gte_context(struct R3000ctxt *t, u32 rnum)
{
    t->gte |= (1L << rnum);
}

static void add_cop_context(struct R3000ctxt *t, u32 rnum)
{
    t->cop |= (1L << rnum);
}

static const char GTE_reg_alias_arr[65][12] = {
        "vxy0", "vz0", "vxy1", "vz1", "vxy2", "vz2", "rgbc", "otz",
        "ir0", "ir1", "ir2", "ir3", "sxy0", "sxy1", "sxy2", "sxyp",
        "sz0", "sz1", "sz2", "sz3", "rgb0", "rgb1", "rgb2", "res1",
        "mac0", "mac1", "mac2", "mac3", "irgb", "orgb", "lzcs", "lzcr",
        "rt11rt12", "rt13rt21", "rt22rt23", "rt31rt32", "rt33", "trx", "try", "trz",
        "l11l12", "l13l21", "l22l23", "l31l32", "l33", "rbk", "gbk", "bbk",
        "lr1lr2", "lr3lg1", "lg2lg3", "lb1lb2", "lb3", "rfc", "gfc", "bfc",
        "ofx", "ofy", "h", "dqa", "dqb", "zsf3", "zsf4", "flag",
        "unknown reg"
};

static const char reg_alias_arr[33][12] = {
        "r0", "at", "v0", "v1",
        "a0", "a1", "a2", "a3",
        "t0", "t1", "t2", "t3",
        "t4", "t5", "t6", "t7",
        "s0", "s1", "s2", "s3",
        "s4", "s5", "s6", "s7",
        "t8", "t9", "k0", "k1",
        "gp", "sp", "fp", "ra",
        "unknown reg"
};

const char *GTE_reg_alias(int reg) {
    if (reg < 64) return GTE_reg_alias_arr[reg];
    return GTE_reg_alias_arr[64];
}

void disassemble_GTE(u32 opcode, struct jsm_string *out, struct R3000ctxt *ctxt) {
    u32 sf = (opcode >> 19) & 1;
    switch(opcode & 0x3F) {
        case 0x00: // N/A
            jsm_string_sprintf(out, "%s", "GTE NA0");
            return;
        case 0x01: //
            jsm_string_sprintf(out, "%s", "RTPS");
            return;
        case 0x06:
            jsm_string_sprintf(out, "%s", "NCLIP");
            return;
        case 0x0C:
            jsm_string_sprintf(out, "%s", "OP(%d)", sf);
            return;
        case 0x10:
            jsm_string_sprintf(out, "%s", "DPCS");
            return;
        case 0x11:
            jsm_string_sprintf(out, "%s", "INTPL");
            return;
        case 0x12:
            jsm_string_sprintf(out, "%s", "MVMVA");
            return;
        case 0x13:
            jsm_string_sprintf(out, "%s", "NCDS");
            return;
        case 0x14:
            jsm_string_sprintf(out, "%s", "CDP");
            return;
        case 0x16:
            jsm_string_sprintf(out, "%s", "NCDT");
            return;
        case 0x1B:
            jsm_string_sprintf(out, "%s", "NCCS");
            return;
        case 0x1C:
            jsm_string_sprintf(out, "%s", "CC");
            return;
        case 0x1E:
            jsm_string_sprintf(out, "%s", "NCS");
            return;
        case 0x20:
            jsm_string_sprintf(out, "%s", "NCT");
            return;
        case 0x28:
            jsm_string_sprintf(out, "%s", "SQRT(%d)", sf);
            return;
        case 0x29:
            jsm_string_sprintf(out, "%s", "DCPL");
            return;
        case 0x2A:
            jsm_string_sprintf(out, "%s", "DPCT");
            return;
        case 0x2D:
            jsm_string_sprintf(out, "%s", "AVSZ3");
            return;
        case 0x2E:
            jsm_string_sprintf(out, "%s", "AVSZ4");
            return;
        case 0x30:
            jsm_string_sprintf(out, "%s", "RTPT");
            return;
        case 0x3D:
            jsm_string_sprintf(out, "%s", "GPF(%d)5", sf);
            return;
        case 0x3E:
            jsm_string_sprintf(out, "%s", "GPL(%d)5", sf);
            return;
        case 0x3F:
            jsm_string_sprintf(out, "%s", "NCCT");
            return;
        default:
            jsm_string_sprintf(out, "%s", "GTE NA%01x", opcode & 0x0F);
    }
}

static void disassemble_COP(u32 opcode, struct jsm_string *out, struct R3000ctxt *ctxt)
{
    u32 copnum = (opcode >> 26) & 3;
    if (copnum == 2) {
        disassemble_GTE(opcode, out, ctxt);
        return;
    }
    u32 opc = (opcode >> 28) & 15;
    u32 bit25 = (opcode >> 25) & 1;
    u32 bits5 = (opcode >> 21) & 0x1F;
    u32 rt = (opcode >> 16) & 0x1F;
    u32 rd = (opcode >> 11) & 0x1F;
    u32 rs = (opcode >> 21) & 0x1F;
    u32 imm16 = (opcode & 0xFFFF)>>0;
    u32 imm25 = (opcode & 0x1FFFFFF)>>0;
    switch(opc) {
        case 4: //
            switch(bit25) {
                case 0:
                    switch(bits5) {
                        case 0: // MFCN rt, rd
                            jsm_string_sprintf(out, "MFC%d %s, COP%dd%d", copnum, reg_alias_arr[rt], copnum, rd);
                            add_cop_context(ctxt, rd);
                            add_r_context(ctxt, rt);
                            return;
                        case 2: // CFCn rt, rd
                            jsm_string_sprintf(out, "CFC%d %s, COP%dc%d", copnum, reg_alias_arr[rt], copnum, rd);
                            add_r_context(ctxt, rt);
                            add_cop_context(ctxt, rd);
                            return;
                        case 4: // MTCn rt, rd
                            jsm_string_sprintf(out, "MTC%d COP%dd%d, %s", copnum, copnum, rd, reg_alias_arr[rt]);
                            add_cop_context(ctxt, rd);
                            add_r_context(ctxt, rt);
                            return;
                        case 5: // CTCn rt, rd
                            jsm_string_sprintf(out, "CTC%d COP%dc%d, %s", copnum, copnum, rd, reg_alias_arr[rt]);
                            add_cop_context(ctxt, rd);
                            add_r_context(ctxt, rt);
                            return;
                        case 8: // rt=0 BCnF, rt=1 BCnT
                            if (rt == 0)
                                jsm_string_sprintf(out, "BC%dF #$%04x", copnum, imm16);
                            else
                                jsm_string_sprintf(out, "BC%dT #$%04x", copnum, imm16);
                            return;
                        default:
                                jsm_string_sprintf(out, "BADCOP8");
                                return;
                    }
                case 1: // // Immediate 25-bit or some COP0 instructions
                    if ((bits5 == 0x10) && (copnum == 0)) {
                        switch(opcode & 0x1F) {
                            case 1:
                                jsm_string_sprintf(out, "tlbr");
                                return;
                            case 2:
                                jsm_string_sprintf(out, "tlbwi");
                                return;
                            case 6:
                                jsm_string_sprintf(out, "tlbwr");
                                return;
                            case 8:
                                jsm_string_sprintf(out, "tlbp");
                                return;
                            case 0x10:
                                jsm_string_sprintf(out, "rfe");
                                return;
                            default:
                                jsm_string_sprintf(out, "BADCOP0");
                                return;
                        }
                    }
                    jsm_string_sprintf(out, "COP%dd #$%04x, %d", copnum, imm25, bits5);
                    return;
            }
            jsm_string_sprintf(out, "COPWHAT?");
            return;
        case 0x0C: // LWCn rt_dat, [rs+imm]
            jsm_string_sprintf(out, "LWC%d COP%dd%d, [%s+#$%04x]", copnum, copnum,  rt, reg_alias_arr[rs], imm16);
            return;
        case 0x0E: // SWCn rt_dat, [rs+imm]
            jsm_string_sprintf(out, "SWC%d COP%dd%d [%s+#$%04x]", copnum, copnum, rt, reg_alias_arr[rs], imm16);
            return;
        default:
            jsm_string_sprintf(out, "UNKNOWN COP INS!");
            return;
    }
    NOGOHERE;
}

void R3000_disassemble(u32 opcode, struct jsm_string *out, i64 ins_addr, struct R3000ctxt *ct)
{
    jsm_string_quickempty(out);
    if (opcode == 0) {
        jsm_string_sprintf(out, "nop        ");
        return;
    }

    char ostr1[100], ostr2[100];
    memset(ostr1, 0, 100);
    memset(ostr2, 0, 100);
    
    u32 p_op = (opcode >> 26) & 63;
    u32 s_op = opcode & 63;
    u32 imm16 = opcode & 0xFFFF;
    u32 rs = (opcode >> 21) & 31;
    u32 rt = (opcode >> 16) & 31;
    u32 rd = (opcode >> 11) & 31;
    u32 imm5 = (opcode >> 6) & 31;
    u32 num = p_op & 3;
    
    switch(p_op) {
        case 0x00: // SPECIAL
            switch(s_op) {
                case 0x00: // SLL
                    snprintf(ostr1, sizeof(ostr1), "sll");
                    snprintf(ostr2, sizeof(ostr2), "%s, %s, %d", reg_alias_arr[rd], reg_alias_arr[rt], imm5);
                    add_r_context(ct, rd);
                    add_r_context(ct, rt);
                    add_r_context(ct, rs);
                    break;
                case 0x02: // SRL
                    snprintf(ostr1, sizeof(ostr1), "srl");
                    snprintf(ostr2, sizeof(ostr2), "%s, %s, %d", reg_alias_arr[rd], reg_alias_arr[rt], imm5);
                    add_r_context(ct, rd);
                    add_r_context(ct, rt);
                    add_r_context(ct, rs);
                    break;
                case 0x03: // SRA
                    snprintf(ostr1, sizeof(ostr1), "sra");
                    snprintf(ostr2, sizeof(ostr2), "%s, %s, %d", reg_alias_arr[rd], reg_alias_arr[rt], imm5);
                    add_r_context(ct, rd);
                    add_r_context(ct, rt);
                    add_r_context(ct, rs);
                    break;
                case 0x04: // SLLV
                    snprintf(ostr1, sizeof(ostr1), "sllv");
                    snprintf(ostr2, sizeof(ostr2), "%s, %s, %s", reg_alias_arr[rd], reg_alias_arr[rt], reg_alias_arr[rs]);
                    add_r_context(ct, rd);
                    add_r_context(ct, rt);
                    add_r_context(ct, rs);
                    break;
                case 0x06: // SRLV
                    snprintf(ostr1, sizeof(ostr1), "srlv");
                    snprintf(ostr2, sizeof(ostr2), "%s, %s, %s", reg_alias_arr[rd], reg_alias_arr[rt], reg_alias_arr[rs]);
                    add_r_context(ct, rd);
                    add_r_context(ct, rt);
                    add_r_context(ct, rs);
                    break;
                case 0x07: // SRAV
                    snprintf(ostr1, sizeof(ostr1), "srav");
                    snprintf(ostr2, sizeof(ostr2), "%s, %s, %s", reg_alias_arr[rd], reg_alias_arr[rt], reg_alias_arr[rs]);
                    add_r_context(ct, rd);
                    add_r_context(ct, rt);
                    add_r_context(ct, rs);
                    break;
                case 0x08: // JR
                    snprintf(ostr1, sizeof(ostr1), "jr");
                    snprintf(ostr2, sizeof(ostr2), "%s", reg_alias_arr[rs]);
                    add_r_context(ct, rs);
                    break;
                case 0x09: // JALR
                    snprintf(ostr1, sizeof(ostr1), "jalr");
                    snprintf(ostr2, sizeof(ostr2), "%s, %s", reg_alias_arr[rd], reg_alias_arr[rs]);
                    add_r_context(ct, rd);
                    add_r_context(ct, rs);
                    break;
                case 0x0C: // SYSCALL
                    snprintf(ostr1, sizeof(ostr1), "syscall");
                    snprintf(ostr2, sizeof(ostr2), "#$%05x", (opcode >> 6) & 0xFFFFF);
                    break;
                case 0x0D: // BREAK
                    snprintf(ostr1, sizeof(ostr1), "break");
                    snprintf(ostr2, sizeof(ostr2), "#$%05x", (opcode >> 6) & 0xFFFFF);
                    break;
                case 0x10: // MFHI
                    snprintf(ostr1, sizeof(ostr1), "mfhi");
                    snprintf(ostr2, sizeof(ostr2), "%s", reg_alias_arr[rd]);
                    add_r_context(ct, rd);
                    break;
                case 0x11: // MTHI
                    snprintf(ostr1, sizeof(ostr1), "mthi");
                    snprintf(ostr2, sizeof(ostr2), "%s", reg_alias_arr[rs]);
                    add_r_context(ct, rs);
                    break;
                case 0x12: // MFLO
                    snprintf(ostr1, sizeof(ostr1), "mfhi");
                    snprintf(ostr2, sizeof(ostr2), "%s", reg_alias_arr[rd]);
                    add_r_context(ct, rd);
                    break;
                case 0x13: // MTLO
                    snprintf(ostr1, sizeof(ostr1), "mfhi");
                    snprintf(ostr2, sizeof(ostr2), "%s", reg_alias_arr[rs]);
                    add_r_context(ct, rs);
                    break;
                case 0x18: // MULT
                    snprintf(ostr1, sizeof(ostr1), "mult");
                    snprintf(ostr2, sizeof(ostr2), "%s, %s", reg_alias_arr[rd], reg_alias_arr[rt]);
                    add_r_context(ct, rd);
                    add_r_context(ct, rt);
                    break;
                case 0x19: // MULTU
                    snprintf(ostr1, sizeof(ostr1), "multu");
                    snprintf(ostr2, sizeof(ostr2), "%s, %s", reg_alias_arr[rd], reg_alias_arr[rt]);
                    add_r_context(ct, rd);
                    add_r_context(ct, rt);
                    break;
                case 0x1A: // DIV
                    snprintf(ostr1, sizeof(ostr1), "div");
                    snprintf(ostr2, sizeof(ostr2), "%s, %s", reg_alias_arr[rd], reg_alias_arr[rt]);
                    add_r_context(ct, rd);
                    add_r_context(ct, rt);
                    break;
                case 0x1B: // DIVU
                    snprintf(ostr1, sizeof(ostr1), "divu");
                    snprintf(ostr2, sizeof(ostr2), "%s, %s", reg_alias_arr[rd], reg_alias_arr[rt]);
                    add_r_context(ct, rd);
                    add_r_context(ct, rt);
                    break;
                case 0x20: // ADD
                    snprintf(ostr1, sizeof(ostr1), "add");
                    snprintf(ostr2, sizeof(ostr2), "%s, %s, %s", reg_alias_arr[rd], reg_alias_arr[rs], reg_alias_arr[rt]);
                    add_r_context(ct, rd);
                    add_r_context(ct, rs);
                    add_r_context(ct, rt);
                    break;
                case 0x21: // ADDU
                    snprintf(ostr1, sizeof(ostr1), "addu");
                    snprintf(ostr2, sizeof(ostr2), "%s, %s, %s", reg_alias_arr[rd], reg_alias_arr[rs], reg_alias_arr[rt]);
                    add_r_context(ct, rd);
                    add_r_context(ct, rs);
                    add_r_context(ct, rt);
                    break;
                case 0x22: // SUB
                    snprintf(ostr1, sizeof(ostr1), "sub");
                    snprintf(ostr2, sizeof(ostr2), "%s, %s, %s", reg_alias_arr[rd], reg_alias_arr[rs], reg_alias_arr[rt]);
                    add_r_context(ct, rd);
                    add_r_context(ct, rs);
                    add_r_context(ct, rt);
                    break;
                case 0x23: // SUBU
                    snprintf(ostr1, sizeof(ostr1), "subu");
                    snprintf(ostr2, sizeof(ostr2), "%s, %s, %s", reg_alias_arr[rd], reg_alias_arr[rs], reg_alias_arr[rt]);
                    add_r_context(ct, rd);
                    add_r_context(ct, rs);
                    add_r_context(ct, rt);
                    break;
                case 0x24: // AND
                    snprintf(ostr1, sizeof(ostr1), "and");
                    snprintf(ostr2, sizeof(ostr2), "%s, %s, %s", reg_alias_arr[rd], reg_alias_arr[rs], reg_alias_arr[rt]);
                    add_r_context(ct, rd);
                    add_r_context(ct, rs);
                    add_r_context(ct, rt);
                    break;
                case 0x25: // OR
                    snprintf(ostr1, sizeof(ostr1), "or");
                    snprintf(ostr2, sizeof(ostr2), "%s, %s, %s", reg_alias_arr[rd], reg_alias_arr[rs], reg_alias_arr[rt]);
                    add_r_context(ct, rd);
                    add_r_context(ct, rs);
                    add_r_context(ct, rt);
                    break;
                case 0x26: // XOR
                    snprintf(ostr1, sizeof(ostr1), "xor");
                    snprintf(ostr2, sizeof(ostr2), "%s, %s, %s", reg_alias_arr[rd], reg_alias_arr[rs], reg_alias_arr[rt]);
                    add_r_context(ct, rd);
                    add_r_context(ct, rs);
                    add_r_context(ct, rt);
                    break;
                case 0x27: // NOR
                    snprintf(ostr1, sizeof(ostr1), "nor");
                    snprintf(ostr2, sizeof(ostr2), "%s, %s, %s", reg_alias_arr[rd], reg_alias_arr[rs], reg_alias_arr[rt]);;
                    add_r_context(ct, rd);
                    add_r_context(ct, rs);
                    add_r_context(ct, rt);

                    break;
                case 0x2A: // SLT
                    snprintf(ostr1, sizeof(ostr1), "slt");
                    snprintf(ostr2, sizeof(ostr2), "%s, %s, %s", reg_alias_arr[rd], reg_alias_arr[rs], reg_alias_arr[rt]);
                    add_r_context(ct, rd);
                    add_r_context(ct, rs);
                    add_r_context(ct, rt);
                    break;
                case 0x2B: // SLTU
                    snprintf(ostr1, sizeof(ostr1), "sltu");
                    snprintf(ostr2, sizeof(ostr2), "%s, %s, %s", reg_alias_arr[rd], reg_alias_arr[rs], reg_alias_arr[rt]);
                    add_r_context(ct, rd);
                    add_r_context(ct, rs);
                    add_r_context(ct, rt);
                    break;
                default:
                    snprintf(ostr1, sizeof(ostr1), "UNKNOWN.b %02x %08x", s_op, opcode);
                    break;
            }
            break;
        case 0x01: // BcondZ
            switch((opcode >> 16) & 31) {
                case 0:
                    snprintf(ostr1, sizeof(ostr1), "bltz");
                    break;
                case 1:
                    snprintf(ostr1, sizeof(ostr1), "bgez");
                    break;
                case 0x10:
                    snprintf(ostr1, sizeof(ostr1), "bltzal");
                    break;
                case 0x11:
                    snprintf(ostr1, sizeof(ostr1), "bgezal");
                    break;
                default:
                    snprintf(ostr1, sizeof(ostr1), "UNKNOWN.a %08x", opcode);
                    break;
            }
            snprintf(ostr2, sizeof(ostr2), "%s, #$%04x", reg_alias_arr[rs], imm16);
            add_r_context(ct, rs);
            break;
        case 0x02: // J
            snprintf(ostr1, sizeof(ostr1), "j");
            snprintf(ostr2, sizeof(ostr2), "$%08x", 0xF0000000 + ((opcode & 0x3FFFFFF) * 4));
            break;
        case 0x03: // JAL
            snprintf(ostr1, sizeof(ostr1), "jalr");
            snprintf(ostr2, sizeof(ostr2), "$%08x", 0xF0000000 + ((opcode & 0x3FFFFFF) * 4));
            break;
        case 0x04: // BEQ
            snprintf(ostr1, sizeof(ostr1), "beq");
            snprintf(ostr2, sizeof(ostr2), "%s, %s, #$%04x", reg_alias_arr[rs], reg_alias_arr[rt], imm16*4);
            add_r_context(ct, rs);
            break;
        case 0x05: // BNE
            snprintf(ostr1, sizeof(ostr1), "bne");
            snprintf(ostr2, sizeof(ostr2), "%s, %s, #$%04x", reg_alias_arr[rs], reg_alias_arr[rt], imm16*4);
            add_r_context(ct, rs);
            add_r_context(ct, rt);
            break;
        case 0x06: // BLEZ
            snprintf(ostr1, sizeof(ostr1), "blez");
            snprintf(ostr2, sizeof(ostr2), "%s, #$%04x", reg_alias_arr[rs], imm16*4);
            add_r_context(ct, rs);
            break;
        case 0x07: // BGTZ
            snprintf(ostr1, sizeof(ostr1), "bgtz");
            snprintf(ostr2, sizeof(ostr2), "%s, #$%04x", reg_alias_arr[rs], imm16*4);
            add_r_context(ct, rs);
            break;
        case 0x08: // ADDI
            snprintf(ostr1, sizeof(ostr1), "addi");
            snprintf(ostr2, sizeof(ostr2), "%s, %s, #$%04x", reg_alias_arr[rt], reg_alias_arr[rs], imm16);
            add_r_context(ct, rt);
            add_r_context(ct, rs);
            break;
        case 0x09: // ADDIU
            snprintf(ostr1, sizeof(ostr1), "addiu");
            snprintf(ostr2, sizeof(ostr2), "%s, %s, #$%04x", reg_alias_arr[rt], reg_alias_arr[rs], imm16);
            add_r_context(ct, rt);
            add_r_context(ct, rs);
            break;
        case 0x0A: // SLTI
            snprintf(ostr1, sizeof(ostr1), "setlt slti");
            snprintf(ostr2, sizeof(ostr2), "%s, %s, #$%04x", reg_alias_arr[rt], reg_alias_arr[rs], imm16);
            add_r_context(ct, rt);
            add_r_context(ct, rs);
            break;
        case 0x0B: // SLTIU
            snprintf(ostr1, sizeof(ostr1), "setb sltiu");
            snprintf(ostr2, sizeof(ostr2), "%s, %s, #$%04x", reg_alias_arr[rt], reg_alias_arr[rs], imm16);
            add_r_context(ct, rt);
            add_r_context(ct, rs);
            break;
        case 0x0C: // ANDI
            snprintf(ostr1, sizeof(ostr1), "andi");
            snprintf(ostr2, sizeof(ostr2), "%s, %s, #$%04x", reg_alias_arr[rt], reg_alias_arr[rs], imm16);
            add_r_context(ct, rt);
            add_r_context(ct, rs);
            break;
        case 0x0D: // ORI
            snprintf(ostr1, sizeof(ostr1), "ori");
            snprintf(ostr2, sizeof(ostr2), "%s, %s, #$%04x", reg_alias_arr[rt], reg_alias_arr[rs], imm16);
            add_r_context(ct, rt);
            add_r_context(ct, rs);
            break;
        case 0x0E: // XORI
            snprintf(ostr1, sizeof(ostr1), "xori");
            snprintf(ostr2, sizeof(ostr2), "%s, %s, #$%04x", reg_alias_arr[rt], reg_alias_arr[rs], imm16);
            add_r_context(ct, rt);
            add_r_context(ct, rs);
            break;
        case 0x0F: // LUI
            snprintf(ostr1, sizeof(ostr1), "lui");
            snprintf(ostr2, sizeof(ostr2), "%s, $%04x0000", reg_alias_arr[rt], imm16);
            add_r_context(ct, rt);
            break;
        case 0x13: // COP3
        case 0x12: // COP2
        case 0x11: // COP1
        case 0x10: // COP0
            disassemble_COP(opcode, out, ct);
            return;
        case 0x20: // LB
            snprintf(ostr1, sizeof(ostr1), "lb");
            snprintf(ostr2, sizeof(ostr2), "%s, %04x(%s)", reg_alias_arr[rt], imm16, reg_alias_arr[rs]);
            add_r_context(ct, rt);
            add_r_context(ct, rs);
            break;
        case 0x21: // LH
            snprintf(ostr1, sizeof(ostr1), "lh");
            snprintf(ostr2, sizeof(ostr2), "%s, %04x(%s)", reg_alias_arr[rt], imm16, reg_alias_arr[rs]);
            add_r_context(ct, rt);
            add_r_context(ct, rs);
            break;
        case 0x22: // LWL
            snprintf(ostr1, sizeof(ostr1), "lwl");
            snprintf(ostr2, sizeof(ostr2), "%s, %04x(%s)", reg_alias_arr[rt], imm16, reg_alias_arr[rs]);
            add_r_context(ct, rt);
            add_r_context(ct, rs);
            break;
        case 0x23: // LW
            snprintf(ostr1, sizeof(ostr1), "lw");
            snprintf(ostr2, sizeof(ostr2), "%s, %04x(%s)", reg_alias_arr[rt], imm16, reg_alias_arr[rs]);
            add_r_context(ct, rt);
            add_r_context(ct, rs);
            break;
        case 0x24: // LBU
            snprintf(ostr1, sizeof(ostr1), "lbu");
            snprintf(ostr2, sizeof(ostr2), "%s, %04x(%s)", reg_alias_arr[rt], imm16, reg_alias_arr[rs]);
            add_r_context(ct, rt);
            add_r_context(ct, rs);
            break;
        case 0x25: // LHU
            snprintf(ostr1, sizeof(ostr1), "lhu");
            snprintf(ostr2, sizeof(ostr2), "%s, %04x(%s)", reg_alias_arr[rt], imm16, reg_alias_arr[rs]);
            add_r_context(ct, rt);
            add_r_context(ct, rs);
            break;
        case 0x26: // LWR
            snprintf(ostr1, sizeof(ostr1), "lwr");
            snprintf(ostr2, sizeof(ostr2), "%s, %04x(%s)", reg_alias_arr[rt], imm16, reg_alias_arr[rs]);
            add_r_context(ct, rt);
            add_r_context(ct, rs);
            break;
        case 0x28: // SB
            snprintf(ostr1, sizeof(ostr1), "sb");
            snprintf(ostr2, sizeof(ostr2), "%s, %04x(%s)", reg_alias_arr[rt], imm16, reg_alias_arr[rs]);
            add_r_context(ct, rt);
            add_r_context(ct, rs);
            break;
        case 0x29: // SH
            snprintf(ostr1, sizeof(ostr1), "sh");
            snprintf(ostr2, sizeof(ostr2), "%s, %04x(%s)", reg_alias_arr[rt], imm16, reg_alias_arr[rs]);
            add_r_context(ct, rt);
            add_r_context(ct, rs);
            break;
        case 0x2A: // SWL
            snprintf(ostr1, sizeof(ostr1), "swl");
            snprintf(ostr2, sizeof(ostr2), "%s, %04x(%s)", reg_alias_arr[rt], imm16, reg_alias_arr[rs]);
            add_r_context(ct, rt);
            add_r_context(ct, rs);
            break;
        case 0x2B: // SW
            snprintf(ostr1, sizeof(ostr1), "sw");
            snprintf(ostr2, sizeof(ostr2), "%s, %04x(%s)", reg_alias_arr[rt], imm16, reg_alias_arr[rs]);
            add_r_context(ct, rt);
            add_r_context(ct, rs);
            break;
        case 0x2E: // SWR
            snprintf(ostr1, sizeof(ostr1), "swr");
            snprintf(ostr2, sizeof(ostr2), "%s, %04x(%s)", reg_alias_arr[rt], imm16, reg_alias_arr[rs]);
            add_r_context(ct, rt);
            add_r_context(ct, rs);
            break;
        case 0x33: // LWC3
        case 0x32: // LWC2
        case 0x31: // LWC1
        case 0x30: // LWC0
            snprintf(ostr1, sizeof(ostr1), "lwc%d", num);
            snprintf(ostr2, sizeof(ostr2), "%s, %04x(%s)", reg_alias_arr[rt], imm16, reg_alias_arr[rs]);
            add_r_context(ct, rt);
            add_r_context(ct, rs);
            break;
        case 0x3B: // SWC3
        case 0x3A: // SWC2
        case 0x39: // SWC1
        case 0x38: // SWC0
            snprintf(ostr1, sizeof(ostr1), "swc%d", num);
            snprintf(ostr2, sizeof(ostr2), "%s, %04x(%s)", reg_alias_arr[rt], imm16, reg_alias_arr[rs]);
            add_r_context(ct, rt);
            add_r_context(ct, rs);
            break;
    }

    u32 s1l = strlen(ostr1);
    char *ptr = ostr1 + s1l;
    while(s1l < 11) {
        *ptr = ' ';
        ptr++;
        s1l++;
    }
    *ptr = 0;

    if (strlen(ostr2) > 0) {
        jsm_string_sprintf(out, "%s%s", ostr1, ostr2);
    }
    else {
        jsm_string_sprintf(out, "%s", ostr1);
    }
}