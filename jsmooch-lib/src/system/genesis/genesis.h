//
// Created by . on 6/1/24.
//

#pragma once
#include "helpers/enums.h"
#include "helpers/debug.h"
#include "helpers/int.h"
#include "helpers/sys_interface.h"

namespace genesis {
enum controller_kinds {
    controller_none = 0,
    controller_3button,
    controller_6button
};
}

jsm_system *genesis_new(jsm::systems kind);
void genesis_delete(jsm_system* system);
