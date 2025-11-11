//
// Created by . on 9/8/24.
//

#pragma once

#include "helpers/int.h"
#include "helpers/serialize/serialize.h"

struct NES;

struct NES_APU {
    NES_APU();

    void write_IO(u32 addr, u8 val);
    u8 read_IO(u32 addr, u8 old_val, u32 has_effect);
    void cycle();
    float mix_sample(u32 is_debug);
    float sample_channel(int cnum);
    void reset();
    void serialize(serialized_state &state);
    void deserialize(serialized_state &state);

private:
    i32 get_pulse_channel_output(u32 pc, u32 is_debug);
    void update_IF();
    i32 get_noise_channel_output(u32 is_debug);
    i32 get_triangle_channel_output(u32 is_debug);
    void set_length_counter(u32 pc, u32 val);
    void write_length_counter_enable(u32 pc, u32 val);
    void quarter_frame();
    void half_frame();
    void clock_env(u32 pc);
    void clock_linear_counter();
    void clock_envs_and_linear_counters();
    void clock_length_counters_and_sweeps();
    void clock_irq();
    void clock_frame_counter();
    void clock_triangle();
    void clock_noise();
    void clock_pulse(u32 pc);

public:
    u32 ext_enable=1;
    
    struct NESSNDCHAN {
        NESSNDCHAN() = default;
        u32 ext_enable=1;
        u32 number=0;
        i32 output=0;

        u32 vol=0;

        struct {
            i32 reload=0;
            i32 enabled=0;
            i32 count=0;
            u32 reload_flag=0;
        } linear_counter{};

        struct {
            u32 counter=0;
            u32 kind=0;
            u32 current=0;
        } duty{};

        u32 env_loop_or_length_counter_halt{}, constant_volume{}, volume_or_envelope{};
        u32 mode{};
        struct {
            i32 reload{}, count{};
        } period{};

        struct {
            i32 period_count{};
            u32 decay_level{};
            u32 start_flag{};
        } envelope{};

        struct {
            u32 enabled{}, shift{}, overflow{}, reload{};
            i32 negate{};
            struct {
                i32 counter{}, reload{};
            } period{};
        } sweep{};

        struct {
            i32 count{};
            u32 overflow{};
            u32 enabled{};
            u32 count_write_value{};
        } length_counter{};

        i32 first_clock=0;
    } channels[4];


    u64 *master_cycles{};

    struct {
        i32 bytes_remaining{};
        u32 ext_enable=1;
        u32 enabled{};

        u32 IF{}, new_IF{};
    } dmc{};

    struct {
        u32 step5_mode{};
    } io{};

    struct {
        u32 step{};
        u32 step_mod=4;
        u32 mode{};
        u32 IF{}; // interrupt flag
        u32 new_IF{};
        u32 interrupt_inhibit{};
    } frame_counter{};

    struct {
        u32 counter_1{};
        i32 counter_240hz{};
        i32 every_other=1;
    } clocks{};

    struct {
        void (*func)(void *, u32){};
        void *ptr{};
        u32 just_set{};
    } IRQ_pin{};
};
