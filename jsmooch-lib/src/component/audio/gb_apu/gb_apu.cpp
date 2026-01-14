//
// Created by . on 9/6/24.
//

#include <cstdlib>
#include <cassert>
#include <cstdio>

#include "gb_apu.h"

namespace GB_APU {

static constexpr i32 PWR_WAVES[16] = { 0x84, 0x40, 0x43, 0xAA, 0x2D, 0x78, 0x92, 0x3C, 0x60, 0x59, 0x59, 0xB0, 0x34, 0xB8, 0x2E, 0xDA };

core::core()
{
    for (u32 i = 0; i < 4; i++) {
        channels[i].number = i;
    }
    for (u32 i = 0; i < 15; i++)
        channels[3].samples[i] = PWR_WAVES[i];
}

u8 CHAN::read_NRx0() const
{
    u8 r = 0;
    r |= sweep.pace << 4;
    r |= sweep.direction << 3;
    r |= sweep.individual_step;
    return r;
}

u8 CHAN::read_NRx1() const {
    u8 r = 0;
    switch(number) {
        case 0:
        case 1:
            r |= wave_duty << 6;
            r |= (length_counter ^ 0x3F);
            return r;
        case 2:
            return length_counter ^ 0xFF;
        default:
            return 0;
    }
}

u8 CHAN::read_NRx2() const
{
    u8 r = 0;
    switch(number) {
        case 0:
        case 1:
            r |= env.initial_vol << 4;
            r |= env.direction << 3;
            r |= env.period;
            return r;
        case 2:
            r |= env.initial_vol << 5;
            return r;
        case 3:
            r |= env.initial_vol << 4;
            r |= env.direction << 3;
            r |= env.period;
            return r;
        default:
            NOGOHERE;
    }
    return 0;
}

u8 CHAN::read_NRx3() const
{
    if (number == 3) {
        u8 r = 0;
        r |= divisor;
        r |= (short_mode << 3);
        r |= (clock_shift << 4);
        return r;
    }
    return period & 0xFF;
}

#define C0 channels[0]
#define C1 channels[1]
#define C2 channels[2]
#define C3 channels[3]
u8 core::read_IO(u32 addr, u8 old_val, bool has_effect) const {
    u8 r = 0;
    switch(addr) {
        case 0xFF10: // ch0 sweep
            return C0.read_NRx0();
        case 0xFF11: // ch0 length timer & duty cycle
            return C0.read_NRx1();
        case 0xFF12: // ch0 volume and env
            return C0.read_NRx2();
        case 0xFF13:
            return C0.read_NRx3();
        case 0xFF16: // ch1 length timer & duty cycle
            return C1.read_NRx1();
        case 0xFF17: // ch1 volume & envelope
            return C1.read_NRx2();
        case 0xFF18:
            return C1.read_NRx3();
        case 0xFF1A:
            return channels[2].dac_on << 7;
        case 0xFF1B:
            return C2.read_NRx1();
        case 0xFF1C:
            return C2.read_NRx2();
        case 0xFF1D:
            return C2.read_NRx3();
        case 0xFF20:
            return C3.read_NRx1();
        case 0xFF21:
            return C3.read_NRx2();
        case 0xFF22:
            return C3.read_NRx3();
        case 0xFF25: // panning
            r |= channels[3].left << 7;
            r |= channels[2].left << 6;
            r |= channels[1].left << 5;
            r |= channels[0].left << 4;
            r |= channels[3].right << 3;
            r |= channels[2].right << 2;
            r |= channels[1].right << 1;
            r |= channels[0].right;
            return r;
        case 0xFF26: // NR52 audio master control
            r |= (io.master_on << 7);
            r |= channels[0].on;
            r |= (channels[1].on << 1);
            r |= (channels[2].on << 2);
            r |= (channels[3].on << 3);
            return r;
        default:
    }
    if ((addr >= 0xFF30) && (addr < 0xFF40)) {
        return 0xFF;
    }
    return 0xFF;
}
#undef C0
#undef C1
#undef C2
#undef C3

static constexpr u32 noise_periods[8] = { 8, 16, 32, 48, 64, 80, 96, 112};

void CHAN::trigger() {
    on = dac_on;
    env.on = 1;
    period_counter = 0;
    switch (number) {
        case 2:
            length_counter = 256;
            duty_counter = 0;
            vol = env.initial_vol;
            env.period_counter = 0;
            break;
        case 0:
        case 1:
            length_counter = 64;
            vol = env.initial_vol;
            env.period_counter = 0;
            if (vol == 0) on = 0;
            break;
        case 3:
            period_counter = static_cast<i32>(period);
            vol = env.initial_vol;
            duty_counter = 0x7FFF;
            if (vol == 0) on = 0;
            break;
        default:
    }
}

void CHAN::write_NRx0(u8 val)
{
    sweep.pace = (val >> 4) & 7;
    sweep.direction = (val >> 3) & 1;
    sweep.individual_step = val & 7;
}

void CHAN::write_NRx1(u8 val)
{
    switch(number) {
        case 0:
        case 1:
            wave_duty = (val >> 6) & 3;
            FALLTHROUGH;
        case 2:
        case 3: {
            u32 reload_value = number == 3 ? 256 : 64;
            u32 mask = reload_value - 1;
            length_counter = reload_value - (val & mask);
            return; }
        default:
            NOGOHERE;
    }
}

void CHAN::write_NRx2(u8 val)
{
    switch(number) {
        case 0:
        case 1:
        case 3:
            env.initial_vol = (val >> 4) & 15;
            env.direction = (val >> 3) & 1;
            env.period = val & 7;
            if ((val & 0xF8) == 0) {
                dac_on = 0;
                on = 0;
            }
            else {
                dac_on = 1;
            }
            return;
        case 2:
            env.initial_vol = (val >> 5) & 3;
            switch(env.initial_vol) {
                case 0:
                    env.rshift = 4;
                    break;
                case 1:
                    env.rshift = 0;
                    break;
                case 2:
                    env.rshift = 1;
                    break;
                case 3:
                    env.rshift = 2;
                    break;
                default:
                    NOGOHERE;
            }
            return;
        default:
    }
}

void CHAN::write_NRx3(u8 val)
{
    switch(number) {
        case 0:
        case 1:
            period = (period & 0x700) | val;
            return;
        case 2:
            next_period = (next_period & 0x700) | val;
            period = next_period;
            return;
        case 3:
            divisor = val & 7;
            short_mode = (val >> 3) & 1;
            clock_shift = (val >> 4) & 15;
            period = (noise_periods[divisor] << clock_shift) >> 2;
            return;
        default:
            NOGOHERE;
    }
}

void CHAN::write_NRx4(u8 val)
{
    switch(number) {
        case 0:
        case 1:
            period = (period & 0xFF) | ((val & 7) << 8);
            break;
        case 2:
            next_period = (next_period & 0xFF) | ((val & 7) << 8);
            period = next_period;
            break;
        case 3:
            break;
        default:
    }
    length_enable = (val >> 6) & 1;
    /*if (length_enable) {
        printf("\nLEN ENABLE FOR CHAN %d", number);
    }*/
    if (val & 0x80) {
        trigger();
    }
}

#define C0 channels[0]
#define C1 channels[1]
#define C2 channels[2]
#define C3 channels[3]
void core::write_IO(u32 addr, u8 val)
{
    switch(addr) {
        case 0xFF10: // CH0 sweep
            return C0.write_NRx0(val);
        case 0xFF11: // ch0 length timer & duty cycle
            return C0.write_NRx1(val);
        case 0xFF12: // ch0 volume and env
            return C0.write_NRx2(val);
        case 0xFF13: // ch0 period low
            return C0.write_NRx3(val);
        case 0xFF14: // ch0 trigger and such
            return C0.write_NRx4(val);
        case 0xFF16: // CH1 length timer & Duty cycle
            return C1.write_NRx1(val);
        case 0xFF17:
            return C1.write_NRx2(val);
        case 0xFF18:
            return C1.write_NRx3(val);
        case 0xFF19:
            return C1.write_NRx4(val);
        case 0xFF1A:
            channels[2].dac_on = (val >> 7) & 1;
            channels[2].on = channels[2].dac_on;
            return;
        case 0xFF1B:
            return C2.write_NRx1(val);
        case 0xFF1C:
            return C2.write_NRx2(val);
        case 0xFF1D:
            return C2.write_NRx3(val);
        case 0xFF1E:
            return C2.write_NRx4(val);
        case 0xFF20:
            return C3.write_NRx1(val);
        case 0xFF21:
            return C3.write_NRx2(val);
        case 0xFF22:
            return C3.write_NRx3(val);
        case 0xFF23:
            return C3.write_NRx4(val);
        case 0xFF25: // panning
            channels[3].left = (val >> 7) & 1;
            channels[2].left = (val >> 6) & 1;
            channels[1].left = (val >> 5) & 1;
            channels[0].left = (val >> 4) & 1;
            channels[3].right = (val >> 3) & 1;
            channels[2].right = (val >> 2) & 1;
            channels[1].right = (val >> 1) & 1;
            channels[0].right = (val >> 0) & 1;
            return;
        case 0xFF26:
            io.master_on = (val >> 7) & 1;
            return;
        default:
    }
    if ((addr >= 0xFF30) && (addr < 0xFF40)) {
        // upper 4 first
        channels[2].samples[addr & 15] = val;
    }
}


void CHAN::tick_sweep()
{
    if (sweep.pace != 0) {
        sweep.pace_counter--;
        if (sweep.pace_counter <= 0) {
            sweep.pace_counter = sweep.pace;
            if (sweep.direction == 0) {
                period += sweep.individual_step;
                if (period == 2047) on = 0;
            } else {
                period -= sweep.individual_step;
                if (period == 0) on = 0;
            }
            period &= 0x7FF;
        }
    }

}

void CHAN::tick_length_timer()
{
    if (length_enable) {
        length_counter = length_counter - 1;
        if (length_counter <= 0) {
            on = 0;
            if (number < 2) duty_counter = 0;
        }
    }
}

static constexpr i32 sq_duty[4][8] = {
        { 0, 0, 0, 0, 0, 0, 0, 1 }, // 00 - 12.5%
        { 1, 0, 0, 0, 0, 0, 0, 1 }, // 01 - 25%
        { 1, 0, 0, 0, 0, 1, 1, 1 }, // 10 - 50%
        { 0, 1, 1, 1, 1, 1, 1, 0 }, // 11 - 75%
};

void CHAN::tick_env()
{
    if (env.period != 0 && env.on) {
        env.period_counter = (env.period_counter - 1) & 7;
        if (env.period_counter <= 0) {
            env.period_counter = env.period;
            if (env.direction == 0) {
                if (vol > 0) vol--;
                else env.on = 0;
            }
            else {
                if (vol < 15) vol++;
                else env.on = 0;
            }
        }
    }
}

void CHAN::tick_wave_period_twice() {
    for (u32 i = 0; i < 2; i++) {
        if (on) {
            period_counter++;
            if (period_counter >= 0x7FF) {
                period_counter = static_cast<i32>(period);
                duty_counter = (duty_counter + 1) & 31;
                //period = next_period;
                if ((duty_counter & 1) == 0) {
                    u8 p = samples[duty_counter >> 1];
                    sample_buffer[0] = p >> 4;
                    sample_buffer[1] = p & 15;
                }
                polarity = sample_buffer[duty_counter & 1] >> env.rshift;
            }
        }
    }
}

void CHAN::tick_noise_period()
{
    if (on && (clock_shift < 14) && (period != 0)) {
        period_counter--;
        if (period_counter <= 0) {
            period_counter = static_cast<i32>(period);
            u32 flipbits = ~(short_mode ? 0x4040 : 0x4000);
            u32 lfsr = duty_counter;
            u32 l2b = lfsr;
            lfsr >>= 1;
            l2b = (l2b ^ lfsr) & 1;
            lfsr &= flipbits;
            lfsr |= l2b * ~flipbits;
            duty_counter = lfsr;
            polarity = (duty_counter & 1) ^ 1;
        }
    }
}

void CHAN::tick_pulse_period()
{
    if (on) {
        period_counter++;
        if (period_counter >= 0x7FF) {
            period_counter = static_cast<i32>(period);
            duty_counter = (duty_counter + 1) & 7;
            polarity = sq_duty[wave_duty][duty_counter];
        }
    }
}

#define C0 channels[0]
#define C1 channels[1]
#define C2 channels[2]
#define C3 channels[3]

void core::cycle()
{
    C0.tick_pulse_period();
    C1.tick_pulse_period();
    C2.tick_wave_period_twice();
    C3.tick_noise_period();
    clocks.divider_2048--;
    if (clocks.divider_2048 <= 0) {
        clocks.divider_2048 = 2048;
        clocks.frame_sequencer = (clocks.frame_sequencer + 1) & 7;
        switch(clocks.frame_sequencer) {
            case 6:
            case 2:
                C0.tick_sweep();
                FALLTHROUGH;
            case 0:
            case 4:
                C0.tick_length_timer();
                C1.tick_length_timer();
                C2.tick_length_timer();
                C3.tick_length_timer();
                break;
            case 7:
                C0.tick_env();
                C1.tick_env();
                C3.tick_env();
                break;
            default:
        }
    }
}

float core::mix_sample(const bool is_debug) const {
    float output = 0;
    if (!is_debug && (!ext_enable || !io.master_on)) {
        return 0;
    }

    for (u32 i = 0; i < 4; i++) {
        auto &chan = channels[i];
        switch(i) {
            case 3:
            case 0:
            case 1: {
                if (chan.on && chan.ext_enable) {
                    const float intensity = (1.0f/15.0f) * static_cast<float>(chan.vol);
                    const float sample = (static_cast<float>((static_cast<i32>(chan.polarity) * -2) + 1) * intensity);
                    assert(sample >= -1.0f);
                    assert(sample <= 1.0f);
                    output += sample * .25f;
                }
                break;
            }
            case 2: {
                if (chan.on && chan.ext_enable && chan.dac_on) {
                    float sample = ((static_cast<float>(chan.polarity) / 15.0f) * -2.0f) + 1.0f;
                    output += sample * .25f;
                }
                break;
            }
            default:
        }
    }
    return output;
}

float CHAN::sample() const {
    float output = 0;

    switch(number) {
        case 3:
        case 0:
        case 1: {
            if (on) {
                float intensity = (1.0f / 15.0f) * static_cast<float>(vol);
                output = (static_cast<float>((polarity * 2) - 1) * intensity);
            }
            break; }
        case 2: {
            output = ((static_cast<float>(polarity) / 15.0f) * -2.0f) + 1.0f;
            break; }
        default:
            NOGOHERE;
    }
    return output;
}
}