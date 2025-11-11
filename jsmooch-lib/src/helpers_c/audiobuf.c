//
// Created by . on 9/5/24.
//

#include <string.h>
#include <stdlib.h>

#include "audiobuf.h"

void audiobuf_init(audiobuf *this)
{
    memset(this, 0, sizeof(audiobuf));
}

void audiobuf_allocate(audiobuf* this, u32 num_channels, float num_samples)
{
    this->samples_len = num_samples;
    this->alloc_len = ((u32)num_samples + 1) * 4 * num_channels;
    this->num_channels = num_channels;
    this->fpos = 0;
    this->upos = 0;
    if (this->ptr) {
        free(this->ptr);
        this->ptr = NULL;
    }
    this->ptr = malloc(this->alloc_len);
}

void audiobuf_delete(audiobuf *this)
{
    if (this->ptr) {
        free(this->ptr);
        this->ptr = NULL;
    }
    memset(this, 0, sizeof(audiobuf));
}