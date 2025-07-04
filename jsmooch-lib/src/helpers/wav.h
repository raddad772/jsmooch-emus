//
// Created by . on 3/31/25.
//

#ifndef JSMOOCH_EMUS_WAV_H
#define JSMOOCH_EMUS_WAV_H

#include <stdio.h>
#include "helpers/int.h"
#include "helpers/pack.h"

PACK_BEGIN
struct wav_header {
    // RIFF Header
    char riff_header[4]; // Contains "RIFF"
    i32 file_sz;
    char wave_header[4]; // Contains "WAVE"

    // Format Header
    char fmt_header[4];
    i32 fmt_chunk_sz;
    i16 audio_format;
    i16 num_channels;
    i32 sample_rate;
    i32 byte_rate;
    i16 sample_alignment;
    i16 bit_depth;

    // Data
    char data_header[4];
    i32 data_sz;
} PACK_END;

struct wav_stream {
    char fpath[300];
    struct wav_header header;

    u32 bytes_per_sample;
    u32 total_samples_written;
    FILE *f;
};

void wav_stream_create(struct wav_stream *, char *fpath, u32 sampling_rate, u32 bytes_per_sample);
void wav_stream_output(struct wav_stream *, u32 num_samples, void *data);
void wav_stream_close(struct wav_stream *);

#endif //JSMOOCH_EMUS_WAV_H
