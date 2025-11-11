//
// Created by . on 6/1/24.
//

#ifndef JSMOOCH_EMUS_GENESIS_H
#define JSMOOCH_EMUS_GENESIS_H

#include "helpers/enums.h"
#include "helpers/debug.h"
#include "helpers/int.h"
#include "helpers/sys_interface.h"

enum genesis_controller_kinds {
    genesis_controller_none = 0,
    genesis_controller_3button,
    genesis_controller_6button
};

void genesis_new(jsm_system*, enum jsm::systems kind);
void genesis_delete(jsm_system* system);

#endif //JSMOOCH_EMUS_GENESIS_H
