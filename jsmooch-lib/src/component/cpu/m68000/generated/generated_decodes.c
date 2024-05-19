
    for (u32 x = 0; x < 8; x++) {
        for (u32 y = 0; y < 8; y++) {

            op1 = mk_ea(M68k_AM_data_register_direct, y);
            op2 = mk_ea(M68k_AM_data_register_direct, x);
            bind_opcode("1100 ...1 0000 ....", 4, &M68K_ins_ABCD, &M68K_disasm_BAD, (x << 9) | y, &op1, &op2);

            op1 = mk_ea(M68k_AM_address_register_indirect_with_predecrement, y);
            op2 = mk_ea(M68k_AM_address_register_indirect_with_predecrement, x);
            bind_opcode("1100 ...1 0000 ....", 4, &M68K_ins_ABCD, &M68K_disasm_BAD, (x << 9) | y | 8, &op1, &op2);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {

                if (!(((m == 7) && (r >= 5)))) {
                    op1 = mk_ea(m, r);
                    op2 = mk_ea(M68k_AM_data_register_direct, d);
                    if (m != 1) bind_opcode("1101 .... 00.. ....", 1, &M68K_ins_ADD, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                    bind_opcode("1101 .... 01.. ....", 2, &M68K_ins_ADD, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                    bind_opcode("1101 .... 10.. ....", 4, &M68K_ins_ADD, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                }
                if (!(((m <= 1) || ((m == 7) && (r >= 2))))) {
                    op1 = mk_ea(M68k_AM_data_register_direct, d);
                    op2 = mk_ea(m, r);
                    bind_opcode("1101 .... 00.. ....", 1, &M68K_ins_ADD, &M68K_disasm_BAD, (d << 9) | (m << 3) | r | 0x100, &op1, &op2);
                    bind_opcode("1101 .... 01.. ....", 2, &M68K_ins_ADD, &M68K_disasm_BAD, (d << 9) | (m << 3) | r | 0x100, &op1, &op2);
                    bind_opcode("1101 .... 10.. ....", 4, &M68K_ins_ADD, &M68K_disasm_BAD, (d << 9) | (m << 3) | r | 0x100, &op1, &op2);
                }
            }
        }
    }

    for (u32 a = 0; a < 8; a++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 7) && (r >= 5)) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(M68k_AM_address_register_direct, a);
                bind_opcode("1101 ...0 .11. ....", 2, &M68K_ins_ADDA, &M68K_disasm_BAD, (a << 9) | (m << 3) | r, &op1, &op2);
                bind_opcode("1101 ...1 .11. ....", 4, &M68K_ins_ADDA, &M68K_disasm_BAD, (a << 9) | (m << 3) | r, &op1, &op2);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0000 0110 00.. ....", 1, &M68K_ins_ADDI, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            bind_opcode("0000 0110 01.. ....", 2, &M68K_ins_ADDI, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            bind_opcode("0000 0110 10.. ....", 4, &M68K_ins_ADDI, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 7) && (r >= 2)) continue;
                op1 = mk_ea(M68k_AM_immediate, d ? d : 8);
                if (m == 1) {
                    op2 = mk_ea(M68k_AM_address_register_direct, r);
                    bind_opcode("0101 ...0 01.. ....", 2, &M68K_ins_ADDQ, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                    bind_opcode("0101 ...0 10.. ....", 4, &M68K_ins_ADDQ, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                }
                else {
                    op2 = mk_ea(m, r);
                    bind_opcode("0101 ...0 00.. ....", 1, &M68K_ins_ADDQ, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                    bind_opcode("0101 ...0 01.. ....", 2, &M68K_ins_ADDQ, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                    bind_opcode("0101 ...0 10.. ....", 4, &M68K_ins_ADDQ, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                }
            }
        }
    }

    for (u32 x = 0; x < 8; x++) {
        for (u32 y = 0; y < 8; y++) {

            op1 = mk_ea(M68k_AM_data_register_direct, y);
            op2 = mk_ea(M68k_AM_data_register_direct, x);
            bind_opcode("1101 ...1 0000 ....", 1, &M68K_ins_ADDX, &M68K_disasm_BAD, (x << 9) | y, &op1, &op2);
            bind_opcode("1101 ...1 0100 ....", 2, &M68K_ins_ADDX, &M68K_disasm_BAD, (x << 9) | y, &op1, &op2);
            bind_opcode("1101 ...1 1000 ....", 4, &M68K_ins_ADDX, &M68K_disasm_BAD, (x << 9) | y, &op1, &op2);

            op1 = mk_ea(M68k_AM_address_register_indirect_with_predecrement, y);
            op2 = mk_ea(M68k_AM_address_register_indirect_with_predecrement, x);
            bind_opcode("1101 ...1 0000 ....", 1, &M68K_ins_ADDX, &M68K_disasm_BAD, (x << 9) | y | 8, &op1, &op2);
            bind_opcode("1101 ...1 0100 ....", 2, &M68K_ins_ADDX, &M68K_disasm_BAD, (x << 9) | y | 8, &op1, &op2);
            bind_opcode("1101 ...1 1000 ....", 4, &M68K_ins_ADDX, &M68K_disasm_BAD, (x << 9) | y | 8, &op1, &op2);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {

                if (!(((m == 1) || ((m == 7) && (r >= 5))))) {
                    op1 = mk_ea(m, r);
                    op2 = mk_ea(M68k_AM_data_register_direct, d);
                    bind_opcode("1100 .... 00.. ....", 1, &M68K_ins_AND, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                    bind_opcode("1100 .... 01.. ....", 2, &M68K_ins_AND, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                    bind_opcode("1100 .... 10.. ....", 4, &M68K_ins_AND, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                }
                if (!(((m <= 1) || ((m == 7) && (r >= 2))))) {
                    op1 = mk_ea(M68k_AM_data_register_direct, d);
                    op2 = mk_ea(m, r);
                    bind_opcode("1100 .... 00.. ....", 1, &M68K_ins_AND, &M68K_disasm_BAD, (d << 9) | (m << 3) | r | 0x100, &op1, &op2);
                    bind_opcode("1100 .... 01.. ....", 2, &M68K_ins_AND, &M68K_disasm_BAD, (d << 9) | (m << 3) | r | 0x100, &op1, &op2);
                    bind_opcode("1100 .... 10.. ....", 4, &M68K_ins_AND, &M68K_disasm_BAD, (d << 9) | (m << 3) | r | 0x100, &op1, &op2);
                }
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0000 0010 00.. ....", 1, &M68K_ins_ANDI, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            bind_opcode("0000 0010 01.. ....", 2, &M68K_ins_ANDI, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            bind_opcode("0000 0010 10.. ....", 4, &M68K_ins_ANDI, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    bind_opcode("0000 0010 0011 1100", 4, &M68K_ins_ANDI_TO_CCR, &M68K_disasm_BAD, 0, NULL, NULL);

    bind_opcode("0000 0010 0111 1100", 4, &M68K_ins_ANDI_TO_SR, &M68K_disasm_BAD, 0, NULL, NULL);

    for (u32 i = 0; i < 8; i++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_immediate, i ? i : 8);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...1 0000 0...", 1, &M68K_ins_ASL, &M68K_disasm_BAD, (i << 9) | d, &op1, &op2);
            bind_opcode("1110 ...1 0100 0...", 2, &M68K_ins_ASL, &M68K_disasm_BAD, (i << 9) | d, &op1, &op2);
            bind_opcode("1110 ...1 1000 0...", 4, &M68K_ins_ASL, &M68K_disasm_BAD, (i << 9) | d, &op1, &op2);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_data_register_direct, s);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...1 0010 0...", 1, &M68K_ins_ASL, &M68K_disasm_BAD, (s << 9) | d, &op1, &op2);
            bind_opcode("1110 ...1 0110 0...", 2, &M68K_ins_ASL, &M68K_disasm_BAD, (s << 9) | d, &op1, &op2);
            bind_opcode("1110 ...1 1010 0...", 4, &M68K_ins_ASL, &M68K_disasm_BAD, (s << 9) | d, &op1, &op2);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("1110 0001 11.. ....", 4, &M68K_ins_ASL, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    for (u32 i = 0; i < 8; i++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_immediate, i ? i : 8);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...0 0000 0...", 1, &M68K_ins_ASR, &M68K_disasm_BAD, (i << 9) | d, &op1, &op2);
            bind_opcode("1110 ...0 0100 0...", 2, &M68K_ins_ASR, &M68K_disasm_BAD, (i << 9) | d, &op1, &op2);
            bind_opcode("1110 ...0 1000 0...", 4, &M68K_ins_ASR, &M68K_disasm_BAD, (i << 9) | d, &op1, &op2);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_data_register_direct, s);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...0 0010 0...", 1, &M68K_ins_ASR, &M68K_disasm_BAD, (s << 9) | d, &op1, &op2);
            bind_opcode("1110 ...0 0110 0...", 2, &M68K_ins_ASR, &M68K_disasm_BAD, (s << 9) | d, &op1, &op2);
            bind_opcode("1110 ...0 1010 0...", 4, &M68K_ins_ASR, &M68K_disasm_BAD, (s << 9) | d, &op1, &op2);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("1110 0000 11.. ....", 4, &M68K_ins_ASR, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    for (u32 t = 0; t < 16; t++) {
        for (u32 d = 0; d < 256; d++) {
            if (t<=1) continue;
            op1 = mk_ea(M68k_AM_immediate, t);
            op2 = mk_ea(M68k_AM_immediate, d);
            bind_opcode("0110 .... .... ....", 4, &M68K_ins_BCC, &M68K_disasm_BAD, (t << 8) | d, &op1, &op2);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 2))) continue;
                op1 = mk_ea(M68k_AM_data_register_direct, d);
                op2 = mk_ea(m, r);
                if (m == 0) bind_opcode("0000 ...1 01.. ....", 4, &M68K_ins_BCHG, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                else        bind_opcode("0000 ...1 01.. ....", 1, &M68K_ins_BCHG, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            if (m == 0) bind_opcode("0000 1000 01.. ....", 4, &M68K_ins_BCHG, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            else        bind_opcode("0000 1000 01.. ....", 1, &M68K_ins_BCHG, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 2))) continue;
                op1 = mk_ea(M68k_AM_data_register_direct, d);
                op2 = mk_ea(m, r);
                if (m == 0) bind_opcode("0000 ...1 10.. ....", 4, &M68K_ins_BCLR, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                else        bind_opcode("0000 ...1 10.. ....", 1, &M68K_ins_BCLR, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            if (m == 0) bind_opcode("0000 1000 10.. ....", 4, &M68K_ins_BCLR, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            else        bind_opcode("0000 1000 10.. ....", 1, &M68K_ins_BCLR, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    for (u32 d = 0; d < 256; d++) {
        op1 = mk_ea(M68k_AM_immediate, d);
        bind_opcode("0110 0000 .... ....", 4, &M68K_ins_BRA, &M68K_disasm_BAD, d, &op1, NULL);
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 2))) continue;
                op1 = mk_ea(M68k_AM_data_register_direct, d);
                op2 = mk_ea(m, r);
                if (m == 0) bind_opcode("0000 ...1 11.. ....", 4, &M68K_ins_BSET, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                else        bind_opcode("0000 ...1 11.. ....", 1, &M68K_ins_BSET, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            if (m == 0) bind_opcode("0000 1000 11.. ....", 4, &M68K_ins_BSET, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            else        bind_opcode("0000 1000 11.. ....", 1, &M68K_ins_BSET, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    for (u32 d = 0; d < 256; d++) {
        op1 = mk_ea(M68k_AM_immediate, d);
        bind_opcode("0110 0001 .... ....", 4, &M68K_ins_BSR, &M68K_disasm_BAD, d, &op1, NULL);
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 5))) continue;
                op1 = mk_ea(M68k_AM_data_register_direct, d);
                op2 = mk_ea(m, r);
                if (m == 0) bind_opcode("0000 ...1 00.. ....", 4, &M68K_ins_BTST, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                else        bind_opcode("0000 ...1 00.. ....", 1, &M68K_ins_BTST, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 4))) continue;
            op1 = mk_ea(m, r);
            if (m == 0) bind_opcode("0000 1000 00.. ....", 4, &M68K_ins_BTST, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            else        bind_opcode("0000 1000 00.. ....", 1, &M68K_ins_BTST, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 5))) continue;
                op1 = mk_ea(M68k_AM_data_register_direct, d);
                op2 = mk_ea(m, r);
                bind_opcode("0100 ...1 10.. ....", 4, &M68K_ins_CHK, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 0010 00.. ....", 1, &M68K_ins_CLR, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            bind_opcode("0100 0010 01.. ....", 2, &M68K_ins_CLR, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            bind_opcode("0100 0010 10.. ....", 4, &M68K_ins_CLR, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 7) && (r >= 5)) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(M68k_AM_data_register_direct, d);
                if (m!=1) bind_opcode("1011 ...0 00.. ....", 1, &M68K_ins_CMP, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                bind_opcode("1011 ...0 01.. ....", 2, &M68K_ins_CMP, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                bind_opcode("1011 ...0 10.. ....", 4, &M68K_ins_CMP, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
            }
        }
    }

    for (u32 a = 0; a < 8; a++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 7) && (r >= 5)) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(M68k_AM_address_register_direct, a);
                bind_opcode("1011 ...0 .11. ....", 2, &M68K_ins_CMPA, &M68K_disasm_BAD, (a << 9) | (m << 3) | r, &op1, &op2);
                bind_opcode("1011 ...1 .11. ....", 4, &M68K_ins_CMPA, &M68K_disasm_BAD, (a << 9) | (m << 3) | r, &op1, &op2);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0000 1100 00.. ....", 1, &M68K_ins_CMPI, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            bind_opcode("0000 1100 01.. ....", 2, &M68K_ins_CMPI, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            bind_opcode("0000 1100 10.. ....", 4, &M68K_ins_CMPI, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    for (u32 x = 0; x < 8; x++) {
        for (u32 y = 0; y < 8; y++) {
            op1 = mk_ea(M68k_AM_address_register_indirect_with_postincrement, y);
            op2 = mk_ea(M68k_AM_address_register_indirect_with_postincrement, x);
            bind_opcode("1011 ...1 0000 1...", 1, &M68K_ins_CMPM, &M68K_disasm_BAD, (x << 9) | y, &op1, &op2);
            bind_opcode("1011 ...1 0100 1...", 2, &M68K_ins_CMPM, &M68K_disasm_BAD, (x << 9) | y, &op1, &op2);
            bind_opcode("1011 ...1 1000 1...", 4, &M68K_ins_CMPM, &M68K_disasm_BAD, (x << 9) | y, &op1, &op2);
        }
    }

    for (u32 c = 0; c < 16; c++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_immediate, c);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("0101 .... 1100 1...", 4, &M68K_ins_DBCC, &M68K_disasm_BAD, (c << 8) | d, &op1, &op2);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 5))) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(M68k_AM_data_register_direct, d);
                bind_opcode("1000 ...1 11.. ....", 4, &M68K_ins_DIVS, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
            }
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 5))) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(M68k_AM_data_register_direct, d);
                bind_opcode("1000 ...0 11.. ....", 4, &M68K_ins_DIVU, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
            }
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 2))) continue;
                op1 = mk_ea(M68k_AM_data_register_direct, d);
                op2 = mk_ea(m, r);
                bind_opcode("1011 ...1 00.. ....", 1, &M68K_ins_EOR, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                bind_opcode("1011 ...1 01.. ....", 2, &M68K_ins_EOR, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                bind_opcode("1011 ...1 10.. ....", 4, &M68K_ins_EOR, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0000 1010 00.. ....", 1, &M68K_ins_EORI, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            bind_opcode("0000 1010 01.. ....", 2, &M68K_ins_EORI, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            bind_opcode("0000 1010 10.. ....", 4, &M68K_ins_EORI, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    bind_opcode("0000 1010 0011 1100", 4, &M68K_ins_EORI_TO_CCR, &M68K_disasm_BAD, 0, NULL, NULL);

    bind_opcode("0000 1010 0111 1100", 4, &M68K_ins_EORI_TO_SR, &M68K_disasm_BAD, 0, NULL, NULL);

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_data_register_direct, s);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1100 ...1 0100 0...", 4, &M68K_ins_EXG, &M68K_disasm_BAD, (s << 9) | d, &op1, &op2);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_address_register_direct, s);
            op2 = mk_ea(M68k_AM_address_register_direct, d);
            bind_opcode("1100 ...1 0100 1...", 4, &M68K_ins_EXG, &M68K_disasm_BAD, (s << 9) | d, &op1, &op2);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_address_register_direct, s);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1100 ...1 1000 1...", 4, &M68K_ins_EXG, &M68K_disasm_BAD, (s << 9) | d, &op1, &op2);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        op1 = mk_ea(M68k_AM_data_register_direct, d);
        bind_opcode("0100 1000 10.0 00..", 2, &M68K_ins_EXT, &M68K_disasm_BAD, d, &op1, NULL);
        bind_opcode("0100 1000 11.0 00..", 4, &M68K_ins_EXT, &M68K_disasm_BAD, d, &op1, NULL);
    }

    bind_opcode("0100 1010 1111 1100", 4, &M68K_ins_ILLEGAL, &M68K_disasm_BAD, 0, NULL, NULL);

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || (m == 3) || (m == 4) || ((m == 7) && (r >= 4))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 1110 11.. ....", 4, &M68K_ins_JMP, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || (m == 3) || (m == 4) || ((m == 7) && (r >= 4))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 1110 10.. ....", 4, &M68K_ins_JSR, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    for (u32 a = 0; a < 8; a++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m <= 1) || (m == 3) || (m == 4) || ((m == 7) && (r >= 4))) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(M68k_AM_address_register_direct, a);
                bind_opcode("0100 ...1 11.. ....", 4, &M68K_ins_LEA, &M68K_disasm_BAD, (a << 9) | (m << 3) | r, &op1, &op2);
            }
        }
    }

    for (u32 a = 0; a < 8; a++) {
        op1 = mk_ea(M68k_AM_address_register_direct, a);
        bind_opcode("0100 1110 0101 0...", 4, &M68K_ins_LINK, &M68K_disasm_BAD, a, &op1, NULL);
    }

    for (u32 i = 0; i < 8; i++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_immediate, i ? i : 8);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...1 0000 1...", 1, &M68K_ins_LSL, &M68K_disasm_BAD, (i << 9) | d, &op1, &op2);
            bind_opcode("1110 ...1 0100 1...", 2, &M68K_ins_LSL, &M68K_disasm_BAD, (i << 9) | d, &op1, &op2);
            bind_opcode("1110 ...1 1000 1...", 4, &M68K_ins_LSL, &M68K_disasm_BAD, (i << 9) | d, &op1, &op2);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_data_register_direct, s);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...1 0010 1...", 1, &M68K_ins_LSL, &M68K_disasm_BAD, (s << 9) | d, &op1, &op2);
            bind_opcode("1110 ...1 0110 1...", 2, &M68K_ins_LSL, &M68K_disasm_BAD, (s << 9) | d, &op1, &op2);
            bind_opcode("1110 ...1 1010 1...", 4, &M68K_ins_LSL, &M68K_disasm_BAD, (s << 9) | d, &op1, &op2);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("1110 0011 11.. ....", 4, &M68K_ins_LSL, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    for (u32 i = 0; i < 8; i++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_immediate, i ? i : 8);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...0 0000 1...", 1, &M68K_ins_LSR, &M68K_disasm_BAD, (i << 9) | d, &op1, &op2);
            bind_opcode("1110 ...0 0100 1...", 2, &M68K_ins_LSR, &M68K_disasm_BAD, (i << 9) | d, &op1, &op2);
            bind_opcode("1110 ...0 1000 1...", 4, &M68K_ins_LSR, &M68K_disasm_BAD, (i << 9) | d, &op1, &op2);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_data_register_direct, s);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...0 0010 1...", 1, &M68K_ins_LSR, &M68K_disasm_BAD, (s << 9) | d, &op1, &op2);
            bind_opcode("1110 ...0 0110 1...", 2, &M68K_ins_LSR, &M68K_disasm_BAD, (s << 9) | d, &op1, &op2);
            bind_opcode("1110 ...0 1010 1...", 4, &M68K_ins_LSR, &M68K_disasm_BAD, (s << 9) | d, &op1, &op2);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("1110 0010 11.. ....", 4, &M68K_ins_LSR, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    for (u32 a = 0; a < 8; a++) {
        for (u32 b = 0; b < 8; b++) {
            for (u32 c = 0; c < 8; c++) {
                for (u32 d = 0; d < 8; d++) {
                    if (((b == 1) || ((b == 7) && (a >= 2))) || ((c == 7) && (d >= 5))) continue;
                    op1 = mk_ea(c, d);
                    op2 = mk_ea(b, a);
                    if (c!=1) bind_opcode("0001 .... .... ....", 1, &M68K_ins_MOVE, &M68K_disasm_BAD, (a << 9) | (b << 6) | (c << 3) | d, &op1, &op2);
                    bind_opcode("0011 .... .... ....", 2, &M68K_ins_MOVE, &M68K_disasm_BAD, (a << 9) | (b << 6) | (c << 3) | d, &op1, &op2);
                    bind_opcode("0010 .... .... ....", 4, &M68K_ins_MOVE, &M68K_disasm_BAD, (a << 9) | (b << 6) | (c << 3) | d, &op1, &op2);
                }
            }
        }
    }

    for (u32 a = 0; a < 8; a++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 7) && (r >= 5)) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(M68k_AM_address_register_direct, a);
                bind_opcode("0000 ...0 01.. ....", 2, &M68K_ins_MOVEA, &M68K_disasm_BAD, (a << 9) | (m << 3) | r, &op1, &op2);
                bind_opcode("0001 ...0 01.. ....", 4, &M68K_ins_MOVEA, &M68K_disasm_BAD, (a << 9) | (m << 3) | r, &op1, &op2);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || (m == 3) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 1000 10.. ....", 2, &M68K_ins_MOVEM_TO_MEM, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            bind_opcode("0100 1000 11.. ....", 4, &M68K_ins_MOVEM_TO_MEM, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || (m == 4) || ((m == 7) && (r >= 4))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 1100 10.. ....", 2, &M68K_ins_MOVEM_TO_REG, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            bind_opcode("0100 1100 11.. ....", 4, &M68K_ins_MOVEM_TO_REG, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 r = 0; r < 8; r++) {

            op1 = mk_ea(M68k_AM_address_register_indirect_with_displacement, r);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("0000 ...1 .0.0 01..", 2, &M68K_ins_MOVEP, &M68K_disasm_BAD, (d << 9) | r, &op1, &op2);
            bind_opcode("0000 ...1 .1.0 01..", 4, &M68K_ins_MOVEP, &M68K_disasm_BAD, (d << 9) | r, &op1, &op2);

            op1 = mk_ea(M68k_AM_data_register_direct, d);
            op2 = mk_ea(M68k_AM_address_register_indirect_with_displacement, r);
            bind_opcode("0000 ...1 .0.0 01..", 2, &M68K_ins_MOVEP, &M68K_disasm_BAD, (d << 9) | r | 0x80, &op1, &op2);
            bind_opcode("0000 ...1 .1.0 01..", 4, &M68K_ins_MOVEP, &M68K_disasm_BAD, (d << 9) | r | 0x80, &op1, &op2);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 i = 0; i < 256; i++) {
            op1 = mk_ea(M68k_AM_immediate, i);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("0111 ...0 .... ....", 4, &M68K_ins_MOVEQ, &M68K_disasm_BAD, (d << 9) | i, &op1, &op2);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 0000 11.. ....", 4, &M68K_ins_MOVE_FROM_SR, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 5))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 0100 11.. ....", 4, &M68K_ins_MOVE_TO_CCR, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 5))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 0110 11.. ....", 4, &M68K_ins_MOVE_TO_SR, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    for (u32 a = 0; a < 8; a++) {
        op1 = mk_ea(M68k_AM_address_register_direct, a);
        bind_opcode("0100 1110 0110 1...", 4, &M68K_ins_MOVE_FROM_USP, &M68K_disasm_BAD, a, &op1, NULL);
    }

    for (u32 a = 0; a < 8; a++) {
        op1 = mk_ea(M68k_AM_address_register_direct, a);
        bind_opcode("0100 1110 0110 0...", 4, &M68K_ins_MOVE_TO_USP, &M68K_disasm_BAD, a, &op1, NULL);
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 5))) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(M68k_AM_data_register_direct, d);
                bind_opcode("1100 ...1 11.. ....", 4, &M68K_ins_MULS, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
            }
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 5))) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(M68k_AM_data_register_direct, d);
                bind_opcode("1100 ...0 11.. ....", 4, &M68K_ins_MULU, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 1000 00.. ....", 4, &M68K_ins_NBCD, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 0100 00.. ....", 1, &M68K_ins_NEG, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            bind_opcode("0100 0100 01.. ....", 2, &M68K_ins_NEG, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            bind_opcode("0100 0100 10.. ....", 4, &M68K_ins_NEG, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 0000 00.. ....", 1, &M68K_ins_NEGX, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            bind_opcode("0100 0000 01.. ....", 2, &M68K_ins_NEGX, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            bind_opcode("0100 0000 10.. ....", 4, &M68K_ins_NEGX, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    bind_opcode("0100 1110 0111 0001", 4, &M68K_ins_NOP, &M68K_disasm_BAD, 0, NULL, NULL);

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 0110 00.. ....", 1, &M68K_ins_NOT, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            bind_opcode("0100 0110 01.. ....", 2, &M68K_ins_NOT, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            bind_opcode("0100 0110 10.. ....", 4, &M68K_ins_NOT, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {

                if (!(((m == 1) || ((m == 7) && (r >= 5))))) {
                    op1 = mk_ea(m, r);
                    op2 = mk_ea(M68k_AM_data_register_direct, d);
                    bind_opcode("1000 .... 00.. ....", 1, &M68K_ins_OR, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                    bind_opcode("1000 .... 01.. ....", 2, &M68K_ins_OR, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                    bind_opcode("1000 .... 10.. ....", 4, &M68K_ins_OR, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                }
                if (!(((m <= 1) || ((m == 7) && (r >= 2))))) {
                    op1 = mk_ea(M68k_AM_data_register_direct, d);
                    op2 = mk_ea(m, r);
                    bind_opcode("1000 .... 00.. ....", 1, &M68K_ins_OR, &M68K_disasm_BAD, (d << 9) | (m << 3) | r | 0x100, &op1, &op2);
                    bind_opcode("1000 .... 01.. ....", 2, &M68K_ins_OR, &M68K_disasm_BAD, (d << 9) | (m << 3) | r | 0x100, &op1, &op2);
                    bind_opcode("1000 .... 10.. ....", 4, &M68K_ins_OR, &M68K_disasm_BAD, (d << 9) | (m << 3) | r | 0x100, &op1, &op2);
                }
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0000 0000 00.. ....", 1, &M68K_ins_ORI, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            bind_opcode("0000 0000 01.. ....", 2, &M68K_ins_ORI, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            bind_opcode("0000 0000 10.. ....", 4, &M68K_ins_ORI, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    bind_opcode("0000 0000 0011 1100", 4, &M68K_ins_ORI_TO_CCR, &M68K_disasm_BAD, 0, NULL, NULL);

    bind_opcode("0000 0000 0111 1100", 4, &M68K_ins_ORI_TO_SR, &M68K_disasm_BAD, 0, NULL, NULL);

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || (m == 3) || (m == 4) || ((m == 7) && (r >= 4))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 1000 01.. ....", 4, &M68K_ins_PEA, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    bind_opcode("0100 1110 0111 0000", 4, &M68K_ins_RESET, &M68K_disasm_BAD, 0, NULL, NULL);

    for (u32 i = 0; i < 8; i++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_immediate, i ? i : 8);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...1 0001 1...", 1, &M68K_ins_ROL, &M68K_disasm_BAD, (i << 9) | d, &op1, &op2);
            bind_opcode("1110 ...1 0101 1...", 2, &M68K_ins_ROL, &M68K_disasm_BAD, (i << 9) | d, &op1, &op2);
            bind_opcode("1110 ...1 1001 1...", 4, &M68K_ins_ROL, &M68K_disasm_BAD, (i << 9) | d, &op1, &op2);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_data_register_direct, s);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...1 0011 1...", 1, &M68K_ins_ROL, &M68K_disasm_BAD, (s << 9) | d, &op1, &op2);
            bind_opcode("1110 ...1 0111 1...", 2, &M68K_ins_ROL, &M68K_disasm_BAD, (s << 9) | d, &op1, &op2);
            bind_opcode("1110 ...1 1011 1...", 4, &M68K_ins_ROL, &M68K_disasm_BAD, (s << 9) | d, &op1, &op2);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("1110 0111 11.. ....", 4, &M68K_ins_ROL, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    for (u32 i = 0; i < 8; i++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_immediate, i ? i : 8);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...0 0001 1...", 1, &M68K_ins_ROR, &M68K_disasm_BAD, (i << 9) | d, &op1, &op2);
            bind_opcode("1110 ...0 0101 1...", 2, &M68K_ins_ROR, &M68K_disasm_BAD, (i << 9) | d, &op1, &op2);
            bind_opcode("1110 ...0 1001 1...", 4, &M68K_ins_ROR, &M68K_disasm_BAD, (i << 9) | d, &op1, &op2);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_data_register_direct, s);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...0 0011 1...", 1, &M68K_ins_ROR, &M68K_disasm_BAD, (s << 9) | d, &op1, &op2);
            bind_opcode("1110 ...0 0111 1...", 2, &M68K_ins_ROR, &M68K_disasm_BAD, (s << 9) | d, &op1, &op2);
            bind_opcode("1110 ...0 1011 1...", 4, &M68K_ins_ROR, &M68K_disasm_BAD, (s << 9) | d, &op1, &op2);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("1110 0110 11.. ....", 4, &M68K_ins_ROR, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    for (u32 i = 0; i < 8; i++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_immediate, i ? i : 8);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...1 0001 0...", 1, &M68K_ins_ROXL, &M68K_disasm_BAD, (i << 9) | d, &op1, &op2);
            bind_opcode("1110 ...1 0101 0...", 2, &M68K_ins_ROXL, &M68K_disasm_BAD, (i << 9) | d, &op1, &op2);
            bind_opcode("1110 ...1 1001 0...", 4, &M68K_ins_ROXL, &M68K_disasm_BAD, (i << 9) | d, &op1, &op2);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_data_register_direct, s);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...1 0011 0...", 1, &M68K_ins_ROXL, &M68K_disasm_BAD, (s << 9) | d, &op1, &op2);
            bind_opcode("1110 ...1 0111 0...", 2, &M68K_ins_ROXL, &M68K_disasm_BAD, (s << 9) | d, &op1, &op2);
            bind_opcode("1110 ...1 1011 0...", 4, &M68K_ins_ROXL, &M68K_disasm_BAD, (s << 9) | d, &op1, &op2);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("1110 0101 11.. ....", 4, &M68K_ins_ROXL, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    for (u32 i = 0; i < 8; i++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_immediate, i ? i : 8);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...0 0001 0...", 1, &M68K_ins_ROXR, &M68K_disasm_BAD, (i << 9) | d, &op1, &op2);
            bind_opcode("1110 ...0 0101 0...", 2, &M68K_ins_ROXR, &M68K_disasm_BAD, (i << 9) | d, &op1, &op2);
            bind_opcode("1110 ...0 1001 0...", 4, &M68K_ins_ROXR, &M68K_disasm_BAD, (i << 9) | d, &op1, &op2);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_data_register_direct, s);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...0 0011 0...", 1, &M68K_ins_ROXR, &M68K_disasm_BAD, (s << 9) | d, &op1, &op2);
            bind_opcode("1110 ...0 0111 0...", 2, &M68K_ins_ROXR, &M68K_disasm_BAD, (s << 9) | d, &op1, &op2);
            bind_opcode("1110 ...0 1011 0...", 4, &M68K_ins_ROXR, &M68K_disasm_BAD, (s << 9) | d, &op1, &op2);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("1110 0100 11.. ....", 4, &M68K_ins_ROXR, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    bind_opcode("0100 1110 0111 0011", 4, &M68K_ins_RTE, &M68K_disasm_BAD, 0, NULL, NULL);

    bind_opcode("0100 1110 0111 0111", 4, &M68K_ins_RTR, &M68K_disasm_BAD, 0, NULL, NULL);

    bind_opcode("0100 1110 0111 0101", 4, &M68K_ins_RTS, &M68K_disasm_BAD, 0, NULL, NULL);

    for (u32 x = 0; x < 8; x++) {
        for (u32 y = 0; y < 8; y++) {

            op1 = mk_ea(M68k_AM_data_register_direct, y);
            op2 = mk_ea(M68k_AM_data_register_direct, x);
            bind_opcode("1100 ...1 0000 ....", 4, &M68K_ins_ABCD, &M68K_disasm_BAD, (x << 9) | y, &op1, &op2);

            op1 = mk_ea(M68k_AM_address_register_indirect_with_predecrement, y);
            op2 = mk_ea(M68k_AM_address_register_indirect_with_predecrement, x);
            bind_opcode("1100 ...1 0000 ....", 4, &M68K_ins_ABCD, &M68K_disasm_BAD, (x << 9) | y | 8, &op1, &op2);
        }
    }

    for (u32 x = 0; x < 8; x++) {
        for (u32 y = 0; y < 8; y++) {

            op1 = mk_ea(M68k_AM_data_register_direct, y);
            op2 = mk_ea(M68k_AM_data_register_direct, x);
            bind_opcode("1000 ...1 0000 ....", 4, &M68K_ins_SBCD, &M68K_disasm_BAD, (x << 9) | y, &op1, &op2);

            op1 = mk_ea(M68k_AM_address_register_indirect_with_predecrement, y);
            op2 = mk_ea(M68k_AM_address_register_indirect_with_predecrement, x);
            bind_opcode("1000 ...1 0000 ....", 4, &M68K_ins_SBCD, &M68K_disasm_BAD, (x << 9) | y | 8, &op1, &op2);
        }
    }

    for (u32 t = 0; t < 16; t++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 2))) continue;
                op1 = mk_ea(M68k_AM_immediate, t);
                op2 = mk_ea(m, r);
                bind_opcode("0101 .... 11.. ....", 4, &M68K_ins_SCC, &M68K_disasm_BAD, (t << 8) | (m << 3) | r, &op1, &op2);
            }
        }
    }

    bind_opcode("0100 1110 0111 0010", 4, &M68K_ins_STOP, &M68K_disasm_BAD, 0, NULL, NULL);

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {

                if (!(((m == 7) && (r >= 5)))) {
                    op1 = mk_ea(m, r);
                    op2 = mk_ea(M68k_AM_data_register_direct, d);
                    bind_opcode("1001 .... 00.. ....", 1, &M68K_ins_SUB, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                    bind_opcode("1001 .... 01.. ....", 2, &M68K_ins_SUB, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                    bind_opcode("1001 .... 10.. ....", 4, &M68K_ins_SUB, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                }
                if (!(((m <= 1) || ((m == 7) && (r >= 2))))) {
                    op1 = mk_ea(M68k_AM_data_register_direct, d);
                    op2 = mk_ea(m, r);
                    bind_opcode("1001 .... 00.. ....", 1, &M68K_ins_SUB, &M68K_disasm_BAD, (d << 9) | (m << 3) | r | 0x100, &op1, &op2);
                    bind_opcode("1001 .... 01.. ....", 2, &M68K_ins_SUB, &M68K_disasm_BAD, (d << 9) | (m << 3) | r | 0x100, &op1, &op2);
                    bind_opcode("1001 .... 10.. ....", 4, &M68K_ins_SUB, &M68K_disasm_BAD, (d << 9) | (m << 3) | r | 0x100, &op1, &op2);
                }
            }
        }
    }

    for (u32 a = 0; a < 8; a++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 7) && (r >= 5)) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(M68k_AM_address_register_direct, a);
                bind_opcode("1001 ...0 .11. ....", 2, &M68K_ins_SUBA, &M68K_disasm_BAD, (a << 9) | (m << 3) | r, &op1, &op2);
                bind_opcode("1001 ...1 .11. ....", 4, &M68K_ins_SUBA, &M68K_disasm_BAD, (a << 9) | (m << 3) | r, &op1, &op2);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0000 0100 00.. ....", 1, &M68K_ins_SUBI, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            bind_opcode("0000 0100 01.. ....", 2, &M68K_ins_SUBI, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            bind_opcode("0000 0100 10.. ....", 4, &M68K_ins_SUBI, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 7) && (r >= 2)) continue;
                op1 = mk_ea(M68k_AM_immediate, d ? d : 8);
                if (m == 1) {
                    op2 = mk_ea(M68k_AM_address_register_direct, r);
                    bind_opcode("0101 ...1 01.. ....", 2, &M68K_ins_SUBQ, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                    bind_opcode("0101 ...1 10.. ....", 4, &M68K_ins_SUBQ, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                }
                else {
                    op2 = mk_ea(m, r);
                    bind_opcode("0101 ...1 00.. ....", 1, &M68K_ins_SUBQ, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                    bind_opcode("0101 ...1 01.. ....", 2, &M68K_ins_SUBQ, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                    bind_opcode("0101 ...1 10.. ....", 4, &M68K_ins_SUBQ, &M68K_disasm_BAD, (d << 9) | (m << 3) | r, &op1, &op2);
                }
            }
        }
    }

    for (u32 x = 0; x < 8; x++) {
        for (u32 y = 0; y < 8; y++) {

            op1 = mk_ea(M68k_AM_data_register_direct, y);
            op2 = mk_ea(M68k_AM_data_register_direct, x);
            bind_opcode("1001 ...1 0000 ....", 1, &M68K_ins_SUBX, &M68K_disasm_BAD, (x << 9) | y, &op1, &op2);
            bind_opcode("1001 ...1 0100 ....", 2, &M68K_ins_SUBX, &M68K_disasm_BAD, (x << 9) | y, &op1, &op2);
            bind_opcode("1001 ...1 1000 ....", 4, &M68K_ins_SUBX, &M68K_disasm_BAD, (x << 9) | y, &op1, &op2);

            op1 = mk_ea(M68k_AM_address_register_indirect_with_predecrement, y);
            op2 = mk_ea(M68k_AM_address_register_indirect_with_predecrement, x);
            bind_opcode("1001 ...1 0000 ....", 1, &M68K_ins_SUBX, &M68K_disasm_BAD, (x << 9) | y | 8, &op1, &op2);
            bind_opcode("1001 ...1 0100 ....", 2, &M68K_ins_SUBX, &M68K_disasm_BAD, (x << 9) | y | 8, &op1, &op2);
            bind_opcode("1001 ...1 1000 ....", 4, &M68K_ins_SUBX, &M68K_disasm_BAD, (x << 9) | y | 8, &op1, &op2);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        op1 = mk_ea(M68k_AM_data_register_direct, d);
        bind_opcode("0100 1000 0100 0...", 4, &M68K_ins_SWAP, &M68K_disasm_BAD, d, &op1, NULL);
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 1010 11.. ....", 4, &M68K_ins_TAS, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    bind_opcode("0100 1110 0111 0110", 4, &M68K_ins_TRAP, &M68K_disasm_BAD, 0, NULL, NULL);

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            if (m!=1) bind_opcode("0100 1010 00.. ....", 1, &M68K_ins_TST, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            bind_opcode("0100 1010 01.. ....", 2, &M68K_ins_TST, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
            bind_opcode("0100 1010 10.. ....", 4, &M68K_ins_TST, &M68K_disasm_BAD, (m << 3) | r, &op1, NULL);
        }
    }

    for (u32 a = 0; a < 8; a++) {
        op1 = mk_ea(M68k_AM_address_register_direct, a);
        bind_opcode("0100 1110 0101 1...", 4, &M68K_ins_UNLK, &M68K_disasm_BAD, a, &op1, NULL);
    }
