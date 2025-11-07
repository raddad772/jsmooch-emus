//
// Created by . on 8/5/24.
//

#ifndef JSMOOCH_EMUS_GENERIC_FLOPPY_H
#define JSMOOCH_EMUS_GENERIC_FLOPPY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "helpers_c/int.h"
#include "helpers_c/cvec.h"
#include "helpers_c/buf.h"
#include "helpers_c/bitbuffer.h"


struct generic_floppy_sector {
    u8 track, head, sector, info;
    u8 *tag, *data;
};

struct generic_floppy_track {
    u32 id, head;
    double radius_mm, length_mm, rpm;

    struct cvec sectors;

    struct buf unencoded_data;
    struct bitbuf encoded_data;

    struct buf tags_data;
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
void generic_floppy_sector_init(struct generic_floppy_sector *);
void generic_floppy_sector_delete(struct generic_floppy_sector *);

#ifdef __cplusplus
}
#endif

#endif //JSMOOCH_EMUS_GENERIC_FLOPPY_H
