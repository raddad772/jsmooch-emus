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
    u32 num_heads;
};

void mac_floppy_init(mac_floppy *);
void mac_floppy_delete(mac_floppy *);
void mac_floppy_encode_track(generic_floppy_track *track);
int mac_floppy_plain_load(mac_floppy *mflpy, buf *b);
int mac_floppy_load(mac_floppy *mflpy, const char* fname, buf *b);
int mac_floppy_dc42_load(mac_floppy *mflpy, buf *b);


#ifdef __cplusplus
}
#endif


#endif //JSMOOCH_EMUS_MAC_FLOPPY_H
