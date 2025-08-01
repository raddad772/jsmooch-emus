//
// Created by . on 7/24/24.
//

#ifndef JSMOOCH_EMUS_MAC_H
#define JSMOOCH_EMUS_MAC_H

#include "helpers/sys_interface.h"

enum mac_variants {
    mac128k = 0,
    mac512k = 1,
    macplus_1mb = 2
};

void mac_new(struct jsm_system* jsm, enum mac_variants variant);
void mac_delete(struct jsm_system* system);


#endif //JSMOOCH_EMUS_MAC_H
