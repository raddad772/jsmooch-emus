//
// Created by . on 9/5/24.
//

#include <string.h>
#include <stdlib.h>
#include <printf.h>

#include "audiobuf.h"

void audiobuf_init(struct audiobuf *this)
{
    memset(this, 0, sizeof(struct audiobuf));
}

void audiobuf_allocate(struct audiobuf* this, u32 num_channels, float num_samples)
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

void audiobuf_delete(struct audiobuf *this)
{
    if (this->ptr) {
        free(this->ptr);
        this->ptr = NULL;
    }
    memset(this, 0, sizeof(struct audiobuf));
}