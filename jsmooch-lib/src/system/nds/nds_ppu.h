//
// Created by . on 1/18/25.
//

#pragma once

#include "helpers/int.h"
#include "helpers/physical_io.h"
#include "system/nds/3d/nds_ge.h"
namespace NDS {
struct core;

namespace PPU {

enum SCREEN_KINDS {
    SK_none = 0,
    SK_3donly = 20,
    SK_text = 1,
    SK_affine = 2,
    SK_extended = 3,
    SK_large = 4,
    SK_rotscale_16bit = 5,
    SK_rotscale_8bit = 6,
    SK_rotscale_direct = 7,
    SK_gba_mode_3,
    SK_gba_mode_4,
    SK_gba_mode_5
};

struct WINDOW {
    bool enable{};

    u32 v_flag{}, h_flag{};

    i32 left{}, right{}, top{}, bottom{};

    bool active[6]{}; // In OBJ{}, bg0{}, bg1{}, bg2{}, bg3{}, special FX order
    u8 inside[256]{}; // for sprites
};

union PX {
    struct {
        u32 color : 18;
        u32 priority : 3; // =21
        u32 has : 1; // =22
        u32 sp_translucent : 1; //=23
        u32 sp_mosaic : 1; //=24
        u32 alpha : 4; // =28

        u32 dbg_mode : 2; // = 30
        u32 dbg_bpp : 2; // =32. 0=4bpp{}, 1=8bpp{}, 2=16bpp
    };
    u32 u{};
};

struct DBG_line_bg {
    PX buf[256]{};
    u32 hscroll{}, vscroll{};
    i32 hpos{}, vpos{};
    i32 x_lerp{}, y_lerp{};
    i32 pa{}, pb{}, pc{}, pd{};
    u32 reset_x{}, reset_y{};
    u32 htiles{}, vtiles{};
    u32 display_overflow{};
    u32 screen_base_block{}, character_base_block{};
    u32 priority{};
    u32 bpp8{};
};


struct DBG_line {
    DBG_line_bg bg[4];
    PX sprite_buf[256]{};
    u16 dispcap_px[256]{};
    u32 bg_mode{};
};


struct DBG_eng {
    DBG_line line[192]{};
};


struct OBJ {
    bool enable{};
    PX line[256]{};
    i32 drawing_cycles{};
};

static constexpr u32 WIN0 = 0;
static constexpr u32 WIN1 = 1;
static constexpr u32 WINOBJ = 2;
static constexpr u32 WINOUTSIDE = 3;

union DISPCAPCNT {
    struct {
        u32 eva : 5; // 0-4
        u32 _un1: 3; // 5-7
        u32 evb: 5; // 8-12
        u32 _un2: 3; // 13-15
        u32 vram_write_block : 2;
        u32 vram_write_offset : 2;
        u32 capture_size : 2;
        u32 _un3: 2;
        u32 source_a : 1;
        u32 source_b : 1;
        u32 vram_read_offset : 2;
        u32 _un4: 1;
        u32 capture_source: 2;
        u32 capture_enable : 1;
    };
    u32 u{};
};

struct ENG3D {
    struct {
        u8 *texture[4]{};
        u8 *palette[6]{};
    } slots{};
};

struct MBG {
    SCREEN_KINDS kind{};
    bool enable{};
    u32 num{};

    void update_x(u32 which, u32 val);
    void update_y(u32 which, u32 val);

    bool bpp8{};
    u32 htiles{}, vtiles{};
    u32 htiles_mask{}, vtiles_mask{};
    u32 hpixels{}, vpixels{};
    u32 last_y_rendered{};
    u32 hpixels_mask{}, vpixels_mask{};

    u32 priority{};
    u32 extrabits{}; // IO bits that are saved but useless
    u32 character_base_block{}, screen_base_block{};
    u32 mosaic_enable{}, display_overflow{}, ext_pal_slot{};
    u32 screen_size{};

    i32 pa{0x100}, pb{0x100}, pc{0x100}, pd{0x100};
    i32 x{}, y{};
    bool x_written{}, y_written{};
    i32 x_lerp{}, y_lerp{};
    i32 fx{}, fy{};

    PX line[256+8]{};

    u32 mosaic_y{};

    u32 hscroll{}, vscroll{};

    bool do3d{}; // Just for BG0 on Engine A{}, lol.
};


struct core;
struct ENG2D;
typedef void (ENG2D::*affinerenderfunc)(MBG &, i32 x, i32 y, PX &, void *) const;

struct ENG2D {
    explicit ENG2D(core *parent) : ppu(parent) {}
    core *ppu;
    [[nodiscard]] u64 read_vram_bg(u32 addr, u8 sz) const;
    [[nodiscard]] u32 read_vram_obj(u32 addr, u8 sz) const;
    [[nodiscard]] u32 read_pram_bg_ext(u32 slotnum, u32 addr) const;
    [[nodiscard]] u32 read_pram_bg(u32 addr, u8 sz) const;
    [[nodiscard]] u32 read_ext_palette_obj(u32 palette, u32 index) const;
    [[nodiscard]] u32 read_pram_obj2(u32 palette, u32 index) const;
    [[nodiscard]] u32 read_pram_obj(u32 addr, u8 sz) const;
    [[nodiscard]] u32 read_oam(u32 addr, u8 sz) const;
    void draw_obj_line();
    void draw_obj_on_line(u32 oam_offset, u32 prio_to_draw);
    void affine_line_start(MBG &bg);
    void affine_line_end(MBG &bg);
    void render_affine_loop(MBG &bg, i32 width, i32 height, affinerenderfunc render_func, void *xtra);
    void affine_normal(MBG &bg, i32 x, i32 y, PX &out, void *xtra) const;
    void affine_rotodc(MBG &bg, i32 x, i32 y, PX &out, void *xtra) const;
    void affine_rotobpp8(MBG &bg, i32 x, i32 y, PX &out, void *xtra) const;
    void affine_rotobpp16(MBG &bg, i32 x, i32 y, PX &out, void *xtra) const;
    void draw_bg_line_normal(u32 bgnum);;
    void draw_bg_line_extended(u32 bgnum);
    void draw_bg_line_affine(u32 bgnum);
    WINDOW *get_active_window(u32 x);
    void calculate_windows();
    void draw_3d_line(u32 bgnum);;
    void apply_mosaic();
    void output_pixel(u32 x, bool obj_enable, bool bg_enables[4]);
    void draw_line0(DBG_line *l);
    void draw_line1(DBG_line *l);
    void draw_line2(DBG_line *l);
    void draw_line3(DBG_line *l);
    void draw_line5(DBG_line *l);
    void calc_screen_sizeA(u32 num);
    void calc_screen_sizeB(u32 num);
    void calc_screen_size(u32 bgnum);

    u32 num{};
    bool enable{};

    struct {
        u8 *bg_vram[32]{};
        u8 *obj_vram[16]{};
        u8 *bg_extended_palette[4]{};
        u8 *obj_extended_palette{};

    } memp{};

    struct {
        u8 bg_palette[512]{};
        u8 obj_palette[512]{};
        u8 oam[1024]{};
    } mem{};

    struct {
        u32 mode{};
        u32 targets_a[6]{};
        u32 targets_b[6]{};
        u32 eva_a{}, eva_b{};
        u32 use_eva_a{}, use_eva_b{}, use_bldy{};
        u32 bldy{};
    } blend{};

    struct {
        u32 force_blank{};
        u32 frame{}, hblank_free{};
        u32 display_mode{};
        u32 bg_mode{};

        struct {
            bool do_3d{};
            u32 character_base{}, screen_base{};
            u32 extended_palettes{};
        } bg{};

        struct {
            struct {
                u32 boundary_1d{}, stride_1d{};
                u32 map_1d{};
            } tile{};
            struct {
                u32 boundary_1d{}, stride_1d{};
                u32 map_1d{}, map_2d{};
                u32 dim_2d{};
            } bitmap{};
            u32 extended_palettes{};
        } obj{};
    } io{};

    u32 line_px[256]{};

    MBG BG[4]{};
    struct {
        struct {
            u32 hsize{1}, vsize{1};
            bool enable{};
        } bg{};
        struct {
            u32 hsize{1}, vsize{1};
            bool enable{};
        } obj{};
    } mosaic{};

    WINDOW window[4]{}; // win0{}, win1{}, win-else and win-obj
    OBJ obj{};
} ;

struct core {
    // 2 mostly-identical 2d engines (A and B) and a 3d engine
    explicit core(NDS::core *parent);
    NDS::core *bus;
    void reset();
    void calculate_windows_vflags(ENG2D &eng);
    void run_dispcap();
    void draw_line(u32 eng_num);
    void new_frame();
    void write_2d_bg_palette(u32 eng_num, u32 addr, u8 sz, u32 val);
    void write_2d_obj_palette(u32 eng_num, u32 addr, u8 sz, u32 val);

    void write7_io(u32 addr, u8 sz, u32 access, u32 val);
    void write7_io8(u32 addr, u8 sz, u32 access, u32 val);

    void write9_io(u32 addr, u8 sz, u32 access, u32 val);
    void write9_io8(u32 addr, u8 sz, u32 access, u32 val);

    u32 read9_io(u32 addr, u8 sz, u32 access, bool has_effect);
    u32 read9_io8(u32 addr, u8 sz, u32 access, bool has_effect);


    u32 read7_io(u32 addr, u8 sz, u32 access, bool has_effect) const;
    u32 read7_io8(u32 addr, u8 sz, u32 access, bool has_effect) const;

    u32 *cur_output{};
    cvec_ptr<physical_io_device> display_ptr{};
    JSM_DISPLAY *display{};

    u16 line_a[256]{};
    u16 line_b[256]{};
    bool doing_capture{};

    struct {
        bool vblank_irq_enable7{}, hblank_irq_enable7{}, vcount_irq_enable7{};
        u32 vcount_at7{};
        bool vblank_irq_enable9{}, hblank_irq_enable9{}, vcount_irq_enable9{};
        u32 vcount_at9{};
        u32 display_block{}; // A B C or D
        bool display_swap{};
        DISPCAPCNT DISPCAPCNT;
    } io{};

    ENG2D eng2d[2]{ENG2D(this), ENG2D(this)};
    ENG3D eng3d{};

    struct {
        struct {
            u32 y_counter{}, y_current{};
        } bg{};
        struct {
            u32 y_counter{}, y_current{};
        } obj{};
    } mosaic{};
};

}
}