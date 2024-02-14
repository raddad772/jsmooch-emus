//
// Created by Dave on 2/10/2024.
//

#include "string.h"
#include "assert.h"
#include "stdio.h"

#include "sh4_interpreter.h"
#include "sh4_interpreter_opcodes.h"

#define SH4args  struct SH4* this, struct SH4_ins_t *ins
#define PCinc mPC += 2
#define mPC this->regs.PC

// Sign extend 32 bits
#define SIGNe8to32(x) (((x >> 7) * 0xFFFFFF00) | (x & 0xFF))
#define SIGNe16to32(x) (((x >> 15) * 0xFFFF0000) | (x & 0xFFFF))

#define R(x) this->regs.R[x]
#define RM R(ins->Rm)
#define RN R(ins->Rn)
#define IMM ins->imm
#define DISP ins->disp

#define BADOPCODE printf("\nUNIMPLEMENTED INSTRUCTION %s", __func__); fflush(stdout)

#define READ8(addr) this->read8(this->mptr, addr)
#define READ16(addr) this->read16(this->mptr, addr)
#define READ32(addr) this->read32(this->mptr, addr)
#define WRITE8(addr, val) this->write8(this->mptr, addr, val)
#define WRITE16(addr, val) this->write16(this->mptr, addr, val)
#define WRITE32(addr, val) this->write32(this->mptr, addr, val)

#define SH4ins(x) void SH4_##x(SH4args)

SH4ins(EMPTY) {
    printf("UNKNOWN OPCODE EXECUTION ATTEMPTED: %04x", ins->opcode);
    PCinc;
}

SH4ins(MOV) {
    RN = READ32(RM);
    PCinc;
}

SH4ins(MOVI) {
    RN = SIGNe8to32(IMM);
    PCinc;
}

SH4ins(MOVA) {
    this->regs.R[0] = (mPC & 0xFFFFFFFC) + (DISP*4) + 4;
    PCinc;
}

SH4ins(MOVWI) {
    u32 addr = mPC + (DISP*2) + 4;
    u32 val = READ16(addr);
    RN = SIGNe16to32(val);
    PCinc;
}

SH4ins(MOVLI) {
    u32 addr = (mPC & 0xFFFFFFFC) + (DISP*4) + 4;
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
    RN = RM;
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
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(MOVBL0) { // (R0 + Rm) -> sign extension -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(MOVWL0) { // (R0 + Rm) -> sign extension -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(MOVLL0) { // (R0 + Rm) -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(MOVBS0) { // Rm -> (R0 + Rn)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(MOVWS0) { // Rm -> (R0 + Rn)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(MOVLS0) { // Rm -> (R0 + Rn)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(MOVBLG) { // (disp + GBR) -> sign extension -> R0
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(MOVWLG) { // (disp*2 + GBR) -> sign extension -> R0
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(MOVLLG) { // (disp*4 + GBR) -> R0
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(MOVBSG) { // R0 -> (disp + GBR)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(MOVWSG) { // R0 -> (disp*2 + GBR)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(MOVLSG) { // R0 -> (disp*4 + GBR)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(MOVT) { // T -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(SWAPB) { // Rm -> swap lower 2 bytes -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(SWAPW) { // Rm -> swap upper/lower words -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(XTRCT) { // Rm:Rn middle 32 bits -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(ADD) { // Rn + Rm -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(ADDI) { // Rn + (sign extension)imm
    u32 val = IMM;
    printf("\nR%d:%08x val:%08x", ins->Rn, this->regs.R[0], IMM);
    RN += SIGNe8to32(val);
    PCinc;
}

SH4ins(ADDC) { // Rn + Rm + T -> Rn, carry -> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(ADDV) { // Rn + Rm -> Rn, overflow -> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(CMPIM) { // If R0 = (sign extension)imm: 1 -> T, Else: 0 -> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(CMPEQ) { // If Rn = Rm: 1 -> T, Else: 0 -> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(CMPHS) { // If Rn >= Rm (unsigned): 1 -> T, Else: 0 -> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(CMPGE) { // If Rn >= Rm (signed): 1 -> T, Else: 0 -> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(CMPHI) { // If Rn > Rm (unsigned): 1 -> T, Else: 0 -> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(CMPGT) { // If Rn > Rm (signed): 1 -> T, Else: 0 -> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(CMPPL) { // If Rn > 0 (signed): 1 -> T, Else: 0 -> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(CMPPZ) { // If Rn >= 0 (signed): 1 -> T, Else: 0 -> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(CMPSTR) { // If Rn and Rm have an equal byte: 1 -> T, Else: 0 -> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(DIV0S) { // MSB of Rn -> Q, MSB of Rm -> M, M ^ Q -> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(DIV0U) { // 0 -> M, 0 -> Q, 0 -> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(DIV1) { // 1-step division (Rn / Rm)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(DMULS) { // Signed, Rn * Rm -> MACH:MACL, 32 * 32 -> 64 bits
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(DMULU) { // Unsigned, Rn * Rm -> MACH:MACL, 32 * 32 -> 64 bits
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(DT) { // Rn-1 -> Rn, If Rn = 0: 1 -> T, Else: 0 -> T
    RN--;
    this->regs.SR.T = RN == 0;
    PCinc;
}

SH4ins(EXTSB) { // Rm sign-extended from byte -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(EXTSW) { // Rm sign-extended from word -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(EXTUB) { // Rm zero-extended from byte -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(EXTUW) { // Rm zero-extended from word -> Rn
    BADOPCODE; // Crash on unimplemented opcode
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
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(MULS) { // Signed, Rn * Rm -> MACL, 16 * 16 -> 32 bits
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(MULU) { // Unsigned, Rn * Rm -> MACL, 16 * 16 -> 32 bits
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(NEG) { // 0 - Rm -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(NEGC) { // 0 - Rm - T -> Rn, borrow -> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(SUB) { // Rn - Rm -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(SUBC) { // Rn - Rm - T -> Rn, borrow -> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(SUBV) { // Rn - Rm -> Rn, underflow -> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(AND) { // Rn & Rm -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(ANDI) { // R0 & (zero extend)imm -> R0
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(ANDM) { // (R0 + GBR) & (zero extend)imm -> (R0 + GBR)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(NOT) { // ~Rm -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(OR) { // Rn | Rm -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(ORI) { // R0 | (zero extend)imm -> R0
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(ORM) { // (R0 + GBR) | (zero extend)imm -> (R0 + GBR)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(TAS) { // If (Rn) = 0: 1 -> T, Else: 0 -> T, 1 -> MSB of (Rn)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(TST) { // If Rn & Rm = 0: 1 -> T, Else: 0 -> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(TSTI) { // If R0 & (zero extend)imm = 0: 1 -> T, Else: 0 -> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(TSTM) { // If (R0 + GBR) & (zero extend)imm = 0: 1 -> T, Else 0: -> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(XOR) { // Rn ^ Rm -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(XORI) { // R0 ^ (zero extend)imm -> R0
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(XORM) { // (R0 + GBR) ^ (zero extend)imm -> (R0 + GBR)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(ROTCL) { // T << Rn << T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(ROTCR) { // T >> Rn >> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(ROTL) { // T << Rn << MSB
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(ROTR) { // LSB >> Rn >> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(SHAD) { // If Rm >= 0: Rn << Rm -> Rn, If Rm < 0: Rn >> |Rm| -> [MSB -> Rn]
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(SHAL) { // T << Rn << 0
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(SHAR) { // MSB >> Rn >> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(SHLD) { // If Rm >= 0: Rn << Rm -> Rn, If Rm < 0: Rn >> |Rm| -> [0 -> Rn]
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(SHLL) { // T << Rn << 0
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(SHLL2) { // Rn << 2 -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(SHLL8) { // Rn << 8 -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(SHLL16) { // Rn << 16 -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(SHLR) { // 0 >> Rn >> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(SHLR2) { // Rn >> 2 -> [0 -> Rn]
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(SHLR8) { // Rn >> 8 -> [0 -> Rn]
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(SHLR16) { // Rn >> 16 -> [0 -> Rn]
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(BF) { // If T = 0: disp*2 + PC + 4 -> PC, Else: nop
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

#define DELAY_SLOT(x) this->delay_slot = (x);

SH4ins(BFS) { // If T = 0: disp*2 + PC + 4 -> PC, Else: nop, (Delayed branch)
    u32 val = DISP;
    val = (2 * SIGNe8to32(val)) - 2;
    DELAY_SLOT(mPC+2);
    mPC = mPC + 4 + (val * (this->regs.SR.T ^ 1)); // DISP*2 * !this->regs.SR.T
}

SH4ins(BT) { // If T = 1: disp*2 + mPC + 4 -> mPC, Else: nop
    BADOPCODE;
}

SH4ins(BTS) { // If T = 1: disp*2 + PC + 4 -> PC, Else: nop, (Delayed branch)
    u32 val = DISP;
    val = (2 * SIGNe8to32(val)) - 2;
    DELAY_SLOT(mPC+2);
    mPC = mPC + 4 + (val * this->regs.SR.T); // DISP*2 * !this->regs.SR.T
}

SH4ins(BRA) { // disp*2 + PC + 4 -> PC, (Delayed branch)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(BRAF) { // Rm + PC + 4 -> PC, (Delayed branch)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(BSR) { // PC + 4 -> PR, disp*2 + PC + 4 -> PC, (Delayed branch)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(BSRF) { // PC + 4 -> PR, Rm + PC + 4 -> PC, (Delayed branch)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(JMP) { // Rm -> PC, (Delayed branch)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(JSR) { // PC + 4 -> PR, Rm -> PC, (Delayed branch)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(RTS) { // PR -> PC, Delayed branch
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
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
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(LDCMSR) { // (Rm) -> SR, Rm+4 -> Rm
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(LDCGBR) { // Rm -> GBR
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(LDCMGBR) { // (Rm) -> GBR, Rm+4 -> Rm
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(LDCVBR) { // Rm -> VBR
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(LDCMVBR) { // (Rm) -> VBR, Rm+4 -> Rm
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(LDCSSR) { // Rm -> SSR
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(LDCMSSR) { // (Rm) -> SSR, Rm+4 -> Rm
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(LDCSPC) { // Rm -> SPC
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(LDCMSPC) { // (Rm) -> SPC, Rm+4 -> Rm
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(LDCDBR) { // Rm -> DBR
    BADOPCODE; // Crash on unimplemented opcode
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
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(LDSMMACH) { // (Rm) -> MACH, Rm+4 -> Rm
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(LDSMACL) { // Rm -> MACL
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(LDSMMACL) { // (Rm) -> MACL, Rm+4 -> Rm
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(LDSPR) { // Rm -> PR
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(LDSMPR) { // (Rm) -> PR, Rm+4 -> Rm
    BADOPCODE; // Crash on unimplemented opcode
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
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(OCBI) { // Invalidate operand cache block
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(OCBP) { // Write back and invalidate operand cache block
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(OCBWB) { // Write back operand cache block
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(PREF) { // (Rn) -> operand cache
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(RTE) { // Delayed branch, SH1*,SH2*: stack area -> PC/SR, SH3*,SH4*: SSR/SPC -> SR/PC
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(SETS) { // 1 -> S
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(SETT) { // 1 -> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(SLEEP) { // Sleep or standby
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STCSR) { // SR -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STCMSR) { // Rn-4 -> Rn, SR -> (Rn)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STCGBR) { // GBR -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STCMGBR) { // Rn-4 -> Rn, GBR -> (Rn)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STCVBR) { // VBR -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STCMVBR) { // Rn-4 -> Rn, VBR -> (Rn)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STCSGR) { // SGR -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STCMSGR) { // Rn-4 -> Rn, SGR -> (Rn)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STCSSR) { // SSR -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STCMSSR) { // Rn-4 -> Rn, SSR -> (Rn)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STCSPC) { // SPC -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STCMSPC) { // Rn-4 -> Rn, SPC -> (Rn)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STCDBR) { // DBR -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STCMDBR) { // Rn-4 -> Rn, DBR -> (Rn)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STCRm_BANK) { // Rm_BANK -> Rn (m = 0-7)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STCMRm_BANK) { // Rn-4 -> Rn, Rm_BANK -> (Rn) (m = 0-7)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STSMACH) { // MACH -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STSMMACH) { // Rn-4 -> Rn, MACH -> (Rn)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STSMACL) { // MACL -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STSMMACL) { // Rn-4 -> Rn, MACL -> (Rn)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STSPR) { // PR -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STSMPR) { // Rn-4 -> Rn, PR -> (Rn)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(TRAPA) { // SH1*,SH2*: PC/SR -> stack area, (imm*4 + VBR) -> PC, SH3*,SH4*: PC/SR -> SPC/SSR, imm*4 -> TRA, 0x160 -> EXPEVT, VBR + 0x0100 -> PC
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FMOV) { // FRm -> FRn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FMOV_LOAD) { // (Rm) -> FRn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FMOV_STORE) { // FRm -> (Rn)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FMOV_RESTORE) { // (Rm) -> FRn, Rm+4 -> Rm
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FMOV_SAVE) { // Rn-4 -> Rn, FRm -> (Rn)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FMOV_INDEX_LOAD) { // (R0 + Rm) -> FRn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FMOV_INDEX_STORE) { // FRm -> (R0 + Rn)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FMOV_DR) { // DRm -> DRn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FMOV_DRXD) { // DRm -> XDn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FMOV_XDDR) { // XDm -> DRn
    BADOPCODE; // Crash on unimplemented opcode
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
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FMOV_RESTORE_XD) { // (Rm) -> XDn, Rm+8 -> Rm
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FMOV_SAVE_DR) { // Rn-8 -> Rn, DRm -> (Rn)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FMOV_SAVE_XD) { // Rn-8 -> Rn, (Rn) -> XDm
    BADOPCODE; // Crash on unimplemented opcode
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
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FLDI0) { // 0x00000000 -> FRn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FLDI1) { // 0x3F800000 -> FRn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FLDS) { // FRm -> FPUL
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FSTS) { // FPUL -> FRn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FABS) { // FRn & 0x7FFFFFFF -> FRn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FNEG) { // FRn ^ 0x80000000 -> FRn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FADD) { // FRn + FRm -> FRn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FSUB) { // FRn - FRm -> FRn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FMUL) { // FRn * FRm -> FRn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FMAC) { // FR0 * FRm + FRn -> FRn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FDIV) { // FRn / FRm -> FRn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FSQRT) { // sqrt (FRn) -> FRn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FCMP_EQ) { // If FRn = FRm: 1 -> T, Else: 0 -> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FCMP_GT) { // If FRn > FRm: 1 -> T, Else: 0 -> T
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FLOAT_single) { // (float)FPUL -> FRn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FTRC_single) { // (long)FRm -> FPUL
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FIPR) { // inner_product (FVm, FVn) -> FR[n+3]
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FTRV) { // transform_vector (XMTRX, FVn) -> FVn
    BADOPCODE; // Crash on unimplemented opcode
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
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STSFPSCR) { // FPSCR -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(LDSMFPSCR) { // (Rm) -> FPSCR, Rm+4 -> Rm
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STSMFPSCR) { // Rn-4 -> Rn, FPSCR -> (Rn)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(LDSFPUL) { // Rm -> FPUL
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STSFPUL) { // FPUL -> Rn
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(LDSMFPUL) { // (Rm) -> FPUL, Rm+4 -> Rm
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(STSMFPUL) { // Rn-4 -> Rn, FPUL -> (Rn)
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FRCHG) { // If FPSCR.PR = 0: ~FPSCR.FR -> FPSCR.FR, Else: Undefined Operation
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}

SH4ins(FSCHG) { // If FPSCR.PR = 0: ~FPSCR.SZ -> FPSCR.SZ, Else: Undefined Operation
    BADOPCODE; // Crash on unimplemented opcode
    PCinc;
}
#undef mPC
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

struct SH4_ins_t SH4_decoded[65536];
char SH4_disassembled[65536][30];
char SH4_mnemonic[65536][30];

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

static void emplace_mnemonic(u32 opcode, const char *mnemonic, u32 n, u32 m, u32 d, u32 imm)
{
    strcpy(SH4_mnemonic[opcode], mnemonic);

    char *copy_to = SH4_disassembled[opcode];
    *copy_to = 0;

    for (char *mn = (char *)mnemonic; *mn!=0; mn++) {
        if ((*mn == 'R') && ((*(mn+1) == 'n') || (*(mn+1) == 'm'))) {
            u32 *wh = *(mn+1) == 'n' ? &n : &m;
            sprintf(copy_to, "R%d", *wh);
            copy_to += 2;
            mn++;
            if ((*wh) > 9) copy_to++; // 2 digits is the max
            *copy_to = 0;
            continue;
        }
        if ((*mn == 'd') && (*(mn+1) == 'i') && (*(mn+2) == 's')) {
            // disp
            sprintf(copy_to, "%02x", d);
            copy_to += 2; // 2 digit minimum
            d >>= 4;
            mn += 3;
            while(d>0) {
                if (d > 0xF) copy_to++;  // 1 character hex
                d >>= 4;
            }
            continue;
        }
        if ((*mn == 'i') && (*(mn+1) == 'm' && (*(mn+2) == 'm'))) {
            // imm
            sprintf(copy_to, "%d", imm);
            copy_to++; // 1 digit minimum
            mn += 2;
            while(imm>0) {
                if (imm > 9) copy_to++;  // 1 character hex
                imm = (imm - (imm % 10)) / 10;
            }
            continue;
        }
        *copy_to = *mn;
        copy_to++;
    }
    *copy_to = 0;
}


static void iterate_opcodes(struct sh4_str_ret* r, SH4_ins_func ins, const char* mnemonic, u32 override)
{
    u32 d_times = 1;
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

                    struct SH4_ins_t *is = &SH4_decoded[opcode];
                    assert((!override) && (is->decoded == 0));
                    is->Rn = n;
                    is->Rm = m;
                    is->imm = i;
                    is->disp = (i32)(d*d_times);
                    is->exec = ins;
                    is->decoded = 1;
                    emplace_mnemonic(opcode, mnemonic, n, m, d, i);
                    //printf("\n%s", SH4_disassembled[opcode]);
                }
            }
        }
    }
}

static void decode_and_iterate_opcodes(const char* inpt, SH4_ins_func ins, const char *mnemonic, u32 override)
{
    struct sh4_str_ret a;
    process_SH4_instruct(&a, inpt);
    iterate_opcodes(&a, ins, mnemonic, override);
}


#define OE(opcstr, func, mn) decode_and_iterate_opcodes(opcstr, func, mn, 0)
#define OE_override(opcstr, func, mn) decode_and_iterate_opcodes(opcstr, func, mn, 1)

void do_sh4_decode() {
    for (u32 i = 0; i < 65536; i++) {
        SH4_decoded[i] = (struct SH4_ins_t) {
                .opcode = i,
                .Rn = -1,
                .Rm = -1,
                .imm = 0,
                .disp = 0,
                .exec = NULL,
                .decoded = 0
        };
    }

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
    OE("10001011dddddddd", &SH4_BF, "bf label"); // If T = 0: disp*2 + PC + 4 -> PC, Else: nop
    OE("10001111dddddddd", &SH4_BFS, "bf/s disp"); // If T = 0: disp*2 + PC + 4 -> PC, Else: nop, (Delayed branch)
    OE("10001001dddddddd", &SH4_BT, "bt label"); // If T = 1: disp*2 + PC + 4 -> PC, Else: nop
    OE("10001101dddddddd", &SH4_BTS, "bt/s label"); // If T = 1: disp*2 + PC + 4 -> PC, Else: nop, (Delayed branch)
    OE("1010dddddddddddd", &SH4_BRA, "bra label"); // disp*2 + PC + 4 -> PC, (Delayed branch)
    OE("0000mmmm00100011", &SH4_BRAF, "braf Rm"); // Rm + PC + 4 -> PC, (Delayed branch)
    OE("1011dddddddddddd", &SH4_BSR, "bsr label"); // PC + 4 -> PR, disp*2 + PC + 4 -> PC, (Delayed branch)
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
    /*OE("1111nnn0mmm01100", &SH4_FMOV_DR, "fmov DRm,DRn"); // DRm -> DRn
    OE("1111nnn1mmm01100", &SH4_FMOV_DRXD, "fmov DRm,XDn"); // DRm -> XDn
    OE("1111nnn0mmm11100", &SH4_FMOV_XDDR, "fmov XDm,DRn"); // XDm -> DRn
    OE("1111nnn1mmm11100", &SH4_FMOV_XDXD, "fmov XDm,XDn"); // XDm -> XDn
    OE("1111nnn0mmmm1000", &SH4_FMOV_LOAD_DR, "fmov.d @Rm,DRn"); // (Rm) -> DRn
    OE("1111nnn1mmmm1000", &SH4_FMOV_LOAD_XD, "fmov.d @Rm,XDn"); // (Rm) -> XDn
    OE("1111nnnnmmm01010", &SH4_FMOV_STORE_DR, "fmov.d DRm,@Rn"); // DRm -> (Rn)
    OE("1111nnnnmmm11010", &SH4_FMOV_STORE_XD, "fmov.d XDm,@Rn"); // XDm -> (Rn)
    OE("1111nnn0mmmm1001", &SH4_FMOV_RESTORE_DR, "fmov.d @Rm+,DRn"); // (Rm) -> DRn, Rm + 8 -> Rm
    OE("1111nnn1mmmm1001", &SH4_FMOV_RESTORE_XD, "fmov.d @Rm+,XDn"); // (Rm) -> XDn, Rm+8 -> Rm
    OE("1111nnnnmmm01011", &SH4_FMOV_SAVE_DR, "fmov.d DRm,@-Rn"); // Rn-8 -> Rn, DRm -> (Rn)
    OE("1111nnnnmmm11011", &SH4_FMOV_SAVE_XD, "fmov.d XDm,@-Rn"); // Rn-8 -> Rn, (Rn) -> XDm
    OE("1111nnn0mmmm0110", &SH4_FMOV_INDEX_LOAD_DR, "fmov.d @(R0,Rm),DRn"); // (R0 + Rm) -> DRn
    OE("1111nnn1mmmm0110", &SH4_FMOV_INDEX_LOAD_XD, "fmov.d @(R0,Rm),XDn"); // (R0 + Rm) -> XDn
    OE("1111nnnnmmm00111", &SH4_FMOV_INDEX_STORE_DR, "fmov.d DRm,@(R0,Rn)"); // DRm -> (R0 + Rn)
    OE("1111nnnnmmm10111", &SH4_FMOV_INDEX_STORE_XD, "fmov.d XDm,@(R0,Rn)"); // XDm -> (R0 + Rn)
     */
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
    /*OE("1111nnn001011101", &SH4_FABSDR, "fabs DRn"); // DRn & 0x7FFFFFFFFFFFFFFF -> DRn
    OE("1111nnn001001101", &SH4_FNEGDR, "fneg DRn"); // DRn ^ 0x8000000000000000 -> DRn
    OE("1111nnn0mmm00000", &SH4_FADDDR, "fadd DRm,DRn"); // DRn + DRm -> DRn
    OE("1111nnn0mmm00001", &SH4_FSUBDR, "fsub DRm,DRn"); // DRn - DRm -> DRn
    OE("1111nnn0mmm00010", &SH4_FMULDR, "fmul DRm,DRn"); // DRn * DRm -> DRn
    OE("1111nnn0mmm00011", &SH4_FDIVDR, "fdiv DRm,DRn"); // DRn / DRm -> DRn
    OE("1111nnn001101101", &SH4_FSQRTDR, "fsqrt DRn"); // sqrt (DRn) -> DRn
    OE("1111nnn0mmm00100", &SH4_FCMP_EQDR, "fcmp/eq DRm,DRn"); // If DRn = DRm: 1 -> T, Else: 0 -> T
    OE("1111nnn0mmm00101", &SH4_FCMP_GTDR, "fcmp/gt DRm,DRn"); // If DRn > DRm: 1 -> T, Else: 0 -> T
    OE("1111nnn000101101", &SH4_FLOAT_double, "float FPUL,DRn"); // (double)FPUL -> DRn
    OE("1111mmm000111101", &SH4_FTRC_double, "ftrc DRm,FPUL"); // (long)DRm -> FPUL*/
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

    u32 unencoded = 0;
    for (u32 i = 0; i < 65536; i++) {
        if (SH4_decoded[i].decoded == 0) {
            SH4_decoded[i].exec = &SH4_EMPTY;
            sprintf(SH4_disassembled[i], "UNKNOWN OPCODE %04x", i);
            sprintf(SH4_mnemonic[i], "UNKNOWN OPCODE %04x", i);
            unencoded++;
        }
    }
}