//
// Created by Dave on 2/10/2024.
//

#include <cmath>
#include <cassert>

#include "fsca.h"
#include "sh4_interpreter.h"
#include "sh4_interpreter_opcodes.h"

namespace SH4 {

#define SH4args  ins_t *ins
#define PCinc rPC += 2
#define rPC regs.PC

#define R(x) regs.R[x]
#define RM R(ins->Rm)
#define RN R(ins->Rn)

#define fFR regs.FPSCR.FR
#define IMM ins->imm
#define DISP ins->disp

#define rMACL regs.MACL
#define rMACH regs.MACH

#define fpFR(x) regs.fb[0].FP32[(x) ^ 1]
#define fpFRn fpFR(ins->Rn)
#define fpFRm fpFR(ins->Rm)
#define fpFRU(x) regs.fb[0].U32[(x) ^ 1]
#define fpFRUn regs.fb[0].U32[ins->Rn ^ 1]
#define fpFRUm regs.fb[0].U32[ins->Rm ^ 1]
#define fpDR(x) regs.fb[0].FP64[(x)]
#define fpDRU(x) regs.fb[0].U64[(x)]
#define fpXF(x) regs.fb[1].FP32[(x) ^ 1]
#define fpXFU(x) regs.fb[1].U32[(x) ^ 1]
#define fpXD(x) regs.fb[1].FP64[(x)]
#define fpXDU(x) regs.fb[1].U64[(x)]
#define fpFV(x) regs.fb[0].FV[(x)]
#define fpXMTX(x) regs.fb[1].MTX

#define BADOPCODE { printf("\nUNIMPLEMENTED INSTRUCTION %s PR=%d SZ=%d", __func__, regs.FPSCR.PR, regs.FPSCR.SZ); fflush(stdout); dbg_printf("\nUNIMPLEMENTED INSTRUCTION %s PR=%d SZ=%d", __func__, regs.FPSCR.PR, regs.FPSCR.SZ); }

#define READ8(addr) read(mptr, addr, 1)
#define READ16(addr) read(mptr, addr, 2)
#define READ32(addr) read(mptr, addr, 4)
#define READ64(addr) read(mptr, addr, 8)
#define WRITE8(addr, val) write(mptr, addr, (val) & 0xFF, 1)
#define WRITE16(addr, val) write(mptr, addr, (val) & 0xFFFF, 2)
#define WRITE32(addr, val) write(mptr, addr, (val) & 0xFFFFFFFF, 4)
#define WRITE64(addr, val) write(mptr, addr, val, 8)


#define FP64SAVE(addr, from) u64 v = (from); WRITE64(addr, (v << 32) | (v >> 32))
#define FP64LOAD(addr, to) u64 v = READ64(addr); (to) = (v << 32) | (v >> 32)


#define SH4ins(x) void core::ins_##x(SH4args)

#define DELAY_SLOT { PCinc; fetch_and_exec(true); }

static float fixNaN(float f) {
    u32 hex = *reinterpret_cast<u32 *>(&f);
	if ((hex & 0x7fffffff) > 0x7f800000)
		hex = 0x7fbfffff;
    return f;
}

SH4ins(EMPTY) {
    printf("\nUNKNOWN OPCODE EXECUTION ATTEMPTED: %08x %04x %llu", regs.PC, ins->opcode, *trace.cycles);
    fflush(stdout);
    PCinc;
}

SH4ins(MOV) {
    RN = RM;
    PCinc;
}

SH4ins(MOVI) {
    RN = SIGNe8to32(IMM);
    PCinc;
}

SH4ins(MOVA) {
    regs.R[0] = (rPC & 0xFFFFFFFC) + (DISP*4) + 4;
    PCinc;
}

SH4ins(MOVWI) {
    u32 addr = rPC + (DISP*2) + 4;
    u32 val = READ16(addr);
    RN = SIGNe16to32(val);
    PCinc;
}

SH4ins(MOVLI) {
    u32 addr = (rPC & 0xFFFFFFFC) + (DISP*4) +4;
    RN = READ32(addr);
    PCinc;
}

SH4ins(MOVBL)
{
    u32 val = READ8(RM);
    RN = SIGNe8to32(val);
    PCinc;
}

SH4ins(MOVWL)
{
    u32 val = READ16(RM);
    RN = SIGNe16to32(val);
    PCinc;
}

SH4ins(MOVLL)
{
    RN = READ32(RM);
    /*R[n] = Read_32 (R[m]);
    PC += 2;*/
    PCinc;
}

SH4ins(MOVBS)
{
    WRITE8(RN, RM);
    PCinc;
}

SH4ins(MOVWS)
{
    WRITE16(RN, RM);
    PCinc;
}

SH4ins(MOVLS)
{
    WRITE32(RN, RM);
    PCinc;
}

SH4ins(MOVBP)
{
    u32 val = READ8(RM);
    RN = SIGNe8to32(val);
    if (ins->Rn != ins->Rm)
        RM += 1;
    PCinc;
}

SH4ins(MOVWP)
{
    u32 val = READ16(RM);
    RN = SIGNe16to32(val);
    if (ins->Rn != ins->Rm)
        RM += 2;
    PCinc;
}

SH4ins(MOVLP)
{
    RN = READ32(RM);
    if (ins->Rn != ins->Rm)
        RM += 4;
    PCinc;
}

SH4ins(MOVBM)
{
    WRITE8(RN - 1, RM);
    RN -= 1;
    PCinc;
}

SH4ins(MOVWM)
{
    WRITE16(RN - 2, RM);
    RN -= 2;
    PCinc;
}

SH4ins(MOVLM)
{
    WRITE32(RN - 4, RM);
    RN -= 4;
    PCinc;
}

SH4ins(MOVBL4)
{
    u32 val = READ8(RM + DISP);
    R(0) = SIGNe8to32(val);
    PCinc;
}

SH4ins(MOVWL4)
{
    u32 val = READ16(RM + (DISP*2));
    R(0) = SIGNe16to32(val);
    PCinc;
}

SH4ins(MOVLL4)
{
    RN = READ32(RM + (DISP*4));
    PCinc;
}

SH4ins(MOVBS4)
{
    WRITE8(RN + DISP, R(0));
    PCinc;
}

SH4ins(MOVWS4) { // R0 -> (disp*2 + Rn)
    WRITE16(RN + (DISP*2), R(0));
    PCinc;
}

SH4ins(MOVLS4) { // Rm -> (disp*4 + Rn)
    WRITE32(RN + (DISP << 2), RM);
    PCinc;
}

SH4ins(MOVBL0) { // (R0 + Rm) -> sign extension -> Rn
    u32 val = READ8(R(0)+RM);
    RN = SIGNe8to32(val);
    PCinc;
}

SH4ins(MOVWL0) { // (R0 + Rm) -> sign extension -> Rn
    RN = READ16(RM + R(0));
    RN = SIGNe16to32(RN);
    PCinc;
}

SH4ins(MOVLL0) { // (R0 + Rm) -> Rn
    RN = READ32(R(0) + RM);
    PCinc;
}

SH4ins(MOVBS0) { // Rm -> (R0 + Rn)
    WRITE8(R(0)+RN, RM);
    PCinc;
}

SH4ins(MOVWS0) { // Rm -> (R0 + Rn)
    WRITE16(R(0) + RN, RM);
    PCinc;
}

SH4ins(MOVLS0) { // Rm -> (R0 + Rn)
    WRITE32(R(0) + RN, RM);
    PCinc;
}

SH4ins(MOVBLG) { // (disp + GBR) -> sign extension -> R0
    u32 val = READ8(ins->disp + regs.GBR);
    R(0) = SIGNe8to32(val);
    PCinc;
}

SH4ins(MOVWLG) { // (disp*2 + GBR) -> sign extension -> R0
    u32 val = READ8((ins->disp*2) + regs.GBR);
    R(0) = SIGNe16to32(val);
    PCinc;
}

SH4ins(MOVLLG) { // (disp*4 + GBR) -> R0
    R(0) = READ32((ins->disp*4) + regs.GBR);
    PCinc;
}

SH4ins(MOVBSG) { // R0 -> (disp + GBR)
    WRITE8(ins->disp + regs.GBR, R(0));
    PCinc;
}

SH4ins(MOVWSG) { // R0 -> (disp*2 + GBR)
    WRITE16((ins->disp << 1) + regs.GBR, R(0));
    PCinc;
}

SH4ins(MOVLSG) { // R0 -> (disp*4 + GBR)
    WRITE32((ins->disp << 2) + regs.GBR, R(0));
    PCinc;
}

SH4ins(MOVT) { // T -> Rn
    RN = regs.SR.T;
    assert(RN < 2); // TODO: remove
    PCinc;
}

SH4ins(SWAPB) { // Rm -> swap lower 2 bytes -> Rn
    u32 tmp = RM;
    RN = tmp & 0xFFFF0000;
    RN |= ((tmp >> 8) & 0xFF);
    RN |= (tmp << 8) & 0xFF00;
    PCinc;
}

SH4ins(SWAPW) { // Rm -> swap upper/lower words -> Rn
    RN = ((RM & 0xFFFF0000) >> 16) | ((RM & 0xFFFF) << 16);
    PCinc;
}

SH4ins(XTRCT) { // Rm:Rn middle 32 bits -> Rn
    RN = ((RM << 16) & 0xFFFF0000) | ((RN >> 16) & 0x0000FFFF);
    PCinc;
}

SH4ins(ADD) { // Rn + Rm -> Rn
    RN += RM;
    PCinc;
}

SH4ins(ADDI) { // Rn + (sign extension)imm
    u32 val = IMM;
    RN += SIGNe8to32(val);
    PCinc;
}

SH4ins(ADDC) { // Rn + Rm + T -> Rn, carry -> T
    u32 tmp1 = RN + RM;
    u32 tmp0 = RN;
    RN = tmp1 + regs.SR.T;

    regs.SR.T = tmp0 > tmp1;

    if (tmp1 > RN)
        regs.SR.T = 1;

    PCinc;
}

SH4ins(ADDV) { // Rn + Rm -> Rn, overflow -> T
    // Thanks Reicast!
    i64 br=static_cast<i64>(static_cast<i32>(RN))+static_cast<i64>(static_cast<i32>(RM));

    regs.SR.T = (br >=0x80000000) || (br < static_cast<i64>(0xFFFFFFFF80000000u));

    RN += RM;

    PCinc;
}

SH4ins(CMPIM) { // If R0 = (sign extension)imm: 1 -> T, Else: 0 -> T
    regs.SR.T = R(0) == SIGNe8to32(IMM);
    PCinc;
}

SH4ins(CMPEQ) { // If Rn = Rm: 1 -> T, Else: 0 -> T
    regs.SR.T = RN == RM;
    PCinc;
}

SH4ins(CMPHS) { // If Rn >= Rm (unsigned): 1 -> T, Else: 0 -> T
    regs.SR.T = RN >= RM;
    PCinc;
}

SH4ins(CMPGE) { // If Rn >= Rm (signed): 1 -> T, Else: 0 -> T
    regs.SR.T = (static_cast<i32>(RN) >= static_cast<i32>(RM));
    PCinc;
}

SH4ins(CMPHI) { // If Rn > Rm (unsigned): 1 -> T, Else: 0 -> T
    regs.SR.T = RN > RM;
    PCinc;
}

SH4ins(CMPGT) { // If Rn > Rm (signed): 1 -> T, Else: 0 -> T
    regs.SR.T = static_cast<i32>(RN) > static_cast<i32>(RM);
    PCinc;
}

SH4ins(CMPPL) { // If Rn > 0 (signed): 1 -> T, Else: 0 -> T
    regs.SR.T = static_cast<i32>(RN) > 0;
    PCinc;
}

SH4ins(CMPPZ) { // If Rn >= 0 (signed): 1 -> T, Else: 0 -> T
    regs.SR.T = static_cast<i32>(RN) >= 0;
    PCinc;
}

SH4ins(CMPSTR) { // If Rn and Rm have an equal byte: 1 -> T, Else: 0 -> T
    u32 r = RN ^ RM;
    regs.SR.T = (!(r & 0xFF000000)) || (!(r & 0xFF0000)) || (!(r & 0xFF00)) || (!(r & 0xFF));
    PCinc;
}

SH4ins(DIV0S) { // MSB of Rn -> Q, MSB of Rm -> M, M ^ Q -> T
    regs.SR.Q = (RN & 0x80000000) != 0;

    regs.SR.M = (RM & 0x80000000) != 0;

    regs.SR.T = regs.SR.M != regs.SR.Q;
    PCinc;
}

SH4ins(DIV0U) { // 0 -> M, 0 -> Q, 0 -> T
    regs.SR.M = regs.SR.Q = regs.SR.T = 0;
    PCinc;
}

#define fQ regs.SR.Q
#define fM regs.SR.M
#define fT regs.SR.T

SH4ins(DIV1) { // 1-step division (Rn / Rm)
    unsigned long tmp0;
    unsigned char tmp1;

    unsigned char old_q = fQ;
    fQ = (0x80000000 & RN) != 0;
    unsigned long tmp2 = RM;
    RN <<= 1;
    RN |= fT;

    if (old_q == 0) {
        if (fM == 0) {
            tmp0 = RN;
            RN -= tmp2;
            tmp1 = RN > tmp0;

            if (fQ == 0)
                fQ = tmp1;
            else if (fQ == 1)
                fQ = tmp1 == 0;
        } else if (fM == 1) {
            tmp0 = RN;
            RN += tmp2;
            tmp1 = RN < tmp0;

            if (fQ == 0)
                fQ = tmp1 == 0;
            else if (fQ == 1)
                fQ = tmp1;
        }
    } else if (old_q == 1) {
        if (fM == 0) {
            tmp0 = RN;
            RN += tmp2;
            tmp1 = RN < tmp0;

            if (fQ == 0)
                fQ = tmp1;
            else if (fQ == 1)
                fQ = tmp1 == 0;
        } else if (fM == 1) {
            tmp0 = RN;
            RN -= tmp2;
            tmp1 = RN > tmp0;

            if (fQ == 0)
                fQ = tmp1 == 0;
            else if (fQ == 1)
                fQ = tmp1;
        }
    }

    fT = (fQ == fM);
    PCinc;
}

SH4ins(DMULS) { // Signed, Rn * Rm -> MACH:MACL, 32 * 32 -> 64 bits
    const i64 rn = (i64)static_cast<i32>(RN);
    const i64 rm = (i64)static_cast<i32>(RM);
    const i64 r = rn * rm;
    regs.MACH = static_cast<u32>(r >> 32);
    regs.MACL = static_cast<u32>(r & 0xFFFFFFFF);
    PCinc;
}

SH4ins(DMULU) { // Unsigned, Rn * Rm -> MACH:MACL, 32 * 32 -> 64 bits
    // TODO: this appears correct...but...maybe try weird sh4-instrs version?
    u64 n = RN;
    u64 m = RM;
    u64 r = n * m;
    regs.MACH = ((r >> 32) & 0xFFFFFFFF);
    regs.MACL = r & 0xFFFFFFFF;
    PCinc;
}

SH4ins(DT) { // Rn-1 -> Rn, If Rn = 0: 1 -> T, Else: 0 -> T
    RN--;
    regs.SR.T = RN == 0;
    PCinc;
}

SH4ins(EXTSB) { // Rm sign-extended from byte -> Rn
    RN = RM & 0xFF;
    RN = SIGNe8to32(RN);
    PCinc;
}

SH4ins(EXTSW) { // Rm sign-extended from word -> Rn
    RN = RM & 0xFFFF;
    RN = SIGNe16to32(RN);
    PCinc;
}

SH4ins(EXTUB) { // Rm zero-extended from byte -> Rn
    RN = RM & 0xFF;
    PCinc;
}

SH4ins(EXTUW) { // Rm zero-extended from word -> Rn
    RN = RM & 0xFFFF;
    PCinc;
}

SH4ins(MACL) { // Signed, (Rn) * (Rm) + MAC -> MAC, 32 * 32 + 64 -> 64 bits
    i64 rn = (i64)static_cast<i32>(READ32(RN));
    i64 rm = (i64)static_cast<i32>(READ32(RM));
    RM += 4;
    RN += 4;
    i64 mac = (static_cast<i64>(regs.MACH) << 32) | regs.MACL;
    i64 r = (rn * rm) + mac;
    regs.MACH = (r >> 32);
    regs.MACL = (r & 0xFFFFFFFF);
    PCinc;
}

SH4ins(MACW) { // Signed, (Rn) * (Rm) + MAC -> MAC, SH1: 16 * 16 + 42 -> 42 bits, Other: 16 * 16 + 64 -> 64 bits
    i64 rn = (i64)static_cast<i16>(READ16(RN));
    i64 rm = (i64)static_cast<i16>(READ16(RM));
    RN += 2;
    RM += 2;
    i64 mac = (static_cast<i64>(regs.MACH) << 32) | regs.MACL;
    i64 r = (rn * rm) + mac;
    regs.MACH = static_cast<u32>(static_cast<i32>(r >> 32));
    regs.MACL = static_cast<u32>(static_cast<i32>(r & 0xFFFFFFFF));
    PCinc;
}

SH4ins(MULL) { // Rn * Rm -> MACL, 32 * 32 -> 32 bits
    rMACL = RN * RM;
    PCinc;
}

SH4ins(MULS) { // Signed, Rn * Rm -> MACL, 16 * 16 -> 32 bits
    regs.MACL = static_cast<u32>(static_cast<i32>(static_cast<i16>(RN)) * static_cast<i32>(static_cast<i16>(RM)));
    PCinc;
}

SH4ins(MULU) { // Unsigned, Rn * Rm -> MACL, 16 * 16 -> 32 bits
    regs.MACL = (RN & 0xFFFF) * (RM & 0xFFFF);
    PCinc;
}

SH4ins(NEG) { // 0 - Rm -> Rn
    RN = static_cast<u32>(0 - static_cast<i32>(RM));
    PCinc;
}

SH4ins(NEGC) { // 0 - Rm - T -> Rn, borrow -> T
    u32 temp = 0 - RM;
    RN = temp - regs.SR.T;

    if (0 < temp)
        regs.SR.T = 1;
    else
        regs.SR.T = 0;

    if (temp < RN)
        regs.SR.T = 1;
    PCinc;
}

SH4ins(SUB) { // Rn - Rm -> Rn
    RN -= RM;
    PCinc;
}

SH4ins(SUBC) { // Rn - Rm - T -> Rn, borrow -> T
    u32 tmp1 = RN - RM;
    u32 tmp0 = RN;
    RN = tmp1 - regs.SR.T;

    regs.SR.T = tmp0 < tmp1;
    if (tmp1 < RN)
        regs.SR.T = 1;

    PCinc;
}

SH4ins(SUBV) { // Rn - Rm -> Rn, underflow -> T
    /*u32 original_rn = RN;
    u32 original_rm = RM;
    RN -= RM;
    regs.SR.T = original_rn < original_rm;*/

    i64 br=static_cast<i64>(static_cast<i32>(RN))-static_cast<i64>(static_cast<i32>(RM));

    regs.SR.T = (br >=0x80000000) || (br < static_cast<i64>(0xFFFFFFFF80000000u));
    RN -= RM;

    PCinc;
}

SH4ins(AND) { // Rn & Rm -> Rn
    RN &= RM;
    PCinc;
}

SH4ins(ANDI) { // R0 & (zero extend)imm -> R0
    R(0) &= IMM;
    PCinc;
}

SH4ins(ANDM) { // (R0 + GBR) & (zero extend)imm -> (R0 + GBR)
    u32 r = READ8(R(0) + regs.GBR) & IMM & 0xFF;
    WRITE8(R(0) + regs.GBR, r);
    PCinc;
}

SH4ins(NOT) { // ~Rm -> Rn
    RN = ~RM;
    PCinc;
}

SH4ins(OR) { // Rn | Rm -> Rn
    RN |= RM;
    PCinc;
}

SH4ins(ORI) { // R0 | (zero extend)imm -> R0
    R(0) |= IMM;
    PCinc;
}

SH4ins(ORM) { // (R0 + GBR) | (zero extend)imm -> (R0 + GBR)
    u32 addr = regs.GBR + R(0);
    WRITE8(addr, READ8(addr) | IMM);
    PCinc;
}

SH4ins(TAS) { // If (Rn) = 0: 1 -> T, Else: 0 -> T, 1 -> MSB of (Rn)
    u32 val = READ8(RN);
    regs.SR.T = val == 0;
    val |= 0x80;
    WRITE8(RN, val);
    PCinc;
}

SH4ins(TST) { // If Rn & Rm = 0: 1 -> T, Else: 0 -> T
    regs.SR.T = (RN & RM) == 0;
    PCinc;
}

SH4ins(TSTI) { // If R0 & (zero extend)imm = 0: 1 -> T, Else: 0 -> T
    regs.SR.T = (R(0) & IMM) == 0;
    PCinc;
}

SH4ins(TSTM) { // If (R0 + GBR) & (zero extend)imm = 0: 1 -> T, Else 0: -> T
    u32 tmp = READ8(R(0) + regs.GBR);
    regs.SR.T = (tmp & IMM) == 0;
    PCinc;
}

SH4ins(XOR) { // Rn ^ Rm -> Rn
    RN ^= RM;
    PCinc;
}

SH4ins(XORI) { // R0 ^ (zero extend)imm -> R0
    R(0) ^= IMM;
    PCinc;
}

SH4ins(XORM) { // (R0 + GBR) ^ (zero extend)imm -> (R0 + GBR)
    u32 tmp = READ8(R(0) + regs.GBR) ^ IMM;
    WRITE8(R(0) + regs.GBR, tmp);
    PCinc;
}

SH4ins(ROTCL) { // T << Rn << T
    u32 v = (RN >> 31);
    assert(regs.SR.T < 2);
    RN = (RN << 1) | regs.SR.T;
    regs.SR.T = v;
    PCinc;
}

SH4ins(ROTCR) { // T >> Rn >> T
    u32 tmp = RN & 1;
    assert(regs.SR.T < 2);
    RN = (RN >> 1) | (regs.SR.T << 31);
    regs.SR.T = tmp;
    PCinc;
}

SH4ins(ROTL) { // T << Rn << MSB
    regs.SR.T = RN >> 31;
    RN <<= 1;
    RN |= regs.SR.T;
    PCinc;
}

SH4ins(ROTR) { // LSB >> Rn >> T
    regs.SR.T = RN & 1;
    RN = (RN >> 1) | (regs.SR.T << 31);
    PCinc;
}

SH4ins(SHAD) { // If Rm >= 0: Rn << Rm -> Rn, If Rm < 0: Rn >> |Rm| -> [MSB -> Rn]
    u32 s = RM & 0x80000000;

    if (s == 0)
        RN <<= RM & 0x1F;
    else if ((RM & 0x1F) == 0)
    {
        //RN=(u32)(((i32)RN)>>31);
        if ((RN & 0x80000000) == 0)
            RN = 0;
        else
            RN = 0xFFFFFFFF;
    }
    else
        RN = static_cast<u32>(static_cast<i32>(RN) >> ((~RM & 0x1F) + 1));

    PCinc;
}

SH4ins(SHAL) { // T << Rn << 0
    regs.SR.T = RN >> 31;
    RN <<= 1;
    PCinc;
}

SH4ins(SHAR) { // MSB >> Rn >> T
    regs.SR.T = RN & 1;
    RN = (RN >> 1) | (RN & 0x80000000);
    PCinc;
}

SH4ins(SHLD) { // If Rm >= 0: Rn << Rm -> Rn, If Rm < 0: Rn >> |Rm| -> [0 -> Rn]
    u32 sgn = RM & 0x80000000;

    if (sgn == 0)
        RN <<= (RM & 0x1F);
    else if ((RM & 0x1F) == 0)
        RN = 0;
    else
        RN = (u32)RN >> ((~RM & 0x1F) + 1);

    PCinc;
}

SH4ins(SHLL) { // T << Rn << 0
    regs.SR.T = (RN >> 31) & 1;
    RN <<= 1;
    PCinc;
}

SH4ins(SHLL2) { // Rn << 2 -> Rn
    RN <<= 2;
    PCinc;
}

SH4ins(SHLL8) { // Rn << 8 -> Rn
    RN <<= 8;
    PCinc;
}

SH4ins(SHLL16) { // Rn << 16 -> Rn
    RN <<= 16;
    PCinc;
}

SH4ins(SHLR) { // 0 >> Rn >> T
    regs.SR.T = RN & 1;
    RN >>= 1;
    PCinc;
}

SH4ins(SHLR2) { // Rn >> 2 -> [0 -> Rn]
    RN >>= 2;
    PCinc;
}

SH4ins(SHLR8) { // Rn >> 8 -> [0 -> Rn]
    RN >>= 8;
    PCinc;
}

SH4ins(SHLR16) { // Rn >> 16 -> [0 -> Rn]
    RN >>= 16;
    PCinc;
}

SH4ins(BF) { // If T = 0: disp*2 + PC + 4 -> PC, Else: nop
    //printf("\nBF! %llu", clock.trace_cycles);
    assert(regs.SR.T < 2);
    // PC += 2, kind is normal
    // add ((2 * DISP) + 2) * !T
    // so it'll add 0 if T=1
    // it'll add (2*DISP)+2 if T=0
    /*i64 val = rPC + 2;
    val += ((2 * (i32)SIGNe8to32(DISP)) + 2) * (i32)(regs.SR.T ^ 1);
    rPC = (u32)val;*/

    i32 disp = static_cast<i32>(SIGNe8to32(DISP));

    if (regs.SR.T == 0)
        rPC += 4 + (disp << 1);
    else
        rPC += 2;
}

SH4ins(BFS) { // If T = 0: disp*2 + PC + 4 -> PC, Else: nop, (Delayed branch)
    //printf("\nBFS! %llu", clock.trace_cycles);
    // PC is gonna be +4 no matter what, kind is correct
    // then we add 2*disp to that after if T = 0
    /*i64 val = (i64)rPC + 4;
    val += (2 * (i32)SIGNe8to32(DISP)) * (i32)(regs.SR.T ^ 1);
    DELAY_SLOT;
    rPC = (u32)val; // DISP*2 * !regs.SR.T*/
    i32 disp = static_cast<i32>(SIGNe8to32(DISP));
    u32 temp = rPC;
    if (fT == 0)
        temp += 4 + (disp << 1);
    else
        temp += 4;

    DELAY_SLOT;

    rPC = temp;
}

SH4ins(BT) { // If T = 1: disp*2 + rPC + 4 -> rPC, Else: nop
    /*i64 val = (i64)rPC + 2;
    val += ((2 * (i32)SIGNe8to32(DISP)) + 2) * (i32)regs.SR.T;
    rPC = (u32)val;*/
    i32 disp = static_cast<i32>(SIGNe8to32(DISP));

    if (fT == 1)
        rPC += 4 + (disp << 1);
    else
        rPC += 2;
}

SH4ins(BTS) { // If T = 1: disp*2 + PC + 4 -> PC, Else: nop, (Delayed branch)
    /*i64 val = (i64)rPC + 4;
    val += (2 * (i32)SIGNe8to32(DISP)) * (i32)regs.SR.T;*/
    i32 disp = static_cast<i32>(SIGNe8to32(DISP));
    u32 temp = rPC;

    if (fT == 1)
        temp += 4 + (disp << 1);
    else
        temp += 4;

    DELAY_SLOT;
    rPC = temp; // DISP*2 * !regs.SR.T
}

SH4ins(BRA) { // disp*2 + PC + 4 -> PC, (Delayed branch)
    /*i64 val = (i64)rPC + 4;
    val += (2 * (i32)SIGNe12to32(DISP));
    DELAY_SLOT;
    rPC = (u32)val;*/
    i32 disp = static_cast<i32>(SIGNe12to32(DISP));
    u32 temp = rPC;
    temp += 4 + (disp << 1);
    DELAY_SLOT;
    rPC = temp;
}

SH4ins(BRAF) { // Rm + PC + 4 -> PC, (Delayed branch)
    u32 val = rPC + 4 + RM;
    DELAY_SLOT;
    rPC = val;
}

SH4ins(BSR) { // PC + 4 -> PR, disp*2 + PC + 4 -> PC, (Delayed branch)
    /*i64 val = (i32)SIGNe12to32(DISP) * 2;
    regs.PR = rPC + 4;
    val = rPC + 4 + val;
    DELAY_SLOT;
    rPC = (u32)val;*/
    i32 disp = static_cast<i32>(SIGNe12to32(DISP));
    u32 temp = rPC;

    regs.PR = rPC + 4;
    temp += 4 + (disp << 1);
    DELAY_SLOT;
    rPC = temp;
}

SH4ins(BSRF) { // PC + 4 -> PR, Rm + PC + 4 -> PC, (Delayed branch)
    regs.PR = rPC + 4;
    u32 val = RM + rPC + 4;
    DELAY_SLOT;
    rPC = val;
}

SH4ins(JMP) { // Rm -> PC, (Delayed branch)
    u32 val = RM;
    DELAY_SLOT;
    rPC = val;
}

SH4ins(JSR) { // PC + 4 -> PR, Rm -> PC, (Delayed branch)
    regs.PR = rPC + 4;
    u32 val = RM;
    DELAY_SLOT;
    rPC = val;
}

SH4ins(RTS) { // PR -> PC, Delayed branch
    u32 val = regs.PR;
    DELAY_SLOT;
    rPC = val;
}

SH4ins(CLRMAC) { // 0 -> MACH, 0 -> MACL
    regs.MACH = regs.MACL = 0;
    PCinc;
}

SH4ins(CLRS) { // 0 -> S
    regs.SR.S = 0;
    PCinc;
}

SH4ins(CLRT) { // 0 -> T
    regs.SR.T = 0;
    PCinc;
}

SH4ins(LDCSR) { // Rm -> SR
    regs.SR_set(RM & 0x700083F3);
    PCinc;
}

SH4ins(LDCMSR) { // (Rm) -> SR, Rm+4 -> Rm
    u32 r = READ32(RM) & 0x700083F3;
    RM += 4;
    regs.SR_set(r);
   PCinc;
}

SH4ins(LDCGBR) { // Rm -> GBR
    regs.GBR = RM;
    PCinc;
}

SH4ins(LDCMGBR) { // (Rm) -> GBR, Rm+4 -> Rm
    regs.GBR = READ32(RM);
    RM += 4;
    PCinc;
}

SH4ins(LDCVBR) { // Rm -> VBR
    regs.VBR = RM;
    PCinc;
}

SH4ins(LDCMVBR) { // (Rm) -> VBR, Rm+4 -> Rm
    regs.VBR = READ32(RM);
    RM += 4;
    PCinc;
}

SH4ins(LDCSSR) { // Rm -> SSR
    regs.SSR = RM;
    PCinc;
}

SH4ins(LDCMSSR) { // (Rm) -> SSR, Rm+4 -> Rm
    regs.SSR = READ32(RM);
    RM += 4;
    PCinc;
}

SH4ins(LDCSPC) { // Rm -> SPC
    regs.SPC = RM;
    PCinc;
}

SH4ins(LDCMSPC) { // (Rm) -> SPC, Rm+4 -> Rm
    regs.SPC = READ32(RM);
    RM += 4;
    PCinc;
}

SH4ins(LDCDBR) { // Rm -> DBR
    regs.DBR = RM;
    PCinc;
}

SH4ins(LDCMDBR) { // (Rm) -> DBR, Rm+4 -> Rm
    regs.DBR = READ32(RM);
    RM += 4;
    PCinc;
}

SH4ins(LDCRn_BANK) { // Rm -> Rn_BANK (n = 0-7)
    regs.R_[ins->Rn] = RM;
    PCinc;
}

SH4ins(LDCMRn_BANK) { // (Rm) -> Rn_BANK, Rm+4 -> Rm
    regs.R_[ins->Rn] = READ32(RM);
    RM += 4;
    PCinc;
}

SH4ins(LDSMACH) { // Rm -> MACH
    rMACH = RM;
    PCinc;
}

SH4ins(LDSMMACH) { // (Rm) -> MACH, Rm+4 -> Rm
    rMACH = READ32(RM);
    RM += 4;
    PCinc;
}

SH4ins(LDSMACL) { // Rm -> MACL
    rMACL = RM;
    PCinc;
}

SH4ins(LDSMMACL) { // (Rm) -> MACL, Rm+4 -> Rm
    regs.MACL = READ32(RM);
    RM += 4;
    PCinc;
}

SH4ins(LDSPR) { // Rm -> PR
    regs.PR = RM;
    PCinc;
}

SH4ins(LDSMPR) { // (Rm) -> PR, Rm+4 -> Rm
    regs.PR = READ32(RM);
    RM += 4;
    PCinc;
}

SH4ins(LDTLB) { // PTEH/PTEL -> TLB
    PCinc;
}

SH4ins(MOVCAL) { // R0 -> (Rn) (without fetching cache block)
    WRITE32(RN, R(0));
    PCinc;
}

SH4ins(NOP) { // No operation
    PCinc;
}

SH4ins(OCBI) { // Invalidate operand cache block
    // We don't emulate operand cache except on-chip RAM mode. Writes will have already just happened as they were done
    PCinc;
}

SH4ins(OCBP) { // Write back and invalidate operand cache block
    // We don't emulate operand cache except on-chip RAM mode. Writes will have already just happened as they were done
    PCinc;
}

SH4ins(OCBWB) { // Write back operand cache block
    // We don't emulate operand cache except on-chip RAM mode. Writes will have already just happened as they were done
    PCinc;
}

SH4ins(PREF) { // (Rn) -> operand cache
    u32 addr = RN;
    // Flush store queue to RAM!
    if ((addr & 0xEC000000) == 0xE0000000) {
        u32 sq = (addr >> 5) & 1;
        u32 naddr = addr & 0x03FFFFE0 | (((regs.QACR[sq]) & 0b11100) << 24);
        for (u32 i = 0; i < 8; i++) {
            WRITE32(naddr, *reinterpret_cast<u32 *>(&SQ[sq][i << 2]));
            naddr += 4;
        }
    }
    else if (dbg.trace_on)
    {
#ifndef LYCODER
#ifndef REICAST_DIFF
        dbg_printf("\nPREFETCH %08x", RN);
#endif
#endif
    }
    //read in (RN & 0xFFFFFFE0) to operand cache
    PCinc;
}

SH4ins(RTE) { // Delayed branch, SH1*,SH2*: stack area -> PC/SR, SH3*,SH4*: SSR/SPC -> SR/PC
    u32 val = regs.SPC;
    DELAY_SLOT;
    regs.SR_set(regs.SSR);
    rPC = val;
}

SH4ins(SETS) { // 1 -> S
    regs.SR.S = 1;
    PCinc;
}

SH4ins(SETT) { // 1 -> T
    regs.SR.T = 1;
    PCinc;
}

SH4ins(SLEEP) { // Sleep or standby
    dbg_break("Sh4 SLEEP", *trace.cycles);
    dbg_LT_dump();
    PCinc;
}

SH4ins(STCSR) { // SR -> Rn
    RN = regs.SR_get();
    PCinc;
}

SH4ins(STCMSR) { // Rn-4 -> Rn, SR -> (Rn)
    RN -= 4;
    WRITE32(RN, regs.SR_get());
    PCinc;
}

SH4ins(STCGBR) { // GBR -> Rn
    RN = regs.GBR;
    PCinc;
}

SH4ins(STCMGBR) { // Rn-4 -> Rn, GBR -> (Rn)
    RN -= 4;
    WRITE32(RN, regs.GBR);
    PCinc;
}

SH4ins(STCVBR) { // VBR -> Rn
    RN = regs.VBR;
    PCinc;
}

SH4ins(STCMVBR) { // Rn-4 -> Rn, VBR -> (Rn)
    RN -= 4;
    WRITE32(RN, regs.VBR);
    PCinc;
}

SH4ins(STCSGR) { // SGR -> Rn
    RN = regs.SGR;
    PCinc;
}

SH4ins(STCMSGR) { // Rn-4 -> Rn, SGR -> (Rn)
    RN -= 4;
    WRITE32(RN, regs.SGR);
    PCinc;
}

SH4ins(STCSSR) { // SSR -> Rn
    RN = regs.SSR;
    PCinc;
}

SH4ins(STCMSSR) { // Rn-4 -> Rn, SSR -> (Rn)
    RN -= 4;
    WRITE32(RN, regs.SSR);
    PCinc;
}

SH4ins(STCSPC) { // SPC -> Rn
    RN = regs.SPC;
    PCinc;
}

SH4ins(STCMSPC) { // Rn-4 -> Rn, SPC -> (Rn)
    RN -= 4;
    WRITE32(RN, regs.SPC);
    PCinc;
}

SH4ins(STCDBR) { // DBR -> Rn
    RN = regs.DBR;
    PCinc;
}

SH4ins(STCMDBR) { // Rn-4 -> Rn, DBR -> (Rn)
    RN -= 4;
    WRITE32(RN, regs.DBR);
    PCinc;
}

SH4ins(STCRm_BANK) { // Rm_BANK -> Rn (m = 0-7)
    RN = regs.R_[ins->Rm];
    PCinc;
}

SH4ins(STCMRm_BANK) { // Rn-4 -> Rn, Rm_BANK -> (Rn) (m = 0-7)
    RN -= 4;
    WRITE32(RN, regs.R_[ins->Rm]);
    PCinc;
}

SH4ins(STSMACH) { // MACH -> Rn
    RN = regs.MACH;
    PCinc;
}

SH4ins(STSMMACH) { // Rn-4 -> Rn, MACH -> (Rn)
    RN -= 4;
    WRITE32(RN, regs.MACH);
    PCinc;
}

SH4ins(STSMACL) { // MACL -> Rn
    RN = rMACL;
    PCinc;
}

SH4ins(STSMMACL) { // Rn-4 -> Rn, MACL -> (Rn)
    RN -= 4;
    WRITE32(RN, regs.MACL);
    PCinc;
}

SH4ins(STSPR) { // PR -> Rn
    RN = regs.PR;
    PCinc;
}

SH4ins(STSMPR) { // Rn-4 -> Rn, PR -> (Rn)
    RN -= 4;
    WRITE32(RN, regs.PR);
    PCinc;
}

SH4ins(TRAPA) { // SH1*,SH2*: PC/SR -> stack area, (imm*4 + VBR) -> PC, SH3*,SH4*: PC/SR -> SPC/SSR, imm*4 -> TRA, 0x160 -> EXPEVT, VBR + 0x0100 -> PC
/*    regs.TRAPA = IMM << 2;
    regs.SSR = regs.SR_get();
    regs.SPC = regs.PC + 2;
    regs.SGR = R(15);
    regs.SR_set(regs.SSR | 0x70000000); // MD, BL, and RB
    regs.EXPEVT = 0x160;
    regs.PC = regs.VBR + 0x00000100;*/
    PCinc;
}

SH4ins(FMOV) { // FRm -> FRn
    if (regs.FPSCR.SZ == 0) { // 32-bit
        fpFRUn = fpFRUm;
    }  else BADOPCODE;
    PCinc;
}

SH4ins(FMOV_LOAD) { // (Rm) -> FRn
    if (regs.FPSCR.SZ == 0) { // 32-bit
        fpFRUn = READ32(RM);
    } else BADOPCODE;
    PCinc;
}

SH4ins(FMOV_STORE) { // FRm -> (Rn)
    if (regs.FPSCR.SZ == 0) { // 32-bit
        WRITE32(RN, fpFRUm);
    } else BADOPCODE;
    PCinc;
}

SH4ins(FMOV_RESTORE) { // (Rm) -> FRn, Rm+4 -> Rm
    if (regs.FPSCR.SZ == 0) {// float
        fpFRUn = READ32(RM);
        RM += 4;
    }  else {
        printf("\nWAIT WHAT2? %d %d", regs.FPSCR.SZ, regs.FPSCR.PR);
    }
    PCinc;
}

SH4ins(FMOV_SAVE) { // Rn-4 -> Rn, FRm -> (Rn)
    if (regs.FPSCR.SZ == 0) { // 32-bit
        RN -= 4;
        WRITE32(RN, fpFRUm);
    } else {
        printf("\nWAIT WAHT? %d %d", regs.FPSCR.SZ, regs.FPSCR.PR);
        BADOPCODE;
    }
    PCinc;
}

SH4ins(FMOV_INDEX_LOAD) { // (R0 + Rm) -> FRn
    if (regs.FPSCR.SZ == 0) { // 32-bit
        fpFRUn = READ32(R(0) + RM);
    }  else BADOPCODE;
    PCinc;
}

SH4ins(FMOV_INDEX_STORE) { // FRm -> (R0 + Rn)
    if (regs.FPSCR.SZ == 0) { // 32-bit
        WRITE32(R(0) + RN, fpFRUm);
    } else BADOPCODE;
    PCinc;
}

SH4ins(FMOV_DR) { // DRm -> DRn
    fpDR(ins->Rn) = fpDR(ins->Rm);
    PCinc;
}

SH4ins(FMOV_DRXD) { // DRm -> XDn
    //fpDRU(ins->Rn) = fpXDU(ins->Rm);
    fpXDU(ins->Rn) = fpDRU(ins->Rm);
    PCinc;
}

SH4ins(FMOV_XDDR) { // XDm -> DRn
    fpDRU(ins->Rn) = fpXDU(ins->Rm);
    PCinc;
}

SH4ins(FMOV_XDXD) { // XDm -> XDn
    fpXD(ins->Rn) = fpXD(ins->Rm);
    PCinc;
}

SH4ins(FMOV_LOAD_DR) { // (Rm) -> DRn
    FP64LOAD(RM, fpDRU(ins->Rn));
    PCinc;
}

SH4ins(FMOV_LOAD_XD) { // (Rm) -> XDn
    FP64LOAD(RM, fpXDU(ins->Rn));
    PCinc;
}

SH4ins(FMOV_STORE_DR) { // DRm -> (Rn)
    FP64SAVE(RN, fpDRU(ins->Rm));
    PCinc;
}

SH4ins(FMOV_STORE_XD) { // XDm -> (Rn)
    FP64SAVE(RN, fpXDU(ins->Rm));
    PCinc;
}

SH4ins(FMOV_RESTORE_DR) { // (Rm) -> DRn, Rm + 8 -> Rm
    FP64LOAD(RM, fpDRU(ins->Rn));
    RM += 8;
    PCinc;
}

SH4ins(FMOV_RESTORE_XD) { // (Rm) -> XDn, Rm+8 -> Rm
    FP64LOAD(RM, fpXDU(ins->Rn));
    RM += 8;
    PCinc;
}

SH4ins(FMOV_SAVE_DR) { // Rn-8 -> Rn, DRm -> (Rn)
    RN -= 8;
    FP64SAVE(RN, fpDRU(ins->Rm));
    PCinc;
}

SH4ins(FMOV_SAVE_XD) { // Rn-8 -> Rn, (Rn) -> XDm
    RN -= 8;
    FP64SAVE(RN, fpXDU(ins->Rm));
    PCinc;
}

SH4ins(FMOV_INDEX_LOAD_DR) { // (R0 + Rm) -> DRn
    FP64LOAD(R(0) + RM, fpDRU(ins->Rn));
    PCinc;
}

SH4ins(FMOV_INDEX_LOAD_XD) { // (R0 + Rm) -> XDn
    FP64LOAD(R(0) + RM, fpXDU(ins->Rn));
    PCinc;
}

SH4ins(FMOV_INDEX_STORE_DR) { // DRm -> (R0 + Rn)
    FP64SAVE(R(0) + RN, fpDRU(ins->Rm));
    PCinc;
}

SH4ins(FMOV_INDEX_STORE_XD) { // XDm -> (R0 + Rn)
    FP64SAVE(R(0) + RN, fpXDU(ins->Rm));
    PCinc;
}

SH4ins(FLDI0) { // 0x00000000 -> FRn
    fpFRn = 0.0f;
    PCinc;
}

SH4ins(FLDI1) { // 0x3F800000 -> FRn
    fpFRUn = 0x3F800000;
    PCinc;
}

SH4ins(FLDS) { // FRm -> FPUL
    regs.FPUL.u = regs.fb[0].U32[ins->Rm ^ 1];
    PCinc;
}

SH4ins(FSTS) { // FPUL -> FRn
    fpFRn = regs.FPUL.f;
    PCinc;
}

SH4ins(FABS) { // FRn & 0x7FFFFFFF -> FRn
    fpFRUn &= 0x7FFFFFFF;
    PCinc;
}

SH4ins(FNEG) { // FRn ^ 0x80000000 -> FRn
    fpFRUn^=0x80000000;
    PCinc;
}

SH4ins(FADD) { // FRn + FRm -> FRn
    fpFRn += fpFRm;
    PCinc;
}

SH4ins(FSUB) { // FRn - FRm -> FRn
    // TODO: more precision
    fpFRn -= fpFRm;
    PCinc;
}

SH4ins(FMUL) { // FRn * FRm -> FRn
    fpFRn *= fpFRm;
    PCinc;
}

SH4ins(FMAC) { // FR0 * FRm + FRn -> FRn
    //double res = (double)fpFRn + ((double)fpFR(0) * (double)fpFRm);
    fpFRn =static_cast<f32>(static_cast<f64>(fpFRn) + static_cast<f64>(fpFR(0)) * static_cast<f64>(fpFRm));
    //fpFRn = (float)res;
    PCinc;
}

SH4ins(FDIV) { // FRn / FRm -> FRn
    fpFRn /= fpFRm;
    PCinc;
}

// Thanks Reicast!
SH4ins(FSQRT) { // sqrt (FRn) -> FRn
    fpFRn = sqrtf(fpFRn);
    //CHECK_FPU_32(fr[n]);
    PCinc;
}

SH4ins(FCMP_EQ) { // If FRn = FRm: 1 -> T, Else: 0 -> T
    regs.SR.T = !!(fpFRm == fpFRn);
    PCinc;
}

SH4ins(FCMP_GT) { // If FRn > FRm: 1 -> T, Else: 0 -> T
    //TODO: better accuracy
    regs.SR.T = fpFRn > fpFRm;
    PCinc;
}

SH4ins(FLOAT_single) { // (float)FPUL -> FRn
    if (regs.FPSCR.PR == 0) { // single precision
        fpFRn = static_cast<float>(static_cast<i32>(regs.FPUL.u));
    } else { // double precision
        BADOPCODE;
    }
    PCinc;
}

// Thanks to the amazing website for how to implement this properly

SH4ins(FTRC_single) { // (long)FRm -> FPUL
    regs.FPUL.u = static_cast<u32>(static_cast<i32>((fpFRm < 2147483520.0f ? fpFRm : 2147483520.0f)));     // IEEE 754: 0x4effffff
    // thanks reicast
    // Intel CPUs convert out of range float numbers to 0x80000000. Manually set the correct sign
    if (regs.FPUL.u == 0x80000000)
    {
        if (*reinterpret_cast<int *>(&fpFRm) > 0) // Using integer math to avoid issues with Inf and NaN
            regs.FPUL.u--;
    }

    PCinc;
}
// thanks Reicast
SH4ins(FIPR) { // inner_product (FVm, FVn) -> FR[n+3]
    if (regs.FPSCR.PR == 0)
    {
        //clear_cause ();
        i64 n=(ins->Rn & 3) << 2;
        i64 m=(ins->Rm & 3) << 2;
        assert(ins->Rn < 4);
        assert(ins->Rm < 4);
        float idp = fpFR(n+0) * fpFR(m+0);
        idp += fpFR(n+1) * fpFR(m+1);
        idp += fpFR(n+2) * fpFR(m+2);
        idp += fpFR(n+3) * fpFR(m+3);

        //CHECK_FPU_32(idp);
        fpFR(n+3)=idp;
    }
    else
        BADOPCODE;
    PCinc;
}

// Thanks Reicast!
SH4ins(FTRV) { // transform_vector (XMTRX, FVn) -> FVn
    u32 n=ins->Rn;
    assert(ins->Rn < 4);

    if (regs.FPSCR.PR==0)
    {

        float v1 = fpXF(0)  * fpFR(n + 0) +
             fpXF(4)  * fpFR(n + 1) +
             fpXF(8)  * fpFR(n + 2) +
             fpXF(12) * fpFR(n + 3);

        float v2 = fpXF(1)  * fpFR(n + 0) +
             fpXF(5)  * fpFR(n + 1) +
             fpXF(9)  * fpFR(n + 2) +
             fpXF(13) * fpFR(n + 3);

        float v3 = fpXF(2)  * fpFR(n + 0) +
             fpXF(6)  * fpFR(n + 1) +
             fpXF(10) * fpFR(n + 2) +
             fpXF(14) * fpFR(n + 3);

        float v4 = fpXF(3)  * fpFR(n + 0) +
             fpXF(7)  * fpFR(n + 1) +
             fpXF(11) * fpFR(n + 2) +
             fpXF(15) * fpFR(n + 3);

        /*CHECK_FPU_32(v1);
        CHECK_FPU_32(v2);
        CHECK_FPU_32(v3);
        CHECK_FPU_32(v4);*/

        fpFR(n + 0) = v1;
        fpFR(n + 1) = v2;
        fpFR(n + 2) = v3;
        fpFR(n + 3) = v4;
    }
    else
    {
        BADOPCODE;
    }
    PCinc;
}

SH4ins(FABSDR) { // DRn & 0x7FFFFFFFFFFFFFFF -> DRn
    fpDRU(ins->Rn) &= 0x7FFFFFFFFFFFFFFF;
    PCinc;
}

SH4ins(FNEGDR) { // DRn ^ 0x8000000000000000 -> DRn
    fpDRU(ins->Rn) ^= 0x8000000000000000;
    PCinc;
}

SH4ins(FADDDR) { // DRn + DRm -> DRn
    fpDR(ins->Rn) += fpDR(ins->Rm);
    PCinc;
}

SH4ins(FSUBDR) { // DRn - DRm -> DRn
    fpDR(ins->Rn) -= fpDR(ins->Rm);
    PCinc;
}

SH4ins(FMULDR) { // DRn * DRm -> DRn
    fpDR(ins->Rn) *= fpDR(ins->Rm);
    PCinc;
}

SH4ins(FDIVDR) { // DRn / DRm -> DRn
    fpDR(ins->Rn) /= fpDR(ins->Rm);
    PCinc;
}

SH4ins(FSQRTDR) { // sqrt (DRn) -> DRn
    fpDR(ins->Rn) = sqrt(fpDR(ins->Rn));
    PCinc;
}

SH4ins(FCMP_EQDR) { // If DRn = DRm: 1 -> T, Else: 0 -> T
    regs.SR.T = fpDRU(ins->Rn) == fpDRU(ins->Rm);
    PCinc;
}

SH4ins(FCMP_GTDR) { // If DRn > DRm: 1 -> T, Else: 0 -> T
    regs.SR.T = fpDR(ins->Rn) > fpDR(ins->Rm);
    PCinc;
}

SH4ins(FLOAT_double) { // (double)FPUL -> DRn
    fpDR(ins->Rn) = static_cast<double>(regs.FPUL.u); // ????? !!!! WHAT!?
    PCinc;
}

SH4ins(FTRC_double) { // (long)DRm -> FPUL
    regs.FPUL.u = static_cast<u32>(static_cast<i32>(fpDR(ins->Rm)));
    PCinc;
}

SH4ins(FCNVDS) { // double_to_float (DRm) -> FPUL
    regs.FPUL.f = static_cast<float>(fpDR(ins->Rm));
    PCinc;
}

SH4ins(FCNVSD) { // float_to_double (FPUL) -> DRn
    if (regs.FPSCR.PR == 1)
        fpDR(ins->Rn) = static_cast<double>(regs.FPUL.f);
    else
        printf("\nInvalid FCNVSD when PR=0");
    PCinc;
}

SH4ins(LDSFPSCR) { // Rm -> FPSCR
    regs.FPSCR_set(RM); //  & 0x003FFFFF);
    PCinc;
}

SH4ins(STSFPSCR) { // FPSCR -> Rn
    RN = regs.FPSCR_get();
    PCinc;
}

SH4ins(LDSMFPSCR) { // (Rm) -> FPSCR, Rm+4 -> Rm
    regs.FPSCR_set(READ32(RM));
    RM += 4;
    PCinc;
}

SH4ins(STSMFPSCR) { // Rn-4 -> Rn, FPSCR -> (Rn)
    RN -= 4;
    WRITE32(RN, regs.FPSCR_get());
    PCinc;
}

SH4ins(LDSFPUL) { // Rm -> FPUL
    regs.FPUL.u = RM;
    PCinc;
}

SH4ins(STSFPUL) { // FPUL -> Rn
    RN = regs.FPUL.u;
    PCinc;
}

SH4ins(LDSMFPUL) { // (Rm) -> FPUL, Rm+4 -> Rm
    regs.FPUL.u = READ32(RM);
    RM += 4;
    PCinc;
}

SH4ins(STSMFPUL) { // Rn-4 -> Rn, FPUL -> (Rn)
    RN -= 4;
    WRITE32(RN, regs.FPUL.u);
    PCinc;
}

SH4ins(FRCHG) { // If FPSCR.PR = 0: ~FPSCR.FR -> FPSCR.FR, Else: Undefined Operation
    if (regs.FPSCR.PR == 0) {
        regs.FPSCR.FR ^= 1;
        regs.FPSCR_bankswitch();
    } else {
        assert(1==0);
    }
    PCinc;
}

SH4ins(FSCHG) { // If FPSCR.PR = 0: ~FPSCR.SZ -> FPSCR.SZ, Else: Undefined Operation
    if (regs.FPSCR.PR == 0) {
        regs.FPSCR.SZ ^= 1;
    }
    else assert(1==0);

    PCinc;
}

SH4ins(FSRRA) { //1.0 / sqrt (FRn) -> FRn
    //PC += 2;
    //clear_cause();
    if (regs.FPSCR.PR != 0) {
        BADOPCODE;
    }
    else {
        //clear_cause();
        /*float n = fpFRn;

        switch (fpclassify(fpFRn))
        {
            case FP_NORMAL:
                if (n >= 0)
                {
                    fpFRn = 1.0f / sqrtf(n);
                }
                else {
                    printf("\nINVALID VALUE FOR FSRRA");
                    //invalid(n);
                }
                break;

            case FP_SUBNORMAL:
                //if (sign_of (n) == 0)
                    fpu_error ();
                else
                    invalid (n);
                break;///

            case FP_ZERO:
                //dz (n, sign_of (n));
                break;

            case FP_INFINITE:
                if (n > 0)
                    fpFRn = 0;
                else
                    break;
                    //invalid (n);
                break;
            case FP_NAN:
                //qnan (n);
                break;

            ///case sNAN:
                //invalid (n);
                break;///
        }*/
         fpFRn = (float)(1/sqrtf(fpFRn));
    }
    PCinc;
}

SH4ins(FSCA) { // sin (FPUL) -> FRn, cos (FPUL) -> FR[n+1]
    // thanks to dRk|Raziel for this! using tables dumped from real DC
    if (regs.FPSCR.PR==0)
    {
        u32 n = ins->Rn & 0xE;
        u32 pi_index=static_cast<u16>(regs.FPUL.u);

        fpFR(n | 0) = sin_table[pi_index];//sinf(real_pi);
        fpFR(n | 1) = sin_table[0x4000 + pi_index];//cosf(real_pi);    // -> no need for warparound, sin_table has 0x4000 more entries
    }
    else {
        BADOPCODE;
    }
    PCinc;
}

#undef rPC
#undef R
#undef RN
#undef RM
#undef IMM
#undef DISP

/*#define OE(counter, ins) opcode = counter; sd = &decoded[opcode]; assert(sd->exec=nullptr); sd->exec = &ins
#define OErmrn(counter, ins) OE(counter, ins); sd->Rm = rm; sd->Rn = rn*/
/*
             opcode = 0b0110000000000011 | (rm << 8) | (rn << 4);
            sd = &decoded[opcode];
*
 */

struct ins_t decoded[4][65536];
char disassembled[4][65536][30];
char mnemonic[4][65536][30];

void process_instruct(sh4_str_ret *r, const char* stri)
{
    memset(r, 0, sizeof(sh4_str_ret));

    enum ddd{
        nothing,
        in_d,
        in_n,
        in_m,
        in_i
    } doing=nothing, new_doing=nothing;

    u32 val = 0x10000;

    for (u32 i = 0; i < 16; i++) {
        val >>= 1;
        u8 chr = stri[i];
        u32 *max = nullptr;
        u32 *msk = nullptr;
        if ((chr == '0') || (chr == '1')) {
            if (chr == '1') r->mask |= val;
            switch(doing) {
                case nothing:
                    break;
                case in_d:
                    r->d_shift = 16 - i;
                    break;
                case in_m:
                    r->m_shift = 16 - i;
                    break;
                case in_n:
                    r->n_shift = 16 - i;
                    break;
                case in_i:
                    r->i_shift = 16 - i;
            }
            doing = nothing;
            continue;
        }
        if (chr == 'n') {
            max = &r->n_max;
            msk = &r->n_mask;
            new_doing = in_n;
        }
        else if (chr == 'm') {
            max = &r->m_max;
            msk = &r->m_mask;
            new_doing = in_m;
        }
        else if (chr == 'i') {
            max = &r->i_max;
            msk = &r->i_mask;
            new_doing = in_i;
        }
        else {//if (chr == 'd') {
            max = &r->d_max;
            msk = &r->d_mask;
            new_doing = in_d;
        }
        *msk = ((*msk) << 1) | 1;
        *max = (*msk) + 1;
        if (new_doing != doing) {
            switch(doing) {
                case nothing:
                    break;
                case in_d:
                    r->d_shift = 16 - i;
                    break;
                case in_m:
                    r->m_shift = 16 - i;
                    break;
                case in_n:
                    r->n_shift = 16 - i;
                    break;
                case in_i:
                    r->i_shift = 16 - i;
            }
        }
        doing = new_doing;
    }
}

static void emplace_mnemonic(u32 opcode, const char *mnon, u32 n, u32 m, u32 d, u32 imm, u32 szpr)
{
    strcpy(mnemonic[szpr][opcode], mnon);

    char *copy_to = disassembled[szpr][opcode];
    *copy_to = 0;

    u32 l = 0;
    for (const char *mn = const_cast<char *>(mnon); *mn!=0; mn++) {
        if ((*mn == 'R') && ((*(mn+1) == 'n') || (*(mn+1) == 'm'))) {
            // Rm or Rn
            u32 rl = snprintf(copy_to, 30, "R%d", *(mn+1) == 'n' ? n : m);
            copy_to += rl;
            l += rl;
            mn++;
            continue;
        }
        if ((*mn == 'd') && (*(mn+1) == 'i') && (*(mn+2) == 's')) {
            // disp
            u32 rl = snprintf(copy_to, 30 - l, "%02x", d);
            copy_to += rl;
            l += rl;
            mn += 3;
            continue;
        }
        if ((*mn == 'i') && (*(mn+1) == 'm' && (*(mn+2) == 'm'))) {
            // imm
            u32 rl = snprintf(copy_to, 30 - l, "%d", imm);
            copy_to += rl;
            l += rl;
            mn += 2;
            continue;
        }
        *copy_to = *mn;
        copy_to++;
    }
    *copy_to = 0;
}

static void cpSH4(u32 dest, u32 src) {
    memcpy(&decoded[dest], &decoded[src], 65536*sizeof(ins_t));
    memcpy(&disassembled[dest][0], &disassembled[src][0], 65536*30);
    memcpy(&mnemonic[dest][0], &mnemonic[src][0], 65535*30);
}

static void iterate_opcodes(sh4_str_ret* r, ins_func ins, const char* mnem, u32 override, u32 szpr)
{
    // Go at least once through each of n, m, d, and i
    u32 n_max = r->n_max ? r->n_max : 1;
    u32 m_max = r->m_max ? r->m_max : 1;
    u32 d_max = r->d_max ? r->d_max : 1;
    u32 i_max = r->i_max ? r->i_max : 1;
    for (u32 n = 0; n < n_max; n++) {
        for (u32 m = 0; m < m_max; m++) {
            for (u32 d = 0; d < d_max; d++) {
                for (u32 i = 0; i < i_max; i++) {
                    u32 d_times = 1;
                    u32 opcode = 0;
                    if (r->n_max > 0) opcode |= (n << r->n_shift);
                    if (r->m_max > 0) opcode |= (m << r->m_shift);
                    if (r->d_max > 0) opcode |= (d << r->d_shift);
                    if (r->i_max > 0) opcode |= (i << r->i_shift);
                    opcode |= r->mask;

                    ins_t *is = &decoded[szpr][opcode];
                    if ((!override) && (is->decoded != 0)) { // If no override and already decoded
                        assert(1==0);
                    }
                    else if (override & (is->decoded == 0)) {// If override and NOT already decoded
                        assert(1==0);
                    }
                    is->Rn = n;
                    is->Rm = m;
                    is->imm = i;
                    is->disp = static_cast<i32>(d * d_times);
                    is->exec = ins;
                    is->decoded = true;
                    emplace_mnemonic(opcode, mnem, n, m, d, i, szpr);

                    if (szpr > 0) {
                        // copy to szpr=3
                        memcpy(&decoded[3][opcode], &decoded[szpr][opcode], sizeof(ins_t));
                        memcpy(&disassembled[3][opcode][0], &disassembled[szpr][opcode][0], 30);
                        memcpy(&mnemonic[3][opcode][0], &mnemonic[szpr][opcode][0], 30);
                    }
                    //printf("\n%s", disassembled[0][opcode]);
                }
            }
        }
    }
}

static void decode_and_iterate_opcodes(const char* inpt, ins_func ins, const char *mnem, u32 override, u32 szpr)
{
    sh4_str_ret a;
    process_instruct(&a, inpt);
    iterate_opcodes(&a, ins, mnem, override, szpr);
}


#define OE(opcstr, func, mn) decode_and_iterate_opcodes(opcstr, func, mn, 0, 0)
#define OEo(opcstr, func, mn, sz, pr) decode_and_iterate_opcodes(opcstr, func, mn, 1, ((sz<<1) | pr))

void core::do_sh4_decode() {
    static bool decode_done = false;
    if (decode_done) {
        printf("\nDUPLICATE DECODE DO!?");
        return;
    }
    generate_fsca_table();
    decode_done = true;
    for (u32 szpr = 0; szpr < 4; szpr++) {
        for (u32 i = 0; i < 65536; i++) {
            decoded[szpr][i] = (ins_t) {
                    .opcode = static_cast<u16>(i),
                    .Rm = -1,
                    .Rn = -1,
                    .disp = 0,
                    .imm = 0,
                    .exec = nullptr,
                    .decoded = false
            };
            disassembled[szpr][i][0] = 0;
            mnemonic[szpr][i][0] = 0;
        }
    }

    u32 r = 0;

#include "sh4_decodings.cpp"

    u32 unencoded = 0;
    for (u32 szpr = 0; szpr < 4; szpr++) {
        for (u32 i = 0; i < 65536; i++) {
            if (!decoded[szpr][i].decoded) {
                decoded[szpr][i].exec = &core::ins_EMPTY;
                snprintf(disassembled[szpr][i], 30, "UNKNOWN OPCODE %04x", i);
                snprintf(mnemonic[szpr][i], 30, "UNKNOWN OPCODE %04x", i);
                unencoded++;
                decoded[szpr][i].decoded = true;
            }
        }
    }
}
}