//
// Created by . on 4/21/25.
//

#pragma once

#include "helpers/int.h"
#include "helpers/physical_io.h"
#include "helpers/cvec.h"
#include "snes_mem.h"

union SNES_PPU_px {
    struct {
        u64 color : 16;
        u64 has : 1;
        u64 palette : 3;
        u64 priority : 4;
        u64 source : 3;
        u64 bpp : 2;
        u64 dbg_priority : 2;
        u64 color_math_flags : 4;
    };
    u64 v;
};

struct SNES_PPU_sprite {
    i32 x{}, y{};
    u32 tile_num{};
    u32 name_select{}, name_select_add{};
    u32 hflip{}, vflip{};
    u32 priority{}, palette{}, pal_offset{};
    u32 size{};
};

struct SNES_PPU {
    explicit SNES_PPU(SNES *parent) : snes(parent) {};
    void reset();
    u32 read_oam(u32 addr);
    struct SNES_PPU_BG;

private:
    u32 mode7_mul();
    void write_oam(u32 addr, u32 val);
    void draw_bg_line_mode7(u32 source, i32 y);
    u32 get_addr_by_map();
    void update_video_mode();
    u32 get_tile(SNES_PPU_BG *bg, i32 hoffset, i32 voffset);
    void draw_bg_line(u32 source, u32 y);
    void write_VRAM(u32 addr, u32 val);
    void do_color_math(SNES_PPU_px *main, SNES_PPU_px *sub);
    void draw_sprite_line(i32 ppu_y);

public:
    void write(u32 addr, u32 val, SNES_memmap_block *bl);
    u32 read(u32 addr, u32 old, u32 has_effect, SNES_memmap_block *bl);
    void latch_counters();
    void draw_line();
    void new_scanline(u64 cur_clock);
    void schedule_first();

    SNES *snes;

    u16 *cur_output{};
    cvec_ptr<physical_io_device> display_ptr{};
    JSM_DISPLAY *display{};

    u16 CGRAM[256]{};
    u16 VRAM[0x8000]{};

    struct SNES_PPU_WINDOW {
        u32 left{}, right{};
    } window[2]{};

    struct {
        u32 first{};
        struct {
            u32 main_enable{}, sub_enable{}, mask{};

            u32 enable[2]{};
            u32 invert[2]{};
        } window{};
        u32 main_enable{}, sub_enable{};
        u32 priority[4]{};
        u32 range_overflow{}, time_overflow{};
        SNES_PPU_px line[256]{};

        SNES_PPU_sprite items[128]{};
    } obj{};

    struct {
        struct {
            u32 mask{};
            u32 invert[2]{}, enable[2]{};
            u32 main_mask{}, sub_mask{};
        } window{};
        u32 direct_color{}, blend_mode{};

        u32 enable[7]{};
        u32 halve{}, math_mode{}, fixed_color{};

    } color_math{};

    struct SNES_PPU_BG {
        SNES_PPU_px line[256]{};

        u32 enabled{};
        u32 bpp{};

        u32 screen_x{};

        u32 cols{}, rows{}, col_mask{}, row_mask{};
        u32 tile_px{}, tile_px_mask{};
        u32 scroll_shift{};
        u32 pixels_h{}, pixels_v{};
        u32 pixels_h_mask{}, pixels_v_mask{};
        u32 tiledata_index{};

        u32 palette_offset{}, palette_base{}, palette_shift{}, palette_mask{};
        u32 num_planes{};
        u32 tile_bytes{}, tile_row_bytes{};

        struct {
            u32 enable{}, counter{}, size{};
        } mosaic{};

        struct {
            u32 tile_size{};
            u32 screen_size{}, screen_addr{};
            u32 tiledata_addr{};
            u32 hoffset{}, voffset{};
        } io{};

        struct {
            u32 main_enable{}, sub_enable{};
            u32 enable[2]{}, invert[2]{};
            u32 mask{};
        } window{};
        u32 main_enable{}, sub_enable{};

        enum SNES_PPU_TILE_MODE {
            BPP2,
            BPP4,
            BPP8,
            inactive,
            mode7
        } tile_mode{};
        u32 priority[2]{};
    } pbg[4]{};

    struct {
        u32 vram{}, hcounter{}, vcounter{}, counters{};
        struct {
            u32 mdr{};
            u32 bgofs{};
        } ppu1{};
        struct {
            u32 mdr{};
            u32 bgofs{};
        } ppu2{};
        u32 oam{};
        u32 mode7{};
        u32 cgram_addr{}, cgram{};
    } latch{};

    struct {
        i32 hoffset{}, rhoffset{}, voffset{}, rvoffset{};
        i32 a{}, b{}, c{}, d{};
        i32 x{}, rx{}, y{}, ry{};
        u32 hflip{}, vflip{}, repeat{};
    } mode7{};
    struct {
        u32 counter{};
    } mosaic{};

    struct {
        struct {
            u32 increment{}, addr{}, increment_step{}, increment_mode{}, mapping{};
        } vram{};

        u32 wram_addr{};

        u32 hcounter{}, vcounter{};
        u32 cgram_addr{};

        struct {
            u32 addr{}, priority{};
            //u32 base_addr;
        } oam{};

        struct {
            u32 sz{}, name_select{}, tile_addr{}, interlace{};
        } obj{};
        u32 bg_priority{}, bg_mode{};
        u32 screen_brightness{}, force_blank{};
        struct {
            u32 size{};
        } mosaic{};
        u32 extbg{}, interlace{}, overscan{}, pseudo_hires{};
    } io{};

    struct {
        u64 sched_id{};
        u32 still_sched{};
    } hirq{};

};

