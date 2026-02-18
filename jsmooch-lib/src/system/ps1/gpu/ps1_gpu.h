//
// Created by . on 2/15/25.
//

#pragma once
#include "helpers/cvec.h"
#include "helpers/int.h"
#include "helpers/physical_io.h"
#include "rasterize_tri.h"
#include "helpers/scheduler.h"
namespace PS1 {
struct core;
}

namespace PS1::GPU {
struct core;
struct TEXTURE_SAMPLER;
typedef u16 (*texture_sample_func)(TEXTURE_SAMPLER *ts, i32 u, i32 v);

struct TEXTURE_SAMPLER {
    void mk_new(u32 page_x_in, u32 page_y_in, u32 clut_addr_in, core *bus);
    u32 page_x{}, page_y{}, base_addr{}, clut_addr{};
    u8 *VRAM{};
    u32 semi_mode{};
    texture_sample_func sample{};
};


struct VERTEX2 {
    i32 x{}, y{};
    i32 u{}, v{};
    u32 r{}, g{}, b{};
};


struct core {
    explicit core(PS1::core *parent);
    void reset();
    void write_gp0(u32 cmd);
    void write_gp1(u32 cmd);
    [[nodiscard]] u32 get_gpuread();
    [[nodiscard]] u32 get_gpustat();

    u32 TEXPAGE{};
    u32 out_vres{}, out_hres{};
    u32 force_set_mask{};
    void cmd_end();

    struct {
        u32 GPUREAD{};
        u32 frame{};
        union {
            struct {
                u32 texture_page_x_base : 4; // 0-3
                u32 texture_page_y_base : 1; // 4
                u32 semi_transparency : 2; // 5-6
                u32 texture_page_colors : 2; // 7-8
                u32 dither : 1; // 9
                u32 drawing_to_display_area : 1; // 10
                u32 force_set_mask_bit : 1; // 11
                u32 preserve_masked_pixels : 1; // 12
                u32 interlace_field : 1; // 13
                u32 screen_flip_x : 1;// 14
                u32 texture_page_y_base_2 : 1; // 15
                // 16 bits...
                u32 hres2: 1; // 16
                u32 hres1: 2; // 17-18
                u32 vres : 1; // 19
                u32 video_mode_PAL : 1; // 20
                u32 display_area_24bit : 1; // 21
                u32 interlacing : 1; // 22
                u32 display_disabled : 1; // 23
                u32 irq1 : 1; // 24
                u32 dma_data_request_bit : 1; // 25
                u32 ready_recv_cmd : 1;
                u32 ready_vram_to_cpu : 1;
                u32 ready_recv_dma : 1;
                u32 dma_dir : 2;
                u32 interlaced_odd_frame : 1;
            };
            u32 u{};
        } GPUSTAT{};
    } io{};
    u8 VRAM[1024 * 1024]{};

    void (core::*current_ins)(){};
    void new_frame();
    u8 mmio_buffer[96]{};
    u32 IRQ_bit{};

    u32 CMD[32]{};
    u32 cmd_arg_num{}, cmd_arg_index{};
    PS1::core *bus{};
    u32 ins_special{};

    struct {
        i32 texture_x_flip{};
        i32 texture_y_flip{};
    } rect{};

    u32 tx_win_x_mask{}, tx_win_y_mask{};
    u32 tx_win_x_offset{}, tx_win_y_offset{};
    u32 draw_area_top{}, draw_area_bottom{}, draw_area_left{}, draw_area_right{};
    i32 draw_x_offset{}, draw_y_offset{}; // applied to all vertices
    u32 display_vram_x_start{}, display_vram_y_start{};
    u32 display_horiz_start{}, display_horiz_end{};
    u32 display_line_start{}, display_line_end{};

    struct COLOR_SAMPLER {
        i32 r_start{}, g_start{}, b_start{};
        i32 r{}, g{}, b{};
        i32 r_end{}, g_end{}, b_end{};
    } color1{}, color2{}, color3{};

    RT_POINT2D V0{}, V1{}, V2{}, V3{}, V4{}, V5{}, T0{}, T1{}, T2{}, T3{}, T4{};

    struct VERTEX3u {
        u32 x{}, y{}, z{};
        i32 r{}, g{}, b{};
        float u{}, v{};
    } s0{}, s1{}, s2{}, s3{};

    VERTEX2 vert[4]{};

    struct {
        u32 x{}, y{}, width{}, height{}, img_x{}, img_y{}, line_ptr{};
    } load_buffer{};

    struct POLYGON {
        u32 shading{}, vertices{}, textured{}, transparent{};
        u32 raw_texture{}, rgb{}, num_cmds{}, clut{};
        u32 tx_page{};
        VERTEX2 vert[4]{};
    } polygon{};

    i32 gp0_transfer_remaining{};
    bool VRAM_to_CPU_in_progress{};

    void (core::*handle_gp0)(u32 cmd);

    u16 *cur_output{};
    cvec_ptr<physical_io_device> display_ptr{};
    JSM_DISPLAY *display{};

private:
    void unready_recv_dma() { io.GPUSTAT.ready_recv_dma = 0; }
    void unready_vram_to_CPU() { io.GPUSTAT.ready_vram_to_cpu = 0; }
    void unready_all() { unready_cmd(); unready_recv_dma(); unready_vram_to_CPU(); }
    void ready_cmd() { io.GPUSTAT.ready_recv_cmd = 1; }
    void ready_recv_dma() { io.GPUSTAT.ready_recv_dma = 1; }
    void ready_vram_to_CPU() { io.GPUSTAT.ready_vram_to_cpu = 1; }
    void ready_all() { ready_cmd(); ready_recv_dma(); ready_vram_to_CPU(); }

    void unready_cmd();
    void gp0_cmd(u32 cmd);
    void RT_draw_flat_triangle(const RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2, u32 color);
    void RT_draw_flat_tex_triangle(const RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2, TEXTURE_SAMPLER *ts);
    void RT_draw_flat_tex_triangle_modulated(const RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2, u32 color, TEXTURE_SAMPLER *ts);
    void RT_draw_flat_tex_triangle_modulated_semi(const RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2, u32 color, TEXTURE_SAMPLER *ts);
    void RT_draw_shaded_tex_triangle_modulated(const RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2, TEXTURE_SAMPLER *ts);
    void RT_draw_shaded_tex_triangle_modulated_semi(const RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2, TEXTURE_SAMPLER *ts);
    void RT_draw_flat_tex_triangle_semi(const RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2, TEXTURE_SAMPLER *ts);
    void RT_draw_flat_triangle_semi(const RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2, u32 r, u32 g, u32 b);
    void RT_draw_shaded_triangle(const RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2);
    void RT_draw_shaded_triangle_semi(const RT_POINT2D *v0, RT_POINT2D *v1, RT_POINT2D *v2);

    void cmd02_quick_rect();
    void cmd28_draw_flat4untex();
    void cmd20_tri_flat();
    void cmd22_tri_flat_semi_transparent();
    void cmd24_tri_raw_modulated();
    void cmd25_tri_raw();
    void cmd26_tri_modulated_semi_transparent();
    void cmd27_tri_raw_semi_transparent();
    void cmd2a_quad_flat_semi_transparent();
    void cmd2c_quad_opaque_flat_textured_modulated();
    void cmd2d_quad_opaque_flat_textured();
    void cmd2e_quad_flat_textured_modulated();
    void cmd2f_quad_flat_textured_semi();
    void cmd30_tri_shaded_opaque();
    void cmd32_tri_shaded_semi_transparent();
    void cmd34_tri_shaded_opaque_tex_modulated();
    void cmd36_tri_shaded_opaque_tex_modulated_semi();
    void cmd38_quad_shaded_opaque();
    void cmd3a_quad_shaded_semi_transparent();
    void cmd3c_quad_opaque_shaded_textured_modulated();
    void cmd3e_quad_opaque_shaded_textured_modulated_semi();
    void cmd40_line_opaque();
    void cmd60_rect_opaque_flat();
    void cmd64_rect_opaque_flat_textured_modulated();
    void cmd65_rect_opaque_flat_textured();
    void cmd66_rect_semi_flat_textured_modulated();
    void cmd67_rect_semi_flat_textured();
    void cmd68_rect_1x1();
    void cmd6d_rect_1x1_tex();
    void cmd6c_rect_1x1_tex_modulated();
    void cmd6e_rect_1x1_tex_semi_modulated();
    void cmd6f_rect_1x1_tex_semi();
    void cmd80_vram_copy();
    void rect_opaque_flat_textured_modulated_xx(u32 wh);
    void cmd7c_rect_opaque_flat_textured_modulated_16x16();
    void cmd74_rect_opaque_flat_textured_modulated_8x8();
    void rect_opaque_flat_textured_xx(u32 wh);
    void cmd75_rect_opaque_flat_textured_8x8();
    void cmd7d_rect_opaque_flat_textured_16x16();
    void rect_semi_flat_textured_modulated_xx(u32 wh);
    void cmd76_rect_semi_flat_textured_modulated_8x8();
    void cmd7e_rect_semi_flat_textured_modulated_16x16();
    void rect_semi_flat_textured_xx(u32 wh);
    void cmd77_rect_semi_flat_textured_8x8();
    void cmd7f_rect_semi_flat_textured_16x16();
    void load_buffer_reset(u32 x, u32 y, u32 width, u32 height);
    void gp0_image_load_continue(u32 cmd);
    void gp0_image_load_start();
    void gp0_image_save_start();
    void gp0_image_save_continue();
    void gp0_cmd_unhandled();

    void bresenham_opaque(RT_POINT2D *v1, RT_POINT2D *v2, u32 color);;


    void get_texture_sampler_from_texpage_and_palette(u32 texpage, u32 palette, TEXTURE_SAMPLER *ts);

    void setpix(i32 y, i32 x, u32 color, u32 is_tex, u32 tex_mask);
    void setpix_split(i32 y, i32 x, u32 r, u32 g, u32 b, u32 is_tex, u32 tex_mask);
    void semipix(i32 y, i32 x, u32 color, u32 is_tex, u32 tex_mask);
    void semipixm(i32 y, i32 x, u32 color, u32 mode, u32 is_tex, u32 tex_mask);
    void semipix_split(i32 y, i32 x, u32 r, u32 g, u32 b, u32 is_tex, u32 tex_mask);
    void update_global_texpage(u32 texpage);

    static inline u32 BGR24to15(u32 c)
    {
        return (((c >> 19) & 0x1F) << 10) |
               (((c >> 11) & 0x1F) << 5) |
               ((c >> 3) & 0x1F);
    }

};


}
