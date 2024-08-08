//
// Created by . on 8/5/24.
//

#ifndef JSMOOCH_EMUS_GENERIC_FLOPPY_H
#define JSMOOCH_EMUS_GENERIC_FLOPPY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "helpers/int.h"
#include "helpers/cvec.h"
#include "helpers/buf.h"

struct generic_floppy_track {
    double radius_mm, length_mm, rpm;
    struct buf data;
    u32 num_sectors;
};

struct generic_floppy {
    struct cvec tracks; // struct generic_floppy_track

    struct {
        float size_mm;
        float track_inner_mm;
        float track_outer_mm;
    } geometry;
};

void generic_floppy_init(struct generic_floppy *);
void generic_floppy_delete(struct generic_floppy *);
void generic_floppy_track_init(struct generic_floppy_track *this);

#ifdef __cplusplus
}
#endif

#endif //JSMOOCH_EMUS_GENERIC_FLOPPY_H
