#pragma once

#include "gb_enums.h"
#include "helpers/physical_io.h"
#include "helpers/debugger/debuggerdefs.h"
#include "gb_bus.h"

namespace GB {
    struct clock;
    struct core;
}

namespace GB::PPU {
struct pixel_slice_fetcher;
struct sprite;
struct sprites;

struct sprite {
    u32 x{};
    u32 y{};
    u32 tile{};
    u32 attr{};
    u32 in_q{};
    u32 num{60};
};

struct FIFO_item {
    //FIFO_item();
    void init(u32 pixel, u32 palette, u32 cgb_priority, u32 sprite_priority);
    u32 pixel{};
    u32 palette{};
    u32 cgb_priority{};
    u32 sprite_priority{};
    sprite* sprite_obj{};
};

struct FIFO {
    void serialize(GB::core *gb, serialized_state &state);
    void deserialize(GB::core *gb, serialized_state &state);

    explicit FIFO(variants variant_in, u32 max_in   );
    FIFO_item *pop();
    FIFO_item *push();
    FIFO_item *peek();
    void clear();
    bool empty() const;
    void set_cgb_mode(bool on);
    void sprite_mix(u32 bp0, u32 bp1, u32 attr, u32 sprite_num);
    void discard(u32 num);
    variants variant{}; // default 0
    u8 compat_mode{}; // default 1
    FIFO_item items[10]{};
    u32 max{};
    u32 head{};
    u32 tail{};
    u32 num_items{};
    FIFO_item blank{};
};

struct px {
    bool had_pixel{};
    u32 color{};
    u32 bg_or_sp{}; // 0 for BG
    u32 palette{};
};

struct core;

struct pixel_slice_fetcher {
    explicit pixel_slice_fetcher(GB::clock *clock_in, GB::core *bus_in, core *parent, variants variant_in);
    px *get_px_if_available();
    void run_fetch_cycle();
    void advance_line();
    void trigger_window();
    px *cycle();
    GB::variants variant{};
    core* ppu{};
    clock* clock{};
    GB::core* bus{};

    u32 fetch_cycle{};
    u32 fetch_addr{};

    sprite* fetch_obj{};
    u32 fetch_bp0{};
    u32 fetch_bp1{};
    u32 spfetch_bp0{};
    u32 spfetch_bp1{};
    u32 fetch_cgb_attr{};

    FIFO bg_FIFO;
    FIFO obj_FIFO;

    u32 bg_request_x{};
    u32 sp_request{};
    FIFO sprites_queue;
    px out_px{};
};

struct sprites {
    u32 num{};
    u32 index{};
    u32 search_index{};
};

struct core {
    explicit core(GB::variants variant_in, GB::clock *clock_in, GB::core *parent);
    variants variant{};
    clock* clock{};
    GB::core* bus{};
    void bus_write_OAM(u32 addr, u32 val);
    u32 bus_read_OAM(u32 addr);
    void write_IO(u32 addr, u32 val);
    u32 read_IO(u32 addr, u32 val);
    void OAM_search();
    void pixel_transfer();
    void write_OAM(u32 addr, u32 val);
    u32 read_OAM(u32 addr);
    void reset();
    void cycle();
    void advance_line();
    void quick_boot();
    void run_cycles(u32 howmany);

    pixel_slice_fetcher slice_fetcher;

    u32 line_cycle{};
    i32 cycles_til_vblank{};

    bool enabled{};

    u16 bg_palette[4]{};
    u16 sp_palette[2][4]{};

    u8 bg_CRAM[64]{};
    u8 obj_CRAM[64]{};

    u8 OAM[160]{};
    sprite OBJ[10]{};
    sprites sprites{};


    struct {
        u32 sprites_big{};

        u32 lyc{};

        u32 STAT_IF{};
        u32 STAT_IE{};
        u32 old_mask{};

        u32 window_tile_map_base{};
        u32 window_enable{};
        u32 bg_window_tile_data_base{};
        u32 bg_tile_map_base{};
        u32 obj_enable{1};
        u32 bg_window_enable{1};

        u32 SCX{}; // X scroll
        u32 SCY{};  // Y scroll
        u32 wx{}; // Window X
        u32 wy{}; // Window Y

        u32 cram_bg_increment{};
        u32 cram_bg_addr{};
        u32 cram_obj_increment{};
        u32 cram_obj_addr{};
    } io{};

    DBG_EVENT_VIEW_ONLY;

    u32 is_window_line{};
    u32 window_triggered_on_line{};

    u32 last_used_buffer{};
    u16* cur_output{};
    u16* cur_output_debug_metadata{};
    cvec_ptr<physical_io_device> display_ptr{};
    JSM_DISPLAY *display{};
    bool in_window() const;
    u32 bg_tilemap_addr_window(u32 wlx) const;
    u32 bg_tilemap_addr_nowindow(u32 lx);
    u32 bg_tile_addr_window(u32 tn, u32 attr) const;
    u32 bg_tile_addr_nowindow(u32 tn, u32 attr) const;

private:
    void IRQ_mode1_down();
    void IRQ_mode1_up();
    void IRQ_mode2_down();
    void IRQ_mode2_up();
    void IRQ_mode0_down();
    void IRQ_mode0_up();
    void IRQ_vblank_up();
    void IRQ_lylyc_down();
    void IRQ_lylyc_up();
    void set_mode(modes which);
    void disable();
    void enable();
    void eval_STAT();
    void eval_lyc();
    void advance_frame(bool update_buffer);
};

}
