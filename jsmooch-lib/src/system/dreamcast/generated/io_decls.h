
        union {  // SB_CD2STAT
            struct {
                u32 : 5;
                u32 dma_ch2_tex_mem_start_addr : 21;
            } f;
            u32 u;
        } SB_CD2STAT;  // 0x005F6800
        union {  // SB_CD2LEN
            struct {
                u32 : 5;
                u32 dma_ch2_tx_len : 19;
            } f;
            u32 u;
        } SB_CD2LEN;  // 0x005F6804
        u32 SB_C2DST;  // 0x005F6808
        union {  // SB_SDSTAW
            struct {
                u32 : 5;
                u32 sortdma_start_addr : 22;
            } f;
            u32 u;
        } SB_SDSTAW;  // 0x005F6810
        union {  // SB_SDBAAW
            struct {
                u32 : 5;
                u32 sortdma_base_addr : 22;
            } f;
            u32 u;
        } SB_SDBAAW;  // 0x005F6814
        u32 SB_SDWLT;  // 0x005F6818
        u32 SB_SDLAS;  // 0x005F681C
        u32 SB_SDST;  // 0x005F6820
        u32 SB_DBREQM;  // 0x005F6840
        u32 SB_BAVLWC;  // 0x005F6844
        u32 SB_C2DPRYC;  // 0x005F6848
        u32 SB_C2DMAXL;  // 0x005F684C
        u32 SB_LMMODE0;  // 0x005F6884
        u32 SB_LMMODE1;  // 0x005F6888
        u32 SB_RBSPLT;  // 0x005F68A0
        u32 SB_UKN5F68A4;  // 0x005F68A4
        u32 SB_UKN5F68AC;  // 0x005F68AC
        u32 SB_IML2NRM;  // 0x005F6910
        union {  // SB_IML2EXT
            struct {
                u32 gdrom : 1;
                u32 aica : 1;
                u32 modem : 1;
                u32 ext_device : 1;
            } f;
            u32 u;
        } SB_IML2EXT;  // 0x005F6914
        union {  // SB_IML2ERR
            struct {
                u32 render_isp_out_of_cache : 1;
                u32 render_hazard_processing_strip_buffer : 1;
                u32 ta_isp_parameter_overflow : 1;
                u32 ta_object_list_pointer_overflow : 1;
                u32 ta_illegal_parameter : 1;
                u32 ta_fifo_overflow : 1;
                u32 pvrif_illegal_addr : 1;
                u32 pvrif_dma_overrun : 1;
                u32 maple_illegal_addr : 1;
                u32 maple_dma_overrun : 1;
                u32 maple_write_fifo_overflow : 1;
                u32 maple_illegal_cmd : 1;
                u32 g1_illegal_addr : 1;
                u32 g1_gdma_overrun : 1;
                u32 g1_romflash_access_at_gdma : 1;
                u32 g2_aica_dma_illegal_addr_set : 1;
                u32 g2_ext_dma1_illegal_addr_set : 1;
                u32 g2_ext_dma2_illegal_addr_set : 1;
                u32 g2_dev_dma_illegal_addr_set : 1;
                u32 g2_aica_dma_overrun : 1;
                u32 g2_ext_dma1_overrun : 1;
                u32 g2_ext_dma2_overrun : 1;
                u32 g2_dev_dma_overrun : 1;
                u32 g2_aica_dma_timeout : 1;
                u32 g2_ext_dma1_timeout : 1;
                u32 g2_ext_dma2_timeout : 1;
                u32 g2_dev_dma_timeout : 1;
                u32 cpu_acess_timeout : 1;
                u32 sort_dma_cmd_error : 1;
                u32 : 2;
                u32 sh4_if : 1;
            } f;
            u32 u;
        } SB_IML2ERR;  // 0x005F6918
        u32 SB_IML4NRM;  // 0x005F6920
        union {  // SB_IML4EXT
            struct {
                u32 gdrom : 1;
                u32 aica : 1;
                u32 modem : 1;
                u32 ext_device : 1;
            } f;
            u32 u;
        } SB_IML4EXT;  // 0x005F6924
        union {  // SB_IML4ERR
            struct {
                u32 render_isp_out_of_cache : 1;
                u32 render_hazard_processing_strip_buffer : 1;
                u32 ta_isp_parameter_overflow : 1;
                u32 ta_object_list_pointer_overflow : 1;
                u32 ta_illegal_parameter : 1;
                u32 ta_fifo_overflow : 1;
                u32 pvrif_illegal_addr : 1;
                u32 pvrif_dma_overrun : 1;
                u32 maple_illegal_addr : 1;
                u32 maple_dma_overrun : 1;
                u32 maple_write_fifo_overflow : 1;
                u32 maple_illegal_cmd : 1;
                u32 g1_illegal_addr : 1;
                u32 g1_gdma_overrun : 1;
                u32 g1_romflash_access_at_gdma : 1;
                u32 g2_aica_dma_illegal_addr_set : 1;
                u32 g2_ext_dma1_illegal_addr_set : 1;
                u32 g2_ext_dma2_illegal_addr_set : 1;
                u32 g2_dev_dma_illegal_addr_set : 1;
                u32 g2_aica_dma_overrun : 1;
                u32 g2_ext_dma1_overrun : 1;
                u32 g2_ext_dma2_overrun : 1;
                u32 g2_dev_dma_overrun : 1;
                u32 g2_aica_dma_timeout : 1;
                u32 g2_ext_dma1_timeout : 1;
                u32 g2_ext_dma2_timeout : 1;
                u32 g2_dev_dma_timeout : 1;
                u32 cpu_acess_timeout : 1;
                u32 sort_dma_cmd_error : 1;
                u32 : 2;
                u32 sh4_if : 1;
            } f;
            u32 u;
        } SB_IML4ERR;  // 0x005F6928
        u32 SB_IML6NRM;  // 0x005F6930
        union {  // SB_IML6EXT
            struct {
                u32 gdrom : 1;
                u32 aica : 1;
                u32 modem : 1;
                u32 ext_device : 1;
            } f;
            u32 u;
        } SB_IML6EXT;  // 0x005F6934
        union {  // SB_IML6ERR
            struct {
                u32 render_isp_out_of_cache : 1;
                u32 render_hazard_processing_strip_buffer : 1;
                u32 ta_isp_parameter_overflow : 1;
                u32 ta_object_list_pointer_overflow : 1;
                u32 ta_illegal_parameter : 1;
                u32 ta_fifo_overflow : 1;
                u32 pvrif_illegal_addr : 1;
                u32 pvrif_dma_overrun : 1;
                u32 maple_illegal_addr : 1;
                u32 maple_dma_overrun : 1;
                u32 maple_write_fifo_overflow : 1;
                u32 maple_illegal_cmd : 1;
                u32 g1_illegal_addr : 1;
                u32 g1_gdma_overrun : 1;
                u32 g1_romflash_access_at_gdma : 1;
                u32 g2_aica_dma_illegal_addr_set : 1;
                u32 g2_ext_dma1_illegal_addr_set : 1;
                u32 g2_ext_dma2_illegal_addr_set : 1;
                u32 g2_dev_dma_illegal_addr_set : 1;
                u32 g2_aica_dma_overrun : 1;
                u32 g2_ext_dma1_overrun : 1;
                u32 g2_ext_dma2_overrun : 1;
                u32 g2_dev_dma_overrun : 1;
                u32 g2_aica_dma_timeout : 1;
                u32 g2_ext_dma1_timeout : 1;
                u32 g2_ext_dma2_timeout : 1;
                u32 g2_dev_dma_timeout : 1;
                u32 cpu_acess_timeout : 1;
                u32 sort_dma_cmd_error : 1;
                u32 : 2;
                u32 sh4_if : 1;
            } f;
            u32 u;
        } SB_IML6ERR;  // 0x005F6938
        u32 SB_PDTNRM;  // 0x005F6940
        u32 SB_PDTEXT;  // 0x005F6944
        u32 SB_G2DTNRM;  // 0x005F6950
        u32 SB_GD2TEXT;  // 0x005F6954
        union {  // PTEH
            struct {
                u32 asid : 8;
                u32 : 2;
                u32 vpn : 22;
            } f;
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
            } f;
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
            } f;
            u32 u;
        } MMUCR;  // 0xFF000010
        union {  // PTEA
            struct {
                u32 sa : 3;
                u32 tc : 1;
            } f;
            u32 u;
        } PTEA;  // 0xFF000034
        u32 UKNFF200008;  // 0xFF200008
        u32 UKNFF200014;  // 0xFF200014
        u32 UKNFF200020;  // 0xFF200020
        u32 BCR2;  // 0xFF800004
        u32 WCR1;  // 0xFF800008
        u32 WCR2;  // 0xFF80000C
        u32 WCR3;  // 0xFF800010
        u32 MCR;  // 0xFF800014
        u32 PCR;  // 0xFF800018
        u32 RTCSR;  // 0xFF80001C
        u32 RTCOR;  // 0xFF800024
        u32 PCTRA;  // 0xFF80002C
        u32 PCTRB;  // 0xFF800040
        u32 PDTRB;  // 0xFF800044
        u32 GPIOIC;  // 0xFF800048
        u32 SDMR;  // 0xFF940190
        u32 SAR1;  // 0xFFA00010
        u32 DAR1;  // 0xFFA00014
        u32 DMATCR1;  // 0xFFA00018
        union {  // CHCR1
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
            } f;
            u32 u;
        } CHCR1;  // 0xFFA0001C
        u32 SAR2;  // 0xFFA00020
        u32 DAR2;  // 0xFFA00024
        u32 DMATCR2;  // 0xFFA00028
        union {  // CHCR2
            struct {
                u32 DE : 1;
                u32 TE : 1;
                u32 IE : 1;
                u32 : 9;
                u32 SM : 2;
            } f;
            u32 u;
        } CHCR2;  // 0xFFA0002C
        u32 SAR3;  // 0xFFA00030
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
            } f;
            u32 u;
        } DMAC_CHCR3;  // 0xFFA0003C
        union {  // DMAOR
            struct {
                u32 : 1;
                u32 NMIF : 1;
                u32 AE : 1;
            } f;
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
            } f;
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
            } f;
            u32 u;
        } RCR1;  // 0xFFC80038
        union {  // RCR2
            struct {
                u32 : 7;
                u32 PEF : 1;
            } f;
            u32 u;
        } RCR2;  // 0xFFC80040
        u32 ICR;  // 0xFFD00000
        union {  // IPRA
            struct {
                u32 : 4;
                u32 TMU2 : 4;
                u32 TMU1 : 4;
                u32 TMU0 : 4;
            } f;
            u32 u;
        } IPRA;  // 0xFFD00004
        union {  // IPRB
            struct {
                u32 : 8;
                u32 REF : 4;
                u32 WDT : 4;
            } f;
            u32 u;
        } IPRB;  // 0xFFD00008
        union {  // IPRC
            struct {
                u32 UDI : 4;
                u32 SCIF : 4;
                u32 DMAC : 4;
            } f;
            u32 u;
        } IPRC;  // 0xFFD0000C
        u32 TCOR0;  // 0xFFD80008
        u32 TCNT0;  // 0xFFD8000C
        u32 TCR0;  // 0xFFD80010
        u32 TCOR1;  // 0xFFD80014
        u32 TMU_TCNT1;  // 0xFFD80018
        u32 TMU_TCR1;  // 0xFFD8001C
        u32 TMU_TCOR2;  // 0xFFD80020
        u32 TMU_TCNT2;  // 0xFFD80024
        u32 TMU_TCR2;  // 0xFFD80028
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