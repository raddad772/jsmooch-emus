//
// Created by . on 6/1/24.
//

#ifndef JSMOOCH_EMUS_GENESIS_H
#define JSMOOCH_EMUS_GENESIS_H

#include "helpers_c/enums.h"
#include "helpers_c/debug.h"
#include "helpers_c/int.h"
#include "helpers_c/sys_interface.h"

enum genesis_controller_kinds {
    genesis_controller_none = 0,
    genesis_controller_3button,
    genesis_controller_6button
};

void genesis_new(struct jsm_system*, enum jsm_systems kind);
void genesis_delete(struct jsm_system* system);

#endif //JSMOOCH_EMUS_GENESIS_H
