
    for (u32 x = 0; x < 8; x++) {
        for (u32 y = 0; y < 8; y++) {

            op1 = mk_ea(AM_data_register_direct, y);
            op2 = mk_ea(AM_data_register_direct, x);
            bind_opcode("1100 ...1 0000 ....", 1, &core::ins_ABCD, &disasm_ABCD, (x << 9) | y, &op1, &op2, 0, OM_r_r);

            op1 = mk_ea(AM_address_register_indirect_with_predecrement, y);
            op2 = mk_ea(AM_address_register_indirect_with_predecrement, x);
            bind_opcode("1100 ...1 0000 ....", 1, &core::ins_ABCD, &disasm_ABCD, (x << 9) | y | 8, &op1, &op2, 0, OM_ea_ea);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {

                if (!(((m == 7) && (r >= 5)))) {
                    op1 = mk_ea(m, r);
                    op2 = mk_ea(AM_data_register_direct, d);
                    if (m != 1) bind_opcode("1101 .... 00.. ....", 1, &core::ins_ADD, &disasm_ADD, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
                    bind_opcode("1101 .... 01.. ....", 2, &core::ins_ADD, &disasm_ADD, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
                    bind_opcode("1101 .... 10.. ....", 4, &core::ins_ADD, &disasm_ADD, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
                }
                if (!(((m <= 1) || ((m == 7) && (r >= 2))))) {
                    op1 = mk_ea(AM_data_register_direct, d);
                    op2 = mk_ea(m, r);
                    bind_opcode("1101 .... 00.. ....", 1, &core::ins_ADD, &disasm_ADD, (d << 9) | (m << 3) | r | 0x100, &op1, &op2, 0, OM_r_ea);
                    bind_opcode("1101 .... 01.. ....", 2, &core::ins_ADD, &disasm_ADD, (d << 9) | (m << 3) | r | 0x100, &op1, &op2, 0, OM_r_ea);
                    bind_opcode("1101 .... 10.. ....", 4, &core::ins_ADD, &disasm_ADD, (d << 9) | (m << 3) | r | 0x100, &op1, &op2, 0, OM_r_ea);
                }
            }
        }
    }

    for (u32 a = 0; a < 8; a++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 7) && (r >= 5)) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(AM_address_register_direct, a);
                bind_opcode("1101 ...0 11.. ....", 2, &core::ins_ADDA, &disasm_ADDA, (a << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
                bind_opcode("1101 ...1 11.. ....", 4, &core::ins_ADDA, &disasm_ADDA, (a << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0000 0110 00.. ....", 1, &core::ins_ADDI, &disasm_ADDI, (m << 3) | r, &op1, NULL, 0, OM_ea);
            bind_opcode("0000 0110 01.. ....", 2, &core::ins_ADDI, &disasm_ADDI, (m << 3) | r, &op1, NULL, 0, OM_ea);
            bind_opcode("0000 0110 10.. ....", 4, &core::ins_ADDI, &disasm_ADDI, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 7) && (r >= 2)) continue;
                op1 = mk_ea(AM_quick_immediate, d ? d : 8);
                if (m == 1) {
                    op2 = mk_ea(AM_address_register_direct, r);
                    bind_opcode("0101 ...0 01.. ....", 2, &core::ins_ADDQ_ar, &disasm_ADDQ_ar, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_qimm_r);
                    bind_opcode("0101 ...0 10.. ....", 4, &core::ins_ADDQ_ar, &disasm_ADDQ_ar, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_qimm_r);
                }
                else {
                    op2 = mk_ea(m, r);
                    bind_opcode("0101 ...0 00.. ....", 1, &core::ins_ADDQ, &disasm_ADDQ, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_qimm_ea);
                    bind_opcode("0101 ...0 01.. ....", 2, &core::ins_ADDQ, &disasm_ADDQ, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_qimm_ea);
                    bind_opcode("0101 ...0 10.. ....", 4, &core::ins_ADDQ, &disasm_ADDQ, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_qimm_ea);
                }
            }
        }
    }

    for (u32 x = 0; x < 8; x++) {
        for (u32 y = 0; y < 8; y++) {

            op1 = mk_ea(AM_data_register_direct, y);
            op2 = mk_ea(AM_data_register_direct, x);
            bind_opcode("1101 ...1 0000 ....", 1, &core::ins_ADDX, &disasm_ADDX, (x << 9) | y, &op1, &op2, 0, OM_r_r);
            bind_opcode("1101 ...1 0100 ....", 2, &core::ins_ADDX, &disasm_ADDX, (x << 9) | y, &op1, &op2, 0, OM_r_r);
            bind_opcode("1101 ...1 1000 ....", 4, &core::ins_ADDX, &disasm_ADDX, (x << 9) | y, &op1, &op2, 0, OM_r_r);

            op1 = mk_ea(AM_address_register_indirect_with_predecrement, y);
            op2 = mk_ea(AM_address_register_indirect_with_predecrement, x);
            bind_opcode("1101 ...1 0000 ....", 1, &core::ins_ADDX, &disasm_ADDX, (x << 9) | y | 8, &op1, &op2, 0, OM_ea_ea);
            bind_opcode("1101 ...1 0100 ....", 2, &core::ins_ADDX, &disasm_ADDX, (x << 9) | y | 8, &op1, &op2, 0, OM_ea_ea);
            bind_opcode("1101 ...1 1000 ....", 4, &core::ins_ADDX, &disasm_ADDX, (x << 9) | y | 8, &op1, &op2, 0, OM_ea_ea);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {

                if (!(((m == 1) || ((m == 7) && (r >= 5))))) {
                    op1 = mk_ea(m, r);
                    op2 = mk_ea(AM_data_register_direct, d);
                    bind_opcode("1100 .... 00.. ....", 1, &core::ins_AND, &disasm_AND, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
                    bind_opcode("1100 .... 01.. ....", 2, &core::ins_AND, &disasm_AND, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
                    bind_opcode("1100 .... 10.. ....", 4, &core::ins_AND, &disasm_AND, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
                }
                if (!(((m <= 1) || ((m == 7) && (r >= 2))))) {
                    op1 = mk_ea(AM_data_register_direct, d);
                    op2 = mk_ea(m, r);
                    bind_opcode("1100 .... 00.. ....", 1, &core::ins_AND, &disasm_AND, (d << 9) | (m << 3) | r | 0x100, &op1, &op2, 0, OM_r_ea);
                    bind_opcode("1100 .... 01.. ....", 2, &core::ins_AND, &disasm_AND, (d << 9) | (m << 3) | r | 0x100, &op1, &op2, 0, OM_r_ea);
                    bind_opcode("1100 .... 10.. ....", 4, &core::ins_AND, &disasm_AND, (d << 9) | (m << 3) | r | 0x100, &op1, &op2, 0, OM_r_ea);
                }
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0000 0010 00.. ....", 1, &core::ins_ANDI, &disasm_ANDI, (m << 3) | r, &op1, NULL, 0, OM_ea);
            bind_opcode("0000 0010 01.. ....", 2, &core::ins_ANDI, &disasm_ANDI, (m << 3) | r, &op1, NULL, 0, OM_ea);
            bind_opcode("0000 0010 10.. ....", 4, &core::ins_ANDI, &disasm_ANDI, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    bind_opcode("0000 0010 0011 1100", 4, &core::ins_ANDI_TO_CCR, &disasm_ANDI_TO_CCR, 0, NULL, NULL, 0, OM_none);

    bind_opcode("0000 0010 0111 1100", 4, &core::ins_ANDI_TO_SR, &disasm_ANDI_TO_SR, 0, NULL, NULL, 0, OM_none);

    for (u32 i = 0; i < 8; i++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(AM_immediate, i ? i : 8);
            op2 = mk_ea(AM_data_register_direct, d);
            bind_opcode("1110 ...1 0000 0...", 1, &core::ins_ASL_qimm_dr, &disasm_ASL_qimm_dr, (i << 9) | d, &op1, &op2, 0, OM_qimm_r);
            bind_opcode("1110 ...1 0100 0...", 2, &core::ins_ASL_qimm_dr, &disasm_ASL_qimm_dr, (i << 9) | d, &op1, &op2, 0, OM_qimm_r);
            bind_opcode("1110 ...1 1000 0...", 4, &core::ins_ASL_qimm_dr, &disasm_ASL_qimm_dr, (i << 9) | d, &op1, &op2, 0, OM_qimm_r);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(AM_data_register_direct, s);
            op2 = mk_ea(AM_data_register_direct, d);
            bind_opcode("1110 ...1 0010 0...", 1, &core::ins_ASL_dr_dr, &disasm_ASL_dr_dr, (s << 9) | d, &op1, &op2, 0, OM_r_r);
            bind_opcode("1110 ...1 0110 0...", 2, &core::ins_ASL_dr_dr, &disasm_ASL_dr_dr, (s << 9) | d, &op1, &op2, 0, OM_r_r);
            bind_opcode("1110 ...1 1010 0...", 4, &core::ins_ASL_dr_dr, &disasm_ASL_dr_dr, (s << 9) | d, &op1, &op2, 0, OM_r_r);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("1110 0001 11.. ....", 2, &core::ins_ASL_ea, &disasm_ASL_ea, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    for (u32 i = 0; i < 8; i++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(AM_immediate, i ? i : 8);
            op2 = mk_ea(AM_data_register_direct, d);
            bind_opcode("1110 ...0 0000 0...", 1, &core::ins_ASR_qimm_dr, &disasm_ASR_qimm_dr, (i << 9) | d, &op1, &op2, 0, OM_qimm_r);
            bind_opcode("1110 ...0 0100 0...", 2, &core::ins_ASR_qimm_dr, &disasm_ASR_qimm_dr, (i << 9) | d, &op1, &op2, 0, OM_qimm_r);
            bind_opcode("1110 ...0 1000 0...", 4, &core::ins_ASR_qimm_dr, &disasm_ASR_qimm_dr, (i << 9) | d, &op1, &op2, 0, OM_qimm_r);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(AM_data_register_direct, s);
            op2 = mk_ea(AM_data_register_direct, d);
            bind_opcode("1110 ...0 0010 0...", 1, &core::ins_ASR_dr_dr, &disasm_ASR_dr_dr, (s << 9) | d, &op1, &op2, 0, OM_r_r);
            bind_opcode("1110 ...0 0110 0...", 2, &core::ins_ASR_dr_dr, &disasm_ASR_dr_dr, (s << 9) | d, &op1, &op2, 0, OM_r_r);
            bind_opcode("1110 ...0 1010 0...", 4, &core::ins_ASR_dr_dr, &disasm_ASR_dr_dr, (s << 9) | d, &op1, &op2, 0, OM_r_r);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("1110 0000 11.. ....", 2, &core::ins_ASR_ea, &disasm_ASR_ea, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    for (u32 t = 0; t < 16; t++) {
        for (u32 d = 0; d < 256; d++) {
            if (t<=1) continue;
            op1 = mk_ea(AM_immediate, t);
            op2 = mk_ea(AM_immediate, d);
            bind_opcode("0110 .... .... ....", 4, &core::ins_BCC, &disasm_BCC, (t << 8) | d, &op1, &op2, 0, OM_qimm_qimm);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 2))) continue;
                op1 = mk_ea(AM_data_register_direct, d);
                op2 = mk_ea(m, r);
                if (m == 0) bind_opcode("0000 ...1 01.. ....", 4, &core::ins_BCHG_dr_ea, &disasm_BCHG_dr_ea, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_r_ea);
                else        bind_opcode("0000 ...1 01.. ....", 1, &core::ins_BCHG_dr_ea, &disasm_BCHG_dr_ea, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_r_ea);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            if (m == 0) bind_opcode("0000 1000 01.. ....", 4, &core::ins_BCHG_ea, &disasm_BCHG_ea, (m << 3) | r, &op1, NULL, 0, OM_ea);
            else        bind_opcode("0000 1000 01.. ....", 1, &core::ins_BCHG_ea, &disasm_BCHG_ea, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 2))) continue;
                op1 = mk_ea(AM_data_register_direct, d);
                op2 = mk_ea(m, r);
                if (m == 0) bind_opcode("0000 ...1 10.. ....", 4, &core::ins_BCLR_dr_ea, &disasm_BCLR_dr_ea, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_r_ea);
                else        bind_opcode("0000 ...1 10.. ....", 1, &core::ins_BCLR_dr_ea, &disasm_BCLR_dr_ea, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_r_ea);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            if (m == 0) bind_opcode("0000 1000 10.. ....", 4, &core::ins_BCLR_ea, &disasm_BCLR_ea, (m << 3) | r, &op1, NULL, 0, OM_ea);
            else        bind_opcode("0000 1000 10.. ....", 1, &core::ins_BCLR_ea, &disasm_BCLR_ea, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    for (u32 d = 0; d < 256; d++) {
        op1 = mk_ea(AM_immediate, d);
        bind_opcode("0110 0000 .... ....", 4, &core::ins_BRA, &disasm_BRA, d, &op1, NULL, 0, OM_qimm);
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 2))) continue;
                op1 = mk_ea(AM_data_register_direct, d);
                op2 = mk_ea(m, r);
                if (m == 0) bind_opcode("0000 ...1 11.. ....", 4, &core::ins_BSET_dr_ea, &disasm_BSET_dr_ea, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_r_ea);
                else        bind_opcode("0000 ...1 11.. ....", 1, &core::ins_BSET_dr_ea, &disasm_BSET_dr_ea, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_r_ea);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            if (m == 0) bind_opcode("0000 1000 11.. ....", 4, &core::ins_BSET_ea, &disasm_BSET_ea, (m << 3) | r, &op1, NULL, 0, OM_ea);
            else        bind_opcode("0000 1000 11.. ....", 1, &core::ins_BSET_ea, &disasm_BSET_ea, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    for (u32 d = 0; d < 256; d++) {
        op1 = mk_ea(AM_immediate, d);
        bind_opcode("0110 0001 .... ....", 4, &core::ins_BSR, &disasm_BSR, d, &op1, NULL, 0, OM_qimm);
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 5))) continue;
                op1 = mk_ea(AM_data_register_direct, d);
                op2 = mk_ea(m, r);
                if (m == 0) bind_opcode("0000 ...1 00.. ....", 4, &core::ins_BTST_dr_ea, &disasm_BTST_dr_ea, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_r_ea);
                else        bind_opcode("0000 ...1 00.. ....", 1, &core::ins_BTST_dr_ea, &disasm_BTST_dr_ea, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_r_ea);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 4))) continue;
            op1 = mk_ea(m, r);
            if (m == 0) bind_opcode("0000 1000 00.. ....", 4, &core::ins_BTST_ea, &disasm_BTST_ea, (m << 3) | r, &op1, NULL, 0, OM_ea);
            else        bind_opcode("0000 1000 00.. ....", 1, &core::ins_BTST_ea, &disasm_BTST_ea, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 5))) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(AM_data_register_direct, d);
                bind_opcode("0100 ...1 10.. ....", 2, &core::ins_CHK, &disasm_CHK, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 0010 00.. ....", 1, &core::ins_CLR, &disasm_CLR, (m << 3) | r, &op1, NULL, 0, OM_ea);
            bind_opcode("0100 0010 01.. ....", 2, &core::ins_CLR, &disasm_CLR, (m << 3) | r, &op1, NULL, 0, OM_ea);
            bind_opcode("0100 0010 10.. ....", 4, &core::ins_CLR, &disasm_CLR, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 7) && (r >= 5)) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(AM_data_register_direct, d);
                if (m!=1) bind_opcode("1011 ...0 00.. ....", 1, &core::ins_CMP, &disasm_CMP, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
                bind_opcode("1011 ...0 01.. ....", 2, &core::ins_CMP, &disasm_CMP, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
                bind_opcode("1011 ...0 10.. ....", 4, &core::ins_CMP, &disasm_CMP, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
            }
        }
    }

    for (u32 a = 0; a < 8; a++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 7) && (r >= 5)) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(AM_address_register_direct, a);
                bind_opcode("1011 ...0 11.. ....", 2, &core::ins_CMPA, &disasm_CMPA, (a << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
                bind_opcode("1011 ...1 11.. ....", 4, &core::ins_CMPA, &disasm_CMPA, (a << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0000 1100 00.. ....", 1, &core::ins_CMPI, &disasm_CMPI, (m << 3) | r, &op1, NULL, 0, OM_ea);
            bind_opcode("0000 1100 01.. ....", 2, &core::ins_CMPI, &disasm_CMPI, (m << 3) | r, &op1, NULL, 0, OM_ea);
            bind_opcode("0000 1100 10.. ....", 4, &core::ins_CMPI, &disasm_CMPI, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    for (u32 x = 0; x < 8; x++) {
        for (u32 y = 0; y < 8; y++) {
            op1 = mk_ea(AM_address_register_indirect_with_postincrement, y);
            op2 = mk_ea(AM_address_register_indirect_with_postincrement, x);
            bind_opcode("1011 ...1 0000 1...", 1, &core::ins_CMPM, &disasm_CMPM, (x << 9) | y, &op1, &op2, 0, OM_ea_ea);
            bind_opcode("1011 ...1 0100 1...", 2, &core::ins_CMPM, &disasm_CMPM, (x << 9) | y, &op1, &op2, 0, OM_ea_ea);
            bind_opcode("1011 ...1 1000 1...", 4, &core::ins_CMPM, &disasm_CMPM, (x << 9) | y, &op1, &op2, 0, OM_ea_ea);
        }
    }

    for (u32 c = 0; c < 16; c++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(AM_immediate, c);
            op2 = mk_ea(AM_data_register_direct, d);
            bind_opcode("0101 .... 1100 1...", 2, &core::ins_DBCC, &disasm_DBCC, (c << 8) | d, &op1, &op2, 0, OM_qimm_r);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 5))) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(AM_data_register_direct, d);
                bind_opcode("1000 ...1 11.. ....", 2, &core::ins_DIVS, &disasm_DIVS, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
            }
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 5))) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(AM_data_register_direct, d);
                bind_opcode("1000 ...0 11.. ....", 2, &core::ins_DIVU, &disasm_DIVU, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
            }
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 2))) continue;
                op1 = mk_ea(AM_data_register_direct, d);
                op2 = mk_ea(m, r);
                bind_opcode("1011 ...1 00.. ....", 1, &core::ins_EOR, &disasm_EOR, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_r_ea);
                bind_opcode("1011 ...1 01.. ....", 2, &core::ins_EOR, &disasm_EOR, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_r_ea);
                bind_opcode("1011 ...1 10.. ....", 4, &core::ins_EOR, &disasm_EOR, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_r_ea);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0000 1010 00.. ....", 1, &core::ins_EORI, &disasm_EORI, (m << 3) | r, &op1, NULL, 0, OM_ea);
            bind_opcode("0000 1010 01.. ....", 2, &core::ins_EORI, &disasm_EORI, (m << 3) | r, &op1, NULL, 0, OM_ea);
            bind_opcode("0000 1010 10.. ....", 4, &core::ins_EORI, &disasm_EORI, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    bind_opcode("0000 1010 0011 1100", 4, &core::ins_EORI_TO_CCR, &disasm_EORI_TO_CCR, 0, NULL, NULL, 0, OM_none);

    bind_opcode("0000 1010 0111 1100", 4, &core::ins_EORI_TO_SR, &disasm_EORI_TO_SR, 0, NULL, NULL, 0, OM_none);

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(AM_data_register_direct, s);
            op2 = mk_ea(AM_data_register_direct, d);
            bind_opcode("1100 ...1 0100 0...", 4, &core::ins_EXG, &disasm_EXG, (s << 9) | d, &op1, &op2, 0, OM_r_r);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(AM_address_register_direct, s);
            op2 = mk_ea(AM_address_register_direct, d);
            bind_opcode("1100 ...1 0100 1...", 4, &core::ins_EXG, &disasm_EXG, (s << 9) | d, &op1, &op2, 1, OM_r_r);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(AM_address_register_direct, d);
            op2 = mk_ea(AM_data_register_direct, s);
            bind_opcode("1100 ...1 1000 1...", 4, &core::ins_EXG, &disasm_EXG, (s << 9) | d, &op1, &op2, 2, OM_r_r);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        op1 = mk_ea(AM_data_register_direct, d);
        bind_opcode("0100 1000 1000 0...", 2, &core::ins_EXT, &disasm_EXT, d, &op1, NULL, 0, OM_r);
        bind_opcode("0100 1000 1100 0...", 4, &core::ins_EXT, &disasm_EXT, d, &op1, NULL, 0, OM_r);
    }

    bind_opcode("0100 1010 1111 1100", 4, &core::ins_ILLEGAL, &disasm_ILLEGAL, 0, NULL, NULL, 0, OM_none);

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || (m == 3) || (m == 4) || ((m == 7) && (r >= 4))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 1110 11.. ....", 4, &core::ins_JMP, &disasm_JMP, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || (m == 3) || (m == 4) || ((m == 7) && (r >= 4))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 1110 10.. ....", 4, &core::ins_JSR, &disasm_JSR, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    for (u32 a = 0; a < 8; a++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m <= 1) || (m == 3) || (m == 4) || ((m == 7) && (r >= 4))) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(AM_address_register_direct, a);
                bind_opcode("0100 ...1 11.. ....", 4, &core::ins_LEA, &disasm_LEA, (a << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
            }
        }
    }

    for (u32 a = 0; a < 8; a++) {
        op1 = mk_ea(AM_address_register_direct, a);
        bind_opcode("0100 1110 0101 0...", 4, &core::ins_LINK, &disasm_LINK, a, &op1, NULL, 0, OM_r);
    }

    for (u32 i = 0; i < 8; i++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(AM_immediate, i ? i : 8);
            op2 = mk_ea(AM_data_register_direct, d);
            bind_opcode("1110 ...1 0000 1...", 1, &core::ins_LSL_qimm_dr, &disasm_LSL_qimm_dr, (i << 9) | d, &op1, &op2, 0, OM_qimm_r);
            bind_opcode("1110 ...1 0100 1...", 2, &core::ins_LSL_qimm_dr, &disasm_LSL_qimm_dr, (i << 9) | d, &op1, &op2, 0, OM_qimm_r);
            bind_opcode("1110 ...1 1000 1...", 4, &core::ins_LSL_qimm_dr, &disasm_LSL_qimm_dr, (i << 9) | d, &op1, &op2, 0, OM_qimm_r);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(AM_data_register_direct, s);
            op2 = mk_ea(AM_data_register_direct, d);
            bind_opcode("1110 ...1 0010 1...", 1, &core::ins_LSL_dr_dr, &disasm_LSL_dr_dr, (s << 9) | d, &op1, &op2, 0, OM_r_r);
            bind_opcode("1110 ...1 0110 1...", 2, &core::ins_LSL_dr_dr, &disasm_LSL_dr_dr, (s << 9) | d, &op1, &op2, 0, OM_r_r);
            bind_opcode("1110 ...1 1010 1...", 4, &core::ins_LSL_dr_dr, &disasm_LSL_dr_dr, (s << 9) | d, &op1, &op2, 0, OM_r_r);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("1110 0011 11.. ....", 2, &core::ins_LSL_ea, &disasm_LSL_ea, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    for (u32 i = 0; i < 8; i++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(AM_immediate, i ? i : 8);
            op2 = mk_ea(AM_data_register_direct, d);
            bind_opcode("1110 ...0 0000 1...", 1, &core::ins_LSR_qimm_dr, &disasm_LSR_qimm_dr, (i << 9) | d, &op1, &op2, 0, OM_qimm_r);
            bind_opcode("1110 ...0 0100 1...", 2, &core::ins_LSR_qimm_dr, &disasm_LSR_qimm_dr, (i << 9) | d, &op1, &op2, 0, OM_qimm_r);
            bind_opcode("1110 ...0 1000 1...", 4, &core::ins_LSR_qimm_dr, &disasm_LSR_qimm_dr, (i << 9) | d, &op1, &op2, 0, OM_qimm_r);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(AM_data_register_direct, s);
            op2 = mk_ea(AM_data_register_direct, d);
            bind_opcode("1110 ...0 0010 1...", 1, &core::ins_LSR_dr_dr, &disasm_LSR_dr_dr, (s << 9) | d, &op1, &op2, 0, OM_r_r);
            bind_opcode("1110 ...0 0110 1...", 2, &core::ins_LSR_dr_dr, &disasm_LSR_dr_dr, (s << 9) | d, &op1, &op2, 0, OM_r_r);
            bind_opcode("1110 ...0 1010 1...", 4, &core::ins_LSR_dr_dr, &disasm_LSR_dr_dr, (s << 9) | d, &op1, &op2, 0, OM_r_r);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("1110 0010 11.. ....", 2, &core::ins_LSR_ea, &disasm_LSR_ea, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    for (u32 a = 0; a < 8; a++) {
        for (u32 b = 0; b < 8; b++) {
            for (u32 c = 0; c < 8; c++) {
                for (u32 d = 0; d < 8; d++) {
                    if (((b == 1) || ((b == 7) && (a >= 2))) || ((c == 7) && (d >= 5))) continue;
                    op1 = mk_ea(c, d);
                    op2 = mk_ea(b, a);
                    if (c!=1) bind_opcode("0001 .... .... ....", 1, &core::ins_MOVE, &disasm_MOVE, (a << 9) | (b << 6) | (c << 3) | d, &op1, &op2, 0, OM_ea_ea);
                    bind_opcode("0011 .... .... ....", 2, &core::ins_MOVE, &disasm_MOVE, (a << 9) | (b << 6) | (c << 3) | d, &op1, &op2, 0, OM_ea_ea);
                    bind_opcode("0010 .... .... ....", 4, &core::ins_MOVE, &disasm_MOVE, (a << 9) | (b << 6) | (c << 3) | d, &op1, &op2, 0, OM_ea_ea);
                }
            }
        }
    }

    for (u32 a = 0; a < 8; a++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 7) && (r >= 5)) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(AM_address_register_direct, a);
                bind_opcode("0011 ...0 01.. ....", 2, &core::ins_MOVEA, &disasm_MOVEA, (a << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
                bind_opcode("0010 ...0 01.. ....", 4, &core::ins_MOVEA, &disasm_MOVEA, (a << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || (m == 3) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 1000 10.. ....", 2, &core::ins_MOVEM_TO_MEM, &disasm_MOVEM_TO_MEM, (m << 3) | r, &op1, NULL, 0, OM_ea);
            bind_opcode("0100 1000 11.. ....", 4, &core::ins_MOVEM_TO_MEM, &disasm_MOVEM_TO_MEM, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || (m == 4) || ((m == 7) && (r >= 4))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 1100 10.. ....", 2, &core::ins_MOVEM_TO_REG, &disasm_MOVEM_TO_REG, (m << 3) | r, &op1, NULL, 0, OM_ea);
            bind_opcode("0100 1100 11.. ....", 4, &core::ins_MOVEM_TO_REG, &disasm_MOVEM_TO_REG, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 r = 0; r < 8; r++) {

            op1 = mk_ea(AM_address_register_indirect_with_displacement, r);
            op2 = mk_ea(AM_data_register_direct, d);
            bind_opcode("0000 ...1 .000 1...", 2, &core::ins_MOVEP, &disasm_MOVEP, (d << 9) | r, &op1, &op2, 0, OM_ea_r);
            bind_opcode("0000 ...1 .100 1...", 4, &core::ins_MOVEP, &disasm_MOVEP, (d << 9) | r, &op1, &op2, 0, OM_ea_r);

            op1 = mk_ea(AM_data_register_direct, d);
            op2 = mk_ea(AM_address_register_indirect_with_displacement, r);
            bind_opcode("0000 ...1 .000 1...", 2, &core::ins_MOVEP, &disasm_MOVEP, (d << 9) | r | 0x80, &op1, &op2, 0, OM_r_ea);
            bind_opcode("0000 ...1 .100 1...", 4, &core::ins_MOVEP, &disasm_MOVEP, (d << 9) | r | 0x80, &op1, &op2, 0, OM_r_ea);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 i = 0; i < 256; i++) {
            op1 = mk_ea(AM_immediate, i);
            op2 = mk_ea(AM_data_register_direct, d);
            bind_opcode("0111 ...0 .... ....", 4, &core::ins_MOVEQ, &disasm_MOVEQ, (d << 9) | i, &op1, &op2, 0, OM_qimm_r);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 0000 11.. ....", 2, &core::ins_MOVE_FROM_SR, &disasm_MOVE_FROM_SR, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 5))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 0100 11.. ....", 2, &core::ins_MOVE_TO_CCR, &disasm_MOVE_TO_CCR, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 5))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 0110 11.. ....", 2, &core::ins_MOVE_TO_SR, &disasm_MOVE_TO_SR, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    for (u32 a = 0; a < 8; a++) {
        op1 = mk_ea(AM_address_register_direct, a);
        bind_opcode("0100 1110 0110 1...", 4, &core::ins_MOVE_FROM_USP, &disasm_MOVE_FROM_USP, a, &op1, NULL, 0, OM_r);
    }

    for (u32 a = 0; a < 8; a++) {
        op1 = mk_ea(AM_address_register_direct, a);
        bind_opcode("0100 1110 0110 0...", 4, &core::ins_MOVE_TO_USP, &disasm_MOVE_TO_USP, a, &op1, NULL, 0, OM_r);
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 5))) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(AM_data_register_direct, d);
                bind_opcode("1100 ...1 11.. ....", 2, &core::ins_MULS, &disasm_MULS, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
            }
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 5))) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(AM_data_register_direct, d);
                bind_opcode("1100 ...0 11.. ....", 2, &core::ins_MULU, &disasm_MULU, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 1000 00.. ....", 1, &core::ins_NBCD, &disasm_NBCD, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 0100 00.. ....", 1, &core::ins_NEG, &disasm_NEG, (m << 3) | r, &op1, NULL, 0, OM_ea);
            bind_opcode("0100 0100 01.. ....", 2, &core::ins_NEG, &disasm_NEG, (m << 3) | r, &op1, NULL, 0, OM_ea);
            bind_opcode("0100 0100 10.. ....", 4, &core::ins_NEG, &disasm_NEG, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 0000 00.. ....", 1, &core::ins_NEGX, &disasm_NEGX, (m << 3) | r, &op1, NULL, 0, OM_ea);
            bind_opcode("0100 0000 01.. ....", 2, &core::ins_NEGX, &disasm_NEGX, (m << 3) | r, &op1, NULL, 0, OM_ea);
            bind_opcode("0100 0000 10.. ....", 4, &core::ins_NEGX, &disasm_NEGX, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    bind_opcode("0100 1110 0111 0001", 4, &core::ins_NOP, &disasm_NOP, 0, NULL, NULL, 0, OM_none);

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 0110 00.. ....", 1, &core::ins_NOT, &disasm_NOT, (m << 3) | r, &op1, NULL, 0, OM_ea);
            bind_opcode("0100 0110 01.. ....", 2, &core::ins_NOT, &disasm_NOT, (m << 3) | r, &op1, NULL, 0, OM_ea);
            bind_opcode("0100 0110 10.. ....", 4, &core::ins_NOT, &disasm_NOT, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {

                if (!(((m == 1) || ((m == 7) && (r >= 5))))) {
                    op1 = mk_ea(m, r);
                    op2 = mk_ea(AM_data_register_direct, d);
                    bind_opcode("1000 .... 00.. ....", 1, &core::ins_OR, &disasm_OR, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
                    bind_opcode("1000 .... 01.. ....", 2, &core::ins_OR, &disasm_OR, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
                    bind_opcode("1000 .... 10.. ....", 4, &core::ins_OR, &disasm_OR, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
                }
                if (!(((m <= 1) || ((m == 7) && (r >= 2))))) {
                    op1 = mk_ea(AM_data_register_direct, d);
                    op2 = mk_ea(m, r);
                    bind_opcode("1000 .... 00.. ....", 1, &core::ins_OR, &disasm_OR, (d << 9) | (m << 3) | r | 0x100, &op1, &op2, 0, OM_r_ea);
                    bind_opcode("1000 .... 01.. ....", 2, &core::ins_OR, &disasm_OR, (d << 9) | (m << 3) | r | 0x100, &op1, &op2, 0, OM_r_ea);
                    bind_opcode("1000 .... 10.. ....", 4, &core::ins_OR, &disasm_OR, (d << 9) | (m << 3) | r | 0x100, &op1, &op2, 0, OM_r_ea);
                }
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0000 0000 00.. ....", 1, &core::ins_ORI, &disasm_ORI, (m << 3) | r, &op1, NULL, 0, OM_ea);
            bind_opcode("0000 0000 01.. ....", 2, &core::ins_ORI, &disasm_ORI, (m << 3) | r, &op1, NULL, 0, OM_ea);
            bind_opcode("0000 0000 10.. ....", 4, &core::ins_ORI, &disasm_ORI, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    bind_opcode("0000 0000 0011 1100", 4, &core::ins_ORI_TO_CCR, &disasm_ORI_TO_CCR, 0, NULL, NULL, 0, OM_none);

    bind_opcode("0000 0000 0111 1100", 4, &core::ins_ORI_TO_SR, &disasm_ORI_TO_SR, 0, NULL, NULL, 0, OM_none);

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || (m == 3) || (m == 4) || ((m == 7) && (r >= 4))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 1000 01.. ....", 4, &core::ins_PEA, &disasm_PEA, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    bind_opcode("0100 1110 0111 0000", 4, &core::ins_RESET, &disasm_RESET, 0, NULL, NULL, 0, OM_none);

    for (u32 i = 0; i < 8; i++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(AM_immediate, i ? i : 8);
            op2 = mk_ea(AM_data_register_direct, d);
            bind_opcode("1110 ...1 0001 1...", 1, &core::ins_ROL_qimm_dr, &disasm_ROL_qimm_dr, (i << 9) | d, &op1, &op2, 0, OM_qimm_r);
            bind_opcode("1110 ...1 0101 1...", 2, &core::ins_ROL_qimm_dr, &disasm_ROL_qimm_dr, (i << 9) | d, &op1, &op2, 0, OM_qimm_r);
            bind_opcode("1110 ...1 1001 1...", 4, &core::ins_ROL_qimm_dr, &disasm_ROL_qimm_dr, (i << 9) | d, &op1, &op2, 0, OM_qimm_r);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(AM_data_register_direct, s);
            op2 = mk_ea(AM_data_register_direct, d);
            bind_opcode("1110 ...1 0011 1...", 1, &core::ins_ROL_dr_dr, &disasm_ROL_dr_dr, (s << 9) | d, &op1, &op2, 0, OM_r_r);
            bind_opcode("1110 ...1 0111 1...", 2, &core::ins_ROL_dr_dr, &disasm_ROL_dr_dr, (s << 9) | d, &op1, &op2, 0, OM_r_r);
            bind_opcode("1110 ...1 1011 1...", 4, &core::ins_ROL_dr_dr, &disasm_ROL_dr_dr, (s << 9) | d, &op1, &op2, 0, OM_r_r);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("1110 0111 11.. ....", 2, &core::ins_ROL_ea, &disasm_ROL_ea, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    for (u32 i = 0; i < 8; i++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(AM_immediate, i ? i : 8);
            op2 = mk_ea(AM_data_register_direct, d);
            bind_opcode("1110 ...0 0001 1...", 1, &core::ins_ROR_qimm_dr, &disasm_ROR_qimm_dr, (i << 9) | d, &op1, &op2, 0, OM_qimm_r);
            bind_opcode("1110 ...0 0101 1...", 2, &core::ins_ROR_qimm_dr, &disasm_ROR_qimm_dr, (i << 9) | d, &op1, &op2, 0, OM_qimm_r);
            bind_opcode("1110 ...0 1001 1...", 4, &core::ins_ROR_qimm_dr, &disasm_ROR_qimm_dr, (i << 9) | d, &op1, &op2, 0, OM_qimm_r);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(AM_data_register_direct, s);
            op2 = mk_ea(AM_data_register_direct, d);
            bind_opcode("1110 ...0 0011 1...", 1, &core::ins_ROR_dr_dr, &disasm_ROR_dr_dr, (s << 9) | d, &op1, &op2, 0, OM_r_r);
            bind_opcode("1110 ...0 0111 1...", 2, &core::ins_ROR_dr_dr, &disasm_ROR_dr_dr, (s << 9) | d, &op1, &op2, 0, OM_r_r);
            bind_opcode("1110 ...0 1011 1...", 4, &core::ins_ROR_dr_dr, &disasm_ROR_dr_dr, (s << 9) | d, &op1, &op2, 0, OM_r_r);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("1110 0110 11.. ....", 2, &core::ins_ROR_ea, &disasm_ROR_ea, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    for (u32 i = 0; i < 8; i++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(AM_immediate, i ? i : 8);
            op2 = mk_ea(AM_data_register_direct, d);
            bind_opcode("1110 ...1 0001 0...", 1, &core::ins_ROXL_qimm_dr, &disasm_ROXL_qimm_dr, (i << 9) | d, &op1, &op2, 0, OM_qimm_r);
            bind_opcode("1110 ...1 0101 0...", 2, &core::ins_ROXL_qimm_dr, &disasm_ROXL_qimm_dr, (i << 9) | d, &op1, &op2, 0, OM_qimm_r);
            bind_opcode("1110 ...1 1001 0...", 4, &core::ins_ROXL_qimm_dr, &disasm_ROXL_qimm_dr, (i << 9) | d, &op1, &op2, 0, OM_qimm_r);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(AM_data_register_direct, s);
            op2 = mk_ea(AM_data_register_direct, d);
            bind_opcode("1110 ...1 0011 0...", 1, &core::ins_ROXL_dr_dr, &disasm_ROXL_dr_dr, (s << 9) | d, &op1, &op2, 0, OM_r_r);
            bind_opcode("1110 ...1 0111 0...", 2, &core::ins_ROXL_dr_dr, &disasm_ROXL_dr_dr, (s << 9) | d, &op1, &op2, 0, OM_r_r);
            bind_opcode("1110 ...1 1011 0...", 4, &core::ins_ROXL_dr_dr, &disasm_ROXL_dr_dr, (s << 9) | d, &op1, &op2, 0, OM_r_r);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("1110 0101 11.. ....", 2, &core::ins_ROXL_ea, &disasm_ROXL_ea, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    for (u32 i = 0; i < 8; i++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(AM_immediate, i ? i : 8);
            op2 = mk_ea(AM_data_register_direct, d);
            bind_opcode("1110 ...0 0001 0...", 1, &core::ins_ROXR_qimm_dr, &disasm_ROXR_qimm_dr, (i << 9) | d, &op1, &op2, 0, OM_qimm_r);
            bind_opcode("1110 ...0 0101 0...", 2, &core::ins_ROXR_qimm_dr, &disasm_ROXR_qimm_dr, (i << 9) | d, &op1, &op2, 0, OM_qimm_r);
            bind_opcode("1110 ...0 1001 0...", 4, &core::ins_ROXR_qimm_dr, &disasm_ROXR_qimm_dr, (i << 9) | d, &op1, &op2, 0, OM_qimm_r);
        }
    }

    for (u32 s = 0; s < 8; s++) {
        for (u32 d = 0; d < 8; d++) {
            op1 = mk_ea(AM_data_register_direct, s);
            op2 = mk_ea(AM_data_register_direct, d);
            bind_opcode("1110 ...0 0011 0...", 1, &core::ins_ROXR_dr_dr, &disasm_ROXR_dr_dr, (s << 9) | d, &op1, &op2, 0, OM_r_r);
            bind_opcode("1110 ...0 0111 0...", 2, &core::ins_ROXR_dr_dr, &disasm_ROXR_dr_dr, (s << 9) | d, &op1, &op2, 0, OM_r_r);
            bind_opcode("1110 ...0 1011 0...", 4, &core::ins_ROXR_dr_dr, &disasm_ROXR_dr_dr, (s << 9) | d, &op1, &op2, 0, OM_r_r);
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m <= 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("1110 0100 11.. ....", 2, &core::ins_ROXR_ea, &disasm_ROXR_ea, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    bind_opcode("0100 1110 0111 0011", 4, &core::ins_RTE, &disasm_RTE, 0, NULL, NULL, 0, OM_none);

    bind_opcode("0100 1110 0111 0111", 4, &core::ins_RTR, &disasm_RTR, 0, NULL, NULL, 0, OM_none);

    bind_opcode("0100 1110 0111 0101", 4, &core::ins_RTS, &disasm_RTS, 0, NULL, NULL, 0, OM_none);

    for (u32 x = 0; x < 8; x++) {
        for (u32 y = 0; y < 8; y++) {

            op1 = mk_ea(AM_data_register_direct, y);
            op2 = mk_ea(AM_data_register_direct, x);
            bind_opcode("1000 ...1 0000 ....", 1, &core::ins_SBCD, &disasm_SBCD, (x << 9) | y, &op1, &op2, 0, OM_r_r);

            op1 = mk_ea(AM_address_register_indirect_with_predecrement, y);
            op2 = mk_ea(AM_address_register_indirect_with_predecrement, x);
            bind_opcode("1000 ...1 0000 ....", 1, &core::ins_SBCD, &disasm_SBCD, (x << 9) | y | 8, &op1, &op2, 0, OM_ea_ea);
        }
    }

    for (u32 t = 0; t < 16; t++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 1) || ((m == 7) && (r >= 2))) continue;
                op1 = mk_ea(AM_immediate, t);
                op2 = mk_ea(m, r);
                bind_opcode("0101 .... 11.. ....", 1, &core::ins_SCC, &disasm_SCC, (t << 8) | (m << 3) | r, &op1, &op2, 0, OM_qimm_ea);
            }
        }
    }

    bind_opcode("0100 1110 0111 0010", 4, &core::ins_STOP, &disasm_STOP, 0, NULL, NULL, 0, OM_none);

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {

                if (!(((m == 7) && (r >= 5)))) {
                    op1 = mk_ea(m, r);
                    op2 = mk_ea(AM_data_register_direct, d);
                    bind_opcode("1001 .... 00.. ....", 1, &core::ins_SUB, &disasm_SUB, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
                    bind_opcode("1001 .... 01.. ....", 2, &core::ins_SUB, &disasm_SUB, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
                    bind_opcode("1001 .... 10.. ....", 4, &core::ins_SUB, &disasm_SUB, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
                }
                if (!(((m <= 1) || ((m == 7) && (r >= 2))))) {
                    op1 = mk_ea(AM_data_register_direct, d);
                    op2 = mk_ea(m, r);
                    bind_opcode("1001 .... 00.. ....", 1, &core::ins_SUB, &disasm_SUB, (d << 9) | (m << 3) | r | 0x100, &op1, &op2, 0, OM_r_ea);
                    bind_opcode("1001 .... 01.. ....", 2, &core::ins_SUB, &disasm_SUB, (d << 9) | (m << 3) | r | 0x100, &op1, &op2, 0, OM_r_ea);
                    bind_opcode("1001 .... 10.. ....", 4, &core::ins_SUB, &disasm_SUB, (d << 9) | (m << 3) | r | 0x100, &op1, &op2, 0, OM_r_ea);
                }
            }
        }
    }

    for (u32 a = 0; a < 8; a++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 7) && (r >= 5)) continue;
                op1 = mk_ea(m, r);
                op2 = mk_ea(AM_address_register_direct, a);
                bind_opcode("1001 ...0 11.. ....", 2, &core::ins_SUBA, &disasm_SUBA, (a << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
                bind_opcode("1001 ...1 11.. ....", 4, &core::ins_SUBA, &disasm_SUBA, (a << 9) | (m << 3) | r, &op1, &op2, 0, OM_ea_r);
            }
        }
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0000 0100 00.. ....", 1, &core::ins_SUBI, &disasm_SUBI, (m << 3) | r, &op1, NULL, 0, OM_ea);
            bind_opcode("0000 0100 01.. ....", 2, &core::ins_SUBI, &disasm_SUBI, (m << 3) | r, &op1, NULL, 0, OM_ea);
            bind_opcode("0000 0100 10.. ....", 4, &core::ins_SUBI, &disasm_SUBI, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        for (u32 m = 0; m < 8; m++) {
            for (u32 r = 0; r < 8; r++) {
                if ((m == 7) && (r >= 2)) continue;
                op1 = mk_ea(AM_quick_immediate, d ? d : 8);
                if (m == 1) {
                    op2 = mk_ea(AM_address_register_direct, r);
                    bind_opcode("0101 ...1 01.. ....", 2, &core::ins_SUBQ_ar, &disasm_SUBQ_ar, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_qimm_r);
                    bind_opcode("0101 ...1 10.. ....", 4, &core::ins_SUBQ_ar, &disasm_SUBQ_ar, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_qimm_r);
                }
                else {
                    op2 = mk_ea(m, r);
                    bind_opcode("0101 ...1 00.. ....", 1, &core::ins_SUBQ, &disasm_SUBQ, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_qimm_ea);
                    bind_opcode("0101 ...1 01.. ....", 2, &core::ins_SUBQ, &disasm_SUBQ, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_qimm_ea);
                    bind_opcode("0101 ...1 10.. ....", 4, &core::ins_SUBQ, &disasm_SUBQ, (d << 9) | (m << 3) | r, &op1, &op2, 0, OM_qimm_ea);
                }
            }
        }
    }

    for (u32 x = 0; x < 8; x++) {
        for (u32 y = 0; y < 8; y++) {

            op1 = mk_ea(AM_data_register_direct, y);
            op2 = mk_ea(AM_data_register_direct, x);
            bind_opcode("1001 ...1 0000 ....", 1, &core::ins_SUBX, &disasm_SUBX, (x << 9) | y, &op1, &op2, 0, OM_r_r);
            bind_opcode("1001 ...1 0100 ....", 2, &core::ins_SUBX, &disasm_SUBX, (x << 9) | y, &op1, &op2, 0, OM_r_r);
            bind_opcode("1001 ...1 1000 ....", 4, &core::ins_SUBX, &disasm_SUBX, (x << 9) | y, &op1, &op2, 0, OM_r_r);

            op1 = mk_ea(AM_address_register_indirect_with_predecrement, y);
            op2 = mk_ea(AM_address_register_indirect_with_predecrement, x);
            bind_opcode("1001 ...1 0000 ....", 1, &core::ins_SUBX, &disasm_SUBX, (x << 9) | y | 8, &op1, &op2, 0, OM_ea_ea);
            bind_opcode("1001 ...1 0100 ....", 2, &core::ins_SUBX, &disasm_SUBX, (x << 9) | y | 8, &op1, &op2, 0, OM_ea_ea);
            bind_opcode("1001 ...1 1000 ....", 4, &core::ins_SUBX, &disasm_SUBX, (x << 9) | y | 8, &op1, &op2, 0, OM_ea_ea);
        }
    }

    for (u32 d = 0; d < 8; d++) {
        op1 = mk_ea(AM_data_register_direct, d);
        bind_opcode("0100 1000 0100 0...", 4, &core::ins_SWAP, &disasm_SWAP, d, &op1, NULL, 0, OM_r);
    }

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            bind_opcode("0100 1010 11.. ....", 1, &core::ins_TAS, &disasm_TAS, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    for (u32 v = 0; v < 16; v++) {
        op1 = mk_ea(AM_immediate, v);
        bind_opcode("0100 1110 0100 ....", 4, &core::ins_TRAP, &disasm_TRAP, v, &op1, NULL, 0, OM_qimm);
    }

    bind_opcode("0100 1110 0111 0110", 4, &core::ins_TRAPV, &disasm_TRAPV, 0, NULL, NULL, 0, OM_none);

    for (u32 m = 0; m < 8; m++) {
        for (u32 r = 0; r < 8; r++) {
            if ((m == 1) || ((m == 7) && (r >= 2))) continue;
            op1 = mk_ea(m, r);
            if (m!=1) bind_opcode("0100 1010 00.. ....", 1, &core::ins_TST, &disasm_TST, (m << 3) | r, &op1, NULL, 0, OM_ea);
            bind_opcode("0100 1010 01.. ....", 2, &core::ins_TST, &disasm_TST, (m << 3) | r, &op1, NULL, 0, OM_ea);
            bind_opcode("0100 1010 10.. ....", 4, &core::ins_TST, &disasm_TST, (m << 3) | r, &op1, NULL, 0, OM_ea);
        }
    }

    for (u32 a = 0; a < 8; a++) {
        op1 = mk_ea(AM_address_register_direct, a);
        bind_opcode("0100 1110 0101 1...", 4, &core::ins_UNLK, &disasm_UNLK, a, &op1, NULL, 0, OM_r);
    }
//BAD TEMPLATE: ILLEGALS

    for (u32 d = 0; d < 4096; d++) {
        bind_opcode("1010 .... .... ....", 4, &core::ins_ALINE, &disasm_ALINE, d, NULL, NULL, 0, OM_none);
    }
