//
// Created by Dave on 2/7/2024.
//

#pragma once

#include "helpers/physical_io.h"
#include "helpers/int.h"
#include "helpers/sys_interface.h"
#include "helpers/cvec.h"
#include "helpers/debugger/debuggerdefs.h"
#include "sms_gg_bus.h"
#include "sms_gg.h"

namespace SMSGG {

enum VDP_modes {
    VDP_SMS,
    VDP_GG
};

struct VDP_object {
    u32 x, y, pattern, color;
};

struct VDP {
    explicit VDP(core* parent, jsm::systems in_variant);
    jsm::systems variant{};
    core* bus{};
    u8 VRAM[16384]{};
    u16 CRAM[32]{};
    VDP_modes mode{};
    void cycle();
    void reset();
    u32 read_hcounter();
    u32 read_data();
    void write_data(u32 val);
    u32 read_vcounter();
    u32 read_status();
    void write_control(u32 val);

private:
    void register_write(u32 addr, u32 val);
    void set_scanline_kind(u32 vpos);
    void new_scanline();
    void new_frame();
    void update_videomode();
    void update_irqs();
    void sprite_setup();
    u32 dac_palette(u32 index);
    void dac_gfx();
    void scanline_invisible();
    void scanline_visible();

public:
    void bg_gfx1();
    void bg_gfx2();
    void bg_gfx3();

    void sprite_gfx2();
    void sprite_gfx3();


    u16 *cur_output{};
    cvec_ptr<physical_io_device> display_ptr{};
    JSM_DISPLAY *display{};

    VDP_object objects[64]{};

    u32 sprite_limit{};
    u32 sprite_limit_override{};

    struct SMSGG_VDP_io {
        u32 code{};
        u32 video_mode{};
        u32 display_enable{};
        u32 bg_name_table_address{};
        u32 bg_color_table_address{};
        u32 bg_pattern_table_address{};
        u32 sprite_attr_table_address{};
        u32 sprite_pattern_table_address{};
        u32 bg_color{};
        u32 hscroll{};
        u32 vscroll{};
        u32 line_irq_reload{255};

        u32 sprite_overflow_index{};
        u32 sprite_collision{};
        u32 sprite_overflow{0x1F};
        u32 sprite_shift{};
        u32 sprite_zoom{};
        u32 sprite_size{};
        u32 left_clip{};
        u32 bg_hscroll_lock{};
        u32 bg_vscroll_lock{};

        u32 irq_frame_pending{};
        u32 irq_frame_enabled{};
        u32 irq_line_pending{};
        u32 irq_line_enabled{};

        u32 address{};
    } io{};

    struct SMSGG_VDP_latch {
        u32 control{};
        u32 vram{};
        u32 cram{};
        u32 hcounter{};
        u32 hscroll{};
        u32 vscroll{};
    } latch{};

    u32 bg_color{};
    u32 bg_priority{};
    u32 bg_palette{};
    u32 sprite_color{};

    u32 bg_gfx_vlines{192};
    u32 bg_gfx_mode{};
    void (VDP::*bg_gfx)(){};
    void (VDP::*sprite_gfx)(){};
    u32 doi{};

    void (VDP::*scanline_cycle)(){};

    DBG_EVENT_VIEW_ONLY;
};
}