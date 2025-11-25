//
// Created by . on 11/10/24.
//

#include <cstdlib>
#include <cassert>

#include "sms_gg_clock.h"
#include "sms_gg_io.h"
#include "sms_gg_mapper_sega.h"
#include "sms_gg_bus.h"
#include "sms_gg_vdp.h"

#include "sms_gg_serialize.h"
#include "helpers/sys_present.h"
namespace SMSGG {

void core::serialize_core(serialized_state &state)
{
    // Serialize parts now
    state.new_section("console", SMSGG::SS_kinds::console, 1);
#define S(x) Sadd(state, &x, sizeof(x))
    S(io.disable);
    S(io.gg_start);
    S(io.GGreg);
    S(last_frame);
#undef S
}

void core::serialize_debug(serialized_state &state)
{
    state.new_section("debug", SMSGG::debug, 1);

    for (u32 i = 0; i < 240; i++) {
#define S(x) Sadd(state, &dbg_data.rows[i].io. x, sizeof(dbg_data.rows[i].io. x))
        S(hscroll);
        S(vscroll);
        S(bg_name_table_address);
        S(bg_color_table_address);
        S(bg_pattern_table_address);
        S(bg_color);
        S(bg_hscroll_lock);
        S(bg_vscroll_lock);
        S(num_lines);
        S(left_clip);
#undef S
    }
}

void core::serialize_clock(serialized_state &state)
{
    state.new_section("clock", SMSGG::clock_k, 1);
#define S(x) Sadd(state, &clock. x, sizeof(clock. x))
    S(variant);
    S(region);
    S(master_cycles);
    S(cpu_master_clock);
    S(vdp_master_clock);
    S(apu_master_clock);
    S(frames_since_restart);
    S(cpu_divisor);
    S(vdp_divisor);
    S(apu_divisor);
    S(cpu_frame_cycle);
    S(vdp_frame_cycle);
    S(ccounter);
    S(hpos);
    S(vpos);
    S(line_counter);
    S(timing.fps);
    S(timing.frame_lines);
    S(timing.cc_line);
    S(timing.bottom_rendered_line);
    S(timing.rendered_lines);
    S(timing.vblank_start);
    S(timing.region);
#undef S
}


void core::serialize_z80(serialized_state &state)
{
    state.new_section("z80", SMSGG::z80, 1);
    cpu.serialize(state);
}

void core::serialize_mapper(serialized_state &state)
{
    state.new_section("mapper", SMSGG::mapper_k, 1);
#define S(x) Sadd(state, &mapper. x, sizeof(mapper. x))
    S(sega_mapper_enabled);
    S(has_bios);
    S(is_sms);

    S(io.frame_control_register[0]);
    S(io.frame_control_register[1]);
    S(io.frame_control_register[2]);
    S(io.cart_ram.mapped);
    S(io.cart_ram.bank);
    S(io.bank_shift);
    S(io.bios_enabled);
    S(io.cart_enabled);

    Sadd(state, mapper.RAM.ptr, mapper.RAM.sz);
    Sadd(state, mapper.cart_RAM.ptr, mapper.cart_RAM.sz);
#undef S
}

void core::serialize_vdp(serialized_state &state)
{
    state.new_section("vdp", SMSGG::vdp_k, 1);
#define S(x) Sadd(state, &vdp. x, sizeof(vdp. x))
    S(VRAM);
    S(CRAM);
    S(mode);

    for (u32 i = 0; i < 64; i++) {
        S(objects[i].x);
        S(objects[i].y);
        S(objects[i].pattern);
        S(objects[i].color);
    }

    S(sprite_limit);

    S(io.code);
    S(io.video_mode);
    S(io.display_enable);
    S(io.bg_name_table_address);
    S(io.bg_color_table_address);
    S(io.bg_pattern_table_address);
    S(io.sprite_attr_table_address);
    S(io.bg_color);
    S(io.hscroll);
    S(io.vscroll);
    S(io.line_irq_reload);

    S(io.sprite_overflow_index);
    S(io.sprite_collision);
    S(io.sprite_overflow);
    S(io.sprite_shift);
    S(io.sprite_zoom);
    S(io.sprite_size);
    S(io.left_clip);
    S(io.bg_hscroll_lock);
    S(io.bg_vscroll_lock);

    S(io.irq_frame_pending);
    S(io.irq_frame_enabled);
    S(io.irq_line_pending);
    S(io.irq_line_enabled);
    S(io.address);

    S(latch.control);
    S(latch.vram);
    S(latch.cram);
    S(latch.hcounter);
    S(latch.hscroll);
    S(latch.vscroll);

    S(bg_color);
    S(bg_priority);
    S(bg_palette);
    S(sprite_color);

    S(bg_gfx_vlines);
    S(bg_gfx_mode);
    S(doi);

    //// Now serialize the currently-being-rendered frame...
    //Sadd(&vdp.cur_output, SMSGG_DISPLAY_DRAW_SZ);
#undef S
}

void core::serialize_sn76489(serialized_state &state)
{
    state.new_section("sn76489", SMSGG::sn76489_k, 1);
    sn76489.serialize(state);
}


void core::save_state(serialized_state &state)
{
    // Basic info
    state.version = 1;
    state.opt.len = 0;

    // Make screenshot
    state.has_screenshot = 1;
    state.screenshot.allocate(256, 240);
    state.screenshot.clear();
    switch(variant) {
        case jsm::systems::GG:
            GG_present(vdp.display_ptr.get(), state.screenshot.data.ptr, 0, 0, 256, 240);
            break;
        default:
            SMS_present(vdp.display_ptr.get(), state.screenshot.data.ptr, 0, 0, 256, 240);
            break;
    }
    serialize_core(state);
    serialize_debug(state);
    serialize_clock(state);
    serialize_z80(state);
    serialize_mapper(state);
    serialize_vdp(state);
    serialize_sn76489(state);
}

void core::deserialize_core(serialized_state &state)
{
#define L(x) Sload(state, &( x), sizeof( x))
    L(io.disable);
    L(io.gg_start);
    L(io.GGreg);
    L(last_frame);
#undef L
}

void core::deserialize_debug(serialized_state &state)
{
#define L(x) Sload(state, &(dbg_data.rows[i].io. x), sizeof(dbg_data.rows[i].io. x))
    for (u32 i = 0; i < 240; i++) {
        L(hscroll);
        L(vscroll);
        L(bg_name_table_address);
        L(bg_color_table_address);
        L(bg_pattern_table_address);
        L(bg_color);
        L(bg_hscroll_lock);
        L(bg_vscroll_lock);
        L(num_lines);
        L(left_clip);
    }
#undef L
}

void core::deserialize_vdp(serialized_state &state)
{
#define L(x) Sload(state, &(vdp. x), sizeof(vdp. x))
    L(VRAM);
    L(CRAM);
    L(mode);

    for (u32 i = 0; i < 64; i++) {
        L(objects[i].x);
        L(objects[i].y);
        L(objects[i].pattern);
        L(objects[i].color);
    }

    L(sprite_limit);

    L(io.code);
    L(io.video_mode);
    L(io.display_enable);
    L(io.bg_name_table_address);
    L(io.bg_color_table_address);
    L(io.bg_pattern_table_address);
    L(io.sprite_attr_table_address);
    L(io.bg_color);
    L(io.hscroll);
    L(io.vscroll);
    L(io.line_irq_reload);

    L(io.sprite_overflow_index);
    L(io.sprite_collision);
    L(io.sprite_overflow);
    L(io.sprite_shift);
    L(io.sprite_zoom);
    L(io.sprite_size);
    L(io.left_clip);
    L(io.bg_hscroll_lock);
    L(io.bg_vscroll_lock);

    L(io.irq_frame_pending);
    L(io.irq_frame_enabled);
    L(io.irq_line_pending);
    L(io.irq_line_enabled);
    L(io.address);

    L(latch.control);
    L(latch.vram);
    L(latch.cram);
    L(latch.hcounter);
    L(latch.hscroll);
    L(latch.vscroll);

    L(bg_color);
    L(bg_priority);
    L(bg_palette);
    L(sprite_color);

    L(bg_gfx_vlines);
    L(bg_gfx_mode);
    L(doi);

    //// Now serialize the currently-being-rendered frame...
    //Sload(&vdp.cur_output, SMSGG_DISPLAY_DRAW_SZ);

    vdp.update_videomode();
#undef L
}

void core::deserialize_sn76489(serialized_state &state)
{
    sn76489.deserialize(state);
}

void core::deserialize_z80(serialized_state &state)
{
    cpu.deserialize(state);
}

void core::deserialize_clock(serialized_state &state)
{
#define L(x) Sload(state, &clock. x, sizeof(clock. x))
    L(variant);
    L(region);
    L(master_cycles);
    L(cpu_master_clock);
    L(vdp_master_clock);
    L(apu_master_clock);
    L(frames_since_restart);
    L(cpu_divisor);
    L(vdp_divisor);
    L(apu_divisor);
    L(cpu_frame_cycle);
    L(vdp_frame_cycle);
    L(ccounter);
    L(hpos);
    L(vpos);
    L(line_counter);
    L(timing.fps);
    L(timing.frame_lines);
    L(timing.cc_line);
    L(timing.bottom_rendered_line);
    L(timing.rendered_lines);
    L(timing.vblank_start);
    L(timing.region);
#undef L
}

void core::deserialize_mapper(serialized_state &state)
{
#define L(x) Sload(state, &mapper. x, sizeof(mapper. x))
    L(sega_mapper_enabled);
    L(has_bios);
    L(is_sms);

    L(io.frame_control_register[0]);
    L(io.frame_control_register[1]);
    L(io.frame_control_register[2]);
    L(io.cart_ram.mapped);
    L(io.cart_ram.bank);
    L(io.bank_shift);
    L(io.bios_enabled);
    L(io.cart_enabled);

    Sload(state, mapper.RAM.ptr, mapper.RAM.sz);
    Sload(state, mapper.cart_RAM.ptr, mapper.cart_RAM.sz);

#undef L
    mapper.refresh_mapping();
}

void core::load_state(serialized_state &state, deserialize_ret &ret)
{
    state.iter.offset = 0;
    assert(state.version == 1);

    u32 clock_done = 0;
    u32 vdp_done = 0;
    u32 set_done = 0;

    u32 num_loaded = 0;
    for (u32 i = 0; i < state.sections.size(); i++) {
        serialized_state_section &sec = state.sections.at(i);
        state.iter.offset = sec.offset;
        switch(sec.kind) {
            case console:
                deserialize_core(state);
                break;
            case debug:
                deserialize_debug(state);
                break;
            case vdp_k:
                deserialize_vdp(state);
                vdp_done = 1;
                break;
            case sn76489_k:
                deserialize_sn76489(state);
                break;
            case z80:
                deserialize_z80(state);
                break;
            case clock_k:
                deserialize_clock(state);
                clock_done = 1;
                break;
            case mapper_k:
                deserialize_mapper(state);
                break;
            default:
                NOGOHERE;
                break;
        }
        if ((vdp_done && clock_done) && !set_done) {
            set_done = 1;
            vdp.set_scanline_kind(clock.vpos);
        }
    }

    ret.reason[0] = 0;
    ret.success = 1;
}

}