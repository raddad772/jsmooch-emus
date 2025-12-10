//
// Created by . on 12/21/24.
//

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cassert>

#include "armv5_disassembler.h"
namespace ARM946ES {
static u16 doBITS(u16 val, u16 hi, u16 lo)
{
    u16 shift = lo;
    u16 mask = (1 << ((hi - lo) + 1)) - 1;
    return (val >> shift) & mask;
}

#define OBIT(x) ((opcode >> (x)) & 1)
#define BITS(hi,lo) (doBITS(opcode, hi, lo))
#define ostr(...) out->sprintf(__VA_ARGS__)

static void add_context(ARMctxt *t, u32 rnum)
{
    if (t) t->regs |= (1 << rnum);
}

static void outreg(jsm_string *out, u32 num, bool add_comma) {
    if (num == 13) ostr("sp");
    else if (num == 14) ostr("lr");
    else if (num == 15) ostr("pc");
    else ostr("r%d",num);
    if (add_comma) ostr(",");
}

static void outhex(jsm_string *out, u32 num, u32 num_size, bool add_comma) {
    char fstr[50];
    snprintf(fstr, sizeof(fstr), "%%0%dx", num_size);
    out->sprintf(fstr, num);
    if (add_comma) ostr(",");
}

static void outdec(jsm_string *out, u32 num, bool add_comma)
{
    ostr("%d", num);
    if (add_comma) ostr(",");
}

static int out_cond(jsm_string *out, u32 opc, int num_space)
{
    u32 cond = opc >> 28;
    switch(cond) {
        case 0: ostr("eq"); break;
        case 1: ostr("ne"); break;
        case 2: ostr("cs"); break;
        case 3: ostr("cc"); break;
        case 4: ostr("mi"); break;
        case 5: ostr("pl"); break;
        case 6: ostr("vs"); break;
        case 7: ostr("vc"); break;
        case 8: ostr("hi"); break;
        case 9: ostr("ls"); break;
        case 10: ostr("ge"); break;
        case 11: ostr("lt"); break;
        case 12: ostr("gt"); break;
        case 13: ostr("le"); break;
        case 14: num_space += 2; break;
        case 15: ostr("nv"); break;
        default: NOGOHERE;
    }
    if (num_space < 1) return num_space;
    for (int i = 0; i < num_space; i++) {
        ostr(" ");
    }
    return num_space;
}



static void annoying(jsm_string *out, u32 opcode, const char *name, int num_spaces, const char *suffix)
{
    u32 cnd = opcode >> 28;
    ostr(name);
    switch(cnd) {
        case 0: ostr("eq"); break;
        case 1: ostr("ne"); break;
        case 2: ostr("cs"); break;
        case 3: ostr("cc"); break;
        case 4: ostr("mi"); break;
        case 5: ostr("pl"); break;
        case 6: ostr("vs"); break;
        case 7: ostr("vc"); break;
        case 8: ostr("hi"); break;
        case 9: ostr("ls"); break;
        case 10: ostr("ge"); break;
        case 11: ostr("lt"); break;
        case 12: ostr("gt"); break;
        case 13: ostr("le"); break;
        case 14: num_spaces += 2; break;
        case 15: ostr("nv"); break;
        default: NOGOHERE;
    }
    if (suffix) {
        num_spaces -= ostr("%s", suffix);
    }
    if (num_spaces < 1) num_spaces = 1;
    for (int i = 0; i < num_spaces; i++) {
        ostr(" ");
    }
}


#define oregc(x) outreg(out, x, 1)
#define oreg(x) outreg(out, x, 0)
#define orega(x) { ostr("["); oreg(x); ostr("}"); }

#define ohex(x,x_size) outhex(out, x, x_size, 0)
#define odec(x) outdec(out, x, 0)

#define cond(x) out_cond(out, opcode, x)
#define oreg_dec(x,y) { oregc(x); odec(y); }
#define oreg_hex(x,y,z) { oregc(x); ohex(y,z); }
#define oreg2_dec(x,y,z) { oregc(x); oregc(y); odec(z); }
#define oreg2_hex(x,y,z,z_size) { oregc(x); oregc(y); ohex(z,z_size); }
#define oreg2(x,y) { oregc(x); oreg(y); }
#define oreg3(x,y,z) { oregc(x); oregc(y); oreg(z); }
#define oreg4(x,y,z,a) { oregc(x); oregc(y); oregc(z); oreg(a); }

#define rd_addr_rb_ro    { oregc(Rd); ostr("["); oregc(Rb); oreg(Ro); ostr("]"); }
#define rd_addr_rb_imm    { oregc(Rd); ostr("["); oregc(Rb); odec(imm); ostr("]"); }
#define mn(x,y) { ostr(x); cond(y); }
#define mnp(x,y,z) { annoying(out, opcode, x, y, z); }

static void out_shifted_imm(jsm_string *out, u32 shift_type, u32 Is) {
    if ((shift_type != 0) || (Is != 0)) {
        ostr(", ");
        switch (shift_type) {
            case 0:
                ostr(",lsl#");
                break;
            case 1:
                ostr(",lsr#");
                break;
            case 2:
                ostr(",asr#");
                break;
            case 3: {
                if (Is == 0) {
                    ostr(",rrx");
                } else {
                    ostr(",ror#");
                }
                break;
            }
                if ((Is == 0) && ((shift_type == 1) || (shift_type == 2))) {
                    ostr("32");
                } else if ((Is == 0) && (shift_type == 3)) {
                } else {
                    odec(Is);
                }
        }
    }
}


static void dasm_MUL_MLA(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    u32 accumulate = OBIT(21);
    u32 S = OBIT(20);
    u32 Rdd = (opcode >> 16) & 15;
    u32 Rnd = (opcode >> 12) & 15;
    u32 Rsd = (opcode >> 8) & 15;
    u32 Rmd = opcode & 15;
    add_context(ct, Rdd);
    add_context(ct, Rsd);
    add_context(ct, Rmd);
    if (accumulate) {
        mn("mla", 3);
        oreg4(Rdd,Rmd,Rsd,Rnd);
        add_context(ct, Rnd);
    }
    else {
        mn("mul", 3);
        oreg3(Rdd,Rmd,Rsd);
    }
}

static void dasm_MULL_MLAL(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    u32 S = OBIT(20);
    u32 Rdd = (opcode >> 16) & 15;
    u32 Rnd = (opcode >> 12) & 15;
    u32 Rsd = (opcode >> 8) & 15;
    u32 Rmd = opcode & 15;
    u32 sign = OBIT(22); // signed if =1
    u32 accumulate = OBIT(21); // acumulate if =1
    if (sign) ostr("s");
    else ostr("u");
    add_context(ct, Rdd);
    add_context(ct, Rsd);
    add_context(ct, Rmd);
    if (accumulate) {
        mn("mlal", 1);
        oreg4(Rdd,Rmd,Rsd,Rnd);
        add_context(ct, Rnd);
    }
    else {
        mn("mull", 1);
        oreg3(Rdd,Rmd,Rsd);
    }
}

static void dasm_SWP(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    u32 B = OBIT(22);
    u32 Rnd = (opcode >> 16) & 15;
    u32 Rdd = (opcode >> 12) & 15;
    u32 Rmd = opcode & 15;
    if (B) mnp("swp", 3, "b")
    else mn("swp", 3);
    oregc(Rdd);
    oregc(Rmd);
    orega(Rnd);
    add_context(ct, Rdd);
    add_context(ct, Rmd);
    add_context(ct, Rnd);
}

static void dasm_LDRH_STRH(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    u32 P = OBIT(24); // pre or post. 0=post
    u32 U = OBIT(23); // up/down, 0=down
    u32 I = OBIT(22); // 0 = register, 1 = immediate offset
    u32 L = OBIT(20);
    u32 W = 1;
    if (P) W = OBIT(21);
    u32 imm_off = 0;
    imm_off = ((opcode >> 8) & 15) << 4;
    imm_off |= (opcode & 15);
    u32 Rdd = (opcode >> 12) & 15;
    u32 Rnd = (opcode >> 16) & 15;
    u32 Rmd = opcode & 15; // Offset register
    add_context(ct, Rnd);
    add_context(ct, Rmd);
    if (L) {
        mnp("ldr", 3, "h");
    }
    else {
        add_context(ct, Rdd);
        mnp("str", 3, "h");
    }
    oregc(Rdd);
    ostr("[");
    oreg(Rnd); // base regiter
    if (U) ostr("+");
    else ostr("-");
    if (I) odec(imm_off);
    else oreg(Rmd);
    ostr("]");
}

static void dasm_LDRSB_LDRSH(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    u32 P = OBIT(24); // pre or post. 0=post
    u32 U = OBIT(23); // up/down, 0=down
    u32 I = OBIT(22); //
    u32 W = 1;
    if (P) W = OBIT(21);
    u32 Rnd = (opcode >> 16) & 15;
    u32 Rdd = (opcode >> 12) & 15;
    u32 imm_off = ((opcode >> 8) & 15) << 4;
    imm_off |= (opcode & 15);
    u32 Rmd = opcode & 15; // Offset register
    add_context(ct, Rnd);
    add_context(ct, Rmd);
    u32 H = OBIT(5);
    if (H) mn("ldrsh", 3)
    else mn("ldrsb", 3)
    oregc(Rdd);
    ostr("[");
    oreg(Rnd); // base regiter
    ostr("],");
    if (U) ostr("+");
    else ostr("-");
    if (I) odec(imm_off);
    else oreg(Rmd);
}

static void dasm_MRS(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    u32 Rdd = (opcode >> 12) & 15;
    mn("mrs", 3);
    oregc(Rdd);
    add_context(ct, Rdd);
    ostr("cpsr");
}

static void dasm_MSR_reg(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    u32 PSR = OBIT(22); // 0 = CPSR, 1 = SPSR(current)
    u32 f = OBIT(19);
    u32 s = OBIT(18);
    u32 x = OBIT(17);
    u32 c = OBIT(16);
    u32 Rmd = opcode & 15;
    u32 mask = 0;
    if (f) mask |= 0xFF000000;
    if (s) mask |= 0xFF0000;
    if (x) mask |= 0xFF00;
    if (c) mask |= 0xFF;
    mn("msr", 3);
    if (PSR)
        ostr("spsr_");
    else
        ostr("cpsr_");
    if (f) ostr("f");
    if (s) ostr("s");
    if (x) ostr("x");
    if (c) ostr("c");
    ostr(",");
    oreg(Rmd);
    add_context(ct, Rmd);
}

static void dasm_MSR_imm(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    u32 PSR = OBIT(22); // 0 = CPSR, 1 = SPSR(current)
    u32 f = OBIT(19);
    u32 s = OBIT(18);
    u32 x = OBIT(17);
    u32 c = OBIT(16);
    u32 Is = ((opcode >> 8) & 15) << 1;
    u32 imm = opcode & 255;
    if (Is) imm = (imm << (32 - Is)) | (imm >> Is);
    u32 mask = 0;
    if (f) mask |= 0xFF000000;
    if (s) mask |= 0xFF0000;
    if (x) mask |= 0xFF00;
    if (c) mask |= 0xFF;
    mn("msr", 3);
    if (PSR)
        ostr("spsr ");
    else
        ostr("cpsr ");
    if (f) ostr("f");
    if (s) ostr("s");
    if (x) ostr("x");
    if (c) ostr("c");
    ostr(",#$");
    imm &= mask;
    ohex(imm, 8);
}

static void dasm_BX(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    u32 Rnd = opcode & 15;
    mn("bx", 4);
    oreg(Rnd);
    add_context(ct, Rnd);
}

static void dasm_data_proc_immediate_shift(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    u32 alu_opcode = (opcode >> 21) & 15;
    u32 S = (opcode >> 20) & 1; // set condition codes. 0=no, 1=yes. must be 1 for 8-B
    u32 Rnd = (opcode >> 16) & 15; // first operand
    u32 Rdd = (opcode >> 12) & 15; // dest reg.
    u32 Is = (opcode >> 7) & 31; // shift amount
    u32 shift_type = (opcode >> 5) & 3; // 0=LSL, 1=LSR, 2=ASR, 3=ROR
    u32 Rmd = opcode & 15;
    const char *whichs = S ? "s" : nullptr;
    const char *whichp = nullptr;
    switch(alu_opcode) {
        case 0: mnp("and", 3, whichs); break;
        case 1: mnp("eor", 3, whichs); break;
        case 2: mnp("sub", 3, whichs); break;
        case 3: mnp("rsb", 3, whichs); break;
        case 4: mnp("add", 3, whichs); break;
        case 5: mnp("adc", 3, whichs); break;
        case 6: mnp("sbc", 3, whichs); break;
        case 7: mnp("rsc", 3, whichs); break;
        case 8: mnp("tst", 3, whichp); break;
        case 9: mnp("teq", 3, whichp); break;
        case 10: mnp("cmp", 3, whichp); break;
        case 11: mnp("cmn", 3, whichp); break;
        case 12: mnp("orr", 3, whichs); break;
        case 13: mnp("mov", 3, whichs); break;
        case 14: mnp("bic", 3, whichs); break;
        case 15: mnp("mvn", 3, whichs); break;
        default: NOGOHERE;
    }
    if ((alu_opcode < 8) || (alu_opcode > 11)) {
        oregc(Rdd);
        add_context(ct, Rdd);
    }
    if (alu_opcode < 13) {
        add_context(ct, Rnd);
        oregc(Rnd);
    }
    add_context(ct, Rmd);
    oreg(Rmd);
    out_shifted_imm(out, shift_type, Is);
}

static void dasm_data_proc_register_shift(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    u32 alu_opcode = (opcode >> 21) & 15;
    u32 S = (opcode >> 20) & 1; // set condition codes. 0=no, 1=yes. must be 1 for 8-B
    u32 Rnd = (opcode >> 16) & 15; // first operand
    u32 Rdd = (opcode >> 12) & 15; // dest reg.
    u32 Isd = (opcode >> 8) & 15;
    u32 shift_type = (opcode >> 5) & 3; // 0=LSL, 1=LSR, 2=ASR, 3=ROR
    // R(bit4) = 0 for this
    u32 Rmd = opcode & 15;
    const char *whichs = S ? "s" : nullptr;
    const char *whichp = nullptr;

    switch(alu_opcode) {
        case 0: mnp("and", 3, whichs); break;
        case 1: mnp("eor", 3, whichs); break;
        case 2: mnp("sub", 3, whichs); break;
        case 3: mnp("rsb", 3, whichs); break;
        case 4: mnp("add", 3, whichs); break;
        case 5: mnp("adc", 3, whichs); break;
        case 6: mnp("sbc", 3, whichs); break;
        case 7: mnp("rsc", 3, whichs); break;
        case 8: mnp("tst", 3, whichp); break;
        case 9: mnp("teq", 3, whichp); break;
        case 10: mnp("cmp", 3, whichp); break;
        case 11: mnp("cmn", 3, whichp); break;
        case 12: mnp("orr", 3, whichs); break;
        case 13: mnp("mov", 3, whichs); break;
        case 14: mnp("bic", 3, whichs); break;
        case 15: mnp("mvn", 3, whichs); break;
        default: NOGOHERE;
    }
    if ((alu_opcode < 8) || (alu_opcode > 11)) {
        oregc(Rdd);
        add_context(ct, Rdd);
    }
    oregc(Rnd);
    add_context(ct, Rnd);
    add_context(ct, Rmd);
    oreg(Rmd);
    switch (shift_type) {
        case 0:
            ostr(",lsl ");
            break;
        case 1:
            ostr(",lsr ");
            break;
        case 2:
            ostr(",asr ");
            break;
        case 3:
            ostr(",ror ");
            break;
    }
    oreg(Isd);
    add_context(ct, Isd);
}

static void dasm_undefined_instruction(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    ostr("undefined alu");
}

static void dasm_data_proc_immediate(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    u32 alu_opcode = (opcode >> 21) & 15;
    u32 S = (opcode >> 20) & 1; // set condition codes. 0=no, 1=yes. must be 1 for 8-B
    u32 Rnd = (opcode >> 16) & 15; // first operand
    u32 Rdd = (opcode >> 12) & 15; // dest reg.
    u32 Rm = opcode & 0xFF;
    u32 imm_ROR_amount = (opcode >> 7) & 30;
    Rm = (Rm << (32 - imm_ROR_amount)) | (Rm >> imm_ROR_amount);

    const char *whichs = S ? "s" : nullptr;
    const char *whichp = nullptr;
    switch(alu_opcode) {
        case 0: mnp("and", 3, whichs); break;
        case 1: mnp("eor", 3, whichs); break;
        case 2: mnp("sub", 3, whichs); break;
        case 3: mnp("rsb", 3, whichs); break;
        case 4: mnp("add", 3, whichs); break;
        case 5: mnp("adc", 3, whichs); break;
        case 6: mnp("sbc", 3, whichs); break;
        case 7: mnp("rsc", 3, whichs); break;
        case 8: mnp("tst", 3, whichp); break;
        case 9: mnp("teq", 3, whichp); break;
        case 10: mnp("cmp", 3, whichp); break;
        case 11: mnp("cmn", 3, whichp); break;
        case 12: mnp("orr", 3, whichs); break;
        case 13: mnp("mov", 3, whichs); break;
        case 14: mnp("bic", 3, whichs); break;
        case 15: mnp("mvn", 3, whichs); break;
        default: NOGOHERE;
    }
    if ((alu_opcode < 8) || (alu_opcode > 11)) {
        add_context(ct, Rdd);
        oregc(Rdd);
    }
    if (alu_opcode < 13) {
        oregc(Rnd);
        add_context(ct, Rnd);
    }
    ostr("#$");
    ohex(Rm, 8);
}

static void dasm_LDR_STR_immediate_offset(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    u32 P = OBIT(24); // Pre/post. 0 = after-transfer, post
    u32 U = OBIT(23); // 0 = down, 1 = up
    u32 B = OBIT(22); // byte/word, 0 = 32bit, 1 = 8bit
    // when P=0, bit 21 is T, 0= normal, 1= force nonpriviledged, and W=1
    // when P=1, bit 21 is write-back bit, 0 = normal, 1 = write into base
    u32 T = 0; // 0 = normal, 1 = force unprivileged
    u32 W = 1; // writeback. 0 = no, 1 = write into base
    if (P == 0) { T = OBIT(21); }
    else W = OBIT(21);

    u32 L = OBIT(20); // store = 0, load = 1
    u32 Rnd = (opcode >> 16) & 15; // base register. PC=+8
    u32 Rdd = (opcode >> 12) & 15; // source/dest register. PC=+12
    u32 offset = (opcode & 4095);
    const char *w;
    if (T && B) w = "bt";
    else if (T && (!B)) w = "t";
    else if ((!T) && B) w = "b";
    else w = nullptr;
    if (L) {
        mnp("ldr", 3, w)
    }
    else {
        add_context(ct, Rdd);
        mnp("str", 3, w)
    }
    oregc(Rdd);
    ostr("[");
    if (P) { // pre
        if (offset == 0) {
            oreg(Rnd);
            add_context(ct, Rnd);
            ostr("]");
            return;
        }
        oregc(Rnd);
        add_context(ct, Rnd);
        if (U) ostr(" +#$");
        else ostr(" -#$");
        ohex(offset, 2);
        ostr("]");
    }
    else {
        oreg(Rnd);
        add_context(ct, Rnd);
        ostr("], ");
        if (U) ostr("+#$");
        else ostr("-#$");
        ohex(offset, 2);
    }
    if (W) ostr("!");
}

static void dasm_LDR_STR_register_offset(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    u32 P = OBIT(24);
    u32 U = OBIT(23);
    u32 B = OBIT(22);
    u32 T = 0; // 0 = normal, 1 = force unprivileged
    u32 W = 1; // writeback. 0 = no, 1 = write into base
    if (P == 0) { T = OBIT(21); }
    else W = OBIT(21);
    u32 L = OBIT(20); // 0 store, 1 load
    u32 Rnd = (opcode >> 16) & 15; // base reg
    u32 Rdd = (opcode >> 12) & 15; // source/dest reg
    u32 Is = (opcode >> 7) & 31;
    u32 shift_type = (opcode >> 5) & 3;
    u32 Rmd = opcode & 15;
    const char *w;
    if (T && B) w = "bt";
    else if (T && (!B)) w = "t";
    else if ((!T) && B) w = "b";
    else w = nullptr;
    if (L) {
        mnp("ldr", 2, w)
    }
    else {
        mnp("str", 2, w)
        add_context(ct, Rdd);
    }
    oregc(Rdd);
    add_context(ct, Rnd);
    add_context(ct, Rmd);
    ostr("[");
    if (P) { // pre
        oregc(Rnd);
        if (U) ostr("+");
        else ostr("-");
        oreg(Rmd);
        out_shifted_imm(out, shift_type, Is);
        ostr("]");
    }
    else {
        oregc(Rnd);
        ostr("], ");
        if (U) ostr("+");
        else ostr("-");
        oreg(Rmd);
        out_shifted_imm(out, shift_type, Is);
    }
    if (W) ostr("!");
}

static void dasm_LDM_STM(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    u32 P = OBIT(24); // P=0 add offset after. P=1 add offset first
    u32 U = OBIT(23); // 0=subtract offset, 1 =add
    u32 S = OBIT(22); // 0=no, 1=load PSR or force user bit
    u32 W = OBIT(21); // 0=no writeback, 1= writeback
    u32 L = OBIT(20); // 0=store, 1=load
    u32 Rnd = (opcode >> 16) & 15;
    add_context(ct, Rnd);
    u32 rlist = (opcode & 0xFFFF);
    const char *w;
    if ((P == 1) && (U == 1)) w = "ib";
    else if ((P == 0) && (U == 1)) w = "ia";
    else if ((P == 1) && (U == 0)) w = "db";
    else w = "da";
    if (L) {
        mnp("ldm",3,w);
    }
    else {
        mnp("stm", 3, w);
    }
    oreg(Rnd);
    if (W) ostr("!,<");
    else ostr(",<");
    u32 needs_comma = 0;
    for (u32 i = 0; i < 16; i++) {
        if (rlist & (1 << i)) {
            if (needs_comma) ostr(",");
            oreg(i);
            needs_comma = 1;
        }
    }
    if (S) ostr(">^");
    else ostr(">");
}

static void dasm_B_BL_BLX(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    u32 link = OBIT(24);
    i32 offset = SIGNe24to32(opcode & 0xFFFFFF);
    offset <<= 2;
    if ((opcode >> 28) == 15) {
        mn("blx", 3);
        offset += ((opcode >> 24) & 1) * 2;
    }
    else if (link) mn("bl", 4)
    else mn("b", 5)
    if (instruction_addr == -1) {
        odec(offset);
    }
    else {
        ohex(offset+instruction_addr+8, 8);
    }
}

static void dasm_STC_LDC(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    ostr("std/ldc unsupported");
}

static void dasm_CDP(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    ostr("cdp unsupported");
}

static void dasm_MCR_MRC(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    u32 v2 = (opcode >> 28) == 15;
    u32 copcode = (opcode >> 21) & 7;
    u32 Cn = (opcode >> 16) & 15;
    u32 Rdd = (opcode >> 12) & 15;
    u32 Pnd = (opcode >> 8) & 15;
    u32 CP = (opcode >> 5) & 7;
    u32 Cm = opcode & 15;
    u32 MRC = OBIT(20);
    if (v2) {
        if (MRC) {
            ostr("mrc2   ");
        }
        else {
            ostr("mcr2   ");
            add_context(ct, Rdd);
        }
    }
    else {
        if (MRC) {
            mn("mrc", 3);
        }
        else {
            mn("mcr", 3);
            add_context(ct, Rdd);
        }
    }
    // Pn, cpopc, Rd, Cn, Cm, cp
    ostr("P%d, %d, R%d, %d, %d, %d", Pnd, copcode, Rdd, Cn, Cm, CP);
}

static void dasm_BKPT(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    if ((opcode >> 28) != 14) {
        ostr("BAD ");
    }
    ostr("bkpt (opc: ");
    ohex(opcode, 8);
    ostr(")");
}

static void dasm_SWI(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    ostr("swi #$");
    ohex(opcode & 0xFF, 2);
}

static void dasm_INVALID(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    ostr("unknown opcode %08x", opcode);
}

static void dasm_PLD(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    ostr("pld");
}

static void dasm_SMLAxy(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    u32 Rdd = (opcode >> 16) & 15; // RdHi
    u32 Rnd = (opcode >> 12) & 15; // RdLo
    u32 Rsd = (opcode >> 8) & 15;
    u32 Rmd = opcode & 15;
    add_context(ct, Rdd);
    add_context(ct, Rsd);
    add_context(ct, Rmd);
    add_context(ct, Rnd);
    mn("smlaxy", 1);
    oreg4(Rdd,Rmd,Rsd,Rnd);
}

static void dasm_SMLAWy(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    u32 Rdd = (opcode >> 16) & 15; // RdHi
    u32 Rnd = (opcode >> 12) & 15; // RdLo
    u32 Rsd = (opcode >> 8) & 15;
    u32 Rmd = opcode & 15;
    add_context(ct, Rdd);
    add_context(ct, Rsd);
    add_context(ct, Rmd);
    add_context(ct, Rnd);
    mn("smlawy", 1);
    oreg4(Rdd,Rmd,Rsd,Rnd);
}

static void dasm_SMULWy(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    u32 Rdd = (opcode >> 16) & 15; // RdHi
    u32 Rnd = (opcode >> 12) & 15; // RdLo
    u32 Rsd = (opcode >> 8) & 15;
    u32 Rmd = opcode & 15;
    add_context(ct, Rdd);
    add_context(ct, Rsd);
    add_context(ct, Rmd);
    add_context(ct, Rnd);
    mn("smulwy", 1);
    oreg3(Rdd,Rmd,Rsd);
}

static void dasm_SMLALxy(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    u32 Rdd = (opcode >> 16) & 15; // RdHi
    u32 Rnd = (opcode >> 12) & 15; // RdLo
    u32 Rsd = (opcode >> 8) & 15;
    u32 Rmd = opcode & 15;
    add_context(ct, Rdd);
    add_context(ct, Rsd);
    add_context(ct, Rmd);
    add_context(ct, Rnd);
    mn("smlaxy", 1);
    oreg4(Rnd,Rdd,Rmd,Rsd); // checked
}

static void dasm_SMULxy(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    u32 Rdd = (opcode >> 16) & 15; // RdHi
    u32 Rnd = (opcode >> 12) & 15; // RdLo
    u32 Rsd = (opcode >> 8) & 15;
    u32 Rmd = opcode & 15;
    add_context(ct, Rdd);
    add_context(ct, Rsd);
    add_context(ct, Rmd);
    add_context(ct, Rnd);
    mn("smulxy", 1);
    oreg3(Rdd,Rmd,Rsd);
}

static void dasm_LDRD_STRD(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    u32 P = OBIT(24); // pre or post. 0=post
    u32 U = OBIT(23); // up/down, 0=down
    u32 I = OBIT(22); // 0 = register, 1 = immediate offset
    u32 L = !OBIT(5);
    u32 W = 1;
    if (P) W = OBIT(21);
    u32 imm_off = 0;
    imm_off = ((opcode >> 8) & 15) << 4;
    imm_off |= (opcode & 15);
    u32 Rdd = (opcode >> 12) & 15;
    u32 Rnd = (opcode >> 16) & 15;
    u32 Rmd = opcode & 15; // Offset register
    add_context(ct, Rnd);
    add_context(ct, Rmd);
    if (L) {
        mnp("ldr", 3, "d");
    }
    else {
        add_context(ct, Rdd);
        mnp("str", 3, "d");
    }
    oregc(Rdd);
    ostr("[");
    oreg(Rnd); // base regiter
    if (U) ostr("+");
    else ostr("-");
    if (I) odec(imm_off);
    else oreg(Rmd);
    ostr("]");
}

static void dasm_CLZ(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    u32 Rdd = (opcode >> 12) & 15;
    u32 Rmd = opcode & 15;
    mn("clz", 4);
    oreg2(Rdd, Rmd);
    add_context(ct, Rmd);
}

static void dasm_QADD_QSUB_QDADD_QDSUB(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    u32 Rnd = (opcode >> 16) & 15;
    u32 Rdd = (opcode >> 12) & 15;
    u32 Rmd = opcode & 15;
    u32 sub_opcode = (opcode >> 20) & 15;
    switch(sub_opcode) {
        case 0b0000:
            mn("qadd", 3);
            break;
        case 0b0010:
            mn("qsub", 3);
            break;
        case 0b0100:
            mn("qdadd", 2);
            break;
        case 0b0110:
            mn("qdsub", 2);
            break;
    }
    add_context(ct, Rnd);
    add_context(ct, Rmd);
    oreg3(Rdd, Rmd, Rnd);
}

static void dasm_BLX(u32 opcode, jsm_string *out, i64 instruction_addr, ARMctxt *ct)
{
    u32 Rnd = opcode & 15;
    mn("blx", 3);
    oreg(Rnd);
    add_context(ct, Rnd);
}

void ARMv5_disassemble(u32 opcode, jsm_string *out, i64 ins_addr, ARMctxt *ct)
{
    // i64 instruction_addr, ARMctxt *ctxt
    out->quickempty();
    u32 opc = ((opcode >> 4) & 15) | ((opcode >> 16) & 0xFF0);

    u32 opc2 = ((opcode >> 4) & 15) | ((opcode >> 16) & 0xFFF0);
    if ((opc2 & 0b1111110101110000) == (0b1111010101010000)) // 1111 01.'1.101 ....  PLD
        dasm_PLD(opcode, out, ins_addr, ct);
    else if ((opc2 & 0b1111100000000000) == (0b1111000000000000)) // 1111 0..'..... ....  Undefined
        dasm_undefined_instruction(opcode, out, ins_addr, ct);
    else if ((opc2 & 0b1111111000000000) == (0b1111100000000000)) // 1111 100'..... ....  Undefined
        dasm_undefined_instruction(opcode, out, ins_addr, ct);
    else if ((opc2 & 0b1111111100000000) == (0b1111111100000000)) // 1111 111'1.... ....  Undefined
        dasm_undefined_instruction(opcode, out, ins_addr, ct);
    else if ((opc & 0b111111001111) == 0b000000001001) // 000'000.. 1001  MUL, MLA
        dasm_MUL_MLA(opcode, out, ins_addr, ct);
        //000'01... 1001  MULL, MLAL
    else if ((opc & 0b111110001111) == 0b000010001001)
        dasm_MULL_MLAL(opcode, out, ins_addr, ct);
    else if ((opc & 0b111111111001) == 0b000100001000) // .... 000'10000 1..0  SMLAxy
        dasm_SMLAxy(opcode, out, ins_addr, ct);
    else if ((opc & 0b111111111011) == 0b000100101000) // .... 000'10010 1.00  SMLAWy
        dasm_SMLAWy(opcode, out, ins_addr, ct);
    else if ((opc & 0b111111111011) == 0b000100101010) // .... 000'10010 1.10  SMULWy
        dasm_SMULWy(opcode, out, ins_addr, ct);
    else if ((opc & 0b111111111001) == 0b000101001000) // .... 000'10100 1..0  SMLALxy
        dasm_SMLALxy(opcode, out, ins_addr, ct);
    else if ((opc & 0b111111111001) == 0b000101101000) // .... 000'10110 1..0  SMULxy
        dasm_SMULxy(opcode, out, ins_addr, ct);
    else if ((opc & 0b111110111111) == 0b000100001001) // 000'10.00 1001  SWP
        dasm_SWP(opcode, out, ins_addr, ct);
    else if ((opc & 0b111000001111) == 0b000000001011) // 000'..... 1011  LDRH, STRH
        dasm_LDRH_STRH(opcode, out, ins_addr, ct);
    else if ((opc & 0b111000011101) == 0b000000001101) // .... 000'....0 11.1  LDRD, STRD
        dasm_LDRD_STRD(opcode, out, ins_addr, ct);
    else if ((opc & 0b111000011101) == 0b000000011101) // 000'....1 11.1  LDRSB, LDRSH
        dasm_LDRSB_LDRSH(opcode, out, ins_addr, ct);
    else if ((opc & 0b111110111111) == 0b000100000000) // 000'10.00 0000  MRS
        dasm_MRS(opcode, out, ins_addr, ct);
    else if ((opc & 0b111110111111) == 0b000100100000) // 000'10.10 0000  MSR (register)
        dasm_MSR_reg(opcode, out, ins_addr, ct);
    else if ((opc & 0b111110110000) == 0b001100100000) // 001'10.10 ....  MSR (immediate)
        dasm_MSR_imm(opcode, out, ins_addr, ct);
    else if (opc == 0b000100100001) //000'10010 0001  BX
        dasm_BX(opcode, out, ins_addr, ct);
    else if (opc == 0b000101100001) // .... 000'10110 0001  CLZ
        dasm_CLZ(opcode, out, ins_addr, ct);
    else if (opc == 0b000100100011) // .... 000'10010 0011  BLX (register)
        dasm_BLX(opcode, out, ins_addr, ct);
    else if ((opc & 0b111110011111) == 0b000100000101) // .... 000'10..0 0101  QADD, QSUB, QDADD, QDSUB
        dasm_QADD_QSUB_QDADD_QDSUB(opcode, out, ins_addr, ct);
    else if ((opc & 0b111111111111) == 0b000100100111) //000'10010 0111  BKPT
        dasm_BKPT(opcode, out, ins_addr, ct);
    else if ((opc & 0b111000000001) == 0b000000000000) //000'..... ...0  Data Processing (immediate shift)
        dasm_data_proc_immediate_shift(opcode, out, ins_addr, ct);
    else if ((opc & 0b111000001001) == 0b000000000001) //000'..... 0..1  Data Processing (register shift)
        dasm_data_proc_register_shift(opcode, out, ins_addr, ct);
    else if ((opc & 0b111110110000) == 0b001100000000) //001'10.00 ....  Undefined instructions in Data Processing
        dasm_undefined_instruction(opcode, out, ins_addr, ct);
    else if ((opc & 0b111000000000) == 0b001000000000) //001'..... ....  Data Processing (immediate value)
        dasm_data_proc_immediate(opcode, out, ins_addr, ct);
    else if ((opc & 0b111000000000) == 0b010000000000) //010'..... ....  LDR, STR (immediate offset)
        dasm_LDR_STR_immediate_offset(opcode, out, ins_addr, ct);
    else if ((opc & 0b111000000001) == 0b011000000000) // 011'..... ...0  LDR, STR (register offset)
        dasm_LDR_STR_register_offset(opcode, out, ins_addr, ct);
    else if ((opc & 0b111000000000) == 0b100000000000) //100'..... ....  LDM, STM
        dasm_LDM_STM(opcode, out, ins_addr, ct);
    else if ((opc & 0b111000000000) == 0b101000000000) //101'..... ....  B, BL
        dasm_B_BL_BLX(opcode, out, ins_addr, ct);
    else if ((opc & 0b111000000000) == 0b110000000000) //110'..... ....  STC, LDC
        dasm_STC_LDC(opcode, out, ins_addr, ct);
    else if ((opc & 0b111100000001) == 0b111000000000) //111'0.... ...0  CDP
        dasm_CDP(opcode, out, ins_addr, ct);
    else if ((opc & 0b111100000001) == 0b111000000001) //111'0.... ...1  MCR, MRC
        dasm_MCR_MRC(opcode, out, ins_addr, ct);
    else if ((opc & 0b111100000000) == 0b111100000000) //111'1.... ....  SWI
        dasm_SWI(opcode, out, ins_addr, ct);
    else
        dasm_INVALID(opcode, out, ins_addr, ct);

}
}