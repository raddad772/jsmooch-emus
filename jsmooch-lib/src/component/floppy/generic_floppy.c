//
// Created by . on 8/5/24.
//

#include "generic_floppy.h"

void generic_floppy_init(generic_floppy *this)
{
    cvec_init(&this->tracks, sizeof(generic_floppy_track), 80);
}

void generic_floppy_track_init(generic_floppy_track *this)
{
    cvec_init(&this->sectors, sizeof(generic_floppy_sector), 12);
    bitbuf_init(&this->encoded_data, 49152, 1);
    buf_init(&this->unencoded_data);
    buf_init(&this->tags_data);
    this->length_mm = 0.0;
}

static void generic_floppy_track_delete(generic_floppy_track *track)
{
    for (u32 j = 0; j < cvec_len(&track->sectors); j++) {
        generic_floppy_sector_delete(cvec_get(&track->sectors, j));
    }
    buf_delete(&track->unencoded_data);
    bitbuf_delete(&track->encoded_data);
    buf_delete(&track->tags_data);
}

void generic_floppy_delete(generic_floppy *this)
{
    for (u32 i = 0; i < cvec_len(&this->tracks); i++) {
        generic_floppy_track_delete(cvec_get(&this->tracks, i));
    }
    cvec_delete(&this->tracks);
}

void generic_floppy_sector_init(generic_floppy_sector *this)
{
    this->head = this->info = this->sector = this->track = 0;
    this->data = this->tag = NULL;
}

void generic_floppy_sector_delete(generic_floppy_sector *this)
{
    this->data = this->tag = NULL;
}

