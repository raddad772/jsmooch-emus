//
// Created by . on 12/28/24.
//

#ifndef JSMOOCH_EMUS_GBA_APU_H
#define JSMOOCH_EMUS_GBA_APU_H

#include "helpers/int.h"
#include "component/audio/gb_apu/gb_apu.h"

struct GBA_APU {
    u32 ext_enable;
    struct GB_APU old;
    struct GBA_APU_FIFO {
        u32 timer_id;
        u32 head, tail, len, output_head;
        i8 data[32]; // 8 32-bit entries

        i32 sample;
        i32 output;
        u32 enable_l, enable_r;
        u32 vol;
        u32 needs_commit;

        u32 ext_enable;
    } fifo[2];

    struct {
        u32 wave_ram_size, wave_ram_bank, playback, sound_len;
        u32 enable_l, enable_r;

        i32 output;
        u32 vol;

        u32 snd_vol, force_vol;

        u32 ext_enable;
    } chan[4];

    struct {
        u32 sound_bias;
        u32 ch03_vol;
        u32 ch03_vol_l;
        u32 ch03_vol_r;
        u32 master_enable;
        u32 bias_amplitude;
    } io;

    struct {
        u32 counter;
    } divider1;

    struct {
        u32 counter, divisor, mask;
    } divider2;
};

struct GBA;
void GBA_APU_init(struct GBA*);
u32 GBA_APU_read_IO(struct GBA*, u32 addr, u32 sz, u32 access, u32 has_effect);
void GBA_APU_write_IO(struct GBA*, u32 addr, u32 sz, u32 access, u32 val);
void GBA_APU_cycle(struct GBA*);
void GBA_APU_sound_FIFO(struct GBA*, u32 num);
float GBA_APU_mix_sample(struct GBA*, u32 is_debug);
float GBA_APU_sample_channel(struct GBA*, u32 num);
#endif //JSMOOCH_EMUS_GBA_APU_H
