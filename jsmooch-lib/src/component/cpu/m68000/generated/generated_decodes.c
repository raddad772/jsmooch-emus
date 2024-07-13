
    for (u32 x = 0; x < 8; x++) {
        for (u32 y = 0; y < 8; y++) {

            op1 = mk_ea(M68k_AM_data_register_direct, y);
            op2 = mk_ea(M68k_AM_data_register_direct, x);
            bind_opcode("1100 ...1 0000 ....", 1, &M68k_ins_ABCD, &M68k_disasm_ABCD, (x << 9) | y, &op1, &op2, 0, M68k_OM_r_r);

            op1 = mk_ea(M68k_AM_address_register_indirect_with_predecrement, y);
            op2 = mk_ea(M68k_AM_address_register_indirect_with_predecrement, x);
            bind_opcode("1100 ...1 0000 ....", 1, &M68k_ins_ABCD, &M68k_disasm_ABCD, (x << 9) | y | 8, &op1, &op2, 0, M68k_OM_ea_ea);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {

                if (!(((m == 7) && (r >= 5)))) {
                    op1 = mk_ea(m, r);
                    op2 = mk_ea(M68k_AM_data_register_direct, d);
                    if (m != 1) bind_opcode("1101 .... 00.. ....", 1, &M68k_ins_ADD, &M68k_disasm_ADD, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
                    bind_opcode("1101 .... 01.. ....", 2, &M68k_ins_ADD, &M68k_disasm_ADD, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
                    bind_opcode("1101 .... 10.. ....", 4, &M68k_ins_ADD, &M68k_disasm_ADD, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
                }
                if (!(((m <= 1) || ((m == 7) && (r >= 2))))) {
                    op1 = mk_ea(M68k_AM_data_register_direct, d);
                    op2 = mk_ea(m, r);
                    bind_opcode("1101 .... 00.. ....", 1, &M68k_ins_ADD, &M68k_disasm_ADD, (d << 9) | (m << 3) | r | 0x100, &op1, &op2, 0, M68k_OM_r_ea);
                    bind_opcode("1101 .... 01.. ....", 2, &M68k_ins_ADD, &M68k_disasm_ADD, (d << 9) | (m << 3) | r | 0x100, &op1, &op2, 0, M68k_OM_r_ea);
                    bind_opcode("1101 .... 10.. ....", 4, &M68k_ins_ADD, &M68k_disasm_ADD, (d << 9) | (m << 3) | r | 0x100, &op1, &op2, 0, M68k_OM_r_ea);
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
                bind_opcode("1101 ...0 11.. ....", 2, &M68k_ins_ADDA, &M68k_disasm_ADDA, (a << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
                bind_opcode("1101 ...1 11.. ....", 4, &M68k_ins_ADDA, &M68k_disasm_ADDA, (a << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0000 0110 00.. ....", 1, &M68k_ins_ADDI, &M68k_disasm_ADDI, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
            bind_opcode("0000 0110 01.. ....", 2, &M68k_ins_ADDI, &M68k_disasm_ADDI, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
            bind_opcode("0000 0110 10.. ....", 4, &M68k_ins_ADDI, &M68k_disasm_ADDI, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 7) && (r >= 2)) continue;
                op1 = mk_ea(M68k_AM_quick_immediate, d ? d : 8);
                if (m == 1) {
                    op2 = mk_ea(M68k_AM_address_register_direct, r);
                    bind_opcode("0101 ...0 01.. ....", 2, &M68k_ins_ADDQ_ar, &M68k_disasm_ADDQ_ar, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_qimm_r);
                    bind_opcode("0101 ...0 10.. ....", 4, &M68k_ins_ADDQ_ar, &M68k_disasm_ADDQ_ar, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_qimm_r);
                }
                else {
                    op2 = mk_ea(m, r);
                    bind_opcode("0101 ...0 00.. ....", 1, &M68k_ins_ADDQ, &M68k_disasm_ADDQ, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_qimm_ea);
                    bind_opcode("0101 ...0 01.. ....", 2, &M68k_ins_ADDQ, &M68k_disasm_ADDQ, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_qimm_ea);
                    bind_opcode("0101 ...0 10.. ....", 4, &M68k_ins_ADDQ, &M68k_disasm_ADDQ, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_qimm_ea);
                }
            }
        }
    }

    for (u32 x = 0; x < 8; x++) {
        for (u32 y = 0; y < 8; y++) {

            op1 = mk_ea(M68k_AM_data_register_direct, y);
            op2 = mk_ea(M68k_AM_data_register_direct, x);
            bind_opcode("1101 ...1 0000 ....", 1, &M68k_ins_ADDX, &M68k_disasm_ADDX, (x << 9) | y, &op1, &op2, 0, M68k_OM_r_r);
            bind_opcode("1101 ...1 0100 ....", 2, &M68k_ins_ADDX, &M68k_disasm_ADDX, (x << 9) | y, &op1, &op2, 0, M68k_OM_r_r);
            bind_opcode("1101 ...1 1000 ....", 4, &M68k_ins_ADDX, &M68k_disasm_ADDX, (x << 9) | y, &op1, &op2, 0, M68k_OM_r_r);

            op1 = mk_ea(M68k_AM_address_register_indirect_with_predecrement, y);
            op2 = mk_ea(M68k_AM_address_register_indirect_with_predecrement, x);
            bind_opcode("1101 ...1 0000 ....", 1, &M68k_ins_ADDX, &M68k_disasm_ADDX, (x << 9) | y | 8, &op1, &op2, 0, M68k_OM_ea_ea);
            bind_opcode("1101 ...1 0100 ....", 2, &M68k_ins_ADDX, &M68k_disasm_ADDX, (x << 9) | y | 8, &op1, &op2, 0, M68k_OM_ea_ea);
            bind_opcode("1101 ...1 1000 ....", 4, &M68k_ins_ADDX, &M68k_disasm_ADDX, (x << 9) | y | 8, &op1, &op2, 0, M68k_OM_ea_ea);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {

                if (!(((m == 1) || ((m == 7) && (r >= 5))))) {
                    op1 = mk_ea(m, r);
                    op2 = mk_ea(M68k_AM_data_register_direct, d);
                    bind_opcode("1100 .... 00.. ....", 1, &M68k_ins_AND, &M68k_disasm_AND, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
                    bind_opcode("1100 .... 01.. ....", 2, &M68k_ins_AND, &M68k_disasm_AND, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
                    bind_opcode("1100 .... 10.. ....", 4, &M68k_ins_AND, &M68k_disasm_AND, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
                }
                if (!(((m <= 1) || ((m == 7) && (r >= 2))))) {
                    op1 = mk_ea(M68k_AM_data_register_direct, d);
                    op2 = mk_ea(m, r);
                    bind_opcode("1100 .... 00.. ....", 1, &M68k_ins_AND, &M68k_disasm_AND, (d << 9) | (m << 3) | r | 0x100, &op1, &op2, 0, M68k_OM_r_ea);
                    bind_opcode("1100 .... 01.. ....", 2, &M68k_ins_AND, &M68k_disasm_AND, (d << 9) | (m << 3) | r | 0x100, &op1, &op2, 0, M68k_OM_r_ea);
                    bind_opcode("1100 .... 10.. ....", 4, &M68k_ins_AND, &M68k_disasm_AND, (d << 9) | (m << 3) | r | 0x100, &op1, &op2, 0, M68k_OM_r_ea);
                }
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0000 0010 00.. ....", 1, &M68k_ins_ANDI, &M68k_disasm_ANDI, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
            bind_opcode("0000 0010 01.. ....", 2, &M68k_ins_ANDI, &M68k_disasm_ANDI, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
            bind_opcode("0000 0010 10.. ....", 4, &M68k_ins_ANDI, &M68k_disasm_ANDI, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    bind_opcode("0000 0010 0011 1100", 4, &M68k_ins_ANDI_TO_CCR, &M68k_disasm_ANDI_TO_CCR, 0, NULL, NULL, 0, M68k_OM_none);

    bind_opcode("0000 0010 0111 1100", 4, &M68k_ins_ANDI_TO_SR, &M68k_disasm_ANDI_TO_SR, 0, NULL, NULL, 0, M68k_OM_none);

    for (u32 i = 0; i < 8; i++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_immediate, i ? i : 8);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...1 0000 0...", 1, &M68k_ins_ASL_qimm_dr, &M68k_disasm_ASL_qimm_dr, (i << 9) | d, &op1, &op2, 0, M68k_OM_qimm_r);
            bind_opcode("1110 ...1 0100 0...", 2, &M68k_ins_ASL_qimm_dr, &M68k_disasm_ASL_qimm_dr, (i << 9) | d, &op1, &op2, 0, M68k_OM_qimm_r);
            bind_opcode("1110 ...1 1000 0...", 4, &M68k_ins_ASL_qimm_dr, &M68k_disasm_ASL_qimm_dr, (i << 9) | d, &op1, &op2, 0, M68k_OM_qimm_r);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_data_register_direct, s);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...1 0010 0...", 1, &M68k_ins_ASL_dr_dr, &M68k_disasm_ASL_dr_dr, (s << 9) | d, &op1, &op2, 0, M68k_OM_r_r);
            bind_opcode("1110 ...1 0110 0...", 2, &M68k_ins_ASL_dr_dr, &M68k_disasm_ASL_dr_dr, (s << 9) | d, &op1, &op2, 0, M68k_OM_r_r);
            bind_opcode("1110 ...1 1010 0...", 4, &M68k_ins_ASL_dr_dr, &M68k_disasm_ASL_dr_dr, (s << 9) | d, &op1, &op2, 0, M68k_OM_r_r);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("1110 0001 11.. ....", 2, &M68k_ins_ASL_ea, &M68k_disasm_ASL_ea, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    for (u32 i = 0; i < 8; i++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_immediate, i ? i : 8);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...0 0000 0...", 1, &M68k_ins_ASR_qimm_dr, &M68k_disasm_ASR_qimm_dr, (i << 9) | d, &op1, &op2, 0, M68k_OM_qimm_r);
            bind_opcode("1110 ...0 0100 0...", 2, &M68k_ins_ASR_qimm_dr, &M68k_disasm_ASR_qimm_dr, (i << 9) | d, &op1, &op2, 0, M68k_OM_qimm_r);
            bind_opcode("1110 ...0 1000 0...", 4, &M68k_ins_ASR_qimm_dr, &M68k_disasm_ASR_qimm_dr, (i << 9) | d, &op1, &op2, 0, M68k_OM_qimm_r);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_data_register_direct, s);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...0 0010 0...", 1, &M68k_ins_ASR_dr_dr, &M68k_disasm_ASR_dr_dr, (s << 9) | d, &op1, &op2, 0, M68k_OM_r_r);
            bind_opcode("1110 ...0 0110 0...", 2, &M68k_ins_ASR_dr_dr, &M68k_disasm_ASR_dr_dr, (s << 9) | d, &op1, &op2, 0, M68k_OM_r_r);
            bind_opcode("1110 ...0 1010 0...", 4, &M68k_ins_ASR_dr_dr, &M68k_disasm_ASR_dr_dr, (s << 9) | d, &op1, &op2, 0, M68k_OM_r_r);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("1110 0000 11.. ....", 2, &M68k_ins_ASR_ea, &M68k_disasm_ASR_ea, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    for (u32 t = 0; t < 16; t++) {
        for (u32 d = 0; d < 256; d++) {
            if (t<=1) continue;
            op1 = mk_ea(M68k_AM_immediate, t);
            op2 = mk_ea(M68k_AM_immediate, d);
            bind_opcode("0110 .... .... ....", 4, &M68k_ins_BCC, &M68k_disasm_BCC, (t << 8) | d, &op1, &op2, 0, M68k_OM_qimm_qimm);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 2))) continue;
                op1 = mk_ea(M68k_AM_data_register_direct, d);
                op2 = mk_ea(m, r);
                if (m == 0) bind_opcode("0000 ...1 01.. ....", 4, &M68k_ins_BCHG_dr_ea, &M68k_disasm_BCHG_dr_ea, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_r_ea);
                else        bind_opcode("0000 ...1 01.. ....", 1, &M68k_ins_BCHG_dr_ea, &M68k_disasm_BCHG_dr_ea, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_r_ea);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            if (m == 0) bind_opcode("0000 1000 01.. ....", 4, &M68k_ins_BCHG_ea, &M68k_disasm_BCHG_ea, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
            else        bind_opcode("0000 1000 01.. ....", 1, &M68k_ins_BCHG_ea, &M68k_disasm_BCHG_ea, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 2))) continue;
                op1 = mk_ea(M68k_AM_data_register_direct, d);
                op2 = mk_ea(m, r);
                if (m == 0) bind_opcode("0000 ...1 10.. ....", 4, &M68k_ins_BCLR, &M68k_disasm_BCLR, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_r_ea);
                else        bind_opcode("0000 ...1 10.. ....", 1, &M68k_ins_BCLR, &M68k_disasm_BCLR, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_r_ea);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            if (m == 0) bind_opcode("0000 1000 10.. ....", 4, &M68k_ins_BCLR, &M68k_disasm_BCLR, (m << 3) | r, &op1, NULL, 1, M68k_OM_ea);
            else        bind_opcode("0000 1000 10.. ....", 1, &M68k_ins_BCLR, &M68k_disasm_BCLR, (m << 3) | r, &op1, NULL, 1, M68k_OM_ea);
        }
    }

    for (u32 d = 0; d < 256; d++) {
        op1 = mk_ea(M68k_AM_immediate, d);
        bind_opcode("0110 0000 .... ....", 4, &M68k_ins_BRA, &M68k_disasm_BRA, d, &op1, NULL, 0, M68k_OM_qimm);
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 2))) continue;
                op1 = mk_ea(M68k_AM_data_register_direct, d);
                op2 = mk_ea(m, r);
                if (m == 0) bind_opcode("0000 ...1 11.. ....", 4, &M68k_ins_BSET, &M68k_disasm_BSET, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_r_ea);
                else        bind_opcode("0000 ...1 11.. ....", 1, &M68k_ins_BSET, &M68k_disasm_BSET, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_r_ea);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            if (m == 0) bind_opcode("0000 1000 11.. ....", 4, &M68k_ins_BSET, &M68k_disasm_BSET, (m << 3) | r, &op1, NULL, 1, M68k_OM_ea);
            else        bind_opcode("0000 1000 11.. ....", 1, &M68k_ins_BSET, &M68k_disasm_BSET, (m << 3) | r, &op1, NULL, 1, M68k_OM_ea);
        }
    }

    for (u32 d = 0; d < 256; d++) {
        op1 = mk_ea(M68k_AM_immediate, d);
        bind_opcode("0110 0001 .... ....", 4, &M68k_ins_BSR, &M68k_disasm_BSR, d, &op1, NULL, 0, M68k_OM_qimm);
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 5))) continue;
                op1 = mk_ea(M68k_AM_data_register_direct, d);
                op2 = mk_ea(m, r);
                if (m == 0) bind_opcode("0000 ...1 00.. ....", 4, &M68k_ins_BTST_dr_ea, &M68k_disasm_BTST_dr_ea, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_r_ea);
                else        bind_opcode("0000 ...1 00.. ....", 1, &M68k_ins_BTST_dr_ea, &M68k_disasm_BTST_dr_ea, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_r_ea);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 4))) continue;
            op1 = mk_ea(m, r);
            if (m == 0) bind_opcode("0000 1000 00.. ....", 4, &M68k_ins_BTST_ea, &M68k_disasm_BTST_ea, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
            else        bind_opcode("0000 1000 00.. ....", 1, &M68k_ins_BTST_ea, &M68k_disasm_BTST_ea, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 5))) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(M68k_AM_data_register_direct, d);
                bind_opcode("0100 ...1 10.. ....", 2, &M68k_ins_CHK, &M68k_disasm_CHK, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 0010 00.. ....", 1, &M68k_ins_CLR, &M68k_disasm_CLR, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
            bind_opcode("0100 0010 01.. ....", 2, &M68k_ins_CLR, &M68k_disasm_CLR, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
            bind_opcode("0100 0010 10.. ....", 4, &M68k_ins_CLR, &M68k_disasm_CLR, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 7) && (r >= 5)) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(M68k_AM_data_register_direct, d);
                if (m!=1) bind_opcode("1011 ...0 00.. ....", 1, &M68k_ins_CMP, &M68k_disasm_CMP, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
                bind_opcode("1011 ...0 01.. ....", 2, &M68k_ins_CMP, &M68k_disasm_CMP, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
                bind_opcode("1011 ...0 10.. ....", 4, &M68k_ins_CMP, &M68k_disasm_CMP, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
            }
        }
    }

    for (u32 a = 0; a < 8; a++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 7) && (r >= 5)) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(M68k_AM_address_register_direct, a);
                bind_opcode("1011 ...0 11.. ....", 2, &M68k_ins_CMPA, &M68k_disasm_CMPA, (a << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
                bind_opcode("1011 ...1 11.. ....", 4, &M68k_ins_CMPA, &M68k_disasm_CMPA, (a << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0000 1100 00.. ....", 1, &M68k_ins_CMPI, &M68k_disasm_CMPI, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
            bind_opcode("0000 1100 01.. ....", 2, &M68k_ins_CMPI, &M68k_disasm_CMPI, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
            bind_opcode("0000 1100 10.. ....", 4, &M68k_ins_CMPI, &M68k_disasm_CMPI, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    for (u32 x = 0; x < 8; x++) {
        for (u32 y = 0; y < 8; y++) {
            op1 = mk_ea(M68k_AM_address_register_indirect_with_postincrement, y);
            op2 = mk_ea(M68k_AM_address_register_indirect_with_postincrement, x);
            bind_opcode("1011 ...1 0000 1...", 1, &M68k_ins_CMPM, &M68k_disasm_CMPM, (x << 9) | y, &op1, &op2, 0, M68k_OM_ea_ea);
            bind_opcode("1011 ...1 0100 1...", 2, &M68k_ins_CMPM, &M68k_disasm_CMPM, (x << 9) | y, &op1, &op2, 0, M68k_OM_ea_ea);
            bind_opcode("1011 ...1 1000 1...", 4, &M68k_ins_CMPM, &M68k_disasm_CMPM, (x << 9) | y, &op1, &op2, 0, M68k_OM_ea_ea);
        }
    }

    for (u32 c = 0; c < 16; c++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_immediate, c);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("0101 .... 1100 1...", 4, &M68k_ins_DBCC, &M68k_disasm_DBCC, (c << 8) | d, &op1, &op2, 0, M68k_OM_qimm_r);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 5))) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(M68k_AM_data_register_direct, d);
                bind_opcode("1000 ...1 11.. ....", 4, &M68k_ins_DIVS, &M68k_disasm_DIVS, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
            }
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 5))) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(M68k_AM_data_register_direct, d);
                bind_opcode("1000 ...0 11.. ....", 2, &M68k_ins_DIVU, &M68k_disasm_DIVU, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
            }
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 2))) continue;
                op1 = mk_ea(M68k_AM_data_register_direct, d);
                op2 = mk_ea(m, r);
                bind_opcode("1011 ...1 00.. ....", 1, &M68k_ins_EOR, &M68k_disasm_EOR, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_r_ea);
                bind_opcode("1011 ...1 01.. ....", 2, &M68k_ins_EOR, &M68k_disasm_EOR, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_r_ea);
                bind_opcode("1011 ...1 10.. ....", 4, &M68k_ins_EOR, &M68k_disasm_EOR, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_r_ea);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0000 1010 00.. ....", 1, &M68k_ins_EORI, &M68k_disasm_EORI, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
            bind_opcode("0000 1010 01.. ....", 2, &M68k_ins_EORI, &M68k_disasm_EORI, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
            bind_opcode("0000 1010 10.. ....", 4, &M68k_ins_EORI, &M68k_disasm_EORI, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    bind_opcode("0000 1010 0011 1100", 4, &M68k_ins_EORI_TO_CCR, &M68k_disasm_EORI_TO_CCR, 0, NULL, NULL, 0, M68k_OM_none);

    bind_opcode("0000 1010 0111 1100", 4, &M68k_ins_EORI_TO_SR, &M68k_disasm_EORI_TO_SR, 0, NULL, NULL, 0, M68k_OM_none);

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_data_register_direct, s);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1100 ...1 0100 0...", 4, &M68k_ins_EXG, &M68k_disasm_EXG, (s << 9) | d, &op1, &op2, 0, M68k_OM_r_r);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_address_register_direct, s);
            op2 = mk_ea(M68k_AM_address_register_direct, d);
            bind_opcode("1100 ...1 0100 1...", 4, &M68k_ins_EXG, &M68k_disasm_EXG, (s << 9) | d, &op1, &op2, 1, M68k_OM_r_r);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_address_register_direct, d);
            op2 = mk_ea(M68k_AM_data_register_direct, s);
            bind_opcode("1100 ...1 1000 1...", 4, &M68k_ins_EXG, &M68k_disasm_EXG, (s << 9) | d, &op1, &op2, 2, M68k_OM_r_r);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        op1 = mk_ea(M68k_AM_data_register_direct, d);
        bind_opcode("0100 1000 1000 0...", 2, &M68k_ins_EXT, &M68k_disasm_EXT, d, &op1, NULL, 0, M68k_OM_r);
        bind_opcode("0100 1000 1100 0...", 4, &M68k_ins_EXT, &M68k_disasm_EXT, d, &op1, NULL, 0, M68k_OM_r);
    }

    bind_opcode("0100 1010 1111 1100", 4, &M68k_ins_ILLEGAL, &M68k_disasm_ILLEGAL, 0, NULL, NULL, 0, M68k_OM_none);

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || (m == 3) || (m == 4) || ((m == 7) && (r >= 4))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 1110 11.. ....", 4, &M68k_ins_JMP, &M68k_disasm_JMP, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || (m == 3) || (m == 4) || ((m == 7) && (r >= 4))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 1110 10.. ....", 4, &M68k_ins_JSR, &M68k_disasm_JSR, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    for (u32 a = 0; a < 8; a++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m <= 1) || (m == 3) || (m == 4) || ((m == 7) && (r >= 4))) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(M68k_AM_address_register_direct, a);
                bind_opcode("0100 ...1 11.. ....", 4, &M68k_ins_LEA, &M68k_disasm_LEA, (a << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
            }
        }
    }

    for (u32 a = 0; a < 8; a++) {
        op1 = mk_ea(M68k_AM_address_register_direct, a);
        bind_opcode("0100 1110 0101 0...", 4, &M68k_ins_LINK, &M68k_disasm_LINK, a, &op1, NULL, 0, M68k_OM_r);
    }

    for (u32 i = 0; i < 8; i++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_immediate, i ? i : 8);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...1 0000 1...", 1, &M68k_ins_LSL_qimm_dr, &M68k_disasm_LSL_qimm_dr, (i << 9) | d, &op1, &op2, 0, M68k_OM_qimm_r);
            bind_opcode("1110 ...1 0100 1...", 2, &M68k_ins_LSL_qimm_dr, &M68k_disasm_LSL_qimm_dr, (i << 9) | d, &op1, &op2, 0, M68k_OM_qimm_r);
            bind_opcode("1110 ...1 1000 1...", 4, &M68k_ins_LSL_qimm_dr, &M68k_disasm_LSL_qimm_dr, (i << 9) | d, &op1, &op2, 0, M68k_OM_qimm_r);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_data_register_direct, s);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...1 0010 1...", 1, &M68k_ins_LSL_dr_dr, &M68k_disasm_LSL_dr_dr, (s << 9) | d, &op1, &op2, 0, M68k_OM_r_r);
            bind_opcode("1110 ...1 0110 1...", 2, &M68k_ins_LSL_dr_dr, &M68k_disasm_LSL_dr_dr, (s << 9) | d, &op1, &op2, 0, M68k_OM_r_r);
            bind_opcode("1110 ...1 1010 1...", 4, &M68k_ins_LSL_dr_dr, &M68k_disasm_LSL_dr_dr, (s << 9) | d, &op1, &op2, 0, M68k_OM_r_r);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("1110 0011 11.. ....", 2, &M68k_ins_LSL_ea, &M68k_disasm_LSL_ea, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    for (u32 i = 0; i < 8; i++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_immediate, i ? i : 8);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...0 0000 1...", 1, &M68k_ins_LSR_qimm_dr, &M68k_disasm_LSR_qimm_dr, (i << 9) | d, &op1, &op2, 0, M68k_OM_qimm_r);
            bind_opcode("1110 ...0 0100 1...", 2, &M68k_ins_LSR_qimm_dr, &M68k_disasm_LSR_qimm_dr, (i << 9) | d, &op1, &op2, 0, M68k_OM_qimm_r);
            bind_opcode("1110 ...0 1000 1...", 4, &M68k_ins_LSR_qimm_dr, &M68k_disasm_LSR_qimm_dr, (i << 9) | d, &op1, &op2, 0, M68k_OM_qimm_r);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_data_register_direct, s);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...0 0010 1...", 1, &M68k_ins_LSR_dr_dr, &M68k_disasm_LSR_dr_dr, (s << 9) | d, &op1, &op2, 0, M68k_OM_r_r);
            bind_opcode("1110 ...0 0110 1...", 2, &M68k_ins_LSR_dr_dr, &M68k_disasm_LSR_dr_dr, (s << 9) | d, &op1, &op2, 0, M68k_OM_r_r);
            bind_opcode("1110 ...0 1010 1...", 4, &M68k_ins_LSR_dr_dr, &M68k_disasm_LSR_dr_dr, (s << 9) | d, &op1, &op2, 0, M68k_OM_r_r);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("1110 0010 11.. ....", 2, &M68k_ins_LSR_ea, &M68k_disasm_LSR_ea, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    for (u32 a = 0; a < 8; a++) {
        for (u32 b = 0; b < 8; b++) {
            for (u32 c = 0; c < 8; c++) {
                for (u32 d = 0; d < 8; d++) {
                    if (((b == 1) || ((b == 7) && (a >= 2))) || ((c == 7) && (d >= 5))) continue;
                    op1 = mk_ea(c, d);
                    op2 = mk_ea(b, a);
                    if (c!=1) bind_opcode("0001 .... .... ....", 1, &M68k_ins_MOVE, &M68k_disasm_MOVE, (a << 9) | (b << 6) | (c << 3) | d, &op1, &op2, 0, M68k_OM_ea_ea);
                    bind_opcode("0011 .... .... ....", 2, &M68k_ins_MOVE, &M68k_disasm_MOVE, (a << 9) | (b << 6) | (c << 3) | d, &op1, &op2, 0, M68k_OM_ea_ea);
                    bind_opcode("0010 .... .... ....", 4, &M68k_ins_MOVE, &M68k_disasm_MOVE, (a << 9) | (b << 6) | (c << 3) | d, &op1, &op2, 0, M68k_OM_ea_ea);
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
                bind_opcode("0011 ...0 01.. ....", 2, &M68k_ins_MOVEA, &M68k_disasm_MOVEA, (a << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
                bind_opcode("0010 ...0 01.. ....", 4, &M68k_ins_MOVEA, &M68k_disasm_MOVEA, (a << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || (m == 3) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 1000 10.. ....", 2, &M68k_ins_MOVEM_TO_MEM, &M68k_disasm_MOVEM_TO_MEM, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
            bind_opcode("0100 1000 11.. ....", 4, &M68k_ins_MOVEM_TO_MEM, &M68k_disasm_MOVEM_TO_MEM, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || (m == 4) || ((m == 7) && (r >= 4))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 1100 10.. ....", 2, &M68k_ins_MOVEM_TO_REG, &M68k_disasm_MOVEM_TO_REG, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
            bind_opcode("0100 1100 11.. ....", 4, &M68k_ins_MOVEM_TO_REG, &M68k_disasm_MOVEM_TO_REG, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 r = 0; r < 8; r++) {

            op1 = mk_ea(M68k_AM_address_register_indirect_with_displacement, r);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("0000 ...1 .000 1...", 2, &M68k_ins_MOVEP, &M68k_disasm_MOVEP, (d << 9) | r, &op1, &op2, 0, M68k_OM_ea_r);
            bind_opcode("0000 ...1 .100 1...", 4, &M68k_ins_MOVEP, &M68k_disasm_MOVEP, (d << 9) | r, &op1, &op2, 0, M68k_OM_ea_r);

            op1 = mk_ea(M68k_AM_data_register_direct, d);
            op2 = mk_ea(M68k_AM_address_register_indirect_with_displacement, r);
            bind_opcode("0000 ...1 .000 1...", 2, &M68k_ins_MOVEP, &M68k_disasm_MOVEP, (d << 9) | r | 0x80, &op1, &op2, 0, M68k_OM_r_ea);
            bind_opcode("0000 ...1 .100 1...", 4, &M68k_ins_MOVEP, &M68k_disasm_MOVEP, (d << 9) | r | 0x80, &op1, &op2, 0, M68k_OM_r_ea);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 i = 0; i < 256; i++) {
            op1 = mk_ea(M68k_AM_immediate, i);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("0111 ...0 .... ....", 4, &M68k_ins_MOVEQ, &M68k_disasm_MOVEQ, (d << 9) | i, &op1, &op2, 0, M68k_OM_qimm_r);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 0000 11.. ....", 2, &M68k_ins_MOVE_FROM_SR, &M68k_disasm_MOVE_FROM_SR, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 5))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 0100 11.. ....", 4, &M68k_ins_MOVE_TO_CCR, &M68k_disasm_MOVE_TO_CCR, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 5))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 0110 11.. ....", 2, &M68k_ins_MOVE_TO_SR, &M68k_disasm_MOVE_TO_SR, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    for (u32 a = 0; a < 8; a++) {
        op1 = mk_ea(M68k_AM_address_register_direct, a);
        bind_opcode("0100 1110 0110 1...", 4, &M68k_ins_MOVE_FROM_USP, &M68k_disasm_MOVE_FROM_USP, a, &op1, NULL, 0, M68k_OM_r);
    }

    for (u32 a = 0; a < 8; a++) {
        op1 = mk_ea(M68k_AM_address_register_direct, a);
        bind_opcode("0100 1110 0110 0...", 4, &M68k_ins_MOVE_TO_USP, &M68k_disasm_MOVE_TO_USP, a, &op1, NULL, 0, M68k_OM_r);
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 5))) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(M68k_AM_data_register_direct, d);
                bind_opcode("1100 ...1 11.. ....", 2, &M68k_ins_MULS, &M68k_disasm_MULS, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
            }
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 5))) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(M68k_AM_data_register_direct, d);
                bind_opcode("1100 ...0 11.. ....", 2, &M68k_ins_MULU, &M68k_disasm_MULU, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 1000 00.. ....", 1, &M68k_ins_NBCD, &M68k_disasm_NBCD, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 0100 00.. ....", 1, &M68k_ins_NEG, &M68k_disasm_NEG, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
            bind_opcode("0100 0100 01.. ....", 2, &M68k_ins_NEG, &M68k_disasm_NEG, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
            bind_opcode("0100 0100 10.. ....", 4, &M68k_ins_NEG, &M68k_disasm_NEG, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 0000 00.. ....", 1, &M68k_ins_NEGX, &M68k_disasm_NEGX, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
            bind_opcode("0100 0000 01.. ....", 2, &M68k_ins_NEGX, &M68k_disasm_NEGX, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
            bind_opcode("0100 0000 10.. ....", 4, &M68k_ins_NEGX, &M68k_disasm_NEGX, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    bind_opcode("0100 1110 0111 0001", 4, &M68k_ins_NOP, &M68k_disasm_NOP, 0, NULL, NULL, 0, M68k_OM_none);

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 0110 00.. ....", 1, &M68k_ins_NOT, &M68k_disasm_NOT, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
            bind_opcode("0100 0110 01.. ....", 2, &M68k_ins_NOT, &M68k_disasm_NOT, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
            bind_opcode("0100 0110 10.. ....", 4, &M68k_ins_NOT, &M68k_disasm_NOT, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {

                if (!(((m == 1) || ((m == 7) && (r >= 5))))) {
                    op1 = mk_ea(m, r);
                    op2 = mk_ea(M68k_AM_data_register_direct, d);
                    bind_opcode("1000 .... 00.. ....", 1, &M68k_ins_OR, &M68k_disasm_OR, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
                    bind_opcode("1000 .... 01.. ....", 2, &M68k_ins_OR, &M68k_disasm_OR, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
                    bind_opcode("1000 .... 10.. ....", 4, &M68k_ins_OR, &M68k_disasm_OR, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
                }
                if (!(((m <= 1) || ((m == 7) && (r >= 2))))) {
                    op1 = mk_ea(M68k_AM_data_register_direct, d);
                    op2 = mk_ea(m, r);
                    bind_opcode("1000 .... 00.. ....", 1, &M68k_ins_OR, &M68k_disasm_OR, (d << 9) | (m << 3) | r | 0x100, &op1, &op2, 0, M68k_OM_r_ea);
                    bind_opcode("1000 .... 01.. ....", 2, &M68k_ins_OR, &M68k_disasm_OR, (d << 9) | (m << 3) | r | 0x100, &op1, &op2, 0, M68k_OM_r_ea);
                    bind_opcode("1000 .... 10.. ....", 4, &M68k_ins_OR, &M68k_disasm_OR, (d << 9) | (m << 3) | r | 0x100, &op1, &op2, 0, M68k_OM_r_ea);
                }
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0000 0000 00.. ....", 1, &M68k_ins_ORI, &M68k_disasm_ORI, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
            bind_opcode("0000 0000 01.. ....", 2, &M68k_ins_ORI, &M68k_disasm_ORI, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
            bind_opcode("0000 0000 10.. ....", 4, &M68k_ins_ORI, &M68k_disasm_ORI, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    bind_opcode("0000 0000 0011 1100", 4, &M68k_ins_ORI_TO_CCR, &M68k_disasm_ORI_TO_CCR, 0, NULL, NULL, 0, M68k_OM_none);

    bind_opcode("0000 0000 0111 1100", 4, &M68k_ins_ORI_TO_SR, &M68k_disasm_ORI_TO_SR, 0, NULL, NULL, 0, M68k_OM_none);

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || (m == 3) || (m == 4) || ((m == 7) && (r >= 4))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 1000 01.. ....", 4, &M68k_ins_PEA, &M68k_disasm_PEA, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    bind_opcode("0100 1110 0111 0000", 4, &M68k_ins_RESET, &M68k_disasm_RESET, 0, NULL, NULL, 0, M68k_OM_none);

    for (u32 i = 0; i < 8; i++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_immediate, i ? i : 8);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...1 0001 1...", 1, &M68k_ins_ROL_qimm_dr, &M68k_disasm_ROL_qimm_dr, (i << 9) | d, &op1, &op2, 0, M68k_OM_qimm_r);
            bind_opcode("1110 ...1 0101 1...", 2, &M68k_ins_ROL_qimm_dr, &M68k_disasm_ROL_qimm_dr, (i << 9) | d, &op1, &op2, 0, M68k_OM_qimm_r);
            bind_opcode("1110 ...1 1001 1...", 4, &M68k_ins_ROL_qimm_dr, &M68k_disasm_ROL_qimm_dr, (i << 9) | d, &op1, &op2, 0, M68k_OM_qimm_r);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_data_register_direct, s);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...1 0011 1...", 1, &M68k_ins_ROL_dr_dr, &M68k_disasm_ROL_dr_dr, (s << 9) | d, &op1, &op2, 0, M68k_OM_r_r);
            bind_opcode("1110 ...1 0111 1...", 2, &M68k_ins_ROL_dr_dr, &M68k_disasm_ROL_dr_dr, (s << 9) | d, &op1, &op2, 0, M68k_OM_r_r);
            bind_opcode("1110 ...1 1011 1...", 4, &M68k_ins_ROL_dr_dr, &M68k_disasm_ROL_dr_dr, (s << 9) | d, &op1, &op2, 0, M68k_OM_r_r);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("1110 0111 11.. ....", 2, &M68k_ins_ROL_ea, &M68k_disasm_ROL_ea, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    for (u32 i = 0; i < 8; i++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_immediate, i ? i : 8);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...0 0001 1...", 1, &M68k_ins_ROR_qimm_dr, &M68k_disasm_ROR_qimm_dr, (i << 9) | d, &op1, &op2, 0, M68k_OM_qimm_r);
            bind_opcode("1110 ...0 0101 1...", 2, &M68k_ins_ROR_qimm_dr, &M68k_disasm_ROR_qimm_dr, (i << 9) | d, &op1, &op2, 0, M68k_OM_qimm_r);
            bind_opcode("1110 ...0 1001 1...", 4, &M68k_ins_ROR_qimm_dr, &M68k_disasm_ROR_qimm_dr, (i << 9) | d, &op1, &op2, 0, M68k_OM_qimm_r);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_data_register_direct, s);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...0 0011 1...", 1, &M68k_ins_ROR_dr_dr, &M68k_disasm_ROR_dr_dr, (s << 9) | d, &op1, &op2, 0, M68k_OM_r_r);
            bind_opcode("1110 ...0 0111 1...", 2, &M68k_ins_ROR_dr_dr, &M68k_disasm_ROR_dr_dr, (s << 9) | d, &op1, &op2, 0, M68k_OM_r_r);
            bind_opcode("1110 ...0 1011 1...", 4, &M68k_ins_ROR_dr_dr, &M68k_disasm_ROR_dr_dr, (s << 9) | d, &op1, &op2, 0, M68k_OM_r_r);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("1110 0110 11.. ....", 2, &M68k_ins_ROR_ea, &M68k_disasm_ROR_ea, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    for (u32 i = 0; i < 8; i++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_immediate, i ? i : 8);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...1 0001 0...", 1, &M68k_ins_ROXL_qimm_dr, &M68k_disasm_ROXL_qimm_dr, (i << 9) | d, &op1, &op2, 0, M68k_OM_qimm_r);
            bind_opcode("1110 ...1 0101 0...", 2, &M68k_ins_ROXL_qimm_dr, &M68k_disasm_ROXL_qimm_dr, (i << 9) | d, &op1, &op2, 0, M68k_OM_qimm_r);
            bind_opcode("1110 ...1 1001 0...", 4, &M68k_ins_ROXL_qimm_dr, &M68k_disasm_ROXL_qimm_dr, (i << 9) | d, &op1, &op2, 0, M68k_OM_qimm_r);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_data_register_direct, s);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...1 0011 0...", 1, &M68k_ins_ROXL_dr_dr, &M68k_disasm_ROXL_dr_dr, (s << 9) | d, &op1, &op2, 0, M68k_OM_r_r);
            bind_opcode("1110 ...1 0111 0...", 2, &M68k_ins_ROXL_dr_dr, &M68k_disasm_ROXL_dr_dr, (s << 9) | d, &op1, &op2, 0, M68k_OM_r_r);
            bind_opcode("1110 ...1 1011 0...", 4, &M68k_ins_ROXL_dr_dr, &M68k_disasm_ROXL_dr_dr, (s << 9) | d, &op1, &op2, 0, M68k_OM_r_r);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("1110 0101 11.. ....", 2, &M68k_ins_ROXL_ea, &M68k_disasm_ROXL_ea, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    for (u32 i = 0; i < 8; i++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_immediate, i ? i : 8);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...0 0001 0...", 1, &M68k_ins_ROXR_qimm_dr, &M68k_disasm_ROXR_qimm_dr, (i << 9) | d, &op1, &op2, 0, M68k_OM_qimm_r);
            bind_opcode("1110 ...0 0101 0...", 2, &M68k_ins_ROXR_qimm_dr, &M68k_disasm_ROXR_qimm_dr, (i << 9) | d, &op1, &op2, 0, M68k_OM_qimm_r);
            bind_opcode("1110 ...0 1001 0...", 4, &M68k_ins_ROXR_qimm_dr, &M68k_disasm_ROXR_qimm_dr, (i << 9) | d, &op1, &op2, 0, M68k_OM_qimm_r);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(M68k_AM_data_register_direct, s);
            op2 = mk_ea(M68k_AM_data_register_direct, d);
            bind_opcode("1110 ...0 0011 0...", 1, &M68k_ins_ROXR_dr_dr, &M68k_disasm_ROXR_dr_dr, (s << 9) | d, &op1, &op2, 0, M68k_OM_r_r);
            bind_opcode("1110 ...0 0111 0...", 2, &M68k_ins_ROXR_dr_dr, &M68k_disasm_ROXR_dr_dr, (s << 9) | d, &op1, &op2, 0, M68k_OM_r_r);
            bind_opcode("1110 ...0 1011 0...", 4, &M68k_ins_ROXR_dr_dr, &M68k_disasm_ROXR_dr_dr, (s << 9) | d, &op1, &op2, 0, M68k_OM_r_r);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("1110 0100 11.. ....", 2, &M68k_ins_ROXR_ea, &M68k_disasm_ROXR_ea, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    bind_opcode("0100 1110 0111 0011", 4, &M68k_ins_RTE, &M68k_disasm_RTE, 0, NULL, NULL, 0, M68k_OM_none);

    bind_opcode("0100 1110 0111 0111", 4, &M68k_ins_RTR, &M68k_disasm_RTR, 0, NULL, NULL, 0, M68k_OM_none);

    bind_opcode("0100 1110 0111 0101", 4, &M68k_ins_RTS, &M68k_disasm_RTS, 0, NULL, NULL, 0, M68k_OM_none);

    for (u32 x = 0; x < 8; x++) {
        for (u32 y = 0; y < 8; y++) {

            op1 = mk_ea(M68k_AM_data_register_direct, y);
            op2 = mk_ea(M68k_AM_data_register_direct, x);
            bind_opcode("1000 ...1 0000 ....", 1, &M68k_ins_SBCD, &M68k_disasm_SBCD, (x << 9) | y, &op1, &op2, 0, M68k_OM_r_r);

            op1 = mk_ea(M68k_AM_address_register_indirect_with_predecrement, y);
            op2 = mk_ea(M68k_AM_address_register_indirect_with_predecrement, x);
            bind_opcode("1000 ...1 0000 ....", 1, &M68k_ins_SBCD, &M68k_disasm_SBCD, (x << 9) | y | 8, &op1, &op2, 0, M68k_OM_ea_ea);
        }
    }

    for (u32 t = 0; t < 16; t++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 2))) continue;
                op1 = mk_ea(M68k_AM_immediate, t);
                op2 = mk_ea(m, r);
                bind_opcode("0101 .... 11.. ....", 1, &M68k_ins_SCC, &M68k_disasm_SCC, (t << 8) | (m << 3) | r, &op1, &op2, 0, M68k_OM_qimm_ea);
            }
        }
    }

    bind_opcode("0100 1110 0111 0010", 4, &M68k_ins_STOP, &M68k_disasm_STOP, 0, NULL, NULL, 0, M68k_OM_none);

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {

                if (!(((m == 7) && (r >= 5)))) {
                    op1 = mk_ea(m, r);
                    op2 = mk_ea(M68k_AM_data_register_direct, d);
                    bind_opcode("1001 .... 00.. ....", 1, &M68k_ins_SUB, &M68k_disasm_SUB, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
                    bind_opcode("1001 .... 01.. ....", 2, &M68k_ins_SUB, &M68k_disasm_SUB, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
                    bind_opcode("1001 .... 10.. ....", 4, &M68k_ins_SUB, &M68k_disasm_SUB, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
                }
                if (!(((m <= 1) || ((m == 7) && (r >= 2))))) {
                    op1 = mk_ea(M68k_AM_data_register_direct, d);
                    op2 = mk_ea(m, r);
                    bind_opcode("1001 .... 00.. ....", 1, &M68k_ins_SUB, &M68k_disasm_SUB, (d << 9) | (m << 3) | r | 0x100, &op1, &op2, 0, M68k_OM_r_ea);
                    bind_opcode("1001 .... 01.. ....", 2, &M68k_ins_SUB, &M68k_disasm_SUB, (d << 9) | (m << 3) | r | 0x100, &op1, &op2, 0, M68k_OM_r_ea);
                    bind_opcode("1001 .... 10.. ....", 4, &M68k_ins_SUB, &M68k_disasm_SUB, (d << 9) | (m << 3) | r | 0x100, &op1, &op2, 0, M68k_OM_r_ea);
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
                bind_opcode("1001 ...0 11.. ....", 2, &M68k_ins_SUBA, &M68k_disasm_SUBA, (a << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
                bind_opcode("1001 ...1 11.. ....", 4, &M68k_ins_SUBA, &M68k_disasm_SUBA, (a << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_ea_r);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0000 0100 00.. ....", 1, &M68k_ins_SUBI, &M68k_disasm_SUBI, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
            bind_opcode("0000 0100 01.. ....", 2, &M68k_ins_SUBI, &M68k_disasm_SUBI, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
            bind_opcode("0000 0100 10.. ....", 4, &M68k_ins_SUBI, &M68k_disasm_SUBI, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 7) && (r >= 2)) continue;
                op1 = mk_ea(M68k_AM_quick_immediate, d ? d : 8);
                if (m == 1) {
                    op2 = mk_ea(M68k_AM_address_register_direct, r);
                    bind_opcode("0101 ...1 01.. ....", 2, &M68k_ins_SUBQ_ar, &M68k_disasm_SUBQ_ar, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_qimm_r);
                    bind_opcode("0101 ...1 10.. ....", 4, &M68k_ins_SUBQ_ar, &M68k_disasm_SUBQ_ar, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_qimm_r);
                }
                else {
                    op2 = mk_ea(m, r);
                    bind_opcode("0101 ...1 00.. ....", 1, &M68k_ins_SUBQ, &M68k_disasm_SUBQ, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_qimm_ea);
                    bind_opcode("0101 ...1 01.. ....", 2, &M68k_ins_SUBQ, &M68k_disasm_SUBQ, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_qimm_ea);
                    bind_opcode("0101 ...1 10.. ....", 4, &M68k_ins_SUBQ, &M68k_disasm_SUBQ, (d << 9) | (m << 3) | r, &op1, &op2, 0, M68k_OM_qimm_ea);
                }
            }
        }
    }

    for (u32 x = 0; x < 8; x++) {
        for (u32 y = 0; y < 8; y++) {

            op1 = mk_ea(M68k_AM_data_register_direct, y);
            op2 = mk_ea(M68k_AM_data_register_direct, x);
            bind_opcode("1001 ...1 0000 ....", 1, &M68k_ins_SUBX, &M68k_disasm_SUBX, (x << 9) | y, &op1, &op2, 0, M68k_OM_r_r);
            bind_opcode("1001 ...1 0100 ....", 2, &M68k_ins_SUBX, &M68k_disasm_SUBX, (x << 9) | y, &op1, &op2, 0, M68k_OM_r_r);
            bind_opcode("1001 ...1 1000 ....", 4, &M68k_ins_SUBX, &M68k_disasm_SUBX, (x << 9) | y, &op1, &op2, 0, M68k_OM_r_r);

            op1 = mk_ea(M68k_AM_address_register_indirect_with_predecrement, y);
            op2 = mk_ea(M68k_AM_address_register_indirect_with_predecrement, x);
            bind_opcode("1001 ...1 0000 ....", 1, &M68k_ins_SUBX, &M68k_disasm_SUBX, (x << 9) | y | 8, &op1, &op2, 0, M68k_OM_ea_ea);
            bind_opcode("1001 ...1 0100 ....", 2, &M68k_ins_SUBX, &M68k_disasm_SUBX, (x << 9) | y | 8, &op1, &op2, 0, M68k_OM_ea_ea);
            bind_opcode("1001 ...1 1000 ....", 4, &M68k_ins_SUBX, &M68k_disasm_SUBX, (x << 9) | y | 8, &op1, &op2, 0, M68k_OM_ea_ea);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        op1 = mk_ea(M68k_AM_data_register_direct, d);
        bind_opcode("0100 1000 0100 0...", 4, &M68k_ins_SWAP, &M68k_disasm_SWAP, d, &op1, NULL, 0, M68k_OM_r);
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 1010 11.. ....", 1, &M68k_ins_TAS, &M68k_disasm_TAS, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    for (u32 v = 0; v < 16; v++) {
        op1 = mk_ea(M68k_AM_immediate, v);
        bind_opcode("0100 1110 0100 ....", 4, &M68k_ins_TRAP, &M68k_disasm_TRAP, v, &op1, NULL, 0, M68k_OM_qimm);
    }

    bind_opcode("0100 1110 0111 0110", 4, &M68k_ins_TRAPV, &M68k_disasm_TRAPV, 0, NULL, NULL, 0, M68k_OM_none);

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            if (m!=1) bind_opcode("0100 1010 00.. ....", 1, &M68k_ins_TST, &M68k_disasm_TST, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
            bind_opcode("0100 1010 01.. ....", 2, &M68k_ins_TST, &M68k_disasm_TST, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
            bind_opcode("0100 1010 10.. ....", 4, &M68k_ins_TST, &M68k_disasm_TST, (m << 3) | r, &op1, NULL, 0, M68k_OM_ea);
        }
    }

    for (u32 a = 0; a < 8; a++) {
        op1 = mk_ea(M68k_AM_address_register_direct, a);
        bind_opcode("0100 1110 0101 1...", 4, &M68k_ins_UNLK, &M68k_disasm_UNLK, a, &op1, NULL, 0, M68k_OM_r);
    }
