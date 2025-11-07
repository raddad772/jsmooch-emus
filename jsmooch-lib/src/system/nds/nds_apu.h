//
// Created by . on 3/27/25.
//

#ifndef JSMOOCH_EMUS_NDS_APU_H
#define JSMOOCH_EMUS_NDS_APU_H

#include "helpers_c/int.h"
#include "helpers_c/wav.h"
#include "helpers_c/scheduler.h"

#define NDS_APU_MAX_SAMPLES 131072

enum NDS_APU_FMT {
    NDS_APU_FMT_pcm8 = 0,
    NDS_APU_FMT_pcm16 = 1,
    NDS_APU_FMT_ima_adpcm = 2,
    NDS_APU_FMT_psg = 3
};


// Output: 32768hz, 10-bit. (11-bit with mono mix I'll do)
struct NDS_APU {

    struct NDS_APU_CH {
        u32 dirty, num;
        i16 sample;

        u32 scheduled;
        u64 schedule_id;

        struct {
            u32 pos;
            u32 real_loop_start_pos;
            u32 sampling_interval;
        } status;

        struct {
            i32 tbl_idx;
            i32 loop_tbl_idx;
            i16 loop_sample;
            u32 data;
            i32 sample;
        } adpcm;

        struct NDS_APU_CH_params {
            u32 vol; // 0...127
            u32 real_vol; // 0...126, 128
            u32 vol_div;
            u32 vol_rshift; // 0, 1, 2, 4
            u32 hold;
            u32 pan;
            i32 left_pan, right_pan; // 0...62,64
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
            u32 sample_len;
        } io;

        u32 has_psg, has_noise, has_cap;

        u32 lfsr;
    } ch[16];

    struct NDS_APU_SOUNDCAP {
        u32 status;

        u32 pos;

        u64 scheduler_id;
        u32 scheduled;
        u32 ctrl_src;
        enum NDS_APU_SOUNDCAP_SOURCE {
            NASS_mixer, NASS_ch
        } cap_source;

        enum NDS_APU_SOUNDCAP_REPEAT {
            NASR_loop, NASR_oneshot
        } repeat_mode;

        enum NDS_APU_SOUNDCAP_FORMAT {
            NASF_PCM16, NASF_PCM8
        } format;

        u32 dest_addr, len_words, len_bytes;

    } soundcap[2];

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
    } io;

    struct {
        i32 samples2[NDS_APU_MAX_SAMPLES];
        u32 len, head, tail;
        struct {
            u64 next_timecode;
            u64 last_timecode;

            u64 total_samples;

            u64 sampling_counter;
        } status;
    } buffer;

    i32 left_output, right_output;

    long double sample_cycles, next_sample, total_samples;
};

struct NDS;
void NDS_APU_init(struct NDS *);
u32 NDS_APU_read(struct NDS *, u32 addr, u32 sz, u32 access);
void NDS_APU_write(struct NDS *, u32 addr, u32 sz, u32 access, u32 val);

void NDS_master_sample_callback(void *, u64 nothing, u64 cur_clock, u32 jitter);


#endif //JSMOOCH_EMUS_NDS_APU_H
