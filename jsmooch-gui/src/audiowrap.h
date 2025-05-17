//
// Created by . on 9/5/24.
//

#ifndef JSMOOCH_EMUS_AUDIOWRAP_H
#define JSMOOCH_EMUS_AUDIOWRAP_H

#include "helpers/int.h"
#include "helpers/audiobuf.h"

#define MAX_AUDIO_BUFS 10

struct wkrr;
struct audiowrap {
    audiowrap();
    ~audiowrap();
    int init_wrapper(u32 in_num_channels, u32 in_sample_rate, u32 low_pass_filter);
    void configure_for_fps(float fps);
    void commit_emu_buffer();
    struct audiobuf *get_buf_for_emu();
    struct audiobuf *get_buf_for_playback();
    void pause();
    void play();
    bool playing{};

    wkrr *w{};
    u32 sample_rate{};
    bool ok{};
    u32 num_channels{};
    u32 resample;

    float samples_per_buf{};
    float fps{};
    struct {
        struct {
            i32 current{-1};
        } playback;
        struct {
            u32 head{};
            u32 len{};
            i32 current{-1};

        } emu{};

        struct audiobuf items[MAX_AUDIO_BUFS]{};
    } bufs;
    bool started{};
};

#endif //JSMOOCH_EMUS_AUDIOWRAP_H
