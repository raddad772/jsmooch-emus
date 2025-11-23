//
// Created by Dave on 2/7/2024.
//

#ifndef JSMOOCH_EMUS_SMS_GG_H
#define JSMOOCH_EMUS_SMS_GG_H

#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/sys_interface.h"
#include "component/cpu/z80/z80.h"
#include "helpers/physical_io.h"

#include "component/controller/sms/sms_gamepad.h"
#include "sms_gg_clock.h"
#include "sms_gg_vdp.h"
#include "component/audio/sn76489/sn76489.h"
#include "sms_gg_mapper_sega.h"
#include "sms_gg_io.h"

#define SMSGG_DISPLAY_DRAW_SZ (256 * 240 * 2)

enum SMSGGSS_kinds {
    SMSGGSS_console,
    SMSGGSS_debug,
    SMSGGSS_vdp,
    SMSGGSS_sn76489,
    SMSGGSS_z80,
    SMSGGSS_clock,
    SMSGGSS_mapper
};

void SMSGG_new(jsm_system* jsm, jsm::systems variant, jsm_regions region);
void SMSGG_delete(jsm_system* jsm);

struct core {
    SMSGG_clock clock;
    jsm::systems variant;
    jsm_regions region;
    Z80 cpu;
    SMSGG_mapper_sega mapper;
    SMSGG_VDP vdp;
    SN76489 sn76489;

    struct {
        double master_cycles_per_audio_sample;
        double next_sample_cycle;
        audiobuf *buf;
    } audio;

    cvec* IOs;

    u32 described_inputs;
    u32 last_frame;

    u32 (*cpu_in)(SMSGG*, u32, u32, u32);
    void (*cpu_out)(SMSGG*, u32, u32);

    struct {
        SMSGG_gamepad controllerA;
        SMSGG_gamepad controllerB;
        SMSGG_controller_port portA;
        SMSGG_controller_port portB;
        HID_digital_button *pause_button;
        u32 disable;
        u32 gg_start;
        u32 GGreg;
    } io;

    DBG_START
        DBG_CPU_REG_START1 *A, *B, *C, *D, *E, *HL, *F, *AF_, *BC_, *DE_, *HL_, *PC, *SP, *IX, *IY, *EI, *HALT, *CE DBG_CPU_REG_END1
        DBG_EVENT_VIEW
        DBG_IMAGE_VIEW(nametables)
        DBG_WAVEFORM_START1
            DBG_WAVEFORM_MAIN
            DBG_WAVEFORM_CHANS(4)
        DBG_WAVEFORM_END1
    DBG_END

    struct {
        struct DBGSMSGGROW {
            struct {
                u32 hscroll, vscroll;
                u32 bg_name_table_address;
                u32 bg_color_table_address;
                u32 bg_pattern_table_address;
                u32 bg_color;
                u32 bg_hscroll_lock;
                u32 bg_vscroll_lock;
                u32 num_lines;
                u32 left_clip;
            } io;
        } rows[240];

    } dbg_data;

};

void SMSGG_bus_notify_IRQ(SMSGG*, u32 level);

#endif //JSMOOCH_EMUS_SMS_GG_H
