//
// Created by . on 9/5/24.
//

#include <string.h>
#include <stdlib.h>

#include "audiobuf.h"

void audiobuf::allocate(u32 num_channels, float num_samples)
{
    samples_len = num_samples;
    alloc_len = ((u32)num_samples + 1) * 4 * num_channels;
    num_channels = num_channels;
    fpos = 0;
    upos = 0;
    if (ptr) {
        free(ptr);
        ptr = NULL;
    }
    ptr = malloc(alloc_len);
}

audiobuf::~audiobuf()
{
    if (ptr) {
        free(ptr);
        ptr = nullptr;
    }
}