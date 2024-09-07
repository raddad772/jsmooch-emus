//
// Created by . on 9/5/24.
//
#include <string.h>

#define MINIAUDIO_IMPLEMENTATION
#include "../vendor/miniaudio/miniaudio.h"

#include "helpers/int.h"

#include "audiowrap.h"

#include "stdio.h"
#include "assert.h"

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    auto *me = (audiowrap *)pDevice->pUserData;
    struct audiobuf *b = me->get_buf_for_playback();
    if (!b) {
        return;
    }
    u32 lenleft = frameCount;
    float *outptr = (float *)pOutput;
    while(lenleft > 0) {
        // Grab a sample...
        float *smp = ((float *)b->ptr) + b->upos;
        for (u32 i = 0; i < b->num_channels; i++) {
            *outptr = *smp;
            outptr++;
            smp++;
        }
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
}

struct wkrr  {
    ma_device_config config{};
    ma_device device{};
};

audiowrap::audiowrap()
{
    for (auto & item : bufs.items) audiobuf_init(&item);
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
    w->config.playback.format   = ma_format_f32;   // Set to ma_format_unknown to use the device's native format.
    w->config.playback.channels = in_num_channels;               // Set to 0 to use the device's native channel count.
    w->config.sampleRate        = in_sample_rate;           // Set to 0 to use the device's native sample rate.
    w->config.dataCallback      = data_callback;   // This function will be called when miniaudio needs more data.
    w->config.pUserData         = (void *)this;   // Can be accessed from the device object (device.pUserData).
    w->config.periodSizeInFrames = 800;

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
