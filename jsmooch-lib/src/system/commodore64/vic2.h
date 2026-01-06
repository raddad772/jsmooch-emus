//
// Created by . on 11/25/25.
//
#pragma once

#include "helpers/int.h"
#include "helpers/scheduler.h"
#include "helpers/enums.h"
#include "helpers/physical_io.h"
#include "helpers/cvec.h"

namespace C64 {
    struct core;
}

namespace VIC2 {

enum model {
    V6567,
    V6569
};

typedef u8 (*read_mem_func)(void *ptr, u16 addr);

struct sprite {
    u16 x{};
    u8 y{};
    bool enabled;
    bool on_line;
    bool x_expand, y_expand;
};

struct core {
    explicit core(C64::core *parent);
    jsm::display_standards display_standard{jsm::display_standards::NTSC};
    void setup_timing();
    void reset();
    void do_raster_compare();
    void new_scanline();
    void new_frame();
    u8 read_IO(u16 addr, u8 old, bool has_effect);
    void write_IO(u16 addr, u8 val);
    u8 read_color_ram(u16 addr, u8 old, bool has_effect);
    void write_color_ram(u16 addr, u8 val);
    void cycle();
    void reload_shifter();
    void do_g_access();
    u8 get_color();

    u8 bg_collide;
    model model;
    u32 palette[16]{};
    u8 COLOR[1000]{}; // 4bit
    u8 SCREEN_MTX[40]{}; // 8 bit COLOR MTX
    u8 open_bus{};
    u16 g_access{};
    u32 px_shifter{};
    u8 shift_size{};
    u8 old_color;
    u8 xorit = 0;
    u16 mtx;
    sprite sprites[8];

    read_mem_func read_mem;
    void *read_mem_ptr;
    void update_IRQs();
    void update_SSx(u8 val);
    void update_SDx(u8 val);
    void update_raster_compare(u16 val);
    void update_vpos(u16 val);

    void schedule_first();
    struct {
        struct {
            u32 cycles_per{};
            u32 lines_per{};
            size_t num_pixels{};

            u16 first_line{}, last_line{};
        } frame{};

        struct {
            u32 cycles_per{};
            u32 pixels_per{};
            u16 first_col{}, last_col{};
        } line{};

        struct {
            u32 cycles_per{};
            u32 frames_per{};
        } second {};

        struct {
            u16 first_line{13};
            u16 last_line{40};
        } vblank{};
    } timing{};
    i32 hpos{}, vpos{};
    bool hborder_on{}, vborder_on{};
    void mem_access();
    void output_px();

    enum DISPLAY_STATE {
        idle,
        displaying
    } state{};

    u64 master_frame{};
    i32 vmli{};
    i32 vcbase{};
    i32 vc{};
    i32 rc{};
    u32 field{};
    bool bad_line;
    C64::core *bus;

    u8 *cur_output{};
    u8 *line_output{};
    cvec_ptr<physical_io_device> display_ptr{};
    JSM_DISPLAY *display{};
    struct {
        u8 AEC{}, BA{}; // BA->RDY. AEC
    } signals;
    struct {
        union {
            struct {
                u8 YSCROLL : 3;
                u8 RSEL : 1;
                u8 DEN : 1;
                u8 BMM : 1;
                u8 ECM : 1;
                u8 RST8 : 1;
            };
            u8 u{};
        } CR1{};

        union {
            struct {
                u8 XSCROLL : 3;
                u8 CSEL : 1;
                u8 MCM : 1;
                u8 RES : 1;
            };
            u8 u{};
        } CR2{};

        union {
            struct {
                u8 RST : 1;
                u8 MBC : 1;
                u8 MMC : 1;
                u8 LP : 1;
                u8 _ : 3;
                u8 IRQ : 1;
            };
            u8 u;
        } IF{};

        union {
            struct {
                u8 RST : 1;
                u8 MBC : 1;
                u8 MMC : 1;
                u8 LP : 1;
                u8 _ : 4;
            };
            u8 u;
        } IE{};
        u8 D010{};
        u8 D018{};
        u8 ME{}; // sprite enables
        u8 MYE{}; // sprite Y expansion
        u8 MXE{}; // sprite X expansion
        u8 MMC{};
        u8 MDP{};
        u8 SSx{}; // sprite-sprite collision
        u8 SDx{}; // sprite-data collision
        u8 EC{}; // border color
        u8 BC[4]{}; // background colors 0-3
        u8 MM[2]{}; // sprite multicolor 0-1
        u8 MC[8]{}; // sprite color 0-7
        u8 LPX{}, LPY{};
        u16 raster_compare{};
        struct {
            u16 VM{};
            u16 CB{};
        } ptr{};
        bool raster_compare_protect;

        struct {
            u8 DEN;
        } latch{};
    } io{};

private:

};


}
