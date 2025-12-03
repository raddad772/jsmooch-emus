//
// Created by . on 12/28/24.
//

#ifndef JSMOOCH_EMUS_GBA_APU_H
#define JSMOOCH_EMUS_GBA_APU_H

#include "helpers/int.h"

namespace GBA { struct core; }

namespace GBA::APU {

struct FIFO {
    void reset();
    void write(u32 v);
    void commit();
    u32 timer_id{};
    u32 head{}, tail{}, len{}, output_head{};
    i8 data[32]{}; // 8 32-bit entries

    i32 sample{};
    i32 output{}; // output being 0-15
    bool enable_l{}, enable_r{};
    u32 vol{};
    bool needs_commit{};

    bool ext_enable{true};
};

struct SWEEP {
    i32 pace{};
    i32 pace_counter{};
    i32 direction{};
    i32 individual_step{};
};

struct ENVELOPE {
    i32 period{};
    i32 period_counter{};
    i32 direction{};
    u32 initial_vol{};
    u32 force75{};
    u32 rshift{};
    u32 on{};
};


struct CHANNEL {
    void tick_wave_period_twice(); // only for ch.2
    void tick_noise_period(); // only for ch.3
    void tick_sweep();
    void tick_length_timer();
    void tick_env();
    [[nodiscard]] u8 read_NRx0() const;
    [[nodiscard]] u8 read_NRx1() const;
    [[nodiscard]] u8 read_NRx2() const;
    [[nodiscard]] u8 read_NRx3() const;
    [[nodiscard]] u8 read_NRx4() const;
    void write_NRx0(u8 val);
    void write_NRx1(u8 val);
    void write_NRx2(u8 val);
    void write_NRx3(u8 val);
    void write_NRx4(u8 val);
    void trigger();
    void tick_pulse_period();

    bool ext_enable{true};
    u32 number{};
    u32 dac_on{};

    u32 vol{};
    u32 on{};
    u32 left{}, right{};

    u32 enable_l{}, enable_r{};

    u32 period{};
    u32 next_period{};
    u32 length_enable{};
    u32 period_counter{};
    u32 wave_duty{};
    i32 polarity{};
    u32 duty_counter{};
    u32 short_mode{};
    u32 divisor{};
    u32 clock_shift{};
    i32 length_counter{};

    i32 samples[32]{};
    u32 sample_bank_mode{};
    u32 sample_sample_bank{}; // Keeping track of our bank playing
    u32 cur_sample_bank{};
    u8 sample_buffer[2]{};

    SWEEP sweep{};
    ENVELOPE env{};
};

struct core {
    explicit core(GBA::core *parent);
    GBA::core *bus;
    void write_wave_ram(u32 addr, u32 val);
    [[nodiscard]] u32 read_wave_ram(u32 addr) const;
    u32 read_IO(u32 addr, u32 sz, u32 access, bool has_effect);
    void write_IO(u32 addr, u32 sz, u32 val);
    void sound_FIFO(u32 num);
    float sample_channel_for_GBA(u32 cnum) const;
    float sample_channel(u32 n) const;
    float mix_sample(bool is_debug);
    void tick_psg();
    void sample_psg();
    void cycle();

private:
    void write_IO8(u32 addr, u32 val);
public:

    bool ext_enable{true};

    FIFO fifo[2]{};

    struct {
        u32 divider_16{}; // 16MHz->1MHz divider
        u32 divider_16_4{}; // 16 * 4 divider. 1MHz->262kHz
        u32 divider_2048{}; // / 512Hz @ 1MHz
        u32 frame_sequencer{};
    } clocks{};

    struct {
        float float_l{}, float_r{};
    } output{};

    CHANNEL channels[4]{};

    struct {
        i32 output_l{}, output_r{};
    } psg{};

    struct {
        u32 sound_bias{};
        u32 ch03_vol{};
        u32 ch03_vol_l{};
        u32 ch03_vol_r{};
        u32 master_enable{};
        u32 bias_amplitude{};
    } io{};

    struct {
        u32 counter{}, divisor{}, mask{};
    } divider2{};
};
#endif //JSMOOCH_EMUS_GBA_APU_H
}