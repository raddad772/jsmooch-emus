//
// Created by . on 8/5/24.
//

#include "generic_floppy.h"

void generic_floppy_init(struct generic_floppy *this)
{
    cvec_init(&this->tracks, sizeof(struct generic_floppy_track), 80);
}

static void generic_floppy_track_init(struct generic_floppy_track *this)
{
    buf_init(&this->data);
    this->length_mm = 0.0;
}

void generic_floppy_delete(struct generic_floppy *this)
{
    for (u32 i = 0; i < cvec_len(&this->tracks); i++) {
        struct generic_floppy_track *track = (struct generic_floppy_track *)cvec_get(&this->tracks, i);
        buf_delete(&track->data);
    }
    cvec_clear(&this->tracks);
}
