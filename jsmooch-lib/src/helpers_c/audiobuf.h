//
// Created by . on 9/5/24.
//

#ifndef JSMOOCH_EMUS_AUDIOBUF_H
#define JSMOOCH_EMUS_AUDIOBUF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "helpers_c/int.h"

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

void audiobuf_init(struct audiobuf *);
void audiobuf_delete(struct audiobuf *);
void audiobuf_allocate(struct audiobuf*, u32 num_channels, float num_samples);

#ifdef __cplusplus
}
#endif

#endif //JSMOOCH_EMUS_AUDIOBUF_H
