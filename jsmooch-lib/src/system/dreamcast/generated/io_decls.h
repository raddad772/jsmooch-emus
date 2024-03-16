
        u32 SB_C2DSTAT;  // 0x005F6800
        u32 SB_C2DLEN;  // 0x005F6804
        u32 SB_C2DST;  // 0x005F6808
        u32 SB_SDSTAW;  // 0x005F6810
        u32 SB_SDBAAW;  // 0x005F6814
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
        union {  // SB_ISTNRM
            struct {
                u32 render_end_video : 1;
                u32 render_end_isp : 1;
                u32 render_end_tsp : 1;
                u32 vblank_in : 1;
                u32 vblank_out : 1;
                u32 hblank_in : 1;
                u32 end_of_tx_yuv : 1;
                u32 end_of_tx_opaque_list : 1;
                u32 end_of_tx_opaque_modifier_list : 1;
                u32 end_of_tx_translucent_list : 1;
                u32 end_of_tx_translucent_modifier_list : 1;
                u32 end_of_dma_pvr : 1;
                u32 end_of_dma_maple : 1;
                u32 vblank_over_maple : 1;
                u32 end_of_gdrom_dma : 1;
                u32 end_of_aica_dma : 1;
                u32 end_of_e1_dma : 1;
                u32 end_of_e2_dma : 1;
                u32 end_of_dev_dma : 1;
                u32 end_of_ch2_dma : 1;
                u32 end_of_sort_dma : 1;
                u32 end_of_tx_ptl : 1;
            };
            u32 u;
        } SB_ISTNRM;  // 0x005F6900
        union {  // SB_ISTEXT
            struct {
                u32 gdrom : 1;
                u32 aica : 1;
                u32 modem : 1;
                u32 ext_device : 1;
            };
            u32 u;
        } SB_ISTEXT;  // 0x005F6904
        union {  // SB_ISTERR
            struct {
                u32 render_isp_out_of_cache : 1;
                u32 render_hazard_processing_strip_buffer : 1;
                u32 ta_isp_tsp_parameter_overflow : 1;
                u32 ta_object_list_pointer_overflow : 1;
                u32 ta_illegal_parameter : 1;
                u32 ta_fifo_overflow : 1;
                u32 pvrif_illegal_addr : 1;
                u32 pvrif_dma_overrun : 1;
                u32 maple_illegal_addr : 1;
                u32 maple_dma_overrun : 1;
                u32 maple_write_fifo_overflow : 1;
                u32 maple_illegal_command : 1;
                u32 g1_illegal_addr : 1;
                u32 g1_gddma_overrun : 1;
                u32 g1_romflash_access_at_gdma : 1;
                u32 g2_aica_dma_illegal_addr : 1;
                u32 g2_e1_dma_illegal_addr : 1;
                u32 g2_e2_dma_illegal_addr : 1;
                u32 g2_dev_dma_illegal_adr : 1;
                u32 g2_aica_dma_overrun : 1;
                u32 g2_e1_dma_overrun : 1;
                u32 g2_e2_dma_overrun : 1;
                u32 g2_dev_dma_overrun : 1;
                u32 g2_aica_dma_timeout : 1;
                u32 g2_e1_dma_timeout : 1;
                u32 g2_e2_dma_timeout : 1;
                u32 g2_dev_dma_timeout : 1;
                u32 g2_cpu_timeout : 1;
                u32 ddt_if : 1;
                u32 : 2;
                u32 sh4_if : 1;
            };
            u32 u;
        } SB_ISTERR;  // 0x005F6908
        u32 SB_IML2NRM;  // 0x005F6910
        union {  // SB_IML2EXT
            struct {
                u32 gdrom : 1;
                u32 aica : 1;
                u32 modem : 1;
                u32 ext_device : 1;
            };
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
            };
            u32 u;
        } SB_IML2ERR;  // 0x005F6918
        u32 SB_IML4NRM;  // 0x005F6920
        union {  // SB_IML4EXT
            struct {
                u32 gdrom : 1;
                u32 aica : 1;
                u32 modem : 1;
                u32 ext_device : 1;
            };
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
            };
            u32 u;
        } SB_IML4ERR;  // 0x005F6928
        u32 SB_IML6NRM;  // 0x005F6930
        union {  // SB_IML6EXT
            struct {
                u32 gdrom : 1;
                u32 aica : 1;
                u32 modem : 1;
                u32 ext_device : 1;
            };
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
            };
            u32 u;
        } SB_IML6ERR;  // 0x005F6938
        u32 SB_PDTNRM;  // 0x005F6940
        u32 SB_PDTEXT;  // 0x005F6944
        u32 SB_G2DTNRM;  // 0x005F6950
        u32 SB_GD2TEXT;  // 0x005F6954