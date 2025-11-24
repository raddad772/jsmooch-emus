//
// Created by Dave on 2/9/2024.
//

#pragma once

#include "component/controller/sms/sms_gamepad.h"
#include "helpers/int.h"
#include "helpers/sys_interface.h"

namespace SMSGG{
struct core;

struct controller_port {
private:
    u32 read_reg_ioport1(u32 val);
    u32 read_reg_ioport2(u32 val);
public:

    void write_reg_memory_ctrl(u32 val);
    void write_reg_io_ctrl(u32 val);
    core *bus;
    explicit controller_port(core* bus_in) : bus(bus_in) {}
    void reset();
    jsm::systems variant{};
    u32 which{};
    u32 read();
    void init(jsm::systems variant, u32 which);

    u32 TR_level{};
    u32 TH_level{};
    u32 TR_direction{};
    u32 TH_direction{};

    SMSGG_gamepad *attached_device{};
};

}
