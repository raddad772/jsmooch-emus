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

void SMSGG_new(struct jsm_system* jsm, enum jsm_systems variant, enum jsm_regions region);
void SMSGG_delete(struct jsm_system* jsm);

struct SMSGG {
    struct SMSGG_clock clock;
    enum jsm_systems variant;
    enum jsm_regions region;
    struct Z80 cpu;
    struct SMSGG_VDP vdp;
    struct SMSGG_mapper_sega mapper;
    struct SN76489 sn76489;

    u32 display_enabled;
    u32 tracing;

    struct cvec* IOs;

    u32 described_inputs;
    u32 last_frame;

    u32 (*cpu_in)(struct SMSGG*, u32, u32, u32);
    void (*cpu_out)(struct SMSGG*, u32, u32);

    struct {
        struct SMSGG_gamepad controllerA;
        struct SMSGG_gamepad controllerB;
        struct SMSGG_controller_port portA;
        struct SMSGG_controller_port portB;
        struct HID_digital_button *pause_button;
        u32 disable;
        u32 gg_start;
        u32 GGreg;
    } io;

    DBG_START
        DBG_CPU_REG_START *A, *B, *C, *D, *E, *H, *L, *F DBG_CPU_REG_END
        DBG_EVENT_VIEW
    DBG_END
};

void SMSGG_bus_notify_IRQ(struct SMSGG*, u32 level);

#endif //JSMOOCH_EMUS_SMS_GG_H
