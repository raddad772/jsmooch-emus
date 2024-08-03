//
// Created by . on 7/25/24.
//

#ifndef JSMOOCH_EMUS_MAC_DISPLAY_H
#define JSMOOCH_EMUS_MAC_DISPLAY_H

#include "mac.h"
#include "mac_internal.h"

void mac_step_display2(struct mac* this);
void mac_display_reset(struct mac* this);
u32 mac_display_in_hblank(struct mac* this);

#endif //JSMOOCH_EMUS_MAC_DISPLAY_H
