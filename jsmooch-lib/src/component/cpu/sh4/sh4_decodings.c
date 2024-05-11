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
