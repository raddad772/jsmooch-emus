//
// Created by Dave on 2/7/2024.
//

#ifndef JSMOOCH_EMUS_SMS_GG_VDP_H
#define JSMOOCH_EMUS_SMS_GG_VDP_H

#include "helpers/physical_io.h"
#include "helpers/int.h"
#include "helpers/sys_interface.h"

#include "helpers/debugger/debuggerdefs.h"

enum SMSGG_VDP_modes {
    VDP_SMS,
    VDP_GG
};

struct SMSGG_object {
    u32 x, y, pattern, color;
};

struct SMSGG;

struct SMSGG_VDP {
    enum jsm::systems variant;
    struct SMSGG* bus;
    u8 VRAM[16384];
    u16 CRAM[32];
    enum SMSGG_VDP_modes mode;

    struct cvec_ptr display_ptr;
    struct JSM_DISPLAY *display;
    u16 *cur_output;
    struct SMSGG_object objects[64];

    u32 sprite_limit;
    u32 sprite_limit_override;

    struct SMSGG_VDP_io {
        u32 code;
        u32 video_mode;
        u32 display_enable;
        u32 bg_name_table_address;
        u32 bg_color_table_address;
        u32 bg_pattern_table_address;
        u32 sprite_attr_table_address;
        u32 sprite_pattern_table_address;
        u32 bg_color;
        u32 hscroll;
        u32 vscroll;
        u32 line_irq_reload;

        u32 sprite_overflow_index;
        u32 sprite_collision;
        u32 sprite_overflow;
        u32 sprite_shift;
        u32 sprite_zoom;
        u32 sprite_size;
        u32 left_clip;
        u32 bg_hscroll_lock;
        u32 bg_vscroll_lock;

        u32 irq_frame_pending;
        u32 irq_frame_enabled;
        u32 irq_line_pending;
        u32 irq_line_enabled;

        u32 address;
    } io;

    struct SMSGG_VDP_latch {
        u32 control;
        u32 vram;
        u32 cram;
        u32 hcounter;
        u32 hscroll;
        u32 vscroll;
    } latch;

    u32 bg_color;
    u32 bg_priority;
    u32 bg_palette;
    u32 sprite_color;

    u32 bg_gfx_vlines;
    u32 bg_gfx_mode;
    void (*bg_gfx)(SMSGG_VDP*);
    void (*sprite_gfx)(SMSGG_VDP*);
    u32 doi;

    void (*scanline_cycle)(SMSGG_VDP*);

    DBG_EVENT_VIEW_ONLY;
};

void SMSGG_VDP_init(SMSGG_VDP*, SMSGG* bus, enum jsm::systems variant);
void SMSGG_VDP_cycle(SMSGG_VDP*);
void SMSGG_VDP_reset(SMSGG_VDP*);
void SMSGG_VDP_write_control(SMSGG_VDP*, u32 val);
u32 SMSGG_VDP_read_status(SMSGG_VDP*);
u32 SMSGG_VDP_read_vcounter(SMSGG_VDP*);
void SMSGG_VDP_write_data(SMSGG_VDP*, u32 val);
u32 SMSGG_VDP_read_data(SMSGG_VDP*);
u32 SMSGG_VDP_read_hcounter(SMSGG_VDP*);
void SMSGG_VDP_update_videomode(SMSGG_VDP* this);
void SMSGG_VDP_set_scanline_kind(SMSGG_VDP* this, u32 vpos);

#endif //JSMOOCH_EMUS_SMS_GG_VDP_H
