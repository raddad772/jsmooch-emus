//
// Created by Dave on 2/10/2024.
//

#include "math.h"
#include "string.h"
#include "assert.h"
#include "stdio.h"

#include "fsca.h"
#include "sh4_interpreter.h"
#include "sh4_interpreter_opcodes.h"

#define SH4args  struct SH4* this, struct SH4_ins_t *ins
#define PCinc rPC += 2
#define rPC this->regs.PC

#define R(x) this->regs.R[x]
#define RM R(ins->Rm)
#define RN R(ins->Rn)

#define fFR this->regs.FPSCR.FR
#define IMM ins->imm
#define DISP ins->disp

#define rMACL this->regs.MACL
#define rMACH this->regs.MACH

#define fpFR(x) this->regs.fb[0].FP32[(x)]
#define fpFRU(x) this->regs.fb[0].U32[(x)]
#define fpDR(x) this->regs.fb[0].FP64[(x)]
#define fpDRU(x) this->regs.fb[0].U64[(x)]
#define fpXF(x) this->regs.fb[1].FP32[(x)]
#define fpXD(x) this->regs.fb[1].FP64[(x)]
#define fpXDU(x) this->regs.fb[1].U64[(x)]
#define fpFV(x) this->regs.fb[0].FV[(x)]
#define fpXMTX(x) this->regs.fb[1].MTX

#define BADOPCODE { printf("\nUNIMPLEMENTED INSTRUCTION %s PR=%d SZ=%d", __func__, this->regs.FPSCR.PR, this->regs.FPSCR.SZ); fflush(stdout); dbg_printf("\nUNIMPLEMENTED INSTRUCTION %s PR=%d SZ=%d", __func__, this->regs.FPSCR.PR, this->regs.FPSCR.SZ); }

#define READ8(addr) this->read(this->mptr, addr, DC8)
#define READ16(addr) this->read(this->mptr, addr, DC16)
#define READ32(addr) this->read(this->mptr, addr, DC32)
#define READ64(addr) this->read(this->mptr, addr, DC64)
#define WRITE8(addr, val) this->write(this->mptr, addr, (val) & 0xFF, DC8)
#define WRITE16(addr, val) this->write(this->mptr, addr, (val) & 0xFFFF, DC16)
#define WRITE32(addr, val) this->write(this->mptr, addr, (val) & 0xFFFFFFFF, DC32)
#define WRITE64(addr, val) this->write(this->mptr, addr, val, DC64)

#define SH4ins(x) void SH4_##x(SH4args)

#define DELAY_SLOT { PCinc; SH4_fetch_and_exec(this, 1); }

enum MSIZE {
    DC8 = 1,
    DC16 = 2,
    DC32 = 4,
    DC64 = 8
};
/*
 Source: The interrupt mask bit setting in SR is smaller than the IRL (3ñ0) level, and the BL bit
in SR is 0 (accepted at instruction boundary).
• Transition address: VBR + H'0000 0600
• Transition operations:
The PC contents immediately after the instruction at kind the interrupt is accepted are set in
SPC. The SR and R15 contents at the time of acceptance are set in SSR and SGR.
The code corresponding to the IRL (3ñ0) level is set in INTEVT. See table 19.5, Interrupt
Exception Handling Sources and Priority Order, for the corresponding codes. The BL, MD,
and RB bits are set to 1 in SR, and a branch is made to VBR + H'0600. The acceptance level is
not set in the interrupt mask bits in SR. When the BL bit in SR is 1, the interrupt is masked.
For details, see Interrupt Controller in the hardware manual.
 */
// Interrupt!

static u32 INTEVT_TABLE[15] = {
        0x200,
        0x220,
        0x240,
        0x260,
        0x280,
        0x2A0,
        0x2C0,
        0x2E0,
        0x300,
        0x320,
        0x340,
        0x360,
        0x380,
        0x3A0,
        0x3C0
};

void SH4_interrupt_IRL(struct SH4* this, u32 level) {
    this->regs.SPC = this->regs.PC;
    this->regs.SSR = SH4_regs_SR_get(&this->regs.SR);
    this->regs.SGR = this->regs.R[15];

    u32 old_SR = SH4_regs_SR_get(&this->regs.SR);
    SH4_SR_set(this, old_SR | (1 << 30) | (1 << 29) | (1 << 28));

    /*this->regs.SR.MD = 1;
    this->regs.SR.RB = 1;
    this->regs.SR.BL = 1;*/
    this->regs.PC = this->regs.VBR + 0x00000600;
    /*
     Only the IRL1 and 2 interrupts are used (pull up IRL0 and 3), and these interrupts are used as level
encoding interrupts. The interrupt levels are "2" (IRL3:0 = 1101), "4" (IRL3:0 = 1011), and "6"
(IRL3:0 = 1001).
     */
#ifdef SH4_DBG_SUPPORT
    if (dbg.trace_on) {
        dbg_printf("\nRaising interrupt %d cyc:%llu", level, this->clock.trace_cycles);
    }
#endif
    this->regs.INTEVT = INTEVT_TABLE[level];
}

SH4ins(EMPTY) {
    printf("\nUNKNOWN OPCODE EXECUTION ATTEMPTED: %08x %04x %llu", this->regs.PC, ins->opcode, this->clock.trace_cycles);
    fflush(stdout);
    dbg_break();
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
    this->regs.R[0] = (rPC & 0xFFFFFFFC) + (DISP*4) + 4;
    PCinc;
}

SH4ins(MOVWI) {
    u32 addr = rPC + (DISP*2) + 4;
    u32 val = READ16(addr);
    RN = SIGNe16to32(val);
    PCinc;
}

SH4ins(MOVLI) {
    u32 addr = (rPC & 0xFFFFFFFC) + (DISP*4) + 4;
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
    SIGNe16to32(RN);
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
    u32 val = READ8(ins->disp + this->regs.GBR);
    R(0) = SIGNe8to32(val);
    PCinc;
}

SH4ins(MOVWLG) { // (disp*2 + GBR) -> sign extension -> R0
    u32 val = READ8((ins->disp*2) + this->regs.GBR);
    R(0) = SIGNe16to32(val);
    PCinc;
}

SH4ins(MOVLLG) { // (disp*4 + GBR) -> R0
    R(0) = READ32((ins->disp*4) + this->regs.GBR);
    PCinc;
}

SH4ins(MOVBSG) { // R0 -> (disp + GBR)
    WRITE8(ins->disp + this->regs.GBR, R(0));
    PCinc;
}

SH4ins(MOVWSG) { // R0 -> (disp*2 + GBR)
    WRITE16((ins->disp << 1) + this->regs.GBR, R(0));
    PCinc;
}

SH4ins(MOVLSG) { // R0 -> (disp*4 + GBR)
    WRITE32((ins->disp << 2) + this->regs.GBR, R(0));
    PCinc;
}

SH4ins(MOVT) { // T -> Rn
    RN = this->regs.SR.T;
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
    u32 tmp0, tmp1;
    tmp1 = RN + RM;
    tmp0 = RN;
    RN = tmp1 + this->regs.SR.T;

    this->regs.SR.T = tmp0 > tmp1;

    if (tmp1 > RN)
        this->regs.SR.T = 1;

    PCinc;
}

SH4ins(ADDV) { // Rn + Rm -> Rn, overflow -> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(CMPIM) { // If R0 = (sign extension)imm: 1 -> T, Else: 0 -> T
    this->regs.SR.T = R(0) == SIGNe8to32(IMM);
    PCinc;
}

SH4ins(CMPEQ) { // If Rn = Rm: 1 -> T, Else: 0 -> T
    this->regs.SR.T = RN == RM;
    PCinc;
}

SH4ins(CMPHS) { // If Rn >= Rm (unsigned): 1 -> T, Else: 0 -> T
    this->regs.SR.T = RN >= RM;
    PCinc;
}

SH4ins(CMPGE) { // If Rn >= Rm (signed): 1 -> T, Else: 0 -> T
    this->regs.SR.T = ((i32)RN >= (i32)RM);
    PCinc;
}

SH4ins(CMPHI) { // If Rn > Rm (unsigned): 1 -> T, Else: 0 -> T
    this->regs.SR.T = RN > RM;
    PCinc;
}

SH4ins(CMPGT) { // If Rn > Rm (signed): 1 -> T, Else: 0 -> T
    this->regs.SR.T = ((i32)RN) > ((i32)RM);
    PCinc;
}

SH4ins(CMPPL) { // If Rn > 0 (signed): 1 -> T, Else: 0 -> T
    this->regs.SR.T = ((i32)RN) > 0;
    PCinc;
}

SH4ins(CMPPZ) { // If Rn >= 0 (signed): 1 -> T, Else: 0 -> T
    this->regs.SR.T = ((i32)RN) >= 0;
    PCinc;
}

SH4ins(CMPSTR) { // If Rn and Rm have an equal byte: 1 -> T, Else: 0 -> T
    u32 r = RN ^ RM;
    this->regs.SR.T = (!(r & 0xFF000000)) || (!(r & 0xFF0000)) || (!(r & 0xFF00)) || (!(r & 0xFF));
    PCinc;
}

SH4ins(DIV0S) { // MSB of Rn -> Q, MSB of Rm -> M, M ^ Q -> T
    this->regs.SR.Q = (RN & 0x80000000) != 0;

    this->regs.SR.M = (RM & 0x80000000) != 0;

    this->regs.SR.T = ! (this->regs.SR.M == this->regs.SR.Q);
    PCinc;
}

SH4ins(DIV0U) { // 0 -> M, 0 -> Q, 0 -> T
    this->regs.SR.M = this->regs.SR.Q = this->regs.SR.T = 0;
    PCinc;
}

#define fQ this->regs.SR.Q
#define fM this->regs.SR.M
#define fT this->regs.SR.T

SH4ins(DIV1) { // 1-step division (Rn / Rm)
    unsigned long tmp0, tmp2;
    unsigned char old_q, tmp1;

    old_q = fQ;
    fQ = (0x80000000 & RN) != 0;
    tmp2 = RM;
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
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(DMULU) { // Unsigned, Rn * Rm -> MACH:MACL, 32 * 32 -> 64 bits
    // TODO: this appears correct...but...maybe try weird sh4-instrs version?
    u64 n = RN;
    u64 m = RM;
    u64 r = n * m;
    this->regs.MACH = ((r >> 32) & 0xFFFFFFFF);
    this->regs.MACL = r & 0xFFFFFFFF;
    PCinc;
}

SH4ins(DT) { // Rn-1 -> Rn, If Rn = 0: 1 -> T, Else: 0 -> T
    RN--;
    this->regs.SR.T = RN == 0;
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
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(MACW) { // Signed, (Rn) * (Rm) + MAC -> MAC, SH1: 16 * 16 + 42 -> 42 bits, Other: 16 * 16 + 64 -> 64 bits
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(MULL) { // Rn * Rm -> MACL, 32 * 32 -> 32 bits
    rMACL = RN * RM;
    PCinc;
}

SH4ins(MULS) { // Signed, Rn * Rm -> MACL, 16 * 16 -> 32 bits
    this->regs.MACL = (u32)((i32)(i16)RN * (i32)(i16)RM);
    PCinc;
}

SH4ins(MULU) { // Unsigned, Rn * Rm -> MACL, 16 * 16 -> 32 bits
    this->regs.MACL = (RN & 0xFFFF) * (RM & 0xFFFF);
    PCinc;
}

SH4ins(NEG) { // 0 - Rm -> Rn
    RN = (u32)(0 - ((i32)RM));
    PCinc;
}

SH4ins(NEGC) { // 0 - Rm - T -> Rn, borrow -> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(SUB) { // Rn - Rm -> Rn
    RN -= RM;
    PCinc;
}

SH4ins(SUBC) { // Rn - Rm - T -> Rn, borrow -> T
    u32 tmp0, tmp1;
    tmp1 = RN - RM;
    tmp0 = RN;
    RN = tmp1 - this->regs.SR.T;

    this->regs.SR.T = tmp0 < tmp1;
    if (tmp1 < RN)
        this->regs.SR.T = 1;

    PCinc;
}

SH4ins(SUBV) { // Rn - Rm -> Rn, underflow -> T
    BADOPCODE; // Crash on unimplemented opcode
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
    BADOPCODE; // Crash on unimplemented opcode
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
    u32 addr = this->regs.GBR + R(0);
    WRITE8(addr, READ8(addr) | IMM);
    PCinc;
}

SH4ins(TAS) { // If (Rn) = 0: 1 -> T, Else: 0 -> T, 1 -> MSB of (Rn)
    u32 val = READ8(RN);
    this->regs.SR.T = val == 0;
    val |= 0x80;
    WRITE8(RN, val);
    PCinc;
}

SH4ins(TST) { // If Rn & Rm = 0: 1 -> T, Else: 0 -> T
    this->regs.SR.T = (RN & RM) == 0;
    PCinc;
}

SH4ins(TSTI) { // If R0 & (zero extend)imm = 0: 1 -> T, Else: 0 -> T
    this->regs.SR.T = (R(0) & IMM) == 0;
    PCinc;
}

SH4ins(TSTM) { // If (R0 + GBR) & (zero extend)imm = 0: 1 -> T, Else 0: -> T
    BADOPCODE; // Crash on unimplemented opcode
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
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(ROTCL) { // T << Rn << T
    u32 v = (RN >> 31);
    assert(this->regs.SR.T < 2);
    RN = (RN << 1) | this->regs.SR.T;
    this->regs.SR.T = v;
    PCinc;
}

SH4ins(ROTCR) { // T >> Rn >> T
    u32 tmp = RN & 1;
    assert(this->regs.SR.T < 2);
    RN = (RN >> 1) | (this->regs.SR.T << 31);
    this->regs.SR.T = tmp;
    PCinc;
}

SH4ins(ROTL) { // T << Rn << MSB
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(ROTR) { // LSB >> Rn >> T
    this->regs.SR.T = RN & 1;
    RN = (RN >> 1) | (this->regs.SR.T << 31);
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
        RN = (u32)((i32)RN >> ((~RM & 0x1F) + 1));

    PCinc;
}

SH4ins(SHAL) { // T << Rn << 0
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(SHAR) { // MSB >> Rn >> T
    this->regs.SR.T = RN & 1;
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
        RM = (u32)RN >> ((~RM & 0x1F) + 1);

    PCinc;
}

SH4ins(SHLL) { // T << Rn << 0
    this->regs.SR.T = (RN >> 31) & 1;
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
    this->regs.SR.T = RN & 1;
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
    //printf("\nBF! %llu", this->clock.trace_cycles);
    assert(this->regs.SR.T < 2);
    // PC += 2, kind is normal
    // add ((2 * DISP) + 2) * !T
    // so it'll add 0 if T=1
    // it'll add (2*DISP)+2 if T=0
    /*i64 val = rPC + 2;
    val += ((2 * (i32)SIGNe8to32(DISP)) + 2) * (i32)(this->regs.SR.T ^ 1);
    rPC = (u32)val;*/

    i32 disp = (i32)SIGNe8to32(DISP);

    if (this->regs.SR.T == 0)
        rPC += 4 + (disp << 1);
    else
        rPC += 2;
}

SH4ins(BFS) { // If T = 0: disp*2 + PC + 4 -> PC, Else: nop, (Delayed branch)
    //printf("\nBFS! %llu", this->clock.trace_cycles);
    // PC is gonna be +4 no matter what, kind is correct
    // then we add 2*disp to that after if T = 0
    /*i64 val = (i64)rPC + 4;
    val += (2 * (i32)SIGNe8to32(DISP)) * (i32)(this->regs.SR.T ^ 1);
    DELAY_SLOT;
    rPC = (u32)val; // DISP*2 * !this->regs.SR.T*/
    i32 disp = (i32)SIGNe8to32(DISP);
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
    val += ((2 * (i32)SIGNe8to32(DISP)) + 2) * (i32)this->regs.SR.T;
    rPC = (u32)val;*/
    i32 disp = (i32)SIGNe8to32(DISP);

    if (fT == 1)
        rPC += 4 + (disp << 1);
    else
        rPC += 2;
}

SH4ins(BTS) { // If T = 1: disp*2 + PC + 4 -> PC, Else: nop, (Delayed branch)
    /*i64 val = (i64)rPC + 4;
    val += (2 * (i32)SIGNe8to32(DISP)) * (i32)this->regs.SR.T;*/
    i32 disp = (i32)SIGNe8to32(DISP);
    u32 temp = rPC;

    if (fT == 1)
        temp += 4 + (disp << 1);
    else
        temp += 4;

    DELAY_SLOT;
    rPC = temp; // DISP*2 * !this->regs.SR.T
}

SH4ins(BRA) { // disp*2 + PC + 4 -> PC, (Delayed branch)
    /*i64 val = (i64)rPC + 4;
    val += (2 * (i32)SIGNe12to32(DISP));
    DELAY_SLOT;
    rPC = (u32)val;*/
    i32 disp = (i32)SIGNe12to32(DISP);
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
    this->regs.PR = rPC + 4;
    val = rPC + 4 + val;
    DELAY_SLOT;
    rPC = (u32)val;*/
    i32 disp = (i32)SIGNe12to32(DISP);
    u32 temp = rPC;

    this->regs.PR = rPC + 4;
    temp += 4 + (disp << 1);
    DELAY_SLOT;
    rPC = temp;
}

SH4ins(BSRF) { // PC + 4 -> PR, Rm + PC + 4 -> PC, (Delayed branch)
    this->regs.PR = rPC + 4;
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
    this->regs.PR = rPC + 4;
    u32 val = RM;
    DELAY_SLOT;
    rPC = val;
}

SH4ins(RTS) { // PR -> PC, Delayed branch
    u32 val = this->regs.PR;
    DELAY_SLOT;
    rPC = val;
}

SH4ins(CLRMAC) { // 0 -> MACH, 0 -> MACL
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(CLRS) { // 0 -> S
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(CLRT) { // 0 -> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(LDCSR) { // Rm -> SR
    SH4_SR_set(this, RM & 0x700083F3);
    PCinc;
}

SH4ins(LDCMSR) { // (Rm) -> SR, Rm+4 -> Rm
    SH4_SR_set(this, READ32(RM) & 0x700083F3);
    RM += 4;
    PCinc;
}

SH4ins(LDCGBR) { // Rm -> GBR
    this->regs.GBR = RM;
    PCinc;
}

SH4ins(LDCMGBR) { // (Rm) -> GBR, Rm+4 -> Rm
    this->regs.GBR = READ32(RM);
    RM += 4;
    PCinc;
}

SH4ins(LDCVBR) { // Rm -> VBR
    this->regs.VBR = RM;
    PCinc;
}

SH4ins(LDCMVBR) { // (Rm) -> VBR, Rm+4 -> Rm
    this->regs.VBR = READ32(RM);
    RM += 4;
    PCinc;
}

SH4ins(LDCSSR) { // Rm -> SSR
    this->regs.SSR = RM;
    PCinc;
}

SH4ins(LDCMSSR) { // (Rm) -> SSR, Rm+4 -> Rm
    this->regs.SSR = READ32(RM);
    RM += 4;
    PCinc;
}

SH4ins(LDCSPC) { // Rm -> SPC
    this->regs.SPC = RM;
    PCinc;
}

SH4ins(LDCMSPC) { // (Rm) -> SPC, Rm+4 -> Rm
    this->regs.SPC = READ32(RM);
    RM += 4;
    PCinc;
}

SH4ins(LDCDBR) { // Rm -> DBR
    this->regs.DBR = RM;
    PCinc;
}

SH4ins(LDCMDBR) { // (Rm) -> DBR, Rm+4 -> Rm
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(LDCRn_BANK) { // Rm -> Rn_BANK (n = 0-7)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(LDCMRn_BANK) { // (Rm) -> Rn_BANK, Rm+4 -> Rm
    BADOPCODE; // Crash on unimplemented opcode
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
    this->regs.MACL = READ32(RM);
    RM += 4;
    PCinc;
}

SH4ins(LDSPR) { // Rm -> PR
    this->regs.PR = RM;
    PCinc;
}

SH4ins(LDSMPR) { // (Rm) -> PR, Rm+4 -> Rm
    this->regs.PR = READ32(RM);
    RM += 4;
    PCinc;
}

SH4ins(LDTLB) { // PTEH/PTEL -> TLB
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(MOVCAL) { // R0 -> (Rn) (without fetching cache block)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(NOP) { // No operation
    /*if ((this->regs.PC == 0x8c0c0868) || (this->regs.PC == 0x8c0c086A)) {
        dbg_printf("\nWAIT WOAH %08x PC:%08x", this->regs.R[0], this->regs.PC);
    }*/
    PCinc;
}

SH4ins(OCBI) { // Invalidate operand cache block
    PCinc;
}

SH4ins(OCBP) { // Write back and invalidate operand cache block
    PCinc;
}

SH4ins(OCBWB) { // Write back operand cache block
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(PREF) { // (Rn) -> operand cache
    u32 addr = RN & 0xFFFFFFE0;
    // Flush store queue to RAM!
    if ((addr >= 0xE0000000) && (addr <= 0xE3FFFFFF)) {
        u32 sq = (addr >> 5) & 1;
        //const ext_addr = (addr & 0x03FFFFE0) | (((cpu.read_p4_register(u32, if (sq_addr.sq == 0) .QACR0 else .QACR1) & 0b11100) << 24));
        u32 naddr = addr & 0x03FFFFE0 | (((sq ? this->regs.QACR1 : this->regs.QACR0) & 0b11100) << 24);
        for (u32 i = 0; i < 8; i++) {
            WRITE32(naddr, *(u32*)&this->SQ[sq][i<<2]);
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
    //BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(RTE) { // Delayed branch, SH1*,SH2*: stack area -> PC/SR, SH3*,SH4*: SSR/SPC -> SR/PC
    SH4_SR_set(this, this->regs.SSR);
    u32 val = this->regs.SPC;

    DELAY_SLOT;
    rPC = val;
}

SH4ins(SETS) { // 1 -> S
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(SETT) { // 1 -> T
    this->regs.SR.T = 1;
    PCinc;
}

SH4ins(SLEEP) { // Sleep or standby
    dbg_break();
    printf("\nBREAK FOR SLEEP!");
    dbg_LT_dump();
    PCinc;
}

SH4ins(STCSR) { // SR -> Rn
    RN = SH4_regs_SR_get(&this->regs.SR);
    PCinc;
}

SH4ins(STCMSR) { // Rn-4 -> Rn, SR -> (Rn)
    RN -= 4;
    WRITE32(RN, SH4_regs_SR_get(&this->regs.SR));
    PCinc;
}

SH4ins(STCGBR) { // GBR -> Rn
    RN = this->regs.GBR;
    PCinc;
}

SH4ins(STCMGBR) { // Rn-4 -> Rn, GBR -> (Rn)
    RN -= 4;
    WRITE32(RN, this->regs.GBR);
    PCinc;
}

SH4ins(STCVBR) { // VBR -> Rn
    RN = this->regs.VBR;
    PCinc;
}

SH4ins(STCMVBR) { // Rn-4 -> Rn, VBR -> (Rn)
    RN -= 4;
    WRITE32(RN, this->regs.VBR);
    PCinc;
}

SH4ins(STCSGR) { // SGR -> Rn
    RN = this->regs.SGR;
    PCinc;
}

SH4ins(STCMSGR) { // Rn-4 -> Rn, SGR -> (Rn)
    RN -= 4;
    WRITE32(RN, this->regs.SGR);
    PCinc;
}

SH4ins(STCSSR) { // SSR -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STCMSSR) { // Rn-4 -> Rn, SSR -> (Rn)
    RN -= 4;
    WRITE32(RN, this->regs.SSR);
    PCinc;
}

SH4ins(STCSPC) { // SPC -> Rn
    RN = this->regs.SPC;
    PCinc;
}

SH4ins(STCMSPC) { // Rn-4 -> Rn, SPC -> (Rn)
    RN -= 4;
    WRITE32(RN, this->regs.SPC);
    PCinc;
}

SH4ins(STCDBR) { // DBR -> Rn
    RN = this->regs.DBR;
    PCinc;
}

SH4ins(STCMDBR) { // Rn-4 -> Rn, DBR -> (Rn)
    RN -= 4;
    WRITE32(RN, this->regs.DBR);
    PCinc;
}

SH4ins(STCRm_BANK) { // Rm_BANK -> Rn (m = 0-7)
    RN = this->regs.R_[ins->Rm];
    PCinc;
}

SH4ins(STCMRm_BANK) { // Rn-4 -> Rn, Rm_BANK -> (Rn) (m = 0-7)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STSMACH) { // MACH -> Rn
    RN = this->regs.MACH;
    PCinc;
}

SH4ins(STSMMACH) { // Rn-4 -> Rn, MACH -> (Rn)
    RN -= 4;
    WRITE32(RN, this->regs.MACH);
    PCinc;
}

SH4ins(STSMACL) { // MACL -> Rn
    RN = rMACL;
    PCinc;
}

SH4ins(STSMMACL) { // Rn-4 -> Rn, MACL -> (Rn)
    RN -= 4;
    WRITE32(RN, this->regs.MACL);
    PCinc;
}

SH4ins(STSPR) { // PR -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STSMPR) { // Rn-4 -> Rn, PR -> (Rn)
    RN -= 4;
    WRITE32(RN, this->regs.PR);
    PCinc;
}

SH4ins(TRAPA) { // SH1*,SH2*: PC/SR -> stack area, (imm*4 + VBR) -> PC, SH3*,SH4*: PC/SR -> SPC/SSR, imm*4 -> TRA, 0x160 -> EXPEVT, VBR + 0x0100 -> PC
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FMOV) { // FRm -> FRn
    if (this->regs.FPSCR.SZ == 0) { // 32-bit
        fpFRU(ins->Rn) = fpFRU(ins->Rm);
    }  else BADOPCODE;
    PCinc;
}

SH4ins(FMOV_LOAD) { // (Rm) -> FRn
    if (this->regs.FPSCR.SZ == 0) { // 32-bit
        fpFRU(ins->Rn) = READ32(RM);
    } else BADOPCODE;
    PCinc;
}

SH4ins(FMOV_STORE) { // FRm -> (Rn)
    if (this->regs.FPSCR.SZ == 0) { // 32-bit
        WRITE32(RN, fpFRU(ins->Rm));
    } else BADOPCODE;
    PCinc;
}

SH4ins(FMOV_RESTORE) { // (Rm) -> FRn, Rm+4 -> Rm
    if (this->regs.FPSCR.SZ == 0) {// float
        fpFRU(ins->Rn) = READ32(RM);
        RM += 4;
    }  else {
        printf("\nWAIT WHAT2? %d %d", this->regs.FPSCR.SZ, this->regs.FPSCR.PR);
    }
    PCinc;
}

SH4ins(FMOV_SAVE) { // Rn-4 -> Rn, FRm -> (Rn)
    if (this->regs.FPSCR.SZ == 0) { // 32-bit
        RN -= 4;
        WRITE32(RN, fpFRU(ins->Rm));
    } else {
        printf("\nWAIT WAHT? %d %d", this->regs.FPSCR.SZ, this->regs.FPSCR.PR);
        BADOPCODE;
    }
    PCinc;
}

SH4ins(FMOV_INDEX_LOAD) { // (R0 + Rm) -> FRn
    if (this->regs.FPSCR.SZ == 0) { // 32-bit
        fpFRU(ins->Rn) = READ32(R(0) + RM);
    }  else BADOPCODE;
    PCinc;
}

SH4ins(FMOV_INDEX_STORE) { // FRm -> (R0 + Rn)
    if (this->regs.FPSCR.SZ == 0) { // 32-bit
        WRITE32(R(0) + RN, fpFRU(ins->Rm));
    } else BADOPCODE;
    PCinc;
}

SH4ins(FMOV_DR) { // DRm -> DRn
    fpDR(ins->Rn) = fpDR(ins->Rm);
    PCinc;
}

SH4ins(FMOV_DRXD) { // DRm -> XDn
    fpDR(ins->Rm) = fpXD(ins->Rn);
    PCinc;
}

SH4ins(FMOV_XDDR) { // XDm -> DRn
    fpXD(ins->Rm) = fpDR(ins->Rn);
    PCinc;
}

SH4ins(FMOV_XDXD) { // XDm -> XDn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FMOV_LOAD_DR) { // (Rm) -> DRn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FMOV_LOAD_XD) { // (Rm) -> XDn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FMOV_STORE_DR) { // DRm -> (Rn)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FMOV_STORE_XD) { // XDm -> (Rn)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FMOV_RESTORE_DR) { // (Rm) -> DRn, Rm + 8 -> Rm
    fpDRU(ins->Rn) = READ64(RM);
    RM += 8;
    PCinc;
}

SH4ins(FMOV_RESTORE_XD) { // (Rm) -> XDn, Rm+8 -> Rm
    fpXDU(ins->Rn) = READ64(RM);
    RM += 8;
    PCinc;
}

SH4ins(FMOV_SAVE_DR) { // Rn-8 -> Rn, DRm -> (Rn)
    RN -= 8;
    WRITE64(RN, fpDRU(ins->Rm));
    PCinc;
}

SH4ins(FMOV_SAVE_XD) { // Rn-8 -> Rn, (Rn) -> XDm
    // TODO: some people say XD selects bank based on lower bit of m.
    // some people say it always selects the alternate bank and that bit is ignored
    RN -= 8;
    fpXDU(ins->Rm) = READ64(RN);
    PCinc;
}

SH4ins(FMOV_INDEX_LOAD_DR) { // (R0 + Rm) -> DRn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FMOV_INDEX_LOAD_XD) { // (R0 + Rm) -> XDn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FMOV_INDEX_STORE_DR) { // DRm -> (R0 + Rn)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FMOV_INDEX_STORE_XD) { // XDm -> (R0 + Rn)
    WRITE64(R(0) + RN, fpXDU(ins->Rm));
    PCinc;
}

SH4ins(FLDI0) { // 0x00000000 -> FRn
    this->regs.fb[0].U32[ins->Rn] = 0;
    PCinc;
}

SH4ins(FLDI1) { // 0x3F800000 -> FRn
    this->regs.fb[0].U32[ins->Rn] = 0x3F800000;
    PCinc;
}

SH4ins(FLDS) { // FRm -> FPUL
    this->regs.FPUL.u = this->regs.fb[0].U32[ins->Rm];
    PCinc;
}

SH4ins(FSTS) { // FPUL -> FRn
    fpFR(ins->Rn) = this->regs.FPUL.f;
    PCinc;
}

SH4ins(FABS) { // FRn & 0x7FFFFFFF -> FRn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FNEG) { // FRn ^ 0x80000000 -> FRn
    if (this->regs.FPSCR.PR ==0)
        fpFRU(ins->Rn)^=0x80000000;
    else
        fpFRU(ins->Rn&0xE)^=0x80000000;

    PCinc;
}

SH4ins(FADD) { // FRn + FRm -> FRn
    fpFR(ins->Rn) += fpFR(ins->Rm);
    PCinc;
}

SH4ins(FSUB) { // FRn - FRm -> FRn
    // TODO: more precision
    fpFR(ins->Rn) -= fpFR(ins->Rm);
    PCinc;
}

SH4ins(FMUL) { // FRn * FRm -> FRn
    fpFR(ins->Rn) *= fpFR(ins->Rm);
    PCinc;
}

SH4ins(FMAC) { // FR0 * FRm + FRn -> FRn
    double res = (double)fpFR(ins->Rn) + ((double)fpFR(0) * (double)fpFR(ins->Rm));
    fpFR(ins->Rn) = (float)res;
    PCinc;
}

SH4ins(FDIV) { // FRn / FRm -> FRn
    fpFR(ins->Rn) /= fpFR(ins->Rm);
    PCinc;
}

// Thanks Reicast!
SH4ins(FSQRT) { // sqrt (FRn) -> FRn
    fpFR(ins->Rn) = sqrtf(fpFR(ins->Rn));
    //CHECK_FPU_32(fr[n]);
    PCinc;
}

SH4ins(FCMP_EQ) { // If FRn = FRm: 1 -> T, Else: 0 -> T
    this->regs.SR.T = !!(fpFR(ins->Rm) == fpFR(ins->Rn));
    PCinc;
}

SH4ins(FCMP_GT) { // If FRn > FRm: 1 -> T, Else: 0 -> T
    //TODO: better accuracy
    this->regs.SR.T = fpFR(ins->Rn) > fpFR(ins->Rm);
    PCinc;
}

SH4ins(FLOAT_single) { // (float)FPUL -> FRn
    if (this->regs.FPSCR.PR == 0) { // single precision
        fpFR(ins->Rn) = (float)((i32)this->regs.FPUL.u);
    } else { // double precision
        BADOPCODE;
    }
    PCinc;
}

// Thanks to the amazing website for how to implement this properly
SH4ins(FTRC_single) { // (long)FRm -> FPUL
    // TODO make this much better!
    // x86 overflows oddly, with positive overflow returining INT_MIN
    this->regs.FPUL.u = (int32)fpFR(ins->Rm);
    PCinc;
}
// thanks Reicast
SH4ins(FIPR) { // inner_product (FVm, FVn) -> FR[n+3]
    if (this->regs.FPSCR.PR == 0)
    {
        //clear_cause ();
        i64 n=ins->Rn & 0xC;
        i64 m=(ins->Rn & 0x3) << 2;
        float idp;
        idp  = fpFR(n+0) * fpFR(m+0);
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
    u32 n=ins->Rn&0xC;

    if (this->regs.FPSCR.PR==0)
    {

        float v1, v2, v3, v4;

        v1 = fpXF(0)  * fpFR(n + 0) +
             fpXF(4)  * fpFR(n + 1) +
             fpXF(8)  * fpFR(n + 2) +
             fpXF(12) * fpFR(n + 3);

        v2 = fpXF(1)  * fpFR(n + 0) +
             fpXF(5)  * fpFR(n + 1) +
             fpXF(9)  * fpFR(n + 2) +
             fpXF(13) * fpFR(n + 3);

        v3 = fpXF(2)  * fpFR(n + 0) +
             fpXF(6)  * fpFR(n + 1) +
             fpXF(10) * fpFR(n + 2) +
             fpXF(14) * fpFR(n + 3);

        v4 = fpXF(3)  * fpFR(n + 0) +
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
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FNEGDR) { // DRn ^ 0x8000000000000000 -> DRn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FADDDR) { // DRn + DRm -> DRn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FSUBDR) { // DRn - DRm -> DRn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FMULDR) { // DRn * DRm -> DRn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FDIVDR) { // DRn / DRm -> DRn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FSQRTDR) { // sqrt (DRn) -> DRn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FCMP_EQDR) { // If DRn = DRm: 1 -> T, Else: 0 -> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FCMP_GTDR) { // If DRn > DRm: 1 -> T, Else: 0 -> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FLOAT_double) { // (double)FPUL -> DRn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FTRC_double) { // (long)DRm -> FPUL
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FCNVDS) { // double_to_float (DRm) -> FPUL
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FCNVSD) { // float_to_double (FPUL) -> DRn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(LDSFPSCR) { // Rm -> FPSCR
    SH4_regs_FPSCR_set(&this->regs, RM & 0x003FFFFF);
    PCinc;
}

SH4ins(STSFPSCR) { // FPSCR -> Rn
    RN = SH4_regs_FPSCR_get(&this->regs.FPSCR);
    PCinc;
}

SH4ins(LDSMFPSCR) { // (Rm) -> FPSCR, Rm+4 -> Rm
    SH4_regs_FPSCR_set(&this->regs, READ32(RM) & 0x003FFFFF);
    RM += 4;
    PCinc;
}

SH4ins(STSMFPSCR) { // Rn-4 -> Rn, FPSCR -> (Rn)
    RN -= 4;
    WRITE32(RN, SH4_regs_FPSCR_get(&this->regs.FPSCR));
    PCinc;
}

SH4ins(LDSFPUL) { // Rm -> FPUL
    this->regs.FPUL.u = RM;
    PCinc;
}

SH4ins(STSFPUL) { // FPUL -> Rn
    RN = this->regs.FPUL.u;
    PCinc;
}

SH4ins(LDSMFPUL) { // (Rm) -> FPUL, Rm+4 -> Rm
    this->regs.FPUL.u = READ32(RM);
    RM += 4;
    PCinc;
}

SH4ins(STSMFPUL) { // Rn-4 -> Rn, FPUL -> (Rn)
    RN -= 4;
    WRITE32(RN, this->regs.FPUL.u);
    PCinc;
}

SH4ins(FRCHG) { // If FPSCR.PR = 0: ~FPSCR.FR -> FPSCR.FR, Else: Undefined Operation
    if (this->regs.FPSCR.PR == 0) {
        this->regs.FPSCR.FR ^= 1;
        SH4_regs_FPSCR_bankswitch(&this->regs);
    } else {
        assert(1==0);
    }
    PCinc;
}

SH4ins(FSCHG) { // If FPSCR.PR = 0: ~FPSCR.SZ -> FPSCR.SZ, Else: Undefined Operation
    if (this->regs.FPSCR.PR == 0) {
        this->regs.FPSCR.SZ ^= 1;
    }
    else assert(1==0);

    PCinc;
}

SH4ins(FSRRA) { //1.0 / sqrt (FRn) -> FRn
    //PC += 2;
    //clear_cause();
    if (this->regs.FPSCR.PR != 0) {
        BADOPCODE;
    }
    else {
        //clear_cause();
        float n = fpFR(ins->Rn);

        switch (fpclassify(fpFR(ins->Rn)))
        {
            case FP_NORMAL:
                if (n >= 0)
                {
                    fpFR(ins->Rn) = 1.0f / sqrtf(n);
                }
                else {
                    printf("\nINVALID VALUE FOR FSRRA");
                    //invalid(n);
                }
                break;

            case FP_SUBNORMAL:
                /*if (sign_of (n) == 0)
                    fpu_error ();
                else
                    invalid (n);
                break;*/

            case FP_ZERO:
                //dz (n, sign_of (n));
                break;

            case FP_INFINITE:
                if (n > 0)
                    fpFR(ins->Rn) = 0;
                else
                    break;
                    //invalid (n);
                break;
            case FP_NAN:
                //qnan (n);
                break;

            /*case sNAN:
                //invalid (n);
                break;*/
        }
    }
    PCinc;
}

SH4ins(FSCA) { // sin (FPUL) -> FRn, cos (FPUL) -> FR[n+1]
    // thanks to dRk|Raziel for this! using tables dumped from real DC


    //cosine(x) = sine(pi/2 + x).
    if (this->regs.FPSCR.PR==0)
    {
        //float real_pi=(((float)(s32)fpul)/65536)*(2*pi);
        u32 n = ins->Rn & 0xE;
        u32 pi_index=(u16)this->regs.FPUL.u;

        fpFR(n | 0) = SH4_sin_table[pi_index];//sinf(real_pi);
        fpFR(n | 1) = SH4_sin_table[0x4000 + pi_index];//cosf(real_pi);    // -> no need for warparound, sin_table has 0x4000 more entries

        //CHECK_FPU_32(fr[n]);
        //CHECK_FPU_32(fr[n+1]);
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

/*#define OE(x, ins) opcode = x; sd = &SH4_decoded[opcode]; assert(sd->exec=NULL); sd->exec = &ins
#define OErmrn(x, ins) OE(x, ins); sd->Rm = rm; sd->Rn = rn*/
/*
             opcode = 0b0110000000000011 | (rm << 8) | (rn << 4);
            sd = &SH4_decoded[opcode];
*
 */

struct SH4_ins_t SH4_decoded[4][65536];
char SH4_disassembled[4][65536][30];
char SH4_mnemonic[4][65536][30];

static void process_SH4_instruct(struct sh4_str_ret *r, const char* stri)
{
    memset(r, 0, sizeof(struct sh4_str_ret));

    enum ddd{
        nothing,
        in_d,
        in_n,
        in_m,
        in_i
    } doing, new_doing;

    doing = nothing;
    new_doing = nothing;
    u32 val = 0x10000;

    for (u32 i = 0; i < 16; i++) {
        val >>= 1;
        u8 chr = stri[i];
        u32 *max = NULL;
        u32 *msk = NULL;
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

static void emplace_mnemonic(u32 opcode, const char *mnemonic, u32 n, u32 m, u32 d, u32 imm, u32 szpr)
{
    strcpy(SH4_mnemonic[szpr][opcode], mnemonic);

    char *copy_to = SH4_disassembled[szpr][opcode];
    *copy_to = 0;

    for (char *mn = (char *)mnemonic; *mn!=0; mn++) {
        if ((*mn == 'R') && ((*(mn+1) == 'n') || (*(mn+1) == 'm'))) {
            // Rm or Rn
            copy_to += sprintf(copy_to, "R%d", *(mn+1) == 'n' ? n : m);
            mn++;
            continue;
        }
        if ((*mn == 'd') && (*(mn+1) == 'i') && (*(mn+2) == 's')) {
            // disp
            copy_to += sprintf(copy_to, "%02x", d);
            mn += 3;
            continue;
        }
        if ((*mn == 'i') && (*(mn+1) == 'm' && (*(mn+2) == 'm'))) {
            // imm
            copy_to += sprintf(copy_to, "%d", imm);
            mn += 2;
            continue;
        }
        *copy_to = *mn;
        copy_to++;
    }
    *copy_to = 0;
}

static void cpSH4(u32 dest, u32 src) {
    memcpy(&SH4_decoded[dest], &SH4_decoded[src], 65536*sizeof(struct SH4_ins_t));
    memcpy(&SH4_disassembled[dest][0], &SH4_disassembled[src][0], 65536*30);
    memcpy(&SH4_mnemonic[dest][0], &SH4_mnemonic[src][0], 65535*30);
}

static void iterate_opcodes(struct sh4_str_ret* r, SH4_ins_func ins, const char* mnemonic, u32 override, u32 szpr)
{
    u32 d_times = 1;
    // Go at least once through each of n, m, d, and i
    u32 n_max = r->n_max ? r->n_max : 1;
    u32 m_max = r->m_max ? r->m_max : 1;
    u32 d_max = r->d_max ? r->d_max : 1;
    u32 i_max = r->i_max ? r->i_max : 1;
    for (u32 n = 0; n < n_max; n++) {
        for (u32 m = 0; m < m_max; m++) {
            for (u32 d = 0; d < d_max; d++) {
                for (u32 i = 0; i < i_max; i++) {
                    u32 opcode = 0;
                    if (r->n_max > 0) opcode |= (n << r->n_shift);
                    if (r->m_max > 0) opcode |= (m << r->m_shift);
                    if (r->d_max > 0) opcode |= (d << r->d_shift);
                    if (r->i_max > 0) opcode |= (i << r->i_shift);
                    opcode |= r->mask;

                    struct SH4_ins_t *is = &SH4_decoded[szpr][opcode];
                    if ((!override) && (is->decoded != 0)) { // If no override and already decoded
                        assert(1==0);
                    }
                    else if (override & (is->decoded == 0)) {// If override and NOT already decoded
                        assert(1==0);
                    }
                    is->Rn = n;
                    is->Rm = m;
                    is->imm = i;
                    is->disp = (i32)(d*d_times);
                    is->exec = ins;
                    is->decoded = 1;
                    emplace_mnemonic(opcode, mnemonic, n, m, d, i, szpr);

                    if (szpr > 0) {
                        // copy to szpr=3
                        memcpy(&SH4_decoded[3][opcode], &SH4_decoded[szpr][opcode], sizeof(struct SH4_ins_t));
                        memcpy(&SH4_disassembled[3][opcode][0], &SH4_disassembled[szpr][opcode][0], 30);
                        memcpy(&SH4_mnemonic[3][opcode][0], &SH4_mnemonic[szpr][opcode][0], 30);
                    }
                    //printf("\n%s", SH4_disassembled[0][opcode]);
                }
            }
        }
    }
}

static void decode_and_iterate_opcodes(const char* inpt, SH4_ins_func ins, const char *mnemonic, u32 override, u32 szpr)
{
    struct sh4_str_ret a;
    process_SH4_instruct(&a, inpt);
    iterate_opcodes(&a, ins, mnemonic, override, szpr);
}


#define OE(opcstr, func, mn) decode_and_iterate_opcodes(opcstr, func, mn, 0, 0)
#define OEo(opcstr, func, mn, sz, pr) decode_and_iterate_opcodes(opcstr, func, mn, 1, ((sz<<1) | pr))

void do_sh4_decode() {
    for (u32 szpr = 0; szpr < 4; szpr++) {
        for (u32 i = 0; i < 65536; i++) {
            SH4_decoded[szpr][i] = (struct SH4_ins_t) {
                    .opcode = i,
                    .Rn = -1,
                    .Rm = -1,
                    .imm = 0,
                    .disp = 0,
                    .exec = NULL,
                    .decoded = 0
            };
        }
    }

    u32 r = 0;

    OE("0110nnnnmmmm0011", &SH4_MOV, "mov Rm,Rn"); // Rm -> Rn
    OE("1110nnnniiiiiiii", &SH4_MOVI, "mov #imm,Rn"); // imm -> sign extension -> Rn
    OE("11000111dddddddd", &SH4_MOVA, "mova @(disp,PC),R0"); // (disp*4) + (PC & 0xFFFFFFFC) + 4 -> R0
    OE("1001nnnndddddddd", &SH4_MOVWI, "mov.w @(disp,PC),Rn"); // (disp*2 + PC + 4) -> sign extension -> Rn
    OE("1101nnnndddddddd", &SH4_MOVLI, "mov.l @(disp,PC),Rn"); // (disp*4 + (PC & 0xFFFFFFFC) + 4) -> sign extension -> Rn
    OE("0110nnnnmmmm0000", &SH4_MOVBL, "mov.b @Rm,Rn"); // (Rm) -> sign extension -> Rn
    OE("0110nnnnmmmm0001", &SH4_MOVWL, "mov.w @Rm,Rn"); // (Rm) -> sign extension -> Rn
    OE("0110nnnnmmmm0010", &SH4_MOVLL, "mov.l @Rm,Rn"); // (Rm) -> Rn
    OE("0010nnnnmmmm0000", &SH4_MOVBS, "mov.b Rm,@Rn"); // Rm -> (Rn)
    OE("0010nnnnmmmm0001", &SH4_MOVWS, "mov.w Rm,@Rn"); // Rm -> (Rn)
    OE("0010nnnnmmmm0010", &SH4_MOVLS, "mov.l Rm,@Rn"); // Rm -> (Rn)
    OE("0110nnnnmmmm0100", &SH4_MOVBP, "mov.b @Rm+,Rn"); // (Rm) -> sign extension -> Rn, Rm+1 -> Rm
    OE("0110nnnnmmmm0101", &SH4_MOVWP, "mov.w @Rm+,Rn"); // (Rm) -> sign extension -> Rn, Rm+2 -> Rm
    OE("0110nnnnmmmm0110", &SH4_MOVLP, "mov.l @Rm+,Rn"); // (Rm) -> Rn, Rm+4 -> Rm
    OE("0010nnnnmmmm0100", &SH4_MOVBM, "mov.b Rm,@-Rn"); // Rn-1 -> Rn, Rm -> (Rn)
    OE("0010nnnnmmmm0101", &SH4_MOVWM, "mov.w Rm,@-Rn"); // Rn-2 -> Rn, Rm -> (Rn)
    OE("0010nnnnmmmm0110", &SH4_MOVLM, "mov.l Rm,@-Rn"); // Rn-4 -> Rn, Rm -> (Rn)
    OE("10000100mmmmdddd", &SH4_MOVBL4, "mov.b @(disp,Rm),R0"); // (disp + Rm) -> sign extension -> R0
    OE("10000101mmmmdddd", &SH4_MOVWL4, "mov.w @(disp,Rm),R0"); // (disp*2 + Rm) -> sign extension -> R0
    OE("0101nnnnmmmmdddd", &SH4_MOVLL4, "mov.l @(disp,Rm),Rn"); // (disp*4 + Rm) -> Rn
    OE("10000000nnnndddd", &SH4_MOVBS4, "mov.b R0,@(disp,Rn)"); // R0 -> (disp + Rn)
    OE("10000001nnnndddd", &SH4_MOVWS4, "mov.w R0,@(disp,Rn)"); // R0 -> (disp*2 + Rn)
    OE("0001nnnnmmmmdddd", &SH4_MOVLS4, "mov.l Rm,@(disp,Rn)"); // Rm -> (disp*4 + Rn)
    OE("0000nnnnmmmm1100", &SH4_MOVBL0, "mov.b @(R0,Rm),Rn"); // (R0 + Rm) -> sign extension -> Rn
    OE("0000nnnnmmmm1101", &SH4_MOVWL0, "mov.w @(R0,Rm),Rn"); // (R0 + Rm) -> sign extension -> Rn
    OE("0000nnnnmmmm1110", &SH4_MOVLL0, "mov.l @(R0,Rm),Rn"); // (R0 + Rm) -> Rn
    OE("0000nnnnmmmm0100", &SH4_MOVBS0, "mov.b Rm,@(R0,Rn)"); // Rm -> (R0 + Rn)
    OE("0000nnnnmmmm0101", &SH4_MOVWS0, "mov.w Rm,@(R0,Rn)"); // Rm -> (R0 + Rn)
    OE("0000nnnnmmmm0110", &SH4_MOVLS0, "mov.l Rm,@(R0,Rn)"); // Rm -> (R0 + Rn)
    OE("11000100dddddddd", &SH4_MOVBLG, "mov.b @(disp,GBR),R0"); // (disp + GBR) -> sign extension -> R0
    OE("11000101dddddddd", &SH4_MOVWLG, "mov.w @(disp,GBR),R0"); // (disp*2 + GBR) -> sign extension -> R0
    OE("11000110dddddddd", &SH4_MOVLLG, "mov.l @(disp,GBR),R0"); // (disp*4 + GBR) -> R0
    OE("11000000dddddddd", &SH4_MOVBSG, "mov.b R0,@(disp,GBR)"); // R0 -> (disp + GBR)
    OE("11000001dddddddd", &SH4_MOVWSG, "mov.w R0,@(disp,GBR)"); // R0 -> (disp*2 + GBR)
    OE("11000010dddddddd", &SH4_MOVLSG, "mov.l R0,@(disp,GBR)"); // R0 -> (disp*4 + GBR)
    OE("0000nnnn00101001", &SH4_MOVT, "movt Rn"); // T -> Rn
    OE("0110nnnnmmmm1000", &SH4_SWAPB, "swap.b Rm,Rn"); // Rm -> swap lower 2 bytes -> Rn
    OE("0110nnnnmmmm1001", &SH4_SWAPW, "swap.w Rm,Rn"); // Rm -> swap upper/lower words -> Rn
    OE("0010nnnnmmmm1101", &SH4_XTRCT, "xtrct Rm,Rn"); // Rm:Rn middle 32 bits -> Rn
    OE("0011nnnnmmmm1100", &SH4_ADD, "add Rm,Rn"); // Rn + Rm -> Rn
    OE("0111nnnniiiiiiii", &SH4_ADDI, "add #imm,Rn"); // Rn + (sign extension)imm
    OE("0011nnnnmmmm1110", &SH4_ADDC, "addc Rm,Rn"); // Rn + Rm + T -> Rn, carry -> T
    OE("0011nnnnmmmm1111", &SH4_ADDV, "addv Rm,Rn"); // Rn + Rm -> Rn, overflow -> T
    OE("10001000iiiiiiii", &SH4_CMPIM, "cmp/eq #imm,R0"); // If R0 = (sign extension)imm: 1 -> T, Else: 0 -> T
    OE("0011nnnnmmmm0000", &SH4_CMPEQ, "cmp/eq Rm,Rn"); // If Rn = Rm: 1 -> T, Else: 0 -> T
    OE("0011nnnnmmmm0010", &SH4_CMPHS, "cmp/hs Rm,Rn"); // If Rn >= Rm (unsigned): 1 -> T, Else: 0 -> T
    OE("0011nnnnmmmm0011", &SH4_CMPGE, "cmp/ge Rm,Rn"); // If Rn >= Rm (signed): 1 -> T, Else: 0 -> T
    OE("0011nnnnmmmm0110", &SH4_CMPHI, "cmp/hi Rm,Rn"); // If Rn > Rm (unsigned): 1 -> T, Else: 0 -> T
    OE("0011nnnnmmmm0111", &SH4_CMPGT, "cmp/gt Rm,Rn"); // If Rn > Rm (signed): 1 -> T, Else: 0 -> T
    OE("0100nnnn00010101", &SH4_CMPPL, "cmp/pl Rn"); // If Rn > 0 (signed): 1 -> T, Else: 0 -> T
    OE("0100nnnn00010001", &SH4_CMPPZ, "cmp/pz Rn"); // If Rn >= 0 (signed): 1 -> T, Else: 0 -> T
    OE("0010nnnnmmmm1100", &SH4_CMPSTR, "cmp/str Rm,Rn"); // If Rn and Rm have an equal byte: 1 -> T, Else: 0 -> T
    OE("0010nnnnmmmm0111", &SH4_DIV0S, "div0s Rm,Rn"); // MSB of Rn -> Q, MSB of Rm -> M, M ^ Q -> T
    OE("0000000000011001", &SH4_DIV0U, "div0u"); // 0 -> M, 0 -> Q, 0 -> T
    OE("0011nnnnmmmm0100", &SH4_DIV1, "div1 Rm,Rn"); // 1-step division (Rn / Rm)
    OE("0011nnnnmmmm1101", &SH4_DMULS, "dmuls.l Rm,Rn"); // Signed, Rn * Rm -> MACH:MACL, 32 * 32 -> 64 bits
    OE("0011nnnnmmmm0101", &SH4_DMULU, "dmulu.l Rm,Rn"); // Unsigned, Rn * Rm -> MACH:MACL, 32 * 32 -> 64 bits
    OE("0100nnnn00010000", &SH4_DT, "dt Rn"); // Rn-1 -> Rn, If Rn = 0: 1 -> T, Else: 0 -> T
    OE("0110nnnnmmmm1110", &SH4_EXTSB, "exts.b Rm,Rn"); // Rm sign-extended from byte -> Rn
    OE("0110nnnnmmmm1111", &SH4_EXTSW, "exts.w Rm,Rn"); // Rm sign-extended from word -> Rn
    OE("0110nnnnmmmm1100", &SH4_EXTUB, "extu.b Rm,Rn"); // Rm zero-extended from byte -> Rn
    OE("0110nnnnmmmm1101", &SH4_EXTUW, "extu.w Rm,Rn"); // Rm zero-extended from word -> Rn
    OE("0000nnnnmmmm1111", &SH4_MACL, "mac.l @Rm+,@Rn+"); // Signed, (Rn) * (Rm) + MAC -> MAC, 32 * 32 + 64 -> 64 bits
    OE("0100nnnnmmmm1111", &SH4_MACW, "mac.w @Rm+,@Rn+"); // Signed, (Rn) * (Rm) + MAC -> MAC, SH1: 16 * 16 + 42 -> 42 bits, Other: 16 * 16 + 64 -> 64 bits
    OE("0000nnnnmmmm0111", &SH4_MULL, "mul.l Rm,Rn"); // Rn * Rm -> MACL, 32 * 32 -> 32 bits
    OE("0010nnnnmmmm1111", &SH4_MULS, "muls.w Rm,Rn"); // Signed, Rn * Rm -> MACL, 16 * 16 -> 32 bits
    OE("0010nnnnmmmm1110", &SH4_MULU, "mulu.w Rm,Rn"); // Unsigned, Rn * Rm -> MACL, 16 * 16 -> 32 bits
    OE("0110nnnnmmmm1011", &SH4_NEG, "neg Rm,Rn"); // 0 - Rm -> Rn
    OE("0110nnnnmmmm1010", &SH4_NEGC, "negc Rm,Rn"); // 0 - Rm - T -> Rn, borrow -> T
    OE("0011nnnnmmmm1000", &SH4_SUB, "sub Rm,Rn"); // Rn - Rm -> Rn
    OE("0011nnnnmmmm1010", &SH4_SUBC, "subc Rm,Rn"); // Rn - Rm - T -> Rn, borrow -> T
    OE("0011nnnnmmmm1011", &SH4_SUBV, "subv Rm,Rn"); // Rn - Rm -> Rn, underflow -> T
    OE("0010nnnnmmmm1001", &SH4_AND, "and Rm,Rn"); // Rn & Rm -> Rn
    OE("11001001iiiiiiii", &SH4_ANDI, "and #imm,R0"); // R0 & (zero extend)imm -> R0
    OE("11001101iiiiiiii", &SH4_ANDM, "and.b #imm,@(R0,GBR)"); // (R0 + GBR) & (zero extend)imm -> (R0 + GBR)
    OE("0110nnnnmmmm0111", &SH4_NOT, "not Rm,Rn"); // ~Rm -> Rn
    OE("0010nnnnmmmm1011", &SH4_OR, "or Rm,Rn"); // Rn | Rm -> Rn
    OE("11001011iiiiiiii", &SH4_ORI, "or #imm,R0"); // R0 | (zero extend)imm -> R0
    OE("11001111iiiiiiii", &SH4_ORM, "or.b #imm,@(R0,GBR)"); // (R0 + GBR) | (zero extend)imm -> (R0 + GBR)
    OE("0100nnnn00011011", &SH4_TAS, "tas.b @Rn"); // If (Rn) = 0: 1 -> T, Else: 0 -> T, 1 -> MSB of (Rn)
    OE("0010nnnnmmmm1000", &SH4_TST, "tst Rm,Rn"); // If Rn & Rm = 0: 1 -> T, Else: 0 -> T
    OE("11001000iiiiiiii", &SH4_TSTI, "tst #imm,R0"); // If R0 & (zero extend)imm = 0: 1 -> T, Else: 0 -> T
    OE("11001100iiiiiiii", &SH4_TSTM, "tst.b #imm,@(R0,GBR)"); // If (R0 + GBR) & (zero extend)imm = 0: 1 -> T, Else 0: -> T
    OE("0010nnnnmmmm1010", &SH4_XOR, "xor Rm,Rn"); // Rn ^ Rm -> Rn
    OE("11001010iiiiiiii", &SH4_XORI, "xor #imm,R0"); // R0 ^ (zero extend)imm -> R0
    OE("11001110iiiiiiii", &SH4_XORM, "xor.b #imm,@(R0,GBR)"); // (R0 + GBR) ^ (zero extend)imm -> (R0 + GBR)
    OE("0100nnnn00100100", &SH4_ROTCL, "rotcl Rn"); // T << Rn << T
    OE("0100nnnn00100101", &SH4_ROTCR, "rotcr Rn"); // T >> Rn >> T
    OE("0100nnnn00000100", &SH4_ROTL, "rotl Rn"); // T << Rn << MSB
    OE("0100nnnn00000101", &SH4_ROTR, "rotr Rn"); // LSB >> Rn >> T
    OE("0100nnnnmmmm1100", &SH4_SHAD, "shad Rm,Rn"); // If Rm >= 0: Rn << Rm -> Rn, If Rm < 0: Rn >> |Rm| -> [MSB -> Rn]
    OE("0100nnnn00100000", &SH4_SHAL, "shal Rn"); // T << Rn << 0
    OE("0100nnnn00100001", &SH4_SHAR, "shar Rn"); // MSB >> Rn >> T
    OE("0100nnnnmmmm1101", &SH4_SHLD, "shld Rm,Rn"); // If Rm >= 0: Rn << Rm -> Rn, If Rm < 0: Rn >> |Rm| -> [0 -> Rn]
    OE("0100nnnn00000000", &SH4_SHLL, "shll Rn"); // T << Rn << 0
    OE("0100nnnn00001000", &SH4_SHLL2, "shll2 Rn"); // Rn << 2 -> Rn
    OE("0100nnnn00011000", &SH4_SHLL8, "shll8 Rn"); // Rn << 8 -> Rn
    OE("0100nnnn00101000", &SH4_SHLL16, "shll16 Rn"); // Rn << 16 -> Rn
    OE("0100nnnn00000001", &SH4_SHLR, "shlr Rn"); // 0 >> Rn >> T
    OE("0100nnnn00001001", &SH4_SHLR2, "shlr2 Rn"); // Rn >> 2 -> [0 -> Rn]
    OE("0100nnnn00011001", &SH4_SHLR8, "shlr8 Rn"); // Rn >> 8 -> [0 -> Rn]
    OE("0100nnnn00101001", &SH4_SHLR16, "shlr16 Rn"); // Rn >> 16 -> [0 -> Rn]
    OE("10001011dddddddd", &SH4_BF, "bf disp"); // If T = 0: disp*2 + PC + 4 -> PC, Else: nop
    OE("10001111dddddddd", &SH4_BFS, "bf/s disp"); // If T = 0: disp*2 + PC + 4 -> PC, Else: nop, (Delayed branch)
    OE("10001001dddddddd", &SH4_BT, "bt disp"); // If T = 1: disp*2 + PC + 4 -> PC, Else: nop
    OE("10001101dddddddd", &SH4_BTS, "bt/s disp"); // If T = 1: disp*2 + PC + 4 -> PC, Else: nop, (Delayed branch)
    OE("1010dddddddddddd", &SH4_BRA, "bra disp"); // disp*2 + PC + 4 -> PC, (Delayed branch)
    OE("0000mmmm00100011", &SH4_BRAF, "braf Rm"); // Rm + PC + 4 -> PC, (Delayed branch)
    OE("1011dddddddddddd", &SH4_BSR, "bsr disp"); // PC + 4 -> PR, disp*2 + PC + 4 -> PC, (Delayed branch)
    OE("0000mmmm00000011", &SH4_BSRF, "bsrf Rm"); // PC + 4 -> PR, Rm + PC + 4 -> PC, (Delayed branch)
    OE("0100mmmm00101011", &SH4_JMP, "jmp @Rm"); // Rm -> PC, (Delayed branch)
    OE("0100mmmm00001011", &SH4_JSR, "jsr @Rm"); // PC + 4 -> PR, Rm -> PC, (Delayed branch)
    OE("0000000000001011", &SH4_RTS, "rts"); // PR -> PC, Delayed branch
    OE("0000000000101000", &SH4_CLRMAC, "clrmac"); // 0 -> MACH, 0 -> MACL
    OE("0000000001001000", &SH4_CLRS, "clrs"); // 0 -> S
    OE("0000000000001000", &SH4_CLRT, "clrt"); // 0 -> T
    OE("0100mmmm00001110", &SH4_LDCSR, "ldc Rm,SR"); // Rm -> SR
    OE("0100mmmm00000111", &SH4_LDCMSR, "ldc.l @Rm+,SR"); // (Rm) -> SR, Rm+4 -> Rm
    OE("0100mmmm00011110", &SH4_LDCGBR, "ldc Rm,GBR"); // Rm -> GBR
    OE("0100mmmm00010111", &SH4_LDCMGBR, "ldc.l @Rm+,GBR"); // (Rm) -> GBR, Rm+4 -> Rm
    OE("0100mmmm00101110", &SH4_LDCVBR, "ldc Rm,VBR"); // Rm -> VBR
    OE("0100mmmm00100111", &SH4_LDCMVBR, "ldc.l @Rm+,VBR"); // (Rm) -> VBR, Rm+4 -> Rm
    OE("0100mmmm00111110", &SH4_LDCSSR, "ldc Rm,SSR"); // Rm -> SSR
    OE("0100mmmm00110111", &SH4_LDCMSSR, "ldc.l @Rm+,SSR"); // (Rm) -> SSR, Rm+4 -> Rm
    OE("0100mmmm01001110", &SH4_LDCSPC, "ldc Rm,SPC"); // Rm -> SPC
    OE("0100mmmm01000111", &SH4_LDCMSPC, "ldc.l @Rm+,SPC"); // (Rm) -> SPC, Rm+4 -> Rm
    OE("0100mmmm11111010", &SH4_LDCDBR, "ldc Rm,DBR"); // Rm -> DBR
    OE("0100mmmm11110110", &SH4_LDCMDBR, "ldc.l @Rm+,DBR"); // (Rm) -> DBR, Rm+4 -> Rm
    OE("0100mmmm1nnn1110", &SH4_LDCRn_BANK, "ldc Rm,Rn_BANK"); // Rm -> Rn_BANK (n = 0-7)
    OE("0100mmmm1nnn0111", &SH4_LDCMRn_BANK, "ldc.l @Rm+,Rn_BANK"); // (Rm) -> Rn_BANK, Rm+4 -> Rm
    OE("0100mmmm00001010", &SH4_LDSMACH, "lds Rm,MACH"); // Rm -> MACH
    OE("0100mmmm00000110", &SH4_LDSMMACH, "lds.l @Rm+,MACH"); // (Rm) -> MACH, Rm+4 -> Rm
    OE("0100mmmm00011010", &SH4_LDSMACL, "lds Rm,MACL"); // Rm -> MACL
    OE("0100mmmm00010110", &SH4_LDSMMACL, "lds.l @Rm+,MACL"); // (Rm) -> MACL, Rm+4 -> Rm
    OE("0100mmmm00101010", &SH4_LDSPR, "lds Rm,PR"); // Rm -> PR
    OE("0100mmmm00100110", &SH4_LDSMPR, "lds.l @Rm+,PR"); // (Rm) -> PR, Rm+4 -> Rm
    OE("0000000000111000", &SH4_LDTLB, "ldtlb"); // PTEH/PTEL -> TLB
    OE("0000nnnn11000011", &SH4_MOVCAL, "movca.l R0,@Rn"); // R0 -> (Rn) (without fetching cache block)
    OE("0000000000001001", &SH4_NOP, "nop"); // No operation
    OE("0000nnnn10010011", &SH4_OCBI, "ocbi @Rn"); // Invalidate operand cache block
    OE("0000nnnn10100011", &SH4_OCBP, "ocbp @Rn"); // Write back and invalidate operand cache block
    OE("0000nnnn10110011", &SH4_OCBWB, "ocbwb @Rn"); // Write back operand cache block
    OE("0000nnnn10000011", &SH4_PREF, "pref @Rn"); // (Rn) -> operand cache
    OE("0000000000101011", &SH4_RTE, "rte"); // Delayed branch, SH1*,SH2*: stack area -> PC/SR, SH3*,SH4*: SSR/SPC -> SR/PC
    OE("0000000001011000", &SH4_SETS, "sets"); // 1 -> S
    OE("0000000000011000", &SH4_SETT, "sett"); // 1 -> T
    OE("0000000000011011", &SH4_SLEEP, "sleep"); // Sleep or standby
    OE("0000nnnn00000010", &SH4_STCSR, "stc SR,Rn"); // SR -> Rn
    OE("0100nnnn00000011", &SH4_STCMSR, "stc.l SR,@-Rn"); // Rn-4 -> Rn, SR -> (Rn)
    OE("0000nnnn00010010", &SH4_STCGBR, "stc GBR,Rn"); // GBR -> Rn
    OE("0100nnnn00010011", &SH4_STCMGBR, "stc.l GBR,@-Rn"); // Rn-4 -> Rn, GBR -> (Rn)
    OE("0000nnnn00100010", &SH4_STCVBR, "stc VBR,Rn"); // VBR -> Rn
    OE("0100nnnn00100011", &SH4_STCMVBR, "stc.l VBR,@-Rn"); // Rn-4 -> Rn, VBR -> (Rn)
    OE("0000nnnn00111010", &SH4_STCSGR, "stc SGR,Rn"); // SGR -> Rn
    OE("0100nnnn00110010", &SH4_STCMSGR, "stc.l SGR,@-Rn"); // Rn-4 -> Rn, SGR -> (Rn)
    OE("0000nnnn00110010", &SH4_STCSSR, "stc SSR,Rn"); // SSR -> Rn
    OE("0100nnnn00110011", &SH4_STCMSSR, "stc.l SSR,@-Rn"); // Rn-4 -> Rn, SSR -> (Rn)
    OE("0000nnnn01000010", &SH4_STCSPC, "stc SPC,Rn"); // SPC -> Rn
    OE("0100nnnn01000011", &SH4_STCMSPC, "stc.l SPC,@-Rn"); // Rn-4 -> Rn, SPC -> (Rn)
    OE("0000nnnn11111010", &SH4_STCDBR, "stc DBR,Rn"); // DBR -> Rn
    OE("0100nnnn11110010", &SH4_STCMDBR, "stc.l DBR,@-Rn"); // Rn-4 -> Rn, DBR -> (Rn)
    OE("0000nnnn1mmm0010", &SH4_STCRm_BANK, "stc Rm_BANK,Rn"); // Rm_BANK -> Rn (m = 0-7)
    OE("0100nnnn1mmm0011", &SH4_STCMRm_BANK, "stc.l Rm_BANK,@-Rn"); // Rn-4 -> Rn, Rm_BANK -> (Rn) (m = 0-7)
    OE("0000nnnn00001010", &SH4_STSMACH, "sts MACH,Rn"); // MACH -> Rn
    OE("0100nnnn00000010", &SH4_STSMMACH, "sts.l MACH,@-Rn"); // Rn-4 -> Rn, MACH -> (Rn)
    OE("0000nnnn00011010", &SH4_STSMACL, "sts MACL,Rn"); // MACL -> Rn
    OE("0100nnnn00010010", &SH4_STSMMACL, "sts.l MACL,@-Rn"); // Rn-4 -> Rn, MACL -> (Rn)
    OE("0000nnnn00101010", &SH4_STSPR, "sts PR,Rn"); // PR -> Rn
    OE("0100nnnn00100010", &SH4_STSMPR, "sts.l PR,@-Rn"); // Rn-4 -> Rn, PR -> (Rn)
    OE("11000011iiiiiiii", &SH4_TRAPA, "trapa #imm"); // SH1*,SH2*: PC/SR -> stack area, (imm*4 + VBR) -> PC, SH3*,SH4*: PC/SR -> SPC/SSR, imm*4 -> TRA, 0x160 -> EXPEVT, VBR + 0x0100 -> PC
    OE("1111nnnnmmmm1100", &SH4_FMOV, "fmov FRm,FRn"); // FRm -> FRn
    OE("1111nnnnmmmm1000", &SH4_FMOV_LOAD, "fmov.s @Rm,FRn"); // (Rm) -> FRn
    OE("1111nnnnmmmm1010", &SH4_FMOV_STORE, "fmov.s FRm,@Rn"); // FRm -> (Rn)
    OE("1111nnnnmmmm1001", &SH4_FMOV_RESTORE, "fmov.s @Rm+,FRn"); // (Rm) -> FRn, Rm+4 -> Rm
    OE("1111nnnnmmmm1011", &SH4_FMOV_SAVE, "fmov.s FRm,@-Rn"); // Rn-4 -> Rn, FRm -> (Rn)
    OE("1111nnnnmmmm0110", &SH4_FMOV_INDEX_LOAD, "fmov.s @(R0,Rm),FRn"); // (R0 + Rm) -> FRn
    OE("1111nnnnmmmm0111", &SH4_FMOV_INDEX_STORE, "fmov.s FRm,@(R0,Rn)"); // FRm -> (R0 + Rn)
    OE("1111nnnn10001101", &SH4_FLDI0, "fldi0 FRn"); // 0x00000000 -> FRn
    OE("1111nnnn10011101", &SH4_FLDI1, "fldi1 FRn"); // 0x3F800000 -> FRn
    OE("1111mmmm00011101", &SH4_FLDS, "flds FRm,FPUL"); // FRm -> FPUL
    OE("1111nnnn00001101", &SH4_FSTS, "fsts FPUL,FRn"); // FPUL -> FRn
    OE("1111nnnn01011101", &SH4_FABS, "fabs FRn"); // FRn & 0x7FFFFFFF -> FRn
    OE("1111nnnn01001101", &SH4_FNEG, "fneg FRn"); // FRn ^ 0x80000000 -> FRn
    OE("1111nnnnmmmm0000", &SH4_FADD, "fadd FRm,FRn"); // FRn + FRm -> FRn
    OE("1111nnnnmmmm0001", &SH4_FSUB, "fsub FRm,FRn"); // FRn - FRm -> FRn
    OE("1111nnnnmmmm0010", &SH4_FMUL, "fmul FRm,FRn"); // FRn * FRm -> FRn
    OE("1111nnnnmmmm1110", &SH4_FMAC, "fmac FR0,FRm,FRn"); // FR0 * FRm + FRn -> FRn
    OE("1111nnnnmmmm0011", &SH4_FDIV, "fdiv FRm,FRn"); // FRn / FRm -> FRn
    OE("1111nnnn01101101", &SH4_FSQRT, "fsqrt FRn"); // sqrt (FRn) -> FRn
    OE("1111nnnnmmmm0100", &SH4_FCMP_EQ, "fcmp/eq FRm,FRn"); // If FRn = FRm: 1 -> T, Else: 0 -> T
    OE("1111nnnnmmmm0101", &SH4_FCMP_GT, "fcmp/gt FRm,FRn"); // If FRn > FRm: 1 -> T, Else: 0 -> T
    OE("1111nnnn00101101", &SH4_FLOAT_single, "float FPUL,FRn"); // (float)FPUL -> FRn
    OE("1111mmmm00111101", &SH4_FTRC_single, "ftrc FRm,FPUL"); // (long)FRm -> FPUL
    OE("1111nnmm11101101", &SH4_FIPR, "fipr FVm,FVn"); // inner_product (FVm, FVn) -> FR[n+3]
    OE("1111nn0111111101", &SH4_FTRV, "ftrv XMTRX,FVn"); // transform_vector (XMTRX, FVn) -> FVn
    OE("1111mmm010111101", &SH4_FCNVDS, "fcnvds DRm,FPUL"); // double_to_float (DRm) -> FPUL
    OE("1111nnn010101101", &SH4_FCNVSD, "fcnvsd FPUL,DRn"); // float_to_double (FPUL) -> DRn
    OE("0100mmmm01101010", &SH4_LDSFPSCR, "lds Rm,FPSCR"); // Rm -> FPSCR
    OE("0000nnnn01101010", &SH4_STSFPSCR, "sts FPSCR,Rn"); // FPSCR -> Rn
    OE("0100mmmm01100110", &SH4_LDSMFPSCR, "lds.l @Rm+,FPSCR"); // (Rm) -> FPSCR, Rm+4 -> Rm
    OE("0100nnnn01100010", &SH4_STSMFPSCR, "sts.l FPSCR,@-Rn"); // Rn-4 -> Rn, FPSCR -> (Rn)
    OE("0100mmmm01011010", &SH4_LDSFPUL, "lds Rm,FPUL"); // Rm -> FPUL
    OE("0000nnnn01011010", &SH4_STSFPUL, "sts FPUL,Rn"); // FPUL -> Rn
    OE("0100mmmm01010110", &SH4_LDSMFPUL, "lds.l @Rm+,FPUL"); // (Rm) -> FPUL, Rm+4 -> Rm
    OE("0100nnnn01010010", &SH4_STSMFPUL, "sts.l FPUL,@-Rn"); // Rn-4 -> Rn, FPUL -> (Rn)
    OE("1111101111111101", &SH4_FRCHG, "frchg"); // If FPSCR.PR = 0: ~FPSCR.FR -> FPSCR.FR, Else: Undefined Operation
    OE("1111001111111101", &SH4_FSCHG, "fschg"); // If FPSCR.PR = 0: ~FPSCR.SZ -> FPSCR.SZ, Else: Undefined Operation
    // fsrra and fsca are special to this particlar sh4 IIRC
    OE("1111nnnn01111101", &SH4_FSRRA, "fsrra");
    OE("1111nnn011111101", &SH4_FSCA, "fsca");

    // Now copy all SZ=0 PR=0 instructions to SZ=0 PR=1, SZ=1 PR=0, and SZ=1 PR=1
    for (u32 szpr = 1; szpr < 4; szpr++) {
        cpSH4(szpr, 0);
    }

    // do PR=0 SZ=1, copy to PR=1 SZ=1
    OEo("1111nnn0mmm01100", &SH4_FMOV_DR, "fmov DRm,DRn", 1, 0); // DRm -> DRn
    OEo("1111nnn1mmm01100", &SH4_FMOV_DRXD, "fmov DRm,XDn", 1, 0); // DRm -> XDn
    OEo("1111nnn0mmm11100", &SH4_FMOV_XDDR, "fmov XDm,DRn", 1, 0); // XDm -> DRn
    OEo("1111nnn1mmm11100", &SH4_FMOV_XDXD, "fmov XDm,XDn", 1, 0); // XDm -> XDn
    OEo("1111nnn0mmmm1000", &SH4_FMOV_LOAD_DR, "fmov.d @Rm,DRn", 1, 0); // (Rm) -> DRn
    OEo("1111nnn1mmmm1000", &SH4_FMOV_LOAD_XD, "fmov.d @Rm,XDn", 1, 0); // (Rm) -> XDn
    OEo("1111nnnnmmm01010", &SH4_FMOV_STORE_DR, "fmov.d DRm,@Rn", 1, 0); // DRm -> (Rn)
    OEo("1111nnnnmmm11010", &SH4_FMOV_STORE_XD, "fmov.d XDm,@Rn", 1, 0); // XDm -> (Rn)
    OEo("1111nnn0mmmm1001", &SH4_FMOV_RESTORE_DR, "fmov.d @Rm+,DRn", 1, 0); // (Rm) -> DRn, Rm + 8 -> Rm
    OEo("1111nnn1mmmm1001", &SH4_FMOV_RESTORE_XD, "fmov.d @Rm+,XDn", 1, 0); // (Rm) -> XDn, Rm+8 -> Rm
    OEo("1111nnnnmmm01011", &SH4_FMOV_SAVE_DR, "fmov.d DRm,@-Rn", 1, 0); // Rn-8 -> Rn, DRm -> (Rn)
    OEo("1111nnnnmmm11011", &SH4_FMOV_SAVE_XD, "fmov.d XDm,@-Rn", 1, 0); // Rn-8 -> Rn, (Rn) -> XDm
    OEo("1111nnn0mmmm0110", &SH4_FMOV_INDEX_LOAD_DR, "fmov.d @(R0,Rm),DRn", 1, 0); // (R0 + Rm) -> DRn
    OEo("1111nnn1mmmm0110", &SH4_FMOV_INDEX_LOAD_XD, "fmov.d @(R0,Rm),XDn", 1, 0); // (R0 + Rm) -> XDn
    OEo("1111nnnnmmm00111", &SH4_FMOV_INDEX_STORE_DR, "fmov.d DRm,@(R0,Rn)", 1, 0); // DRm -> (R0 + Rn)
    OEo("1111nnnnmmm10111", &SH4_FMOV_INDEX_STORE_XD, "fmov.d XDm,@(R0,Rn)", 1, 0); // XDm -> (R0 + Rn)

    // do PR=1 SZ=0, copy to PR=1 SZ=1
    OEo("1111nnn001011101", &SH4_FABSDR, "fabs DRn", 0, 1); // DRn & 0x7FFFFFFFFFFFFFFF -> DRn
    OEo("1111nnn001001101", &SH4_FNEGDR, "fneg DRn", 0, 1); // DRn ^ 0x8000000000000000 -> DRn
    OEo("1111nnn0mmm00000", &SH4_FADDDR, "fadd DRm,DRn", 0, 1); // DRn + DRm -> DRn
    OEo("1111nnn0mmm00001", &SH4_FSUBDR, "fsub DRm,DRn", 0, 1); // DRn - DRm -> DRn
    OEo("1111nnn0mmm00010", &SH4_FMULDR, "fmul DRm,DRn", 0, 1); // DRn * DRm -> DRn
    OEo("1111nnn0mmm00011", &SH4_FDIVDR, "fdiv DRm,DRn", 0, 1); // DRn / DRm -> DRn
    OEo("1111nnn001101101", &SH4_FSQRTDR, "fsqrt DRn", 0, 1); // sqrt (DRn) -> DRn
    OEo("1111nnn0mmm00100", &SH4_FCMP_EQDR, "fcmp/eq DRm,DRn", 0, 1); // If DRn = DRm: 1 -> T, Else: 0 -> T
    OEo("1111nnn0mmm00101", &SH4_FCMP_GTDR, "fcmp/gt DRm,DRn", 0, 1); // If DRn > DRm: 1 -> T, Else: 0 -> T
    OEo("1111nnn000101101", &SH4_FLOAT_double, "float FPUL,DRn", 0, 1); // (double)FPUL -> DRn
    OEo("1111mmm000111101", &SH4_FTRC_double, "ftrc DRm,FPUL", 0, 1); // (long)DRm -> FPUL


    u32 unencoded = 0;
    for (u32 szpr = 0; szpr < 4; szpr++) {
        for (u32 i = 0; i < 65536; i++) {
            if (SH4_decoded[szpr][i].decoded == 0) {
                SH4_decoded[szpr][i].exec = &SH4_EMPTY;
                sprintf(SH4_disassembled[szpr][i], "UNKNOWN OPCODE %04x", i);
                sprintf(SH4_mnemonic[szpr][i], "UNKNOWN OPCODE %04x", i);
                unencoded++;
            }
        }
    }
}