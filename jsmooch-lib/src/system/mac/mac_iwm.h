//
// Created by . on 8/5/24.
//

#ifndef JSMOOCH_EMUS_MAC_IWM_H
#define JSMOOCH_EMUS_MAC_IWM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "helpers/int.h"

struct mac;

void mac_iwm_init(mac *);
void mac_iwm_delete(mac *);
void mac_iwm_reset(mac *);

u16 mac_iwm_read(mac *, u8 addr);
void mac_iwm_write(mac *, u8 addr, u8 val);
void mac_iwm_clock(mac*);
u16 mac_iwm_control(mac*, u8 addr, u8 val, u32 is_write);


#ifdef __cplusplus
}
#endif

#endif //JSMOOCH_EMUS_MAC_IWM_H
