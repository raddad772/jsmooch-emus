
        u32 SB_GDSTAR;  // 0x005F7404
        u32 SB_GDLEN;  // 0x005F7408
        u32 SB_GDDIR;  // 0x005F740C
        u32 SB_GDEN;  // 0x005F7414
        u32 SB_GDST;  // 0x005F7418
        u32 SB_G1RRC;  // 0x005F7480
        u32 SB_G1RWC;  // 0x005F7484
        u32 SB_G1FRC;  // 0x005F7488
        u32 SB_G1FWC;  // 0x005F748C
        u32 SB_G1CRC;  // 0x005F7490
        u32 SB_G1CWC;  // 0x005F7494
        u32 SB_G1GDRC;  // 0x005F74A0
        u32 SB_G1GDWC;  // 0x005F74A4
        u32 SB_G1CRDYC;  // 0x005F74B4
        union {  // SB_GDAPRO
            struct {
                u32 bottom_address : 8;
                u32 top_address : 7;
            };
            u32 u;
        } SB_GDAPRO;  // 0x005F74B8