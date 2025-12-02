//
// Created by . on 7/19/24.
//

#pragma once

/* Old version was hybrid.
 * New version is straight Ares port to learn from
 */

#include "helpers/debugger/debuggerdefs.h"
#include "helpers/int.h"

namespace YM2612 {

enum variant {
    OPN2V_ym2612,
    OPN2V_ym3438
};

enum envelope_state{
    EP_attack = 0,
    EP_decay = 1,
    EP_sustain = 2,
    EP_release = 3,
} ;


struct CHANNEL;

struct LFO {
    bool enabled{};
    u8 counter{}, divider{}, period{};

    void run();
    void freq(u8 val);
    void enable(bool enabled_in);

    static u16 fm(u8 lfo_counter, u8 pms, u16 f_num);
    static u16 am(u8 lfo_counter, u8 ams);
};


struct OPERATOR {
    explicit OPERATOR(CHANNEL *parent) : ch(parent) {}
    void run_phase();
    void run_env_ssg();
    void run_env();
    void key_on(u32 val);
    i16 run_output(i32 mod_input);
    u16 attenuation() const;
    void update_env_key_scale_rate(u16 f_num, u16 block);
    void update_freq(u16 f_num, u16 block);
    void update_key_scale(u8 val);

    CHANNEL *ch;

    u32 num{};
    u32 am_enable{}; // 1bit

    u32 lfo_counter{}, ams{};

    i16 output{};
    struct {
        u32 value{}, latch{};
    } f_num{};
    struct {
        u32 value{}, latch{};
    } block{};

    struct {
        u32 counter{}; // 20bit
        u16 output{}; // 10bit
        u16 f_num{}, block{};
        u32 input{};
        u32 multiple{}, detune{};
    } phase{};

    struct YM2612_ENV {
        envelope_state state;
        i32 rate{}, divider{};
        u32 cycle_count{};
        u32 steps{};
        u32 level{}; // 10 bits{}, 4.6 fixed-point
        i32 attenuation{};

        u32 key_scale{}; // 2bit
        u32 key_scale_rate{};
        u32 attack_rate{}; // 5bit
        u32 decay_rate{}; // 5bit
        u32 sustain_rate{}; // 5bit
        u32 sustain_level{}; // 4bit
        u32 release_rate{}; // 5bit

        struct {
            u32 invert_output{}, attack{}, enabled{}, hold{}, alternate{};
        }ssg{};
        u32 total_level{};
    } envelope{};

    u32 keyon{};

};

struct core;

struct CHANNEL {
    explicit CHANNEL(core *parent) : ym2612(parent), op{OPERATOR(this), OPERATOR(this), OPERATOR(this), OPERATOR(this)} {}
    void run();
    void update_phase_generators();

    core *ym2612;
    enum YM2612_FREQ_MODE {
        YFM_single = 0,
        YFM_multiple
    } mode{};
    i32 op0_prior[2]{};
    u32 num{};
    bool ext_enable{true};
    u32 left_enable{}, right_enable{};
    i16 output{};

    u32 algorithm{}, feedback{}, pms{}; //3bit . pms = vibrato
    u32 ams{}; // 2bit ams = tremolo
    struct {
        u16 value{}, latch{}; // 11 bits
    } f_num{};
    struct {
        u16 value{}, latch{}; // 11 bits
    } block{};
    OPERATOR op[4];
} ;

struct core {
    explicit core(variant in_variant, u64 *master_cycle_count_in, u64 master_wait_cycles_in);
    void serialize(serialized_state &m_state);
    void deserialize(serialized_state &m_state);
    void write(u32 addr, u8 val);
    u8 read(u32 addr, u32 old, bool has_effect) const;
    void reset();
    void cycle();
    void write_ch_reg(u8 val, u32 bch);
    void write_op_reg(u8 val, u32 bch);
    void write_group1(u8 val);
    void write_group2(u8 val);
    i16 sample_channel(u32 ch) const;
    void mix_sample();
    variant variant{};
    CHANNEL channel[6];

private:
    u32 cycle_timers();

public:
    i32 phase_compute_increment(const OPERATOR &op) const;

    struct {
        i32 left_output{}, right_output{}, mono_output{}; // Current mixed sample

        struct {
            struct {
                i32 left{}, right{};
            } sample{};

            struct {
                i32 left{}, right{};
            } output{};
        } filter{};

        u64 last_master_clock_op0{};
    } mix{};
    bool ext_enable{true};

    LFO lfo{};

    struct {
        u32 group{};
        u32 addr{};
        u32 csm_enabled{};
    } io{};

    struct {
        u64 busy_until{};
        u64 env_cycle_counter{};
    } status{};


    struct {
        i32 sample{};
        u32 enable{};
    } dac{};

    struct {
        u32 div24{};
        u32 div24_3{};
    } clock{};

    struct {
        u32 clock{}, divider{}; // clock=12bit{}, divider = 32bit
    } envelope{};

    struct {
        u32 enable{}, irq{}, line{}, period{}, counter{};
    } timer_a{};

    struct {
        u32 enable{}, irq{}, line{}, period{}, counter{}, divider{};
    } timer_b{};

    u64 *master_cycle_count{};
    u64 master_wait_cycles{};


    DBG_EVENT_VIEW_ONLY;
};

}