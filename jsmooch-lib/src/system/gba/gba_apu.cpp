//
// Created by . on 12/28/24.
//
#include <cassert>
#include <cstring>
#include <cstdlib>

#include "helpers/int.h"
#include "gba_bus.h"
#include "gba_apu.h"
#include "gba_dma.h"

namespace GBA::APU {
static constexpr i32 sq_duty[4][8] = {
        { 0, 0, 0, 0, 0, 0, 0, 1 }, // 00 - 12.5%
        { 1, 0, 0, 0, 0, 0, 0, 1 }, // 01 - 25%
        { 1, 0, 0, 0, 0, 1, 1, 1 }, // 10 - 50%
        { 0, 1, 1, 1, 1, 1, 1, 0 }, // 11 - 75%
};

core::core(GBA::core *parent) : bus(parent)
{
    for (u32 i = 0; i < 4; i++) {
        channels[i].number = i;
    }
}

#define SOUNDBIAS 0x04000088
#define FIFO_A_L 0x040000A0
#define FIFO_A_H 0x040000A2
#define FIFO_B_L 0x040000A4
#define FIFO_B_H 0x040000A6
#define SOUNDCNT_L 0x04000080
#define SOUNDCNT_H 0x04000082
#define SOUNDCNT_X 0x04000084
#define SOUND1CNT_L 0x04000060 // NR10
#define SOUND1CNT_H 0x04000062 // NR11, NR12
#define SOUND1CNT_X 0x04000064 // NR13, NR14
#define SOUND2CNT_L 0x04000068  // NR21, NR22
#define SOUND2CNT_H 0x0400006C  // NR23, NR24

#define SOUND3CNT_L 0x04000070 // NR30
#define SOUND3CNT_H 0x04000072  // NR31, NR32
#define SOUND3CNT_X 0x04000074  // NR33, NR34
#define SOUND4CNT_L 0x04000078
#define SOUND4CNT_H 0x0400007C

#define WAVE_RAM 0x04000090

u8 CHANNEL::read_NRx0() const
{
    u8 r = 0;
    if (number == 2) {
        r |= sample_bank_mode << 5;
        r |= cur_sample_bank << 6;
        r |= dac_on << 7;
    }
    else {
        r |= sweep.pace << 4;
        r |= sweep.direction << 3;
        r |= sweep.individual_step;
    }
    return r;
}

u8 CHANNEL::read_NRx1() const
{
    u8 r = 0;
    switch(number) {
        case 0:
        case 1:
            r |= wave_duty << 6;
            //r |= (length_counter ^ 0x3F);
            return r;
        case 2:
            return length_counter ^ 0xFF;
        default:
            NOGOHERE;
    }
}

u8 CHANNEL::read_NRx2() const
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
            r |= env.force75 << 7;
            return r;
        case 3:
            r |= env.initial_vol << 4;
            r |= env.direction << 3;
            r |= env.period;
            return r;
        default: NOGOHERE;
    }
}

u8 CHANNEL::read_NRx3() const
{
    if (number == 3) {
        u8 r = 0;
        r |= divisor;
        r |= (short_mode << 3);
        r |= (clock_shift << 4);
        return r;
    }
    return 0;
}

u8 CHANNEL::read_NRx4() const
{
    return length_enable << 6;
}

void CHANNEL::trigger() {
    on = dac_on;
    env.on = 1;
    period_counter = 0;
    switch (number) {
        case 2:
            length_counter = 256;
            duty_counter = 0;
            sample_sample_bank = sample_bank_mode ? 0 : cur_sample_bank;
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
        default: NOGOHERE;
    }
}

void CHANNEL::write_NRx0(const u8 val)
{
    if (number == 2) {
        sample_bank_mode = (val >> 5) & 1;
        cur_sample_bank = (val >> 6) & 1;
        dac_on = (val >> 7) & 1;
        on = dac_on;
    }
    else {
        sweep.pace = (val >> 4) & 7;
        sweep.direction = (val >> 3) & 1;
        sweep.individual_step = val & 7;

    }
}

void CHANNEL::write_NRx1(const u8 val)
{
    switch(number) {
        case 0:
        case 1:
            wave_duty = (val >> 6) & 3;
            FALLTHROUGH;
        case 2:
        case 3: {
            const u32 reload_value = number == 3 ? 256 : 64;
            const u32 mask = reload_value - 1;
            length_counter = static_cast<i32>(reload_value - (val & mask));
            return; }
        default: NOGOHERE;
    }
}

void CHANNEL::write_NRx2(const u8 val)
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
            env.force75 = (val >> 7) & 1;
            switch(env.initial_vol) {
                case 0: env.rshift = 4; return;
                case 1: env.rshift = 0; return;
                case 2: env.rshift = 1; return;
                case 3: env.rshift = 2; return;
                default:
            }
        default:
    }
}

static constexpr u32 noise_periods[8] = { 8, 16, 32, 48, 64, 80, 96, 112};

void CHANNEL::write_NRx3(const u8 val)
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
        default: NOGOHERE;
    }
}

void CHANNEL::write_NRx4(const u8 val)
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
        default: NOGOHERE;
    }
    length_enable = (val >> 6) & 1;

    if (val & 0x80) {
        trigger();
    }
}

void core::write_wave_ram(const u32 addr, const u32 val)
{
    channels[2].samples[((channels[2].cur_sample_bank ^ 1) << 4) | addr] = static_cast<i32>(val);
}

u32 core::read_wave_ram(const u32 addr) const {
    return channels[2].samples[((channels[2].cur_sample_bank ^ 1) << 4) | addr];
}

#define C0 channels[0]
#define C1 channels[1]
#define C2 channels[2]
#define C3 channels[3]

u32 core::read_IO(u32 addr, u8 sz, u8 access, bool has_effect)
{
    u32 v;
    switch(addr) {
        case SOUNDBIAS+0:
            return io.sound_bias & 0xFF;
        case SOUNDBIAS+1:
            return io.sound_bias >> 8;
        case SOUNDCNT_X+0:
            v = channels[0].on;
            v |= channels[1].on << 1;
            v |= channels[2].on << 2;
            v |= channels[3].on << 3;
            v |= io.master_enable << 7;
            return v;
        case SOUNDCNT_X+1:
            return 0;
        case SOUNDCNT_L+0:
            v = io.ch03_vol_r;
            v |= io.ch03_vol_l << 4;
            return v;
        case SOUNDCNT_L+1:
            v = channels[0].enable_r;
            v |= channels[1].enable_r << 1;
            v |= channels[2].enable_r << 2;
            v |= channels[3].enable_r << 3;
            v |= channels[0].enable_l << 4;
            v |= channels[1].enable_l << 5;
            v |= channels[2].enable_l << 6;
            v |= channels[3].enable_l << 7;
            return v;
        case SOUND1CNT_L+0:
            return C0.read_NRx0();
        case SOUND1CNT_H+0:
            return C0.read_NRx1();
        case SOUND1CNT_H+1:
            return C0.read_NRx2();
        case SOUND1CNT_X+0:
            return C0.read_NRx3();
        case SOUND1CNT_X+1:
            return C0.read_NRx4();
        case SOUND2CNT_L+0:
            return C1.read_NRx1();
        case SOUND2CNT_L+1:
            return C1.read_NRx2();
        case SOUND2CNT_H+0:
            return C1.read_NRx3();
        case SOUND2CNT_H+1:
            return C1.read_NRx4();
        case SOUND3CNT_L+0:
            return C2.read_NRx0();
        case SOUND3CNT_H+0:
            return 0;
        case SOUND3CNT_H+1:
            return C2.read_NRx2();
        case SOUND3CNT_X+0:
            return C2.read_NRx3();
        case SOUND3CNT_X+1:
            return C2.read_NRx4();
        case SOUND4CNT_L+0:
            return 0;
        case SOUND4CNT_L+1:
            return C3.read_NRx2();
        case SOUND4CNT_H+0:
            return C3.read_NRx3();
        case SOUND4CNT_H+1:
            return C3.read_NRx4();

        case SOUNDCNT_H+0:
            v = io.ch03_vol;
            v |= fifo[0].vol << 2;
            v |= fifo[1].vol << 3;
            return v;
        case SOUNDCNT_H+1:
            v = fifo[0].enable_r;
            v |= fifo[0].enable_l << 1;
            v |= fifo[0].timer_id << 2;
            v |= fifo[1].enable_r << 4;
            v |= fifo[1].enable_l << 5;
            v |= fifo[1].timer_id << 6;
            return v;

        case WAVE_RAM+0:
        case WAVE_RAM+1:
        case WAVE_RAM+2:
        case WAVE_RAM+3:
        case WAVE_RAM+4:
        case WAVE_RAM+5:
        case WAVE_RAM+6:
        case WAVE_RAM+7:
        case WAVE_RAM+8:
        case WAVE_RAM+9:
        case WAVE_RAM+10:
        case WAVE_RAM+11:
        case WAVE_RAM+12:
        case WAVE_RAM+13:
        case WAVE_RAM+14:
        case WAVE_RAM+15:
            return read_wave_ram(addr & 15);
        case 0x04000061:
        case 0x04000066:
        case 0x04000067:
        case 0x0400006A:
        case 0x0400006B:

        case 0x0400006E:
        case 0x0400006F:
        case 0x04000071:
        case 0x04000076:
        case 0x04000077:
        case 0x0400007A:
        case 0x0400007B:
        case 0x0400007E:
        case 0x0400007F:
        case 0x04000086:
        case 0x04000087:
        case 0x0400008A:
        case 0x0400008B:
            return 0;

        case FIFO_A_L+0:
        case FIFO_A_L+1:
        case FIFO_A_H+0:
        case FIFO_A_H+1:
        case FIFO_B_L+0:
        case FIFO_B_L+1:
        case FIFO_B_H+0:
        case FIFO_B_H+1:

        case 0x0400008C:
        case 0x0400008D:
        case 0x0400008E:
        case 0x0400008F:
        case 0x040000A8:
        case 0x040000A9:
        case 0x040000AA:
        case 0x040000AB:
        case 0x040000AC:
        case 0x040000AD:
        case 0x040000AE:
        case 0x040000AF:
            return bus->open_bus_byte(addr);
        default:
    }
    printf("\nWARN UNDONE READ FROM APU %08x", addr);
    return 0;
}

#undef C0
#undef C1
#undef C2
#undef C3

void core::sound_FIFO(const u32 num) {
    auto &m_fifo = this->fifo[num];

    if (m_fifo.len > 0) {
        m_fifo.len--;
        m_fifo.sample = static_cast<i32>(m_fifo.data[m_fifo.head]);
        m_fifo.head = (m_fifo.head + 1) & 31;
    }

    // If we need more data...
    if (m_fifo.len <= 16) {
        auto &c = bus->dma.channel[1 + num];
        if ((c.io.enable) && (c.io.start_timing == 3)) {
            c.start();
        }
    }
}

void FIFO::reset()
{
    needs_commit = false;
    head = tail;
    len = 0;
    output_head = 0;
}

void FIFO::commit()
{
    if (!needs_commit) return;
    needs_commit = false;
    tail = (tail + 4) & 31;
    len += 4;
    if (len > 32) {
        reset();
    }
}

void FIFO::write(const u32 v)
{
    if (len == 32) head = (head + 1) & 31; // Full buffer, push another byte?...
    else len++;
    data[tail] = static_cast<i8>(v);
    tail = (tail + 1) & 31;
}


#define C0 channels[0]
#define C1 channels[1]
#define C2 channels[2]
#define C3 channels[3]

void core::write_IO8(const u32 addr, u32 val)
{
    switch(addr) {
        case SOUND1CNT_L+0:
            C0.write_NRx0(val);
            return;
        case SOUND1CNT_H+0:
            C0.write_NRx1(val);
            return;
        case SOUND1CNT_H+1:
            C0.write_NRx2(val);
            return;
        case SOUND1CNT_X+0:
            C0.write_NRx3(val);
            return;
        case SOUND1CNT_X+1:
            C0.write_NRx4(val);
            return;
        case SOUND2CNT_L+0:
            C1.write_NRx1(val);
            return;
        case SOUND2CNT_L+1:
            C1.write_NRx2(val);
            return;
        case SOUND2CNT_H+0:
            C1.write_NRx3(val);
            return;
        case SOUND2CNT_H+1:
            C1.write_NRx4(val);
            return;
        case SOUND3CNT_L+0:
            C2.write_NRx0(val);
            return;
        case SOUND3CNT_H+0:
            C2.write_NRx1(val);
            return;
        case SOUND3CNT_H+1:
            C2.write_NRx2(val);
            return;
        case SOUND3CNT_X+0:
            C2.write_NRx3(val);
            return;
        case SOUND3CNT_X+1:
            C2.write_NRx4(val);
            return;
        case SOUND4CNT_L+0:
            C3.write_NRx1(val);
            return;
        case SOUND4CNT_L+1:
            C3.write_NRx2(val);
            return;
        case SOUND4CNT_H+0:
            C3.write_NRx3(val);
            return;
        case SOUND4CNT_H+1:
            C3.write_NRx4(val);
            return;
        case SOUNDBIAS+0:
            io.sound_bias = (io.sound_bias & 0xFF00) | val;
            return;
        case SOUNDBIAS+1: {
            io.sound_bias = (io.sound_bias & 0xFF) | (val << 8);
            u32 v = (val >> 6) & 3;
            io.bias_amplitude = v;
            switch(v) {
                case 0: divider2.mask = 31; break; // /8 = 262khz. =16 = 128khz. =32 = 64khz. /64 = 32khz
                case 1: divider2.mask = 15; break;
                case 2: divider2.mask = 7; break;
                case 3: divider2.mask = 3; break;
                default: NOGOHERE;
            }
            return; }
        case SOUNDCNT_L+0:
            io.ch03_vol_r = val & 7;
            io.ch03_vol_l = (val >> 4) & 7;
            return;
        case SOUNDCNT_L+1:
            channels[0].enable_r = (val >> 0) & 1;
            channels[1].enable_r = (val >> 1) & 1;
            channels[2].enable_r = (val >> 2) & 1;
            channels[3].enable_r = (val >> 3) & 1;
            channels[0].enable_l = (val >> 4) & 1;
            channels[1].enable_l = (val >> 5) & 1;
            channels[2].enable_l = (val >> 6) & 1;
            channels[3].enable_l = (val >> 7) & 1;
            return;
        case SOUNDCNT_H+0:
            io.ch03_vol = val & 3;
            fifo[0].vol = (val >> 2) & 1;
            fifo[1].vol = (val >> 3) & 1;
            return;
        case SOUNDCNT_H+1:
            fifo[0].enable_l = val & 1;
            fifo[0].enable_r = (val >> 1) & 1;
            fifo[0].timer_id = (val >> 2) & 1;
            if ((val >> 3) & 1) fifo[0].reset();
            fifo[1].enable_l = (val >> 4) & 1;
            fifo[1].enable_r = (val >> 5) & 1;
            fifo[1].timer_id = (val >> 6) & 1;
            if ((val >> 7) & 1) fifo[1].reset();
            return;
        case SOUNDCNT_X+0:
            io.master_enable = (val >> 7) & 1;
            return;
        case SOUNDCNT_X+1:
            return;
        case FIFO_A_L+0:
        case FIFO_A_L+1:
        case FIFO_A_H+0:
        case FIFO_A_H+1:
            fifo[0].write(val);
            return;
        case FIFO_B_L+0:
        case FIFO_B_L+1:
        case FIFO_B_H+0:
        case FIFO_B_H+1:
            fifo[1].write(val);
            return;

        case WAVE_RAM+0:
        case WAVE_RAM+1:
        case WAVE_RAM+2:
        case WAVE_RAM+3:
        case WAVE_RAM+4:
        case WAVE_RAM+5:
        case WAVE_RAM+6:
        case WAVE_RAM+7:
        case WAVE_RAM+8:
        case WAVE_RAM+9:
        case WAVE_RAM+10:
        case WAVE_RAM+11:
        case WAVE_RAM+12:
        case WAVE_RAM+13:
        case WAVE_RAM+14:
        case WAVE_RAM+15:
            write_wave_ram(addr & 15, val);
            return;
        case 0x04000061:

        case 0x04000066:
        case 0x04000067:
        case 0x0400006A:
        case 0x0400006B:
        case 0x0400006E:
        case 0x0400006F:
        case 0x04000071:
        case 0x04000076:
        case 0x04000077:
        case 0x0400007A:
        case 0x0400007B:
        case 0x0400007E:
        case 0x0400007F:
        case 0x04000086:
        case 0x04000087:
        case 0x0400008A:
        case 0x0400008B:
        case 0x0400008C:
        case 0x0400008D:
        case 0x0400008E:
        case 0x0400008F:
        case 0x040000A8:
        case 0x040000A9:
        case 0x040000AA:
        case 0x040000AB:
        case 0x040000AC:
        case 0x040000AD:
        case 0x040000AE:
        case 0x040000AF:
            return;
        default:
    }
    printf("\nWARN UNDONE WRITE TO APU %08x", addr);
}
#undef C0
#undef C1
#undef C2
#undef C3

void core::write_IO(const u32 addr, const u8 sz, const u32 val) {
    write_IO8(addr, val & 0xFF);
    if (sz >= 2) {
        write_IO8(addr + 1, (val >> 8) & 0xFF);
    }
    if (sz == 4) {
        write_IO8(addr + 2, (val >> 16) & 0xFF);
        write_IO8(addr + 3, (val >> 24) & 0xFF);
    }
    if (fifo[0].needs_commit) fifo[0].commit();
    if (fifo[1].needs_commit) fifo[1].commit();
}

float core::sample_channel_for_GBA(u32 cnum) const {
    auto &chan = channels[cnum];
    float m_output = 0.0f;

    switch(cnum) {
        case 3:
        case 0:
        case 1: {
            if (chan.on) {
                const float intensity = (1.0f / 15.0f) * static_cast<float>(chan.vol);
                m_output = (static_cast<float>((chan.polarity * 2) - 1) * intensity);
            }
            break; }
        case 2: {
            m_output = ((static_cast<float>(chan.polarity) / 15.0f) * -2.0f) + 1.0f;
            break; }
    }
    return m_output;
}

float core::sample_channel(const u32 n) const {
    if (n < 4) return sample_channel_for_GBA(n);
    const auto &m_fifo = fifo[n - 4];
    i32 sample = m_fifo.output << m_fifo.vol;

    switch(io.bias_amplitude) {
        case 0: break;
        case 1: sample &= ~1; break;
        case 2: sample &= ~3; break;
        case 3: sample &= ~7; break;
        default: NOGOHERE;
    }

    return static_cast<float>(sample << 7) / 32768.0f;
}

float core::mix_sample(const bool is_debug)
{
    float s = 0.0f;
    if (io.master_enable && (ext_enable || is_debug)) {
        i32 left = 0, right = 0;
        for (const auto &m_fifo : fifo) {
            if (!m_fifo.ext_enable && !is_debug) continue;
            if (m_fifo.enable_l) {
                left += m_fifo.output << m_fifo.vol; // 8 to 9 bits
            }
            if (m_fifo.enable_r) {
                right += m_fifo.output << m_fifo.vol; // 8 to 9 bits
            }
        }
        left += psg.output_l;
        right += psg.output_r;
        // up to 11 bits possible now. Clamp!
        if (left < -1024) left = -1024;
        if (left > 1023) left = 1023;
        if (right < -1024) right = -1024;
        if (right > 1023) right = 1023;

        // Get rid of bits...
        switch(io.bias_amplitude) {
            case 0: break;
            case 1: left &= ~1; right &= ~1; break;
            case 2: left &= ~3; right &= ~3; break;
            case 3: left &= ~7; right &= ~7; break;
            default: NOGOHERE;
        }

        const i32 center = left + right;
        // 11 bits possible now! Because we're mixing left and right!
        // Now convert to float
        //s = (((float)(center + 1024)) / 2047.0f) - 1.0f;
        s = static_cast<float>(center << 5) / 32768.0f;
        //s = (((float)center) / 1024.0f);
        output.float_l = static_cast<float>(left << 6) / 32768.0f;
        output.float_r = static_cast<float>(right << 6) / 32768.0f;
        assert(output.float_l <= 1.0f);
        assert(output.float_l >= -1.0f);
    }
    return s;
}

void CHANNEL::tick_pulse_period()
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

void CHANNEL::tick_wave_period_twice() {
    for (u32 i = 0; i < 2; i++) {
        if (on) {
            period_counter++;
            if (period_counter >= 0x7FF) {
                period_counter = period;
                duty_counter = (duty_counter + 1) & 31;
                if ((duty_counter & 1) == 0) { // End of current byte
                    u8 p = samples[(sample_sample_bank << 4) | (duty_counter >> 1)];
                    sample_buffer[0] = p >> 4;
                    sample_buffer[1] = p & 15;
                    if (duty_counter == 0) { // End of current bank, swap to next!
                        sample_sample_bank ^= sample_bank_mode; // don't flip if mode = 0
                    }
                }
                if (env.force75) {
                    polarity = (sample_buffer[duty_counter & 1] * 3) >> 2;
                }
                else {
                    polarity = sample_buffer[duty_counter & 1] >> env.rshift;
                }

            }
        }
    }
}

void CHANNEL::tick_noise_period()
{
    if (on && (clock_shift < 14) && (period != 0)) {
        period_counter--;
        if (period_counter <= 0) {
            period_counter = static_cast<i32>(period);
            const u32 flipbits = ~(short_mode ? 0x4040 : 0x4000);
            u32 lfsr = duty_counter;
            u32 l2b = lfsr;
            lfsr >>= 1;
            l2b = (l2b ^ lfsr) & 1;
            lfsr &= flipbits;
            lfsr |= l2b * ~flipbits;
            duty_counter = lfsr;
            polarity = static_cast<i32>((duty_counter & 1) ^ 1);
        }
    }
}

void CHANNEL::tick_sweep()
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

void CHANNEL::tick_length_timer()
{
    if (length_enable) {
        length_counter = length_counter - 1;
        if (length_counter <= 0) {
            on = 0;
            if (number < 2) duty_counter = 0;
        }
    }
}

void CHANNEL::tick_env()
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

void core::tick_psg()
{
    // @1MHz
    channels[0].tick_pulse_period();
    channels[1].tick_pulse_period();
    channels[2].tick_wave_period_twice();
    channels[3].tick_noise_period();

    if (clocks.divider_2048 == 0) {
        clocks.frame_sequencer = (clocks.frame_sequencer + 1) & 7;
        switch(clocks.frame_sequencer) {
            case 6:
            case 2:
                channels[0].tick_sweep();
                FALLTHROUGH;
            case 0:
            case 4:
                channels[0].tick_length_timer();
                channels[1].tick_length_timer();
                channels[2].tick_length_timer();
                channels[3].tick_length_timer();
                break;
            case 7:
                channels[0].tick_env();
                channels[1].tick_env();
                channels[3].tick_env();
                break;
        }
    }
    clocks.divider_2048 = (clocks.divider_2048 + 1) & 2047;

}

void core::sample_psg()
{
    // So we want a 9-bit signed output?

    i32 left = 0, right = 0;
    if (!(ext_enable && io.master_enable)) {
        psg.output_l = 0;
        psg.output_r = 0;
        return;
    }

    for (u32 i = 0; i < 4; i++) {
        const CHANNEL &chan = channels[i];
        switch(i) {
            case 3:
            case 0:
            case 1: {
                if (chan.on && chan.ext_enable) {
                    const i32 intensity = static_cast<i32>(chan.vol);
                    const i32 sample = chan.polarity * intensity;
                    if (chan.enable_l) left += sample;
                    if (chan.enable_r) right += sample;
                }
                break; }
            case 2: {
                if (chan.on && chan.ext_enable && chan.dac_on) {
                    const i32 sample = chan.polarity;
                    if (chan.enable_l) left += sample;
                    if (chan.enable_r) right += sample;
                }
                break; }
            default: NOGOHERE;
        }
    }
    // Current range: 0...60

    // Apply volumes...
    left = (left * (1 + static_cast<i32>(io.ch03_vol_l))) >> (3 - io.ch03_vol);
    right = (right * (1 + static_cast<i32>(io.ch03_vol_r))) >> (3 - io.ch03_vol);
// New range 0...960, so 10 bits unsigned, 11 bits signed?

    psg.output_l = left;
    psg.output_r = right;
}

void core::cycle()
{
    // 2 MHz divider for PSG, /8 divider.

    // 64 CPU clocks per sample = 262kHz
    // and then divisor...
    tick_psg();

    // Further divide by more for samples...
    if (divider2.counter == 0) {
        fifo[0].output = fifo[0].sample;
        fifo[1].output = fifo[1].sample;
        sample_psg();
    }
    divider2.counter = (divider2.counter + 1) & divider2.mask;
}
}