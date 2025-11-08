//
// Created by Dave on 2/5/2024.
//

#ifndef JSMOOCH_EMUS_NES_PPU_H
#define JSMOOCH_EMUS_NES_PPU_H

#include "helpers/int.h"

struct NES_PPU;
struct NES;
void NES_PPU_init(struct NES_PPU*, struct NES *nes);
u32 NES_PPU_cycle(struct NES_PPU*, u32 howmany);
void NES_PPU_reset(struct NES_PPU*);
void NES_bus_PPU_write_regs(struct NES* nes, u32 addr, u32 val);
u32 NES_bus_PPU_read_regs(struct NES* nes, u32 addr, u32 val, u32 has_effect);


struct PPU_effect_buffer {
    u32 length=16;
    i32 items[16];

    PPU_effect_buffer() {
        for (int &item : items) {
            item = -1;
        }
    }

    i32 get(u32 cycle);
    void set(u32 cycle, i32 value);
};

struct NES_PPU {

private:
    void scanline_visible();
    void scanline_postrender();
    void scanline_prerender();
    void update_nmi() const;
    void write_cgram(u32 addr, u32 val);
    u32 read_cgram(u32 addr) const;
    void mem_write(u32 addr, u32 val);
    u32 fetch_chr_line(u32 table, u32 tile, u32 line, u32 has_effect);
    u32 fetch_chr_line_low(u32 addr) const;
    u32 fetch_chr_line_high(u32 addr, u32 o) const;
    void oam_evaluate_slow();
    void cycle_scanline_addr();
    void perform_bg_fetches();
    void new_frame();
    void set_scanline(u32 to);
    void new_scanline();

public:
    explicit NES_PPU(struct NES *nes);
    void reset();
    void write_regs(u32 addr, u32 val);
    u32 cycle(u32 howmany);
    u32 read_regs(u32 addr, u32 val, u32 has_effect);

    NES* nes;
    void (NES_PPU::*render_cycle)() = &NES_PPU::scanline_visible;

    cvec_ptr<physical_io_device> display_ptr{};
    JSM_DISPLAY *display{};

    i32 line_cycle{};
    u8 OAM[256]{};
    u8 secondary_OAM[32]{};
    u32 secondary_OAM_index{};
    u32 secondary_OAM_sprite_index{};
    u32 secondary_OAM_sprite_total{};
    u32 secondary_OAM_lock{};
    u32 OAM_transfer_latch{};
    u32 OAM_eval_index{};
    u32 OAM_eval_done{};
    u32 sprite0_on_next_line{};
    u32 sprite0_on_this_line{};
    PPU_effect_buffer w2006_buffer{};

    u8 CGRAM[0x20]{};
    u16* cur_output{};

    u32 bg_fetches[4]{};
    u32 bg_shifter{};
    u32 bg_palette_shifter{};
    u32 bg_tile_fetch_addr{};
    u32 bg_tile_fetch_buffer{};
    u32 sprite_pattern_shifters[8]{};
    u32 sprite_attribute_latches[8]{};
    i32 sprite_x_counters[8]{};
    i32 sprite_y_lines[8]{};

    u32 last_sprite_addr{};

    struct {
       u32 nmi_enable{};
       u32 sprite_overflow{};
       u32 sprite0_hit{};
       u32 vram_increment = 1;

       u32 sprite_pattern_table{};
       u32 bg_pattern_table{};

       u32 v{}, t{}, x{}, w{};

       u32 greyscale{};
       u32 bg_hide_left_8{};
       u32 sprite_hide_left_8{};
       u32 bg_enable{};
       u32 sprite_enable{};

       u32 emph_r{}, emph_g{}, emph_b{};
       u32 emph_bits{};

       u32 OAM_addr{};
    }io{};

    struct {
        u32 v{}, t{}, x{}, w{};

        struct { cvec_ptr<debugger_view> view{}; } events{};
    } dbg{};

    struct {
        u32 sprite_height = 8;
        u32 nmi_out{};
    } status{};

    struct {
        u32 VRAM_read{};
    } latch{};

    u32 rendering_enabled = 1;
    u32 new_rendering_enabled = 1;

    void serialize(struct serialized_state &state);
    void deserialize(struct serialized_state &state);

};


#endif //JSMOOCH_EMUS_NES_PPU_H
