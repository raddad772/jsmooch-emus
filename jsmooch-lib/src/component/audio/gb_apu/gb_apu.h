//
// Created by . on 9/6/24.
//

#pragma once

#include "helpers/int.h"
namespace GB_APU {

struct SWEEP {
    i32 pace{};
    i32 pace_counter{};
    i32 direction{};
    i32 individual_step{};
};

struct ENVSWEEP {
    i32 period{};
    i32 period_counter{};
    i32 direction{};
    u32 initial_vol{};
    u32 rshift{};
    u32 on{};
};

struct CHAN {
    [[nodiscard]] u8 read_NRx0() const;
    [[nodiscard]] u8 read_NRx1() const;
    [[nodiscard]] u8 read_NRx2() const;
    [[nodiscard]] u8 read_NRx3() const;
    void write_NRx0(u8 val);;
    void write_NRx1(u8 val);;
    void write_NRx2(u8 val);;
    void write_NRx3(u8 val);;
    void write_NRx4(u8 val);;
    void trigger();
    void tick_sweep();
    void tick_length_timer();
    void tick_env();
    void tick_noise_period();
    void tick_pulse_period();
    float sample() const;
    void tick_wave_period_twice();

    bool ext_enable{true};
    u32 number{};
    u32 dac_on{};

    u32 vol{};
    u32 on{};
    u32 left{}, right{};

    u32 period{};
    u32 next_period{};
    u32 length_enable{};
    i32 period_counter{};
    u32 wave_duty{};
    i32 polarity{};
    u32 duty_counter{};
    u32 short_mode{};
    u32 divisor{};
    u32 clock_shift{};
    i32 length_counter{};

    i32 samples[16]{};
    u8 sample_buffer[2]{};

    SWEEP sweep{};
    ENVSWEEP env{};
};

struct core {
    core();
    void cycle();

    u8 read_IO(u32 addr, u8 old_val, bool has_effect) const;
    void write_IO(u32 addr, u8 val);
    bool ext_enable{true};
    float mix_sample(bool is_debug) const;
    float sample_channel(int cnum);

    CHAN channels[4]{};
    struct {
        u32 master_on{};
    } io{};

    struct {
        i32 divider_2048{}; // / 512Hz @ 1MHz
        u32 frame_sequencer{};
    } clocks{};
};

}