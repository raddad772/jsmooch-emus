
        u32 SB_MDSTAR;  // 0x005F6C04
        u32 SB_MDTSEL;  // 0x005F6C10
        u32 SB_MDEN;  // 0x005F6C14
        u32 SB_MDST;  // 0x005F6C18
        union {  // SB_MSYS
            struct {
                u32 delay_time : 4;
                u32 : 4;
                u32 sending_rate : 2;
                u32 : 2;
                u32 single_hard_trigger : 1;
                u32 : 3;
                u32 time_out_counter : 16;
            };
            u32 u;
        } SB_MSYS;  // 0x005F6C80
        union {  // SB_MDAPRO
            struct {
                u32 bottom_addr : 7;
                u32 : 1;
                u32 top_addr : 7;
            };
            u32 u;
        } SB_MDAPRO;  // 0x005F6C8C
        u32 SB_MMSEL;  // 0x005F6CE8