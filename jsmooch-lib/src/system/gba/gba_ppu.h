//
// Created by . on 12/4/24.
//

#pragma once

#include "helpers/int.h"
#include "helpers/physical_io.h"
#include "helpers/debugger/debugger.h"
#include "helpers/debugger/debuggerdefs.h"

namespace GBA { struct core; }

namespace GBA::PPU {

struct window {
    bool enable;

    bool v_flag, h_flag;

    i32 left, right, top, bottom;

    bool active[6]; // In OBJ, bg0, bg1, bg2, bg3, special FX order
    bool inside[240];
};

struct PX {
    u32 color;
    u8 priority;
    bool has;
    bool translucent_sprite;
    bool mosaic_sprite;
};

struct OBJ {
    bool enable;
    PX line[240];
    i32 drawing_cycles;
};

struct BG {
    bool enable{};

    bool bpp8{};
    u32 htiles{}, vtiles{};
    u32 htiles_mask{}, vtiles_mask{};
    u32 hpixels{}, vpixels{};
    u32 last_y_rendered{};
    u32 hpixels_mask{}, vpixels_mask{};

    u32 priority{};
    u32 extrabits{}; // IO bits that are saved but useless
    u32 character_base_block{}, screen_base_block{};
    u32 mosaic_enable{}, display_overflow{};
    u32 screen_size{};

    i32 pa{1<<8}, pb{1<<8}, pc{1<<8}, pd{1<<8};
    i32 x{}, y{};
    bool x_written{}, y_written{};
    i32 x_lerp{}, y_lerp{};

    PX line[248]{};

    u32 mosaic_y{};

    u32 hscroll{}, vscroll{};
};

struct core {
    explicit core(GBA::core *parent);
    u16 *cur_output{};
    cvec_ptr<physical_io_device> display_ptr{};
    JSM_DISPLAY *display{};

    u16 palette_RAM[512]{};
    u8 VRAM[96 * 1024]{};
    u8 OAM[1024]{};

    GBA::core *gba;

    void reset();

private:
    static void vblank(void *ptr, u64 val, u64 clock, u32 jitter);
    static void hblank(void *ptr, u64 key, u64 clock, u32 jitter);
    static void schedule_frame(void *ptr, u64 key, u64 clock, u32 jitter);

    void do_schedule_frame(i64 cur_clock);
    void do_hblank(bool in_hblank);
    void get_affine_bg_pixel(u32 bgnum, const BG &bg, i32 px, i32 py, PX *opx);
    void get_affine_sprite_pixel(u32 mode, i32 px, i32 py, u32 tile_num, u32 htiles, u32 vtiles,  bool bpp8, u32 palette, u32 priority, u32 obj_mapping_1d, u32 dsize, i32 screen_x, u32 blended, PPU::window *w);
    u32 get_sprite_tile_addr(u32 tile_num, u32 htiles, u32 block_y, u32 line_in_tile,  bool bpp8, u32 d1) const;
    void draw_sprite_affine(const u16 *ptr, PPU::window *w, u32 num);
    void output_sprite_8bpp(const u8 *tptr, u32 mode, i32 screen_x, u32 priority, bool hflip, bool mosaic, PPU::window *w);
    void fetch_bg_slice(const BG &bg, u32 block_x, u32 vpos, PX px[], u32 screen_x);
    void output_sprite_4bpp(const u8 *tptr, u32 mode, i32 screen_x, u32 priority, bool hflip, u32 palette, bool mosaic, PPU::window *w);
    void draw_sprite_normal(const u16 *ptr, PPU::window *w);
    void draw_obj_line();
    void apply_mosaic();
    void calculate_windows_vflag();
    void calculate_windows(bool in_vblank);
    PPU::window *get_active_window(u32 x);
    void affine_line_start(BG &bg, i32 *fx, i32 *fy) const;
    void affine_line_end(BG &bg) const;
    void draw_bg_line_affine(u32 bgnum);
    void draw_bg_line_normal(u32 bgnum);
    void output_pixel(u32 x, bool obj_enable, bool bg_enables[4], u16 *line_output);
    void draw_line0(u16 *line_output);
    void draw_line1(u16 *line_output);
    void draw_line2(u16 *line_output);
    void draw_bg_line3();
    void draw_bg_line4();
    void draw_bg_line5();
    void draw_line3(u16 *line_output);
    void draw_line4(u16 *line_output);
    void draw_line5(u16 *line_output);
    void process_button_IRQ();
    u32 read_invalid(u32 addr, u32 sz, u32 access, bool has_effect);
    void write_invalid(u32 addr, u32 sz, u32 access, u32 val);
    u32 mainbus_read_palette(u32 addr, u32 sz, u32 access, bool has_effect);
    u32 mainbus_read_VRAM(u32 addr, u32 sz, u32 access, bool has_effect);
    u32 mainbus_read_OAM(u32 addr, u32 sz, u32 access, bool has_effect);
    u32 mainbus_read_IO(u32 addr, u32 sz, u32 access, bool has_effect);
    void mainbus_write_palette(u32 addr, u32 sz, u32 access, u32 val);
    void mainbus_write_VRAM(u32 addr, u32 sz, u32 access, u32 val);
    void mainbus_write_OAM(u32 addr, u32 sz, u32 access, u32 val);
    void mainbus_write_IO(u32 addr, u32 sz, u32 access, u32 val);
    void calc_screen_size(u32 num, u32 mode);
    void update_bg_x(u32 bgnum, u32 which, u32 val);
    void update_bg_y(u32 bgnum, u32 which, u32 val);

public:
    struct {
        struct {
            u32 hsize{1}, vsize{1};
            bool enable{};
            u32 y_counter{}, y_current{};
        } bg{};
        struct {
            u32 hsize{1}, vsize{1};
            bool enable{};
            u32 y_counter{}, y_current{};
        } obj{};
    } mosaic{};

    struct {
        u32 mode{};
        u32 targets_a[6]{};
        u32 targets_b[6]{};
        u32 eva_a{}, eva_b{};
        u32 use_eva_a{}, use_eva_b{}, use_bldy{};
        u32 bldy{};
    } blend{};

    struct {
        u32 bg_mode{};
        u32 force_blank{};
        u32 frame{}, hblank_free{}, obj_mapping_1d{};
        u32 vblank_irq_enable{}, hblank_irq_enable{}, vcount_irq_enable{};
        u32 vcount_at{};
    } io{};

    BG mbg[4];
    
    window window[4]{}; // win0, win1, win-else and win-obj
    OBJ obj{};

    DBG_EVENT_VIEW_ONLY;
};

static constexpr int WIN0 = 0;
static constexpr int WIN1 = 1;
static constexpr int WINOBJ = 2;
static constexpr int WINOUTSIDE = 3;
}