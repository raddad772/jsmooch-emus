//
// Created by . on 12/21/24.
//

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "armv4_disassembler.h"
#include "thumb_disassembler.h"

static u16 doBITS(u16 val, u16 hi, u16 lo)
{
    u16 shift = lo;
    u16 mask = (1 << ((hi - lo) + 1)) - 1;
    return (val >> shift) & mask;
}

#define OBIT(x) ((opc >> (x)) & 1)
#define BITS(hi,lo) (doBITS(opc, hi, lo))
#define ostr(...) jsm_string_sprintf(out, __VA_ARGS__)

static void add_context(ARMctxt *t, u32 rnum)
{
    if (t) t->regs |= (1 << rnum);
}

static void outreg(jsm_string *out, u32 num, u32 add_comma) {
    if (num == 13) ostr("SP");
    else if (num == 14) ostr("LR");
    else if (num == 15) ostr("PC");
    else ostr("R%d",num);
    if (add_comma) ostr(",");
}

static void outhex(jsm_string *out, u32 num, u32 num_size, u32 add_comma) {
    char fstr[50];
    snprintf(fstr, sizeof(fstr), "%%0%dx", num_size);
    jsm_string_sprintf(out, fstr, num);
    if (add_comma) ostr(",");
}

static void outdec(jsm_string *out, u32 num, u32 add_comma)
{
    ostr("%d", num);
    if (add_comma) ostr(",");
}

#define oregc(x) outreg(out, x, 1)
#define oreg(x) outreg(out, x, 0)

#define ohex(x,x_size) outhex(out, x, x_size, 0)
#define odec(x) outdec(out, x, 0)

#define oreg_dec(x,y) { oregc(x); odec(y); }
#define oreg_hex(x,y,z) { oregc(x); ohex(y,z); }
#define oreg2_dec(x,y,z) { oregc(x); oregc(y); odec(z); }
#define oreg2_hex(x,y,z,z_size) { oregc(x); oregc(y); ohex(z,z_size); }
#define oreg2(x,y) { oregc(x); oreg(y); }
#define oreg3(x,y,z) { oregc(x); oregc(y); oreg(z); }

#define rd_addr_rb_ro    { oregc(Rd); ostr("["); oregc(Rb); oreg(Ro); ostr("]"); }
#define rd_addr_rb_imm    { oregc(Rd); ostr("["); oregc(Rb); odec(imm); ostr("]"); }

static void dasm_invalid(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    ostr("unknown ");
    ohex(opc, 4);
}

static void dasm_ADD_SUB(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    u32 I = OBIT(10); // 0 = register, 1 = immediate
    u32 sub_opcode = OBIT(9); // 0= add, 1 = sub
    u32 Rn, imm;
    if (!I) {
        Rn = BITS(8, 6);
        add_context(ct, Rn);
    }
    else
        imm = BITS(8, 6);
    u32 Rs = BITS(5, 3);
    u32 Rd = BITS(2, 0);
    add_context(ct, Rs);
    if ((sub_opcode == 2) && (imm == 0)) { // MOV alias
        ostr("mov  ");
        oreg2(Rd, Rs);
        return;
    }
    switch(sub_opcode) {
        case 0: // add Rd,Rs,Rn
            ostr("add   ");
            oreg3(Rd, Rs, Rn);
            return;
        case 1: // sub Rd, Rs, Rn
            ostr("sub   ");
            oreg3(Rd, Rs, Rn);
            return;
        case 2: // add Rd, Rs, #nn
            ostr("add   ");
            oreg2_hex(Rd, Rs, imm, 2);
            return;
        case 3: // sub Rd, Rs, #nn
            ostr("sub   ");
            oreg2_hex(Rd, Rs, imm, 2);
            return;
    }
    NOGOHERE;
}

static void dasm_LSL_LSR_ASR(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    u32 sub_opcode = BITS(12, 11);
    u32 imm = BITS(10, 6);
    u32 Rs = BITS(5, 3);
    u32 Rd = BITS(2, 0);
    add_context(ct, Rs);
    switch(sub_opcode) {
        case 0: ostr("lsl  "); break;
        case 1: ostr("lsr  "); break;
        case 2: ostr("asr  "); break;
        case 3:
            jsm_string_sprintf(out, "BAD INSTRUCTION1");
            return;
        default:
            NOGOHERE;
    }
    oreg2_dec(Rd,Rs,imm);
}
static void dasm_MOV_CMP_ADD_SUB(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    u32 sub_opcode = BITS(12, 11);
    u32 Rd = BITS(10, 8);
    u32 imm = BITS(7, 0);
    switch(sub_opcode) {
        case 0: ostr("movs "); break;
        case 1: ostr("cmps "); break;
        case 2: ostr("adds "); break;
        case 3: ostr("subs "); break;
    }
    oreg_hex(Rd, imm, 2);
}

static void dasm_data_proc(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    u32 sub_opcode = BITS(9, 6);
    u32 Rs = BITS(5, 3);
    u32 Rd = BITS(2, 0);
    add_context(ct, Rs);
    switch(sub_opcode) {
        case 0: ostr("and "); break;
        case 1: ostr("eor "); break;
        case 2: ostr("lsl "); break;
        case 3: ostr("lsr "); break;
        case 4: ostr("asr "); break;
        case 5: ostr("adc "); break;
        case 6: ostr("sbc "); break;
        case 7: ostr("ror "); break;
        case 8: ostr("tst "); break;
        case 9: ostr("neg "); break;
        case 10: ostr("cmp "); break;
        case 11: ostr("cmn "); break;
        case 12: ostr("orr "); break;
        case 13: ostr("mul "); break;
        case 14: ostr("bic "); break;
        case 15: ostr("mvn "); break;
    }
    oreg2(Rd, Rs);
}
static void dasm_BX(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    ostr("bx  ");
    u32 Rs = OBIT(6) << 3;
    Rs |= BITS(5,3);
    add_context(ct, Rs);
    oreg(Rs);
}

static void dasm_ADD_CMP_MOV_hi(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    u32 Rd = OBIT(7) << 3;
    u32 Rs = OBIT(6) << 3;
    Rs |= BITS(5,3);
    Rd |= BITS(2, 0);
    add_context(ct, Rs);
    u32 sub_opcode = BITS(9, 8);
    if ((sub_opcode == 2) && (Rd == 8) && (Rs == 8)) {
        ostr("nop ");
        return;
    }
    switch(sub_opcode) {
        case 0: ostr("add "); break;
        case 1: ostr("cmp "); break;
        case 2: ostr("mov "); break;
        default:
            NOGOHERE;
    }
    oreg2(Rd, Rs);
}
static void dasm_LDR_PC_relative(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    u32 Rd = BITS(10, 8);
    u32 imm = BITS(7, 0);
    imm <<= 2;
    ostr("ldr  ");
    oregc(Rd);
    add_context(ct, Rd);
    ostr("[PC,#");
    ohex(imm, 1);
    ostr("]");
}

static void dasm_LDRH_STRH_reg_offset(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    u32 L = OBIT(11);
    u32 Ro = BITS(8, 6);
    u32 Rb = BITS(5, 3);
    u32 Rd = BITS(2, 0);
    add_context(ct, Ro);
    add_context(ct, Rb);
    if (L)
        ostr("ldrh ");
    else {
        add_context(ct, Rd);
        ostr("strh ");
    }
    rd_addr_rb_ro;
}



static void dasm_LDRSH_LDRSB_reg_offset(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    u32 B = !OBIT(11);
    u32 Ro = BITS(8, 6);
    u32 Rb = BITS(5, 3);
    u32 Rd = BITS(2, 0);
    add_context(ct, Ro);
    add_context(ct, Rb);
    if (B) ostr("ldrsb ");
    else ostr("ldrsh ");
    rd_addr_rb_ro;
}

static void dasm_LDR_STR_reg_offset(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    u32 L = OBIT(11);
    u32 Ro = BITS(8, 6);
    u32 Rb = BITS(5, 3);
    u32 Rd = BITS(2, 0);
    add_context(ct, Rb);
    add_context(ct, Ro);
    if (L) ostr("ldr  ");
    else {
        add_context(ct, Rd);
        ostr("str  ");
    }
    rd_addr_rb_ro;
}

static void dasm_LDRB_STRB_reg_offset(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    u32 L = OBIT(11); // 0=STR, 1=LDR
    u32 Ro = BITS(8, 6);
    u32 Rb = BITS(5, 3);
    u32 Rd = BITS(2, 0);
    add_context(ct, Rb);
    add_context(ct, Ro);
    if (L) ostr("ldrb  ");
    else {
        add_context(ct, Rd);
        ostr("strb  ");
    }
    rd_addr_rb_ro;
}

static void dasm_LDR_STR_imm_offset(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    u32 L = OBIT(11);
    u32 imm = BITS(10, 6);
    imm <<= 2;
    u32 Rb = BITS(5, 3);
    u32 Rd = BITS(2, 0);
    add_context(ct, Rb);
    if (L) ostr("ldr  ");
    else {
        ostr("str  ");
        add_context(ct, Rd);
    }
    rd_addr_rb_imm;
}

static void dasm_LDRB_STRB_imm_offset(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    u32 L = OBIT(11);
    u32 imm = BITS(10, 6);
    u32 Rb = BITS(5, 3);
    u32 Rd = BITS(2, 0);
    add_context(ct, Rb);
    if (L) ostr("ldrb ");
    else {
        ostr("strb ");
        add_context(ct, Rd);
    }
    rd_addr_rb_imm;
}

static void dasm_LDRH_STRH_imm_offset(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    u32 L = OBIT(11);
    u32 imm = BITS(10, 6);
    imm <<= 1;
    u32 Rb = BITS(5, 3);
    add_context(ct, Rb);
    u32 Rd = BITS(2, 0);
    if (L) ostr("ldrh ");
    else {
        ostr("strh ");
        add_context(ct, Rd);
    }
    rd_addr_rb_imm;
}

static void dasm_LDR_STR_SP_relative(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    u32 L = OBIT(11);
    u32 Rd = BITS(10, 8);
    u32 imm = BITS(7, 0);
    imm <<= 2;
    if (L) ostr("ldr   ");
    else {
        add_context(ct, Rd);
        ostr("str   ");
    }
    // Rd,[SP,#nn]
    oregc(Rd);
    ostr("[sp,#");
    odec(imm);
    ostr("]");
}

static void dasm_ADD_SP_or_PC(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    u32 SP = OBIT(11);
    u32 Rd = BITS(10, 8);
    u32 imm = BITS(7, 0);
    imm <<= 2;

    add_context(ct, Rd);
    if (SP) {
        // 1: ADD  Rd,SP,#nn    ;Rd = SP + nn
        ostr("add  ");
        oregc(Rd);
        ostr("SP,#");
        odec(imm);
    }
    else {
        // 0: ADD  Rd,PC,#nn
        ostr("add  ");
        oregc(Rd);
        ostr("PC,#");
        odec(imm);
    }
}

static void dasm_ADD_SUB_SP(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    u32 S = OBIT(7);
    u32 imm = BITS(6, 0);
    imm <<= 2;
    if (S) {
        ostr("add  SP,#-");
        odec(imm);
    }
    else {
        ostr("add  SP,#");
        odec(imm);
    }
}

static void do_rlist(jsm_string *out, u16 rlist)
{
    if (rlist == 0) return;
    u32 did_opening = 0;
    for (u32 i = 0; i < 8; i++) {
        if (rlist & (1 << i)) {
            if (!did_opening) {
                ostr("{");
                did_opening = 1;
            } else {
                ostr(",");
            }
            ostr("r%d", i);
        }
    }
}

static void dasm_PUSH_POP(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    // 0: PUSH {Rlist}{LR}   ;store in memory, decrements SP (R13)
    // 1: POP  {Rlist}{PC}   ;load from memory, increments SP (R13
    u32 sub_opcode = OBIT(11);
    u32 PC_LR = OBIT(8);
    u32 rlist = BITS(7, 0);
    if (sub_opcode == 0) ostr("push ");
    else ostr("pop  ");
    do_rlist(out, rlist);
    if ((sub_opcode == 0) && PC_LR) {
        if (rlist != 0) ostr(",LR}");
        else ostr("{LR}");
    }
    else if ((sub_opcode == 1) && PC_LR) {
        if (rlist != 0) ostr(",PC}");
        else ostr("{PC}");
    }
    else {
        if (rlist != 0) ostr("}");
    }
}
static void dasm_LDM_STM(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    u32 sub_opcode = OBIT(11);
    u32 Rb = BITS(10, 8);
    add_context(ct, Rb);
    u32 rlist = BITS(7, 0);
    /*
 0: STMIA Rb!,{Rlist}   ;store in memory, increments Rb
          1: LDMIA Rb!,{Rlist}   ;load from memory, increments Rb     */
    if (sub_opcode == 0) ostr("stmia ");
    else ostr("ldmia ");
    oreg(Rb);
    ostr("!,");
    do_rlist(out, rlist);
    if (rlist) ostr("}");
}

static void dasm_SWI(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    u32 comment = BITS(7, 0);
    ostr("swi   ");
    ohex(comment, 2);
}
static void dasm_UNDEFINED_BCC(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    u32 sub_opcode = BITS(11, 8);
    u32 imm = BITS(7, 0);
    imm = SIGNe8to32(imm);
    imm <<= 1;
    ostr("bcc   undefined");
}

static void dasm_BCC(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    u32 sub_opcode = BITS(11, 8);
    u32 imm = BITS(7, 0);
    imm = SIGNe8to32(imm);
    imm <<= 1;
    switch(sub_opcode) {
        case 0: ostr("beq   "); break;
        case 1: ostr("bne   "); break;
        case 2: ostr("bcs   "); break;
        case 3: ostr("bcc   "); break;
        case 4: ostr("bni   "); break;
        case 5: ostr("bpl   "); break;
        case 6: ostr("bvs   "); break;
        case 7: ostr("bvc   "); break;
        case 8: ostr("bhi   "); break;
        case 9: ostr("bls   "); break;
        case 10: ostr("bge   "); break;
        case 11: ostr("blt   "); break;
        case 12: ostr("bgt   "); break;
        case 13: ostr("ble   "); break;
        case 14: ostr("bund  "); break;
        case 15: ostr("swi   "); break;
    }
    odec(imm);
}
static void dasm_B(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    u32 imm = BITS(10, 0);
    imm = SIGNe11to32(imm);
    imm <<= 1;
    ostr("b     ");
    odec(imm);
}

static void dasm_BL_BLX_prefix(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    u32 imm = BITS(11, 0);
    imm = (i32)SIGNe11to32(imm); // now SHL 11...
    imm <<= 12;
    ostr("bl1   ");
    odec(imm);
}

static void dasm_BL_suffix(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    u32 imm = BITS(10, 0);
    imm <<= 1;
    ostr("bl2   ");
    odec(imm);
}


void ARM7TDMI_thumb_disassemble(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    jsm_string_quickempty(out);

    if ((opc & 0b1111100000000000) == 0b0001100000000000) { // ADD_SUB
        dasm_ADD_SUB(opc, out, ins_addr, ct);
    }
    else if ((opc & 0b1110000000000000) == 0b0000000000000000) { // LSL, LSR, etc.
        dasm_LSL_LSR_ASR(opc, out, ins_addr, ct);
    }
    else if ((opc & 0b1110000000000000) == 0b0010000000000000) { // MOV, CMP, ADD, SUB
        dasm_MOV_CMP_ADD_SUB(opc, out, ins_addr, ct);
    }
    else if ((opc & 0b1111110000000000) == 0b0100000000000000) { // data proc
        dasm_data_proc(opc, out, ins_addr, ct);
    }
    else if ((opc & 0b1111111100000000) == 0b0100011100000000) { // BX
        dasm_BX(opc, out, ins_addr, ct);
    }
    else if ((opc & 0b1111110000000000) == 0b0100010000000000) { // ADD, CMP, MOV hi
        dasm_ADD_CMP_MOV_hi(opc, out, ins_addr, ct);
    }
    else if ((opc & 0b1111100000000000) == 0b0100100000000000) { // 01001.....
        dasm_LDR_PC_relative(opc, out, ins_addr, ct);
    }
    else if ((opc & 0b1111011000000000) == 0b0101001000000000) { // 0101.01...
        dasm_LDRH_STRH_reg_offset(opc, out, ins_addr, ct);
    }
    else if ((opc & 0b1111011000000000) == 0b0101011000000000) { // 0101.11...
        dasm_LDRSH_LDRSB_reg_offset(opc, out, ins_addr, ct);
    }
    else if ((opc & 0b1111011000000000) == 0b0101000000000000) { // 0101.00...
        dasm_LDR_STR_reg_offset(opc, out, ins_addr, ct);
    }
    else if ((opc & 0b1111011000000000) == 0b0101010000000000) { // 0101.10...
        dasm_LDRB_STRB_reg_offset(opc, out, ins_addr, ct);
    }
    else if ((opc & 0b1111000000000000) == 0b0110000000000000) { // 0110......
        dasm_LDR_STR_imm_offset(opc, out, ins_addr, ct);
    }
    else if ((opc & 0b1111000000000000) == 0b0111000000000000) { // 0111......
        dasm_LDRB_STRB_imm_offset(opc, out, ins_addr, ct);
    }
    else if ((opc & 0b1111000000000000) == 0b1000000000000000) { // 1000......
        dasm_LDRH_STRH_imm_offset(opc, out, ins_addr, ct);
    }
    else if ((opc & 0b1111000000000000) == 0b1001000000000000) { // 1001......
        dasm_LDR_STR_SP_relative(opc, out, ins_addr, ct);
    }
    else if ((opc & 0b1111000000000000) == 0b1010000000000000) { // 1010......
        dasm_ADD_SP_or_PC(opc, out, ins_addr, ct);
    }
    else if ((opc & 0b1111111100000000) == 0b1011000000000000) { // 10110000..
        dasm_ADD_SUB_SP(opc, out, ins_addr, ct);
    }
    else if ((opc & 0b1111011000000000) == 0b1011010000000000) { // 1011.10...
        dasm_PUSH_POP(opc, out, ins_addr, ct);
    }
    else if ((opc & 0b1111000000000000) == 0b1100000000000000) { // 1100......
        dasm_LDM_STM(opc, out, ins_addr, ct);
    }
    else if ((opc & 0b1111111100000000) == 0b1101111100000000) { // 11011111..
        dasm_SWI(opc, out, ins_addr, ct);
    }
    else if ((opc & 0b1111111100000000) == 0b1101111000000000) { // 11011110..
        dasm_UNDEFINED_BCC(opc, out, ins_addr, ct);
    }
    else if ((opc & 0b1111000000000000) == 0b1101000000000000) { // 1101......
        dasm_BCC(opc, out, ins_addr, ct);
    }
    else if ((opc & 0b1111100000000000) == 0b1110000000000000) { // 11100.....
        dasm_B(opc, out, ins_addr, ct);
    }
    else if ((opc & 0b1111100000000000) == 0b1111000000000000) { // 11110.....
        dasm_BL_BLX_prefix(opc, out, ins_addr, ct);
    }
    else if ((opc & 0b1111100000000000) == 0b1111100000000000) { // 11111.....
        dasm_BL_suffix(opc, out, ins_addr, ct);
    }
    else {
        dasm_invalid(opc, out, ins_addr, ct);
    }
#undef OBIT
#undef BITS
}