
        u32 SB_MDSTAR;  // 0x005F6C04
        u32 SB_MDTSEL;  // 0x005F6C10
        u32 SB_MDEN;  // 0x005F6C14
        u32 SB_MDST;  // 0x005F6C18
        u32 SB_MSYS;  // 0x005F6C80
        union {  // SB_MDAPRO
            struct {
                u32 bottom_addr : 7;
                u32 : 1;
                u32 top_addr : 7;
            } f;
            u32 u;
        } SB_MDAPRO;  // 0x005F6C8C
        u32 SB_MMSEL;  // 0x005F6CE8