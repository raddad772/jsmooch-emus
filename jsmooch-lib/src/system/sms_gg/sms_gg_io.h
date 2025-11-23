//
// Created by Dave on 2/9/2024.
//

#pragma once

#include "helpers/int.h"
#include "helpers/sys_interface.h"

namespace SMSGG{
struct SMSGG_controller_port {
    jsm::systems variant{};
    u32 which{};

    u32 TR_level{};
    u32 TH_level{};
    u32 TR_direction{};
    u32 TH_direction{};

    void* attached_device{};
};

}
