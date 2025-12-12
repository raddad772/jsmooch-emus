//
// Created by . on 4/23/25.
//

#pragma once

#include "component/cpu/spc700/spc700.h"

namespace SNES {
struct core;

namespace APU {
struct sample {
    i16 decoded[2][16];
    u32 cur_decode_buf;
    u8 pos;
    u8 first_or_loop;
    u8 filter_pos;
    u8 loop, end;

    u16 start_addr, loop_addr;
    u16 next_read_addr;
};

struct filter {
    i16 prev[2];
};

struct core;

struct ch {
    u32 calc_env_rate() const;
    void update_env();
    void change_envelope(u32 rate);
    u8 read_voice(u8 param) const;
    void write_voice(u8 param, u8 val);
    void calculate_sample_addr();
    void keyon();
    void keyoff();
    void update_sample();
    i16 gaussian_me_up() const;

    SNES::APU::core *apu{};

    u32 num{}, ext_enable{};

    struct {
        i32 l{}, r{};
        i16 debug{};
    } output{};

    struct {
        long double next_sample{}, stride{};
        u64 sch_id{};
        u32 sch_still{};
        u32 counter{};
    } pitch{};

    struct {
        u8 PITCHL{}, PITCHH{}, SRCN{}, ENVX{}, OUTX{};
        i32 VOLL{}, VOLR{};
        union {
            struct {
                u8 attack_rate : 4{};
                u8 decay_rate : 3{};
                u8 adsr_on : 1{};
            };
            u8 v;
        } ADSR1{};
        union {
            struct {
                u8 sustain_rate : 5{};
                u8 sustain_level : 3{};
            };
            u8 v;
        } ADSR2{};
        union {
            struct {
                u8 gain_rate : 5{};
                u8 gain_mode : 2{};
                u8 custom_gain : 1{};
            } custom{};
            struct {
                u8 fixed_vol : 7{};
                u8 custom_gain : 1{};
            } direct;
            u8 v;
        } GAIN{};
    } io{};

    struct APU_envelope {
        enum ADSR_state {
            attack,
            decay,
            sustain,
            release
        } state{};

        u32 attack_rate{};
        u32 attack_step{};
        u32 decay_rate{};
        u32 decay_step{};
        u32 sustain_rate{};
        u32 sustain_level{};

        u32 current_period{};

        i32 attenuation{};

        u64 sch_id{};
        u32 sch_still{};

        long double next_update{}, stride{};

        u32 counter{};
    } env{};

    sample sample_data{};
    filter filter{};

    u8 ended{};
};

struct DSP {
    explicit DSP(APU::core *apu_in);
    void update_noise();
    APU::core *apu;
    //core *snes;

    u32 ext_enable{};
    ch channel[8];

    struct {
        i16 level{};
        u32 rate{};
        long double next_update{}, stride{};
        u64 sch_id{};
        u32 sch_still{};
    } noise{};

    struct {
        u8 MVOLL{}, MVOLR{}, EVOLL{}, EVOLR{}, KON{}, KOFF{}, ESA{}, EDL{}, FIR[8]{};
        u8 EFB{}, unused{}, PMON{}, NON{}, EON{}, DIR{}, ENDX{};
        union {
            struct {
                u8 noise_freq : 5{};
                u8 disable_echo_write : 1{};
                u8 mute_all : 1{};
                u8 soft_reset : 1{};
            };
            u8 u;
        } FLG{};
    } io{};

    struct {
        i32 l{}, r{};
    } output{};

    struct {
        struct {
            u64 sch_id{};
            u32 still_sched{};
        } kon_clear{}, koff_clear{};
    } scheduling{};

    struct {
        void *ptr{};
        void (*func)(void *ptr, u64 key, u64 clock, u32 jitter){};
    } sample{};
};


struct core {
    explicit core(SNES::core *parent, u64 *clock_ptr);
    void schedule_first();
    void reset();
    void mix_sample();
    SNES::core *snes;
    void calculate_sample_addrs();

    u32 read(u32 addr, u32 old, bool has_effect) const;
    void write(u32 addr, u32 val);

public:

    SPC700::core cpu;
    DSP dsp;
};

}
}