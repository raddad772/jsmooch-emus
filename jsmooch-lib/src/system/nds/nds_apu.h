//
// Created by . on 3/27/25.
//
#pragma once

#include "helpers/int.h"
//#include "helpers/wav.h"
#include "helpers/scheduler.h"

namespace NDS::APU {

#define NDS_APU_MAX_SAMPLES 131072

enum FMT {
    FMT_pcm8 = 0,
    FMT_pcm16 = 1,
    FMT_ima_adpcm = 2,
    FMT_psg = 3
};
enum REPEAT_MODE {
    RM_none,
    RM_loop_infinite,
    RM_one_shot,
    RM_bad
};


struct core;
struct MCH {
    static void run(void *ptr, u64 ch_num, u64 cur_clock, u32 jitter);
    void calc_loop_start();
    void update_len();
    void change_freq();
    void run_pcm8();
    void run_pcm16();
    void run_ima_adpcm();
    void run_psg();
    void run_noise();
    void disable();
    void probe_trigger(u32 old_status);

    explicit MCH(core *parent, u32 num_in) : apu(parent), num(num_in) {}
    core *apu;
    u32 dirty{}, num{};
    i16 sample{};

    u32 scheduled{};
    u64 schedule_id{};

    struct {
        u32 pos{};
        u32 real_loop_start_pos{};
        u32 sampling_interval{};
    } status{};

    struct {
        i32 tbl_idx{};
        i32 loop_tbl_idx{};
        i16 loop_sample{};
        u32 data{};
        i32 sample{};
    } adpcm{};

    struct NDS_APU_CH_params {
        u32 vol{}; // 0...127
        u32 real_vol{}; // 0...126{}, 128
        u32 vol_div{};
        u32 vol_rshift{}; // 0{}, 1{}, 2{}, 4
        u32 hold{};
        u32 pan{};
        i32 left_pan{}, right_pan{}; // 0...62,64
        u32 wave_duty{}; // wave duty 0-7 for PSG

        REPEAT_MODE repeat_mode{};
        FMT format{};

        u32 status{}; // 0 = stop. 1 = start/busy

        u32 source_addr{};
        u32 period{};
        u32 loop_start_pos{};
        u32 len{};
        u32 sample_len{};
    } io{};

    bool has_psg{}, has_noise{}, has_cap{};

    u32 lfsr{};
};

enum SOUNDCAP_SOURCE {
    NASS_mixer, NASS_ch
};

enum SOUNDCAP_REPEAT {
    NASR_loop, NASR_oneshot
};

enum SOUNDCAP_FORMAT {
    NASF_PCM16, NASF_PCM8
};

struct SOUNDCAP {
    u32 status{};

    SOUNDCAP_SOURCE cap_source{};
    SOUNDCAP_REPEAT repeat_mode{};
    SOUNDCAP_FORMAT format{};

    u32 pos{};

    u64 scheduler_id{};
    u32 scheduled{};
    u32 ctrl_src{};

    u32 dest_addr{}, len_words{}, len_bytes{};
};

enum APU_OF {
    OF_mixer,
    OF_ch1,
    OF_ch3,
    OF_ch1_ch3
};

    // Output: 32768hz{}, 10-bit. (11-bit with mono mix I'll do)
struct core {
    explicit core(NDS::core *parent, scheduler_t *scheduler_in);
    scheduler_t *scheduler;
    u32 read8(u32 addr);
    void write8(u32 addr, u32 val, MCH &ch);
    void write(u32 addr, u8 sz, u32 access, u32 val);
    u32 read(u32 addr, u8 sz, u32 access);
    static void master_sample_callback(void *ptr, u64 nothing, u64 cur_clock, u32 jitter);
    NDS::core *bus;
    MCH CH[16] {
        MCH(this, 0), MCH(this, 1), MCH(this, 2), MCH(this, 3), MCH(this, 4), MCH(this, 5), MCH(this, 6), MCH(this, 7), MCH(this, 8),MCH(this, 9), MCH(this, 10), MCH(this, 11), MCH(this, 12), MCH(this, 13), MCH(this, 14), MCH(this, 15)
    };

    void run_cap(u32 cap_num);

    SOUNDCAP soundcap[2]{};
    struct NDS_APU_params {
        bool master_enable{true};
        u32 master_vol{};
        u32 sound_bias{};
        APU_OF output_from_left{}, output_from_right{};
        u32 output_ch1_to_mixer{};
        u32 output_ch3_to_mixer{};
    } io{};

    struct {
        i32 samples2[NDS_APU_MAX_SAMPLES]{};
        u32 len{}, head{}, tail{};
        struct {
            u64 next_timecode{};
            u64 last_timecode{};

            u64 total_samples{};

            u64 sampling_counter{};
        } status{};
    } buffer{};

    i32 left_output{}, right_output{};

    long double sample_cycles{}, next_sample{}, total_samples{};
};

}