//
// Created by . on 9/5/24.
//
#include <string.h>

#define MINIAUDIO_IMPLEMENTATION
#include "../vendor/miniaudio/miniaudio.h"

#define CLOWNRESAMPLER_IMPLEMENTATION
#define CLOWNRESAMPLER_STATIC
#include "../vendor/clownresampler/clownresampler.h"

#include "helpers/int.h"

#include "audiowrap.h"

#include "stdio.h"
#include "assert.h"


static ClownResampler_Precomputed precomputed;
static ClownResampler_HighLevel_State resampler;

typedef struct ResamplerCallbackData
{
    ma_int16 *output_pointer;
    ma_uint32 output_buffer_frames_remaining;
    audiowrap *audiowrapper;
} ResamplerCallbackData;


static int get_s16_audio_samples(audiowrap *me, u16 *output, u32 frameCount)
{
    int total_samples = 0;
    struct audiobuf *b = me->get_buf_for_playback();
    if (!b) {
        return 0;
    }
    u32 lenleft = frameCount;
    u16 *outptr = output;
    while (lenleft > 0) {
        // Grab a sample...
        float *smp = ((float *) b->ptr) + (b->upos * b->num_channels);
        for (u32 i = 0; i < b->num_channels; i++) {
            float f = *smp;
            f *= 32767.0f;
            *outptr = (i16)f;
            outptr++;
            smp++;
        }
        total_samples++;
        b->upos++;
        lenleft--;
        if (b->upos >= b->samples_len) {
            b->finished = 1;
            b = me->get_buf_for_playback();
            if (!b) break;
        }
    }
    if (lenleft > 0)
        printf("\nOOPS! SHORT %d SAMPLES!", lenleft);
    return total_samples;
}


static size_t rsin_callback(void *user_data, cc_s16l *buffer, size_t total_frames)
{
    // FEED DATA FOR THE RESAMPLER BEAST
    auto *me = (ResamplerCallbackData *)user_data;

    return get_s16_audio_samples(me->audiowrapper, reinterpret_cast<u16 *>(buffer), (u32)(total_frames / me->audiowrapper->num_channels)) * me->audiowrapper->num_channels;
}

static cc_bool rsout_callback(void *user_data, const cc_s32f *frame, cc_u8f total_samples)
{
    auto const callback_data = (ResamplerCallbackData*)user_data;

    // OUTPUT THE RESAMPLED DATA! which is 32-bit signed! but still should be in 16-bit range?
    for (int i = 0; i < total_samples; i++) {
        cc_s32f sample;

        sample = frame[i];

        /* Clamp the sample to 16-bit. */
        if (sample > 0x7FFF)
            sample = 0x7FFF;
        else if (sample < -0x7FFF)
            sample = -0x7FFF;

        /* Push the sample to the output buffer. */
        *callback_data->output_pointer++ = (ma_int16)sample;
    }

    /* Signal whether there is more room in the output buffer. */
    return --callback_data->output_buffer_frames_remaining != 0;
}

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    auto *me = (audiowrap *)pDevice->pUserData;
    // Resample
    if (me->resample) {
        ResamplerCallbackData r;
        r.audiowrapper = me;
        r.output_buffer_frames_remaining = frameCount;
        r.output_pointer = (ma_int16*)pOutput;
        ClownResampler_HighLevel_Resample(&resampler, &precomputed, rsin_callback, rsout_callback, &r);
    }
    else {
        get_s16_audio_samples(me, (u16 *)pOutput, frameCount);
    }
}

struct wkrr  {
    ma_device_config config{};
    ma_device device{};
};

static u32 clown_init_done = 0;

static void clown_init()
{
    ClownResampler_Precompute(&precomputed);

}


audiowrap::audiowrap()
{
    for (auto & item : bufs.items) audiobuf_init(&item);
    if (!clown_init_done) clown_init();
}

audiowrap::~audiowrap() {
    for (auto & item: bufs.items) audiobuf_delete(&item);
    if (started) { ma_device_stop(&w->device); started = false; }
}

void audiowrap::configure_for_fps(float in_fps)
{
    fps = in_fps;
    samples_per_buf = (float)sample_rate / fps;
    for (auto & item : bufs.items) {
        audiobuf_allocate(&item, num_channels, samples_per_buf);
    }
    bufs.emu.head = 0;
    bufs.emu.len = 0;
    ma_device_start(&w->device);
    started = true;
    ok = true;
}

int audiowrap::init_wrapper(u32 in_num_channels, u32 in_sample_rate) {
    w = new wkrr();
    num_channels = in_num_channels;
    sample_rate = in_sample_rate;
    w->config = ma_device_config_init(ma_device_type_playback);
    w->config.playback.format   = ma_format_s16;   // Set to ma_format_unknown to use the device's native format.
    w->config.playback.channels = in_num_channels;               // Set to 0 to use the device's native channel count.
    if (in_sample_rate > 48000) {
        resample = 1;
        w->config.sampleRate        = 48000;           // Set to 0 to use the device's native sample rate.
        ClownResampler_HighLevel_Init(&resampler, 1, in_sample_rate, 48000, 44100);
    }
    else {
        w->config.sampleRate = in_sample_rate;
    }
    w->config.dataCallback      = data_callback;   // This function will be called when miniaudio needs more data.
    w->config.pUserData         = (void *)this;   // Can be accessed from the device object (device.pUserData).
    w->config.periodSizeInFrames = w->config.sampleRate / 60; // 48 kHz / 60

    if (ma_device_init(nullptr, &w->config, &w->device) != MA_SUCCESS) {
        ok = false;
        printf("\nDEVICE INIT FAIL!");
        return -1;
    }

    started = false;
    //printf("\nDEVICE INIT SUCCEED!");
    return 0;
}

struct audiobuf *audiowrap::get_buf_for_emu()
{
    if (bufs.emu.len >= MAX_AUDIO_BUFS) {
        printf("\nOut of audio buffers for emulation!");
        return nullptr;
    }
    u32 pos = (bufs.emu.head + bufs.emu.len) % MAX_AUDIO_BUFS;
    bufs.items[pos].fpos = 0;
    bufs.items[pos].upos = 0;
    bufs.emu.current = (i32)pos;
    memset(bufs.items[pos].ptr, 0, bufs.items[pos].alloc_len);
    bufs.items[pos].finished = 0;
    //printf("\nRETURN BUFFER %d FOR EMU", bufs.emu.current);
    return &bufs.items[pos];
}

void audiowrap::commit_emu_buffer()
{
    //printf("\nCOMMIT EMU BUFFER %d", bufs.emu.current);
    bufs.items[bufs.emu.current].finished = 1;
    bufs.items[bufs.emu.current].upos = 0;
    bufs.items[bufs.emu.current].fpos = 0;
    bufs.emu.len++;
    bufs.emu.current = -1;
}

void audiowrap::pause()
{
    playing = false;
}

void audiowrap::play()
{
    playing = true;
}


struct audiobuf *audiowrap::get_buf_for_playback()
{
    if (!ok) {
        printf("\nNOT OK!");
        return nullptr;
    }
    if (bufs.playback.current != -1) {
        if (bufs.items[bufs.playback.current].finished) {
            bufs.playback.current = -1;
        }
    }
    if (bufs.playback.current != -1) {
        //printf("\nRETURN BUFFER %d FOR PLAYBACK:", bufs.playback.current);
        return &bufs.items[bufs.playback.current];
    }
    if (bufs.emu.len < 1)
    {
        //if (playing)
            //printf("\nOUT OF AUDIO BUFFERS FOR PLAYBACK!");
        return nullptr;
    }
    int pos = (int)bufs.emu.head;
    bufs.playback.current = pos;
    //printf("\nRETURN BUFFER %d FOR PLAYBACK:", pos);
    bufs.emu.head = (bufs.emu.head + 1) % MAX_AUDIO_BUFS;
    bufs.emu.len--;
    bufs.items[pos].finished = 0;
    bufs.items[pos].fpos = 0;
    bufs.items[pos].upos = 0;
    return &bufs.items[pos];
}
