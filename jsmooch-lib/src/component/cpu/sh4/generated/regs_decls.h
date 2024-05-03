
        union {  // PTEH
            struct {
                u32 asid : 8;
                u32 : 2;
                u32 vpn : 22;
            };
            u32 u;
        } PTEH;  // 0xFF000000
        union {  // PTEL
            struct {
                u32 wt : 1;
                u32 sh : 1;
                u32 d : 1;
                u32 c : 1;
                u32 sz_a : 1;
                u32 pr : 2;
                u32 sz_b : 1;
                u32 v : 1;
                u32 : 1;
                u32 ppn : 19;
            };
            u32 u;
        } PTEL;  // 0xFF000004
        u32 TTB;  // 0xFF000008
        u32 TEA;  // 0xFF00000C
        union {  // MMUCR
            struct {
                u32 at : 1;
                u32 : 1;
                u32 ti : 1;
                u32 : 5;
                u32 sv : 1;
                u32 sqmd : 1;
                u32 urc : 6;
                u32 : 2;
                u32 urb : 6;
                u32 : 2;
                u32 lrui : 6;
            };
            u32 u;
        } MMUCR;  // 0xFF000010
        union {  // CCR
            struct {
                u32 OCE : 1;
                u32 WT : 1;
                u32 CB : 1;
                u32 OCI : 1;
                u32 : 1;
                u32 ORA : 1;
                u32 : 1;
                u32 OIX : 1;
                u32 ICE : 1;
                u32 : 2;
                u32 ICI : 1;
                u32 : 3;
                u32 IIX : 1;
            };
            u32 u;
        } CCR;  // 0xFF00001C
        u32 EXPEVT;  // 0xFF000020
        u32 TRAPA;  // 0xFF000024
        u32 INTEVT;  // 0xFF000028
        union {  // PTEA
            struct {
                u32 sa : 3;
                u32 tc : 1;
            };
            u32 u;
        } PTEA;  // 0xFF000034
        u32 UKNFF200008;  // 0xFF200008
        u32 UKNFF200014;  // 0xFF200014
        u32 UKNFF200020;  // 0xFF200020
        u32 BSCR;  // 0xFF800000
        u32 BCR2;  // 0xFF800004
        u32 WCR1;  // 0xFF800008
        u32 WCR2;  // 0xFF80000C
        u32 WCR3;  // 0xFF800010
        u32 MCR;  // 0xFF800014
        u32 PCR;  // 0xFF800018
        u32 RTCSR;  // 0xFF80001C
        u32 RTCOR;  // 0xFF800024
        u32 RFCR;  // 0xFF800028
        u32 PCTRA;  // 0xFF80002C
        u32 PDTRA;  // 0xFF800030
        u32 PCTRB;  // 0xFF800040
        u32 PDTRB;  // 0xFF800044
        u32 GPIOIC;  // 0xFF800048
        u32 SDMR;  // 0xFF940190
        u32 DMAC_SAR1;  // 0xFFA00010
        u32 DMAC_DAR1;  // 0xFFA00014
        u32 DMAC_DMATCR1;  // 0xFFA00018
        union {  // DMAC_CHCR1
            struct {
                u32 DE : 1;
                u32 TE : 1;
                u32 IE : 1;
                u32 : 1;
                u32 TS : 3;
                u32 : 1;
                u32 RS : 4;
                u32 SM : 2;
                u32 DM : 2;
            };
            u32 u;
        } DMAC_CHCR1;  // 0xFFA0001C
        u32 DMAC_SAR2;  // 0xFFA00020
        u32 DMAC_DAR2;  // 0xFFA00024
        u32 DMAC_DMATCR2;  // 0xFFA00028
        union {  // DMAC_CHCR2
            struct {
                u32 DE : 1;
                u32 TE : 1;
                u32 IE : 1;
                u32 : 9;
                u32 SM : 2;
            };
            u32 u;
        } DMAC_CHCR2;  // 0xFFA0002C
        u32 DMAC_SAR3;  // 0xFFA00030
        u32 DMAC_DAR3;  // 0xFFA00034
        u32 DMAC_DMATCR3;  // 0xFFA00038
        union {  // DMAC_CHCR3
            struct {
                u32 DE : 1;
                u32 TE : 1;
                u32 IE : 1;
                u32 : 1;
                u32 TS : 3;
                u32 : 1;
                u32 RS : 4;
                u32 SM : 2;
                u32 DM : 2;
            };
            u32 u;
        } DMAC_CHCR3;  // 0xFFA0003C
        union {  // DMAOR
            struct {
                u32 : 1;
                u32 NMIF : 1;
                u32 AE : 1;
            };
            u32 u;
        } DMAOR;  // 0xFFA00040
        u32 CPG_FRQCR;  // 0xFFC00000
        u32 STBCR;  // 0xFFC00004
        u32 CPG_WTCNT;  // 0xFFC00008
        union {  // CPG_WTCSR
            struct {
                u32 CKS : 3;
                u32 IOVF : 1;
                u32 WOVF : 1;
                u32 RSTS : 1;
                u32 WT_IT : 1;
                u32 TME : 1;
            };
            u32 u;
        } CPG_WTCSR;  // 0xFFC0000C
        u32 CPG_STBCR2;  // 0xFFC00010
        u32 RDAYAR;  // 0xFFC80030
        u32 RMONAR;  // 0xFFC80034
        union {  // RCR1
            struct {
                u32 AF : 1;
                u32 : 6;
                u32 CF : 1;
            };
            u32 u;
        } RCR1;  // 0xFFC80038
        union {  // RCR2
            struct {
                u32 : 7;
                u32 PEF : 1;
            };
            u32 u;
        } RCR2;  // 0xFFC80040
        union {  // ICR
            struct {
                u16 : 7;
                u16 IRLM : 1;
                u16 NMIE : 1;
                u16 NMIB : 1;
                u16 : 4;
                u16 MAI : 1;
                u16 NMIL : 1;
            };
            u16 u;
        } ICR;  // 0xFFD00000
        union {  // IPRA
            struct {
                u16 : 4;
                u16 TMU2 : 4;
                u16 TMU1 : 4;
                u16 TMU0 : 4;
            };
            u16 u;
        } IPRA;  // 0xFFD00004
        union {  // IPRB
            struct {
                u16 : 8;
                u16 REF : 4;
                u16 WDT : 4;
            };
            u16 u;
        } IPRB;  // 0xFFD00008
        union {  // IPRC
            struct {
                u32 UDI : 4;
                u32 SCIF : 4;
                u32 DMAC : 4;
            };
            u32 u;
        } IPRC;  // 0xFFD0000C
        u32 TCR0;  // 0xFFD80010
        u32 SCIF_SCSMR2;  // 0xFFE80000
        u32 SCIF_SCBRR2;  // 0xFFE80004
        u32 SCIF_SCSCR2;  // 0xFFE80008
        u32 SCIF_SCFTDR2;  // 0xFFE8000C
        u32 SCIF_SCFSR2;  // 0xFFE80010
        u32 SCIF_SCFRDR2;  // 0xFFE80014
        u32 SCIF_SCFCR2;  // 0xFFE80018
        u32 SCIF_SCFDR2;  // 0xFFE8001C
        u32 SCIF_SCSPTR2;  // 0xFFE80020
        u32 SCIF_SCLSR2;  // 0xFFE80024