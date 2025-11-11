//
// Created by . on 9/5/24.
//

#ifndef JSMOOCH_EMUS_AUDIOBUF_H
#define JSMOOCH_EMUS_AUDIOBUF_H

#include "helpers/int.h"

struct audiobuf {
    void *ptr;
    float fpos;
    u32 upos;
    u32 samples_len;
    float fractional_sample_weight;
    u32 alloc_len;
    u32 num_channels;
    float last_sample_weighted;
    u32 finished;
};

void audiobuf_init(audiobuf *);
void audiobuf_delete(audiobuf *);
void audiobuf_allocate(audiobuf*, u32 num_channels, float num_samples);

#endif //JSMOOCH_EMUS_AUDIOBUF_H
