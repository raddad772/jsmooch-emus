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
    [[nodiscard]] u32 get_gpuread() const;
    [[nodiscard]] u32 get_gpustat() const;

    struct {
        u32 GPUSTAT{}, GPUREAD{};
        u32 frame;
    } io{};
    u8 VRAM[1024 * 1024]{};

    void (core::*current_ins)(){};
    void new_frame();
    u8 mmio_buffer[96]{};
    u32 gp0_buffer[256]{};
    u32 gp1_buffer[256]{};
    u32 IRQ_bit{};

    u32 CMD[32]{};
    u32 cmd_arg_num{}, cmd_arg_index{};
    PS1::core *bus{};
    u32 ins_special{};

    u32 recv_gp0[1024 * 1024]{};
    u32 recv_gp1[1024 * 1024]{};
    u32 recv_gp0_len{};
    u32 recv_gp1_len{};

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

    u32 page_base_x{}, page_base_y{};
    i32 semi_transparency{};
    enum TEX_DEPTH {
        e_t4bit, e_t8bit, e_t15bit
    } texture_depth{};
    u32 dithering{};
    u32 draw_to_display{};
    u32 force_set_mask_bit{};
    u32 preserve_masked_pixels{};
    enum FIELD {
        e_top,
        e_bottom
    } field{};
    u32 texture_disable{};
    u32 hr1{}, hr2{}, hres{};
    enum VRES {
        e_y240lines, e_y480lines
    } vres{};
    enum VMODE {
        e_ntsc, e_pal
    } vmode{};
    enum DDEPTH {
        e_d15bits, e_d24bits
    } display_depth{};
    u32 interlaced{};
    u32 display_disabled{};
    u32 interrupt{};
    enum DMADIR {
        e_dma_off,
        e_dma_fifo,
        e_dma_cpu_to_gp0,
        e_dma_vram_to_cpu
    } dma_direction{};

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

    u32 gp0_transfer_remaining{};

    void (core::*handle_gp0)(u32 cmd);

    u16 *cur_output{};
    cvec_ptr<physical_io_device> display_ptr{};
    JSM_DISPLAY *display{};

private:
    void unready_recv_dma() { io.GPUSTAT &= 0xEFFFFFFF; }
    void unready_vram_to_CPU() { io.GPUSTAT &= 0xF7FFFFFF; }
    void unready_all() { unready_cmd(); unready_recv_dma(); unready_vram_to_CPU(); }
    void ready_cmd() { io.GPUSTAT |= 0x4000000; }
    void ready_recv_dma() { io.GPUSTAT |= 0x10000000; }
    void ready_vram_to_CPU() { io.GPUSTAT |= 0x8000000; }
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
    void gp0_cmd_unhandled();

    void GPUSTAT_update();

    void bresenham_opaque(RT_POINT2D *v1, RT_POINT2D *v2, u32 color);;


    void get_texture_sampler_from_texpage_and_palette(u32 texpage, u32 palette, TEXTURE_SAMPLER *ts);

    void setpix(i32 y, i32 x, u32 color, u32 is_tex, u32 tex_mask);
    void setpix_split(i32 y, i32 x, u32 r, u32 g, u32 b, u32 is_tex, u32 tex_mask);
    void semipix(i32 y, i32 x, u32 color, u32 is_tex, u32 tex_mask);
    void semipixm(i32 y, i32 x, u32 color, u32 mode, u32 is_tex, u32 tex_mask);
    void semipix_split(i32 y, i32 x, u32 r, u32 g, u32 b, u32 is_tex, u32 tex_mask);

    static inline u32 BGR24to15(u32 c)
    {
        return (((c >> 19) & 0x1F) << 10) |
               (((c >> 11) & 0x1F) << 5) |
               ((c >> 3) & 0x1F);
    }

};


}
