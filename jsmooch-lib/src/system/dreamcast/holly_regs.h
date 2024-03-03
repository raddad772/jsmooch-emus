struct {
    union DC_FB_R_CTRL {
        struct {
            u32 fb_enable           : 1; // 0 - enable or disable framebuffer reads
            u32 fb_line_double      : 1; // 1 - 0 = no line-doubling, 1 = line-doubling
            u32 fb_depth            : 2; // 2-3 - 0- 0555RGB 15bit, 0x1 = 565 RGB 16bit,2 = 888 RGB24 packed, 3 = 0888 RGB 32bit
            u32 fb_concat           : 3; // 6-4 - value added to lower end of 5- or 6-bit frame buffer data to get to 8bits. default 0
            u32                     : 1;
            u32 fb_chroma_threshold : 8; // 8-15 - when framebuffer is ARGB8888, when pixel_alpha < this, 0 is output on CHROMA pin
            u32 fb_stripsize        : 6; // 16-21 - size of strip buffer in units of 32 lines. 2 = 32, 4 = 64.
            u32 fb_strip_buf_en     : 1; // 22 - default 0. 1 for strip buffer
            u32 vclk_div            : 1; // 23 - 0 = NTSC/PAL, 1 = VGA
        } f;
        u32 u;
    } FB_R_CTRL;

    union {
        struct {
            u32 fb_packmode: 3; // 0-2 - bit config for writing pixel data to buffer. 0 = 0555 KRGB, 1 = 565 RGB, 2 = 4444 ARGB, 3 = 1555ARGB, 4 = 888RGB24 packed, 5= 0888KRGB 32b, 6= 8888ARGB 32bit, 7=reserved
            u32 fb_dither: 1; // 3 - 0 = discard lower bits. 1 = perform dithering for 16BPP
            u32      : 4;
            u32 fb_kval: 8; // 8-15 - K value for writing to framebuffer (?)
            u32 fb_alpha_threshold: 8; // 16-23 - when ARGB1555 is written, if pixel_alpha > this, 1 is written for A
        } f;
        u32 u;
    } FB_W_CTRL;

    union {
        struct {
            u32 fb_x_size: 10; // 0-9 - x pixel size, in 32-bit units
            u32 fb_y_size: 10; // 10-19 - display line #
            u32 fb_modulus: 10; // 20-29 - in 32-bit units, data between last pixel and next pixel. set to 1 in order to put them right after each other
        } f;
        u32 u;
    } FB_R_SIZE;

    union {
        struct {
            u32 : 2;
            u32 field1_ptr : 23;
        } f;
        u32 u;
    } FB_W_SOF1;
    union {
        struct {
            u32 : 2;
            u32 field2_ptr : 23;
        } f;
        u32 u;
    } FB_W_SOF2;
    union {
        struct {
            u32 line_stride : 9;
        } f;
        u32 u;
    } FB_W_LINESTRIDE;
    union {
        struct {
            u32 : 2;
            u32 field: 22;
        } f;
        u32 u;
    } FB_R_SOF1;
    union {
        struct {
            u32 : 2;
            u32 field: 22;
        } f;
        u32 u;
    } FB_R_SOF2;
    union {
        struct {
            u32 fb_x_clipping_min : 11;
            u32 : 5;
            u32 fb_x_clipping_max : 11;
        } f;
        u32 u;
    } FB_X_CLIP;
    union {
        struct {
            u32 fb_y_clipping_min: 10;
            u32 : 6;
            u32 fb_y_clipping_max: 10;
        } f;
        u32 u;
    } FB_Y_CLIP;
    union {
        struct {
            u32 vid_burst: 6;
            u32 : 2;
            u32 vid_lat: 7;
            u32 wr_burst: 4;
        } f;
        u32 u;
    } FB_BURSTCTRL;

    union {
        struct {
            u32 hcount: 10; // 0-9
            u32       : 6;
            u32 vcount: 10; // 16-25
        } f;
        u32 u;
    } SPG_LOAD;

    u32 SPAN_SORT_CFG;
    u32 FOG_COL_RAM;
    u32 FOG_COL_VERT;
    u32 FOG_DENSITY;
    u32 FOG_TABLE[128];
    u32 FOG_CLAMP_MAX;
    u32 FOG_CLAMP_MIN;

    union {
        struct {
            u32 stride           : 5; // 0-4
            u32                  : 3;
            u32 bank_bit         : 5; // 8-12
            u32                  : 3;
            u32 index_endian_reg : 1; // 16
            u32 cb_endian_reg    : 1; // 17
        } f;
        u32 u;
    } TEXT_CONTROL;

    union {
        struct {
            u32 vertical_scale_factor    : 16;
            u32 horizontal_scroll_enable : 1;
            u32 interlace                : 1;
            u32 field_select             : 1;
        } f;
        u32 u;
    } SCALER_CTL;

    union {
        struct {
            u32 hsync_pol    : 1;
            u32 vsync_pol    : 1;
            u32 blank_pol    : 1;
            u32 blank_video  : 1;
            u32 field_mode   : 4;
            u32 pixel_double : 1;
            u32              : 7;
            u32 pclk_delay   : 6;
        };
        u32 u;
    } VO_CONTROL;

    union {
        struct {
            u32 blue  : 8;
            u32 green : 8;
            u32 red : 8;
            u32 chroma : 1;
        } f;
        u32 u;
    } VO_BORDER_COL; // 005F8040

    u32 TA_OL_BASE; // starting address for object lists
    u32 TA_OL_LIMIT;
    u32 TA_ISP_BASE;
    u32 TA_ISP_LIMIT;
    u32 TA_GLOB_TILE_CLIP;
    u32 TA_ALLOC_CTRL;
    u32 TA_NEXT_OPB_INIT;

    union {
        struct {
            u32 vblank_in_line  : 10; // bits 0-9, default 0x104
            u32                 : 6;
            u32 vblank_out_line : 10; // bits 16-25, default 0x150
        } f;
        u32 u;
    } SPG_VBLANK_INT;


    union {
        struct {
            u32 hbstart : 10;
            u32         : 6;
            u32 hbend   : 10;
        } f;
        u32 u;
    } SPG_HBLANK;
    union {
        struct {
            u32 mhsync_pol: 1;
            u32 mvsync_pol: 1;
            u32 mcsync_pol: 1;
            u32 spg_lock: 1;
            u32 interlace: 1;
            u32 force_field2: 1;
            u32 NTSC: 1;
            u32 PAL: 1;
            u32 sync_direction: 1;
            u32 csync_on_h: 1;
        } f;
        u32 u;
    } SPG_CONTROL;
    union {
        struct {
            u32 vbstart: 10;
            u32        : 6;
            u32 vbend  : 10;
        } f;
        u32 u;
    } SPG_VBLANK;
    union {
        struct {
            u32 hswidth : 7;
            u32         : 1;
            u32 vswidth : 4;
            u32 bpwidth : 10;
            u32 eqwidth : 10;
        } f;
        u32 u;
    } SPG_WIDTH;
    union {
        struct {
            u32 line_comp_val: 10;
            u32              : 2;
            u32 hblank_int_mode: 2;
            u32              : 2;
            u32 gblank_in_interrupt: 10;
        } f;
        u32 u;
    } SPG_HBLANK_INT;

    union {
        struct {
            u32 shadow_scale_factor : 8;
            u32 intensity_shadow_enable : 1;
        } f;
        u32 u;
    } FPU_SHAD_SCALE;
    union {
        struct {
            u32 ptr_first_burst_size: 4;
            u32 ptr_burst_size: 4;
            u32 isp_parameter_burst_trigger_threshold: 6;
            u32 tsp_parameter_burst_trigger_threshold: 6;
            u32 : 1;
            u32 region_header_type: 1;
        } f;
        u32 u;
    } FPU_PARAM_CFG;
    union {
        struct {
            u32 fpu_pixel_sampling_position : 1;
            u32 tsp_pixel_sampling_position : 1;
            u32 tsp_texel_sampling_position : 1;
        } f;
        u32 u;
    } HALF_OFFSET;
    union {
        struct {
            u32 coeff_0_2 : 8;
            u32 coeff_1: 8;
        } f;
        u32 u;
    } Y_COEFF;
    float FPU_CULL_VAL;
    float FPU_PERP_VAL;
    float ISP_BACKGND_D;
    union {
        struct {
            u32 tag_offset: 3; // 0-2
            u32 tag_address: 21; // 3-23
            u32 skip: 3;   // 24-26
            u32 shadow: 1; // 27
            u32 cache_bypass: 1; // 28
        } f;
        u32 u;
    } ISP_BACKGND_T;

    u32 last_used_buffer;
    u32 cur_output_num;
    u32 *cur_output;
    u32 *out_buffer[2];

    u64 master_frame;

} holly;
