//
// Created by . on 11/10/24.
//

#include <stdlib.h>

#include "sms_gg.h"
#include "sms_gg_clock.h"
#include "sms_gg_io.h"
#include "sms_gg_mapper_sega.h"
#include "sms_gg_vdp.h"

#include "sms_gg_serialize.h"
#include "helpers/sys_present.h"


static void serialize_console(struct SMSGG *this, struct serialized_state *state)
{
    // Serialize parts now
    serialized_state_new_section(state, "console", SMSGGSS_console, 1);
#define S(x) Sadd(state, &(this-> x), sizeof(this-> x))
    S(io.disable);
    S(io.gg_start);
    S(io.GGreg);
    S(last_frame);
#undef S
}

static void serialize_debug(struct SMSGG *this, struct serialized_state *state)
{
    serialized_state_new_section(state, "debug", SMSGGSS_debug, 1);

    for (u32 i = 0; i < 240; i++) {
#define S(x) Sadd(state, &this->dbg_data.rows[i].io. x, sizeof(this->dbg_data.rows[i].io. x))
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

static void serialize_clock(struct SMSGG *this, struct serialized_state *state)
{
    serialized_state_new_section(state, "clock", SMSGGSS_clock, 1);
#define S(x) Sadd(state, &this->clock. x, sizeof(this->clock. x))
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


static void serialize_z80(struct SMSGG *this, struct serialized_state *state)
{
    serialized_state_new_section(state, "z80", SMSGGSS_z80, 1);
    Z80_serialize(&this->cpu, state);
}

static void serialize_mapper(struct SMSGG *this, struct serialized_state *state)
{
    serialized_state_new_section(state, "mapper", SMSGGSS_mapper, 1);
#define S(x) Sadd(state, &this->mapper. x, sizeof(this->mapper. x))
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
#undef S
}

static void serialize_vdp(struct SMSGG *this, struct serialized_state *state)
{
    serialized_state_new_section(state, "vdp", SMSGGSS_vdp, 1);
#define S(x) Sadd(state, &this->vdp. x, sizeof(this->vdp. x))
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

    // Now serialize the currently-being-rendered frame...
    Sadd(state, &this->vdp.cur_output, SMSGG_DISPLAY_DRAW_SZ);
#undef S
}

static void serialize_sn76489(struct SMSGG *this, struct serialized_state *state)
{
    serialized_state_new_section(state, "sn76489", SMSGGSS_sn76489, 1);
    SN76489_serialize(&this->sn76489, state);
}


void SMSGGJ_save_state(struct jsm_system* jsm, struct serialized_state *state)
{
    struct SMSGG* this = (struct SMSGG*)jsm->ptr;

    // Basic info
    state->version = 1;
    state->opt.len = 0;

    // Make screenshot
    state->has_screenshot = 1;
    jsimg_allocate(&state->screenshot, 256, 240);
    jsimg_clear(&state->screenshot);
    switch(this->variant) {
        case SYS_GG:
            GG_present(cpg(this->vdp.display_ptr), state->screenshot.data.ptr, 0, 0, 256, 240);
            break;
        default:
            SMS_present(cpg(this->vdp.display_ptr), state->screenshot.data.ptr, 0, 0, 256, 240);
            break;
    }
    serialize_console(this, state);
    serialize_debug(this, state);
    serialize_clock(this, state);
    serialize_z80(this, state);
    serialize_mapper(this, state);
    serialize_vdp(this, state);
    serialize_sn76489(this, state);
}

static void deserialize_console(struct SMSGG* this, struct serialized_state *state)
{
#define L(x) Sload(state, &(this-> x), sizeof(this-> x))
    L(io.disable);
    L(io.gg_start);
    L(io.GGreg);
    L(last_frame);
#undef L
}

static void deserialize_debug(struct SMSGG* this, struct serialized_state *state)
{
#define L(x) Sload(state, &(this->dbg_data.rows[i].io. x), sizeof(this->dbg_data.rows[i].io. x))
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

static void deserialize_vdp(struct SMSGG* this, struct serialized_state *state)
{
#define L(x) Sload(state, &(this->vdp. x), sizeof(this->vdp. x))
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

    // Now serialize the currently-being-rendered frame...
    Sload(state, &this->vdp.cur_output, SMSGG_DISPLAY_DRAW_SZ);

    SMSGG_VDP_update_videomode(&this->vdp);
#undef L
}

static void deserialize_sn76489(struct SMSGG* this, struct serialized_state *state)
{
    SN76489_deserialize(&this->sn76489, state);
}

static void deserialize_z80(struct SMSGG* this, struct serialized_state *state)
{
    Z80_deserialize(&this->cpu, state);
}

static void deserialize_clock(struct SMSGG* this, struct serialized_state *state)
{
#define L(x) Sload(state, &this->clock. x, sizeof(this->clock. x))
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

static void deserialize_mapper(struct SMSGG* this, struct serialized_state *state)
{
#define L(x) Sload(state, &this->mapper. x, sizeof(this->mapper. x))
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
#undef L
    SMSGG_mapper_refresh_mapping(&this->mapper);
}

void SMSGGJ_load_state(struct jsm_system *jsm, struct serialized_state *state, struct deserialize_ret *ret)
{
    state->iter.offset = 0;
    assert(state->version == 1);

    u32 clock_done = 0;
    u32 vdp_done = 0;
    u32 set_done = 0;

    struct SMSGG* this = (struct SMSGG*)jsm->ptr;
    for (u32 i = 0; i < cvec_len(&state->sections); i++) {
        struct serialized_state_section *sec = cvec_get(&state->sections, i);
        state->iter.offset = sec->offset;
        switch(sec->kind) {
            case SMSGGSS_console:
                deserialize_console(this, state);
                break;
            case SMSGGSS_debug:
                deserialize_debug(this, state);
                break;
            case SMSGGSS_vdp:
                deserialize_vdp(this, state);
                vdp_done = 1;
                break;
            case SMSGGSS_sn76489:
                deserialize_sn76489(this, state);
                break;
            case SMSGGSS_z80:
                deserialize_z80(this, state);
                break;
            case SMSGGSS_clock:
                deserialize_clock(this, state);
                clock_done = 1;
                break;
            case SMSGGSS_mapper:
                deserialize_mapper(this, state);
                break;
            default:
                NOGOHERE;
                break;
        }
        if ((vdp_done && clock_done) && !set_done) {
            set_done = 1;
            SMSGG_VDP_set_scanline_kind(&this->vdp, this->clock.vpos);
        }
    }

    ret->reason[0] = 0;
    ret->success = 1;
}

