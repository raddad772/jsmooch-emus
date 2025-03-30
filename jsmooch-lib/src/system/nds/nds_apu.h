//
// Created by . on 3/27/25.
//

#ifndef JSMOOCH_EMUS_NDS_APU_H
#define JSMOOCH_EMUS_NDS_APU_H

#include "helpers/int.h"

#define NDS_APU_MAX_SAMPLES 131072

enum NDS_APU_FMT {
    NDS_APU_FMT_pcm8,
    NDS_APU_FMT_pcm16,
    NDS_APU_FMT_ima_adpcm,
    NDS_APU_FMT_psg
};


// Output: 32768hz, 10-bit. (11-bit with mono mix I'll do)
struct NDS_APU {

    struct NDS_APU_CH {
        u32 dirty, num;

        struct {
            u32 playing;
            u32 in_loop;
            u32 pos;

            u32 counter;

            i16 sample;

            u32 mix_buffer_tail; // Current position in the mixing buffer

            u64 last_timecode, next_timecode;
            u64 my_total_samples;

            u64 freq;
            u64 last_freq;
            i64 sampling_counter;

            struct {
                u32 len;
                u32 head, tail;
                i16 samples[16];
            } sample_input_buffer;
        } status;

        struct NDS_APU_CH_params {
            u32 vol; // 0...127
            u32 vol_div;
            u32 vol_rshift; // 0, 1, 2, 4
            u32 hold;
            u32 pan, left_pan, right_pan; // 0...127, 64=center. so 0...63 per ch
            u32 wave_duty; // wave duty 0-7 for PSG

            enum NDS_APU_RM {
                NDS_APU_RM_none,
                NDS_APU_RM_loop_infinite,
                NDS_APU_RM_one_shot,
                NDS_APU_RM_bad
            } repeat_mode;

            enum NDS_APU_FMT format;

            u32 status; // 0 = stop. 1 = start/busy

            u32 source_addr;
            u32 period;
            u32 loop_start_pos;
            u32 len;
        } io, latched;

        u32 has_psg, has_noise;
    } ch[16];

    struct NDS_APU_params {
        u32 master_enable;
        u32 master_vol;
        u32 sound_bias;
        enum NDS_APU_OF {
            NDS_APU_OF_mixer,
            NDS_APU_OF_ch1,
            NDS_APU_OF_ch3,
            NDS_APU_OF_ch1_ch3
        } output_from_left, output_from_right;
        u32 output_ch1_to_mixer;
        u32 output_ch3_to_mixer;
    } io, latched;

    struct {
        i32 samples[NDS_APU_MAX_SAMPLES];
        u32 len, head, tail;
        struct {
            u64 next_timecode;
            u64 last_timecode;

            u64 total_samples;

            u64 sampling_counter;
        } status;
    } buffer;
};

struct NDS;
void NDS_APU_run_to_current(struct NDS *);
void NDS_APU_init(struct NDS *);
u32 NDS_APU_read(struct NDS *, u32 addr, u32 sz, u32 access);
void NDS_APU_write(struct NDS *, u32 addr, u32 sz, u32 access, u32 val);

#endif //JSMOOCH_EMUS_NDS_APU_H
