#ifndef _GB_PPU_H
#define _GB_PPU_H

#include "gb_enums.h"
#include "helpers/physical_io.h"
#include "helpers/debugger/debuggerdefs.h"

#ifndef NULL
#define NULL 0
#endif

struct GB_pixel_slice_fetcher;
struct GB_PPU_sprite;
struct GB_PPU_sprites;

struct GB_PPU_sprite {
    u32 x;
    u32 y;
    u32 tile;
    u32 attr;
    u32 in_q;
    u32 num;
};

struct GB_FIFO_item {
    u32 pixel;
    u32 palette;
    u32 cgb_priority;
    u32 sprite_priority;
    struct GB_PPU_sprite* sprite_obj;
};

struct GB_FIFO {
    enum GB_variants variant; // default 0
    u32 compat_mode; // default 1
    struct GB_FIFO_item items[10];
    u32 max;
    u32 head;
    u32 tail;
    u32 num_items;
    struct GB_FIFO_item blank;
};

struct GB_px {
    u32 had_pixel;
    u32 color;
    u32 bg_or_sp; // 0 for BG
    u32 palette;
};

struct GB_pixel_slice_fetcher {
    enum GB_variants variant;
    struct GB_PPU* ppu;
    struct GB_clock* clock;
    struct GB_bus* bus;

    u32 fetch_cycle;
    u32 fetch_addr;

    struct GB_PPU_sprite* fetch_obj;
    u32 fetch_bp0;
    u32 fetch_bp1;
    u32 spfetch_bp0;
    u32 spfetch_bp1;
    u32 fetch_cgb_attr;

    struct GB_FIFO bg_FIFO;
    struct GB_FIFO obj_FIFO;

    u32 bg_request_x;
    u32 sp_request;
    u32 sp_min;
    struct GB_FIFO sprites_queue;
    //struct GB_queue sprites_queue;
    struct GB_px out_px;
};

struct GB_PPU_sprites {
    u32 num;
    u32 index;
    u32 search_index;
};

struct GB_PPU {
    enum GB_variants variant;
    struct GB_clock* clock;
    struct GB_bus* bus;

    struct GB_pixel_slice_fetcher slice_fetcher;

    u32 line_cycle;
    i32 cycles_til_vblank;

    u32 enabled;
    u32 display_update;

    u16 bg_palette[4];
    u16 sp_palette[2][4];

    u8 bg_CRAM[64];
    u8 obj_CRAM[64];

    u8 OAM[160];
    struct GB_PPU_sprite OBJ[10];

    struct {
        u32 sprites_big;

        u32 lyc;

        u32 STAT_IF;
        u32 STAT_IE;
        u32 old_mask;

        u32 window_tile_map_base;
        u32 window_enable;
        u32 bg_window_tile_data_base;
        u32 bg_tile_map_base;
        u32 obj_enable;
        u32 bg_window_enable;

        u32 SCX; // X scroll
        u32 SCY;  // Y scroll
        u32 wx; // Window X
        u32 wy; // Window Y

        u32 cram_bg_increment;
        u32 cram_bg_addr;
        u32 cram_obj_increment;
        u32 cram_obj_addr;
    } io;

    DBG_EVENT_VIEW_ONLY;


    struct GB_PPU_sprites sprites;
    u16 *out_buffer[2];

    u32 first_reset; // true
    u32 is_window_line;
    u32 window_triggered_on_line;

    u32 update_display;
    u32 last_used_buffer;
    u16* cur_output;
    u16* cur_output_debug_metadata;
    struct cvec_ptr display_ptr;
    struct JSM_DISPLAY *display;
};

void GB_PPU_init(struct GB_PPU*, enum GB_variants variant, struct GB_clock* clock, struct GB_bus* bus);
void GB_PPU_run_cycles(struct GB_PPU*, u32 howmany);
void GB_PPU_quick_boot(struct GB_PPU*);
void GB_PPU_reset(struct GB_PPU*);
u32 GB_PPU_bus_read_IO(struct GB_bus* bus, u32 addr, u32 val);
void GB_PPU_bus_write_IO(struct GB_bus* bus, u32 addr, u32 val);

#endif
