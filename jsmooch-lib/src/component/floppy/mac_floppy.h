//
// Created by . on 8/5/24.
//

#ifndef JSMOOCH_EMUS_MAC_FLOPPY_H
#define JSMOOCH_EMUS_MAC_FLOPPY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "generic_floppy.h"

struct mac_floppy {
    struct generic_floppy disc; // struct generic_floppy_track
    u32 write_protect;
};

void mac_floppy_init(struct mac_floppy *);
void mac_floppy_delete(struct mac_floppy *);

#ifdef __cplusplus
}
#endif


#endif //JSMOOCH_EMUS_MAC_FLOPPY_H
