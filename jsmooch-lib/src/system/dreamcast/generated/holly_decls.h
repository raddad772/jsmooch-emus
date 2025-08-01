
        u32 SPAN_SORT_CFG;  // 0x005F8030
        union {  // VO_BORDER_COL
            struct {
                u32 blue : 8;
                u32 green : 8;
                u32 red : 8;
                u32 chroma : 1;
            };
            u32 u;
        } VO_BORDER_COL;  // 0x005F8040
        union {  // FB_R_CTRL
            struct {
                u32 fb_enable : 1;
                u32 fb_line_double : 1;
                u32 fb_depth : 2;
                u32 fb_concat : 3;
                u32 : 1;
                u32 fb_chroma_threshold : 8;
                u32 fb_stripsize : 6;
                u32 fb_strip_buf_en : 1;
                u32 vclk_div : 1;
            };
            u32 u;
        } FB_R_CTRL;  // 0x005F8044
        union {  // FB_W_CTRL
            struct {
                u32 fb_packmode : 3;
                u32 fb_dither : 1;
                u32 : 4;
                u32 fb_kval : 8;
                u32 fb_alpha_threshold : 8;
            };
            u32 u;
        } FB_W_CTRL;  // 0x005F8048
        union {  // FB_W_LINESTRIDE
            struct {
                u32 line_stride : 9;
            };
            u32 u;
        } FB_W_LINESTRIDE;  // 0x005F804C
        union {  // FB_R_SOF1
            struct {
                u32 : 2;
                u32 field : 22;
            };
            u32 u;
        } FB_R_SOF1;  // 0x005F8050
        union {  // FB_R_SOF2
            struct {
                u32 : 2;
                u32 field : 22;
            };
            u32 u;
        } FB_R_SOF2;  // 0x005F8054
        union {  // FB_R_SIZE
            struct {
                u32 fb_x_size : 10;
                u32 fb_y_size : 10;
                u32 fb_modulus : 10;
            };
            u32 u;
        } FB_R_SIZE;  // 0x005F805C
        union {  // FB_W_SOF1
            struct {
                u32 : 2;
                u32 field1_ptr : 23;
            };
            u32 u;
        } FB_W_SOF1;  // 0x005F8060
        union {  // FB_W_SOF2
            struct {
                u32 : 2;
                u32 field2_ptr : 23;
            };
            u32 u;
        } FB_W_SOF2;  // 0x005F8064
        union {  // FB_X_CLIP
            struct {
                u32 fb_x_clipping_min : 11;
                u32 : 5;
                u32 fb_x_clipping_max : 11;
            };
            u32 u;
        } FB_X_CLIP;  // 0x005F8068
        union {  // FB_Y_CLIP
            struct {
                u32 fb_y_clipping_min : 10;
                u32 : 6;
                u32 fb_y_clipping_max : 10;
            };
            u32 u;
        } FB_Y_CLIP;  // 0x005F806C
        union {  // FPU_SHAD_SCALE
            struct {
                u32 shadow_scale_factor : 8;
                u32 intensity_shadow_enable : 1;
            };
            u32 u;
        } FPU_SHAD_SCALE;  // 0x005F8074
        union {  // FPU_PARAM_CFG
            struct {
                u32 ptr_first_burst_size : 4;
                u32 ptr_burst_size : 4;
                u32 isp_parameter_burst_trigger_threshold : 6;
                u32 tsp_parameter_burst_trigger_threshold : 6;
                u32 : 1;
                u32 region_header_type : 1;
            };
            u32 u;
        } FPU_PARAM_CFG;  // 0x005F807C
        union {  // HALF_OFFSET
            struct {
                u32 fpu_pixel_sampling_position : 1;
                u32 tsp_pixel_sampling_position : 1;
                u32 tsp_texel_sampling_position : 1;
            };
            u32 u;
        } HALF_OFFSET;  // 0x005F8080
        union {  // ISP_BACKGND_T
            struct {
                u32 tag_offset : 3;
                u32 tag_address : 21;
                u32 skip : 3;
                u32 shadow : 1;
                u32 cache_bypass : 1;
            };
            u32 u;
        } ISP_BACKGND_T;  // 0x005F808C
        union {  // ISP_FEED_CFG
            struct {
                u32 pre_sort_mode : 1;
                u32 : 2;
                u32 discard_mode : 1;
                u32 punch_through_chunk_size : 10;
                u32 cache_size_for_translucency : 10;
            };
            u32 u;
        } ISP_FEED_CFG;  // 0x005F8098
        u32 SDRAM_REFRESH;  // 0x005F80A0
        u32 SDRAM_CFG;  // 0x005F80A8
        u32 FOG_COL_RAM;  // 0x005F80B0
        union {  // FOG_COL_VERT
            struct {
                u32 blue : 8;
                u32 green : 8;
                u32 red : 8;
            };
            u32 u;
        } FOG_COL_VERT;  // 0x005F80B4
        union {  // FOG_DENSITY
            struct {
                u32 exponent : 8;
                u32 mantissa : 8;
            };
            u32 u;
        } FOG_DENSITY;  // 0x005F80B8
        u32 FOG_CLAMP_MAX;  // 0x005F80BC
        u32 FOG_CLAMP_MIN;  // 0x005F80C0
        union {  // SPG_HBLANK_INT
            struct {
                u32 line_comp_val : 10;
                u32 : 2;
                u32 hblank_int_mode : 2;
                u32 : 2;
                u32 hblank_in_interrupt : 10;
            };
            u32 u;
        } SPG_HBLANK_INT;  // 0x005F80C8
        union {  // SPG_VBLANK_INT
            struct {
                u32 vblank_in_line : 10;
                u32 : 6;
                u32 vblank_out_line : 10;
            };
            u32 u;
        } SPG_VBLANK_INT;  // 0x005F80CC
        union {  // SPG_CONTROL
            struct {
                u32 mhsync_pol : 1;
                u32 mvsync_pol : 1;
                u32 mcsync_pol : 1;
                u32 spg_lock : 1;
                u32 interlace : 1;
                u32 force_field2 : 1;
                u32 NTSC : 1;
                u32 PAL : 1;
                u32 sync_direction : 1;
                u32 csync_on_h : 1;
            };
            u32 u;
        } SPG_CONTROL;  // 0x005F80D0
        union {  // SPG_HBLANK
            struct {
                u32 hbstart : 10;
                u32 : 6;
                u32 hbend : 10;
            };
            u32 u;
        } SPG_HBLANK;  // 0x005F80D4
        union {  // SPG_LOAD
            struct {
                u32 hcount : 10;
                u32 : 6;
                u32 vcount : 10;
            };
            u32 u;
        } SPG_LOAD;  // 0x005F80D8
        union {  // SPG_VBLANK
            struct {
                u32 vbstart : 10;
                u32 : 6;
                u32 vbend : 10;
            };
            u32 u;
        } SPG_VBLANK;  // 0x005F80DC
        union {  // SPG_WIDTH
            struct {
                u32 hswidth : 7;
                u32 : 1;
                u32 vswidth : 4;
                u32 bpwidth : 10;
                u32 eqwidth : 10;
            };
            u32 u;
        } SPG_WIDTH;  // 0x005F80E0
        union {  // TEXT_CONTROL
            struct {
                u32 stride : 5;
                u32 : 3;
                u32 bank_bit : 5;
                u32 : 3;
                u32 index_endian_reg : 1;
                u32 cb_endian_reg : 1;
            };
            u32 u;
        } TEXT_CONTROL;  // 0x005F80E4
        union {  // VO_CONTROL
            struct {
                u32 hsync_pol : 1;
                u32 vsync_pol : 1;
                u32 blank_pol : 1;
                u32 blank_video : 1;
                u32 field_mode : 4;
                u32 pixel_double : 1;
                u32 : 7;
                u32 pclk_delay : 6;
            };
            u32 u;
        } VO_CONTROL;  // 0x005F80E8
        u32 VO_STARTX;  // 0x005F80EC
        union {  // VO_STARTY
            struct {
                u32 vertical_start_on_field1 : 10;
                u32 : 6;
                u32 vertical_start_on_field2 : 10;
            };
            u32 u;
        } VO_STARTY;  // 0x005F80F0
        union {  // SCALER_CTL
            struct {
                u32 vertical_scale_factor : 16;
                u32 horizontal_scroll_enable : 1;
                u32 interlace : 1;
                u32 field_select : 1;
            };
            u32 u;
        } SCALER_CTL;  // 0x005F80F4
        union {  // FB_BURSTCTRL
            struct {
                u32 vid_burst : 6;
                u32 : 2;
                u32 vid_lat : 7;
                u32 wr_burst : 4;
            };
            u32 u;
        } FB_BURSTCTRL;  // 0x005F8110
        union {  // Y_COEFF
            struct {
                u32 coeff_0_2 : 8;
                u32 coeff_1 : 8;
            };
            u32 u;
        } Y_COEFF;  // 0x005F8118
        union {  // TA_OL_BASE
            struct {
                u32 : 5;
                u32 base_addr : 19;
            };
            u32 u;
        } TA_OL_BASE;  // 0x005F8124
        u32 TA_ISP_BASE;  // 0x005F8128
        union {  // TA_OL_LIMIT
            struct {
                u32 : 5;
                u32 limit_addr : 19;
            };
            u32 u;
        } TA_OL_LIMIT;  // 0x005F812C
        union {  // TA_ISP_LIMIT
            struct {
                u32 : 2;
                u32 limit_addr : 22;
            };
            u32 u;
        } TA_ISP_LIMIT;  // 0x005F8130
        u32 TA_NEXT_OPB;  // 0x005F8134
        u32 TA_ITP_CURRENT;  // 0x005F8138
        union {  // TA_GLOB_TILE_CLIP
            struct {
                u32 tile_x_num : 6;
                u32 : 10;
                u32 tile_y_num : 4;
            };
            u32 u;
        } TA_GLOB_TILE_CLIP;  // 0x005F813C
        union {  // TA_ALLOC_CTRL
            struct {
                u32 O_OPB : 2;
                u32 : 2;
                u32 OM_OPB : 2;
                u32 : 2;
                u32 T_OPB : 2;
                u32 : 6;
                u32 PT_OPB : 2;
                u32 : 2;
                u32 OPB_MODE : 1;
            };
            u32 u;
        } TA_ALLOC_CTRL;  // 0x005F8140
        u32 TA_LIST_CONT;  // 0x005F8160
        u32 TA_NEXT_OPB_INIT;  // 0x005F8164