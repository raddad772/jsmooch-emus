//
// Created by Dave on 2/9/2024.
//

#ifndef JSMOOCH_EMUS_SMS_GG_IO_H
#define JSMOOCH_EMUS_SMS_GG_IO_H

#include "helpers/int.h"
#include "helpers/sys_interface.h"

struct SMSGG_controller_port {
    enum jsm_systems variant;
    u32 which;

    u32 TR_level;
    u32 TH_level;
    u32 TR_direction;
    u32 TH_direction;

    void* attached_device;
};

void SMSGG_controller_port_init(struct SMSGG_controller_port*, enum jsm_systems variant, u32 which);
u32 SMSGG_controller_port_read(struct SMSGG_controller_port*);
void SMSGG_controller_port_reset(struct SMSGG_controller_port*);

struct SMSGG;

u32 SMSGG_bus_cpu_in_sms1(struct SMSGG* bus, u32 addr, u32 val, u32 has_effect);
void SMSGG_bus_cpu_out_sms1(struct SMSGG*, u32 addr, u32 val);
u32 SMSGG_bus_cpu_in_gg(struct SMSGG* bus, u32 addr, u32 val, u32 has_effect);


#endif //JSMOOCH_EMUS_SMS_GG_IO_H
