
        u32 SB_ADSTAG;  // 0x005F7800
        u32 SB_ADSTAR;  // 0x005F7804
        union {  // SB_ADLEN
            struct {
                u32 : 5;
                u32 transfer_len : 20;
                u32 : 6;
                u32 end_restart : 1;
            };
            u32 u;
        } SB_ADLEN;  // 0x005F7808
        u32 SB_ADDIR;  // 0x005F780C
        union {  // SB_ADTSEL
            struct {
                u32 tsel0 : 1;
                u32 tsel1 : 1;
                u32 tsel2 : 1;
            };
            u32 u;
        } SB_ADTSEL;  // 0x005F7810
        u32 SB_ADEN;  // 0x005F7814
        u32 SB_ADST;  // 0x005F7818
        union {  // SB_ADSUSP
            struct {
                u32 req_suspend : 1;
            };
            u32 u;
        } SB_ADSUSP;  // 0x005F781C
        u32 SB_E1STAG;  // 0x005F7820
        u32 SB_E1STAR;  // 0x005F7824
        union {  // SB_E1LEN
            struct {
                u32 : 5;
                u32 transfer_len : 20;
                u32 : 6;
                u32 end_restart : 1;
            };
            u32 u;
        } SB_E1LEN;  // 0x005F7828
        u32 SB_E1DIR;  // 0x005F782C
        u32 SB_E1TSEL;  // 0x005F7830
        u32 SB_E1EN;  // 0x005F7834
        u32 SB_E1ST;  // 0x005F7838
        union {  // SB_E1SUSP
            struct {
                u32 req_suspend : 1;
            };
            u32 u;
        } SB_E1SUSP;  // 0x005F783C
        u32 SB_E2STAG;  // 0x005F7840
        u32 SB_E2STAR;  // 0x005F7844
        union {  // SB_E2LEN
            struct {
                u32 : 5;
                u32 transfer_len : 20;
                u32 : 6;
                u32 end_restart : 1;
            };
            u32 u;
        } SB_E2LEN;  // 0x005F7848
        u32 SB_E2DIR;  // 0x005F784C
        u32 SB_E2TSEL;  // 0x005F7850
        u32 SB_E2EN;  // 0x005F7854
        u32 SB_E2ST;  // 0x005F7858
        union {  // SB_E2SUSP
            struct {
                u32 req_suspend : 1;
            };
            u32 u;
        } SB_E2SUSP;  // 0x005F785C
        u32 SB_DDSTAG;  // 0x005F7860
        u32 SB_DDSTAR;  // 0x005F7864
        union {  // SB_DDLEN
            struct {
                u32 : 5;
                u32 transfer_len : 20;
                u32 : 6;
                u32 end_restart : 1;
            };
            u32 u;
        } SB_DDLEN;  // 0x005F7868
        u32 SB_DDDIR;  // 0x005F786C
        u32 SB_DDTSEL;  // 0x005F7870
        u32 SB_DDEN;  // 0x005F7874
        u32 SB_DDST;  // 0x005F7878
        union {  // SB_DDSUSP
            struct {
                u32 req_suspend : 1;
            };
            u32 u;
        } SB_DDSUSP;  // 0x005F787C
        u32 SB_G2DSTO;  // 0x005F7890
        u32 SB_G2TRTO;  // 0x005F7894
        u32 SB_G2MDMTO;  // 0x005F7898
        u32 SB_G2MDMW;  // 0x005F789C
        u32 UKN005F78A0;  // 0x005F78A0
        u32 UKN005F78A4;  // 0x005F78A4
        u32 UKN005F78A8;  // 0x005F78A8
        u32 UKN005F78AC;  // 0x005F78AC
        u32 UKN005F78B0;  // 0x005F78B0
        u32 UKN005F78B4;  // 0x005F78B4
        u32 UKN005F78B8;  // 0x005F78B8
        union {  // SB_G2APRO
            struct {
                u32 bottom_address : 7;
                u32 : 1;
                u32 top_address : 7;
            };
            u32 u;
        } SB_G2APRO;  // 0x005F78BC
        u32 SB_PDSTAP;  // 0x005F7C00
        u32 SB_PDSTAR;  // 0x005F7C04
        u32 SB_PDSLEN;  // 0x005F7C08
        u32 SB_PDDIR;  // 0x005F7C0C
        u32 SB_PDTSEL;  // 0x005F7C10
        u32 SB_PDEN;  // 0x005F7C14
        u32 SB_PDST;  // 0x005F7C18
        union {  // SB_PDAPRO
            struct {
                u32 bottom_address : 7;
                u32 : 1;
                u32 top_address : 7;
            };
            u32 u;
        } SB_PDAPRO;  // 0x005F7C80