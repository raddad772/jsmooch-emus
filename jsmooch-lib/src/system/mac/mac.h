//
// Created by . on 7/24/24.
//

#pragma once

#include "helpers/sys_interface.h"

namespace mac {
enum variants {
    mac128k = 0,
    mac512k = 1,
    macplus_1mb = 2
};
}

jsm_system *mac_new(mac::variants variant);
void mac_delete(jsm_system* system);

