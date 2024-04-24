//
// Created by Dave on 2/5/2024.
//

#ifndef JSMOOCH_EMUS_NES_PPU_H
#define JSMOOCH_EMUS_NES_PPU_H

#include "helpers/int.h"

struct NES_PPU;
struct NES;
typedef void (*PPU_linerender_func)(struct NES_PPU*);
void NES_PPU_init(struct NES_PPU* this, struct NES *nes);
u32 NES_PPU_cycle(struct NES_PPU* this, u32 howmany);
void NES_PPU_reset(struct NES_PPU* this);
void NES_bus_PPU_write_regs(struct NES* nes, u32 addr, u32 val);
u32 NES_bus_PPU_read_regs(struct NES* nes, u32 addr, u32 val, u32 has_effect);


struct PPU_effect_buffer {
    u32 length;
    i32 items[16];
};

struct NES_PPU {
    struct NES* nes;
    PPU_linerender_func render_cycle;

    struct physical_io_device* display;

    i32 line_cycle;
    u8 OAM[256];
    u8 secondary_OAM[32];
    u32 secondary_OAM_index;
    u32 secondary_OAM_sprite_index;
    u32 secondary_OAM_sprite_total;
    u32 secondary_OAM_lock;
    u32 OAM_transfer_latch;
    u32 OAM_eval_index;
    u32 OAM_eval_done;
    u32 sprite0_on_next_line;
    u32 sprite0_on_this_line;
    struct PPU_effect_buffer w2006_buffer;

    u8 CGRAM[0x20];
    u16* cur_output;

    u32 bg_fetches[4];
    u32 bg_shifter;
    u32 bg_palette_shifter;
    u32 bg_tile_fetch_addr;
    u32 bg_tile_fetch_buffer;
    u32 sprite_pattern_shifters[8];
    u32 sprite_attribute_latches[8];
    i32 sprite_x_counters[8];
    i32 sprite_y_lines[8];

    u32 last_sprite_addr;

    struct {
       u32 nmi_enable;
       u32 sprite_overflow;
       u32 sprite0_hit;
       u32 vram_increment;

       u32 sprite_pattern_table;
       u32 bg_pattern_table;

       u32 v, t, x, w;

       u32 greyscale;
       u32 bg_hide_left_8;
       u32 sprite_hide_left_8;
       u32 bg_enable;
       u32 sprite_enable;

       u32 emph_r, emph_g, emph_b;
       u32 emph_bits;

       u32 OAM_addr;
    }io;

    struct {
        u32 v, t, x, w;
    } dbg;

    struct {
        u32 sprite_height;
        u32 nmi_out;
    } status;

    struct {
        u32 VRAM_read;
    } latch;

    u32 rendering_enabled;
    u32 new_rendering_enabled;
};


#endif //JSMOOCH_EMUS_NES_PPU_H
