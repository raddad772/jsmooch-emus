
        union {  // SB_GDSTAR
            struct {
                u32 : 5;
                u32 gddma_start_addr : 24;
            } f;
            u32 u;
        } SB_GDSTAR;  // 0x005F7404
        union {  // SB_GDLEN
            struct {
                u32 gddma_len : 25;
            } f;
            u32 u;
        } SB_GDLEN;  // 0x005F7408
        union {  // SB_GDDIR
            struct {
                u32 gddma_dir : 1;
            } f;
            u32 u;
        } SB_GDDIR;  // 0x005F740C
        union {  // SB_GDEN
            struct {
                u32 gddma_enable : 1;
            } f;
            u32 u;
        } SB_GDEN;  // 0x005F7414
        u32 SB_GDST;  // 0x005F7418