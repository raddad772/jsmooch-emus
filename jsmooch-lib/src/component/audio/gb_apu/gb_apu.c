//
// Created by . on 9/6/24.
//

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "gb_apu.h"

static i32 PWR_WAVES[16] = { 0x84, 0x40, 0x43, 0xAA, 0x2D, 0x78, 0x92, 0x3C, 0x60, 0x59, 0x59, 0xB0, 0x34, 0xB8, 0x2E, 0xDA };

void GB_APU_init(struct GB_APU* this)
{
    memset(this, 0, sizeof(struct GB_APU));
    for (u32 i = 0; i < 4; i++) {
        this->channels[i].ext_enable = 1;
        this->channels[i].number = i;
    }
    for (u32 i = 0; i < 15; i++)
        this->channels[3].samples[i] = PWR_WAVES[i];
    this->ext_enable = 1;
}

static inline u8 read_NRx0(struct GBSNDCHAN *chan)
{
    u8 r = 0;
    r |= chan->sweep.pace << 4;
    r |= chan->sweep.direction << 3;
    r |= chan->sweep.individual_step;
    return r;
}

static inline u8 read_NRx1(struct GBSNDCHAN *chan)
{
    u8 r = 0;
    switch(chan->number) {
        case 0:
        case 1:
            r |= chan->wave_duty << 6;
            r |= (chan->length_counter ^ 0x3F);
            return r;
        case 2:
            return chan->length_counter ^ 0xFF;
    }
    assert(1==2);
}

static inline u8 read_NRx2(struct GBSNDCHAN *chan)
{
    u8 r = 0;
    switch(chan->number) {
        case 0:
        case 1:
            r |= chan->env.initial_vol << 4;
            r |= chan->env.direction << 3;
            r |= chan->env.period;
            return r;
        case 2:
            r |= chan->env.initial_vol << 5;
            return r;
    }
    assert(1==2);
}

static inline u8 read_NRx3(struct GBSNDCHAN* chan)
{
    if (chan->number == 3) {
        u8 r = 0;
        r |= chan->divisor;
        r |= (chan->short_mode << 3);
        r |= (chan->clock_shift << 4);
        return r;

    }
    return chan->period & 0xFF;
}

#define C0 &this->channels[0]
#define C1 &this->channels[1]
#define C2 &this->channels[2]
#define C3 &this->channels[3]
u8 GB_APU_read_IO(struct GB_APU* this, u32 addr, u8 old_val, u32 has_effect)
{
    u8 r = 0;
    switch(addr) {
        case 0xFF10: // ch0 sweep
            return read_NRx0(C0);
        case 0xFF11: // ch0 length timer & duty cycle
            return read_NRx1(C0);
        case 0xFF12: // ch0 volume and env
            return read_NRx2(C0);
        case 0xFF13:
            return read_NRx3(C0);
        case 0xFF16: // ch1 length timer & duty cycle
            return read_NRx1(C1);
        case 0xFF17: // ch1 volume & envelope
            return read_NRx2(C1);
        case 0xFF18:
            return read_NRx3(C1);
        case 0xFF1A:
            return this->channels[2].dac_on << 7;
        case 0xFF1B:
            return read_NRx1(C2);
        case 0xFF1C:
            return read_NRx2(C2);
        case 0xFF1D:
            return read_NRx3(C2);
        case 0xFF20:
            return read_NRx1(C3);
        case 0xFF21:
            return read_NRx2(C3);
        case 0xFF22:
            return read_NRx3(C3);
        case 0xFF25: // panning
            r |= this->channels[3].left << 7;
            r |= this->channels[2].left << 6;
            r |= this->channels[1].left << 5;
            r |= this->channels[0].left << 4;
            r |= this->channels[3].right << 3;
            r |= this->channels[2].right << 2;
            r |= this->channels[1].right << 1;
            r |= this->channels[0].right;
            return r;
        case 0xFF26: // NR52 audio master control
            r |= (this->io.master_on << 7);
            r |= this->channels[0].on;
            r |= (this->channels[1].on << 1);
            r |= (this->channels[2].on << 2);
            r |= (this->channels[3].on << 3);
            return r;
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

static u32 noise_periods[8] = { 8, 16, 32, 48, 64, 80, 96, 112};

static inline void trigger_channel(struct GBSNDCHAN* chan) {
    chan->on = chan->dac_on;
    chan->env.on = 1;
    chan->period_counter = 0;
    switch (chan->number) {
        case 2:
            chan->length_counter = 256;
            chan->duty_counter = 0;
            chan->vol = chan->env.initial_vol;
            chan->env.period_counter = 0;
            break;
        case 0:
        case 1:
            chan->length_counter = 64;
            chan->vol = chan->env.initial_vol;
            chan->env.period_counter = 0;
            if (chan->vol == 0) chan->on = 0;
            break;
        case 3:
            chan->period_counter = (i32) chan->period;
            chan->vol = chan->env.initial_vol;
            chan->duty_counter = 0x7FFF;
            if (chan->vol == 0) chan->on = 0;
            break;
    }
}

static inline void write_NRx0(struct GBSNDCHAN* chan, u8 val)
{
    chan->sweep.pace = (val >> 4) & 7;
    chan->sweep.direction = (val >> 3) & 1;
    chan->sweep.individual_step = val & 7;
}

static inline void write_NRx1(struct GBSNDCHAN* chan, u8 val)
{
    switch(chan->number) {
        case 0:
        case 1:
            chan->wave_duty = (val >> 6) & 3;
            [[fallthrough]];
        case 2:
        case 3: {
            u32 reload_value = chan->number == 3 ? 256 : 64;
            u32 mask = reload_value - 1;
            chan->length_counter = reload_value - (val & mask);
            return; }
            return;
    }
    assert(1==2);
}

static inline void write_NRx2(struct GBSNDCHAN* chan, u8 val)
{
    switch(chan->number) {
        case 0:
        case 1:
        case 3:
            chan->env.initial_vol = (val >> 4) & 15;
            chan->env.direction = (val >> 3) & 1;
            chan->env.period = val & 7;
            if ((val & 0xF8) == 0) {
                chan->dac_on = 0;
                chan->on = 0;
            }
            else {
                chan->dac_on = 1;
            }
            return;
        case 2:
            chan->env.initial_vol = (val >> 5) & 3;
            switch(chan->env.initial_vol) {
                case 0:
                    chan->env.rshift = 4;
                    break;
                case 1:
                    chan->env.rshift = 0;
                    break;
                case 2:
                    chan->env.rshift = 1;
                    break;
                case 3:
                    chan->env.rshift = 2;
                    break;
            }
            printf("\nCH2 VOL %d", chan->env.rshift);
            return;
    }
}

static inline void write_NRx3(struct GBSNDCHAN* chan, u8 val)
{
    switch(chan->number) {
        case 0:
        case 1:
            chan->period = (chan->period & 0x700) | val;
            return;
        case 2:
            chan->next_period = (chan->next_period & 0x700) | val;
            chan->period = chan->next_period;
            return;
        case 3:
            chan->divisor = val & 7;
            chan->short_mode = (val >> 3) & 1;
            chan->clock_shift = (val >> 4) & 15;
            chan->period = (noise_periods[chan->divisor] << chan->clock_shift) >> 2;
            return;
    }
    assert(1==2);
}

static inline void write_NRx4(struct GBSNDCHAN *chan, u8 val)
{
    switch(chan->number) {
        case 0:
        case 1:
            chan->period = (chan->period & 0xFF) | ((val & 7) << 8);
            break;
        case 2:
            chan->next_period = (chan->next_period & 0xFF) | ((val & 7) << 8);
            chan->period = chan->next_period;
            break;
        case 3:
            break;
    }
    chan->length_enable = (val >> 6) & 1;
    if (chan->length_enable) {
        printf("\nLEN ENABLE FOR CHAN %d", chan->number);
    }
    if (val & 0x80) {
        trigger_channel(chan);
    }
}

#define C0 &this->channels[0], val
#define C1 &this->channels[1], val
#define C2 &this->channels[2], val
#define C3 &this->channels[3], val
void GB_APU_write_IO(struct GB_APU* this, u32 addr, u8 val)
{
    switch(addr) {
        case 0xFF10: // CH0 sweep
            return write_NRx0(C0);
        case 0xFF11: // ch0 length timer & duty cycle
            return write_NRx1(C0);
        case 0xFF12: // ch0 volume and env
            return write_NRx2(C0);
        case 0xFF13: // ch0 period low
            return write_NRx3(C0);
        case 0xFF14: // ch0 trigger and such
            return write_NRx4(C0);
        case 0xFF16: // CH1 length timer & Duty cycle
            return write_NRx1(C1);
        case 0xFF17:
            return write_NRx2(C1);
        case 0xFF18:
            return write_NRx3(C1);
        case 0xFF19:
            return write_NRx4(C1);
        case 0xFF1A:
            this->channels[2].dac_on = (val >> 7) & 1;
            this->channels[2].on = this->channels[2].dac_on;
            return;
        case 0xFF1B:
            return write_NRx1(C2);
        case 0xFF1C:
            return write_NRx2(C2);
        case 0xFF1D:
            return write_NRx3(C2);
        case 0xFF1E:
            return write_NRx4(C2);
        case 0xFF20:
            return write_NRx1(C3);
        case 0xFF21:
            return write_NRx2(C3);
        case 0xFF22:
            return write_NRx3(C3);
        case 0xFF23:
            return write_NRx4(C3);
        case 0xFF25: // panning
            this->channels[3].left = (val >> 7) & 1;
            this->channels[2].left = (val >> 6) & 1;
            this->channels[1].left = (val >> 5) & 1;
            this->channels[0].left = (val >> 4) & 1;
            this->channels[3].right = (val >> 3) & 1;
            this->channels[2].right = (val >> 2) & 1;
            this->channels[1].right = (val >> 1) & 1;
            this->channels[0].right = (val >> 0) & 1;
            return;
        case 0xFF26:
            this->io.master_on = (val >> 7) & 1;
            return;
    }
    if ((addr >= 0xFF30) && (addr < 0xFF40)) {
        // upper 4 first
        this->channels[2].samples[addr & 15] = val;
    }
}


static void tick_sweep(struct GB_APU* this, u32 cnum)
{
    struct GBSNDCHAN *chan = &this->channels[cnum];
    if (chan->sweep.pace != 0) {
        chan->sweep.pace_counter--;
        if (chan->sweep.pace_counter <= 0) {
            chan->sweep.pace_counter = chan->sweep.pace;
            if (chan->sweep.direction == 0) {
                chan->period += chan->sweep.individual_step;
                if (chan->period == 2047) chan->on = 0;
            } else {
                chan->period -= chan->sweep.individual_step;
                if (chan->period == 0) chan->on = 0;
            }
            chan->period &= 0x7FF;
        }
    }

}

static void tick_length_timer(struct GB_APU *this, u32 cnum)
{
    struct GBSNDCHAN *chan = &this->channels[cnum];
    if (chan->length_enable) {
        if (chan->number == 3) printf("\nLENGTH! %d", chan->length_counter);
        chan->length_counter = chan->length_counter - 1;
        if (chan->length_counter <= 0) {
            chan->on = 0;
            if (chan->number < 2) chan->duty_counter = 0;
        }
    }
}

static i32 sq_duty[4][8] = {
        { 0, 0, 0, 0, 0, 0, 0, 1 }, // 00 - 12.5%
        { 1, 0, 0, 0, 0, 0, 0, 1 }, // 01 - 25%
        { 1, 0, 0, 0, 0, 1, 1, 1 }, // 10 - 50%
        { 0, 1, 1, 1, 1, 1, 1, 0 }, // 11 - 75%
};

static void tick_env(struct GB_APU *this, u32 cnum)
{
    struct GBSNDCHAN *chan = &this->channels[cnum];
    if (chan->env.period != 0 && chan->env.on) {
        chan->env.period_counter = (chan->env.period_counter - 1) & 7;
        if (chan->env.period_counter <= 0) {
            chan->env.period_counter = chan->env.period;
            if (chan->env.direction == 0) {
                if (chan->vol > 0) chan->vol--;
                else chan->env.on = 0;
            }
            else {
                if (chan->vol < 15) chan->vol++;
                else chan->env.on = 0;
            }
        }
    }
}

static void tick_wave_period_twice(struct GB_APU *this) {
    struct GBSNDCHAN *chan = &this->channels[2];
    for (u32 i = 0; i < 2; i++) {
        if (chan->on) {
            chan->period_counter++;
            if (chan->period_counter >= 0x7FF) {
                chan->period_counter = (i32) chan->period;
                chan->duty_counter = (chan->duty_counter + 1) & 31;
                //chan->period = chan->next_period;
                if ((chan->duty_counter & 1) == 0) {
                    u8 p = chan->samples[chan->duty_counter >> 1];
                    chan->sample_buffer[0] = p >> 4;
                    chan->sample_buffer[1] = p & 15;
                }
                chan->polarity = chan->sample_buffer[chan->duty_counter & 1] >> chan->env.rshift;
            }
        }
    }
}

static void tick_noise_period(struct GB_APU *this)
{
    struct GBSNDCHAN *chan = &this->channels[3];
    if (chan->on && (chan->clock_shift < 14) && (chan->period != 0)) {
        chan->period_counter--;
        if (chan->period_counter <= 0) {
            chan->period_counter = (i32)chan->period;
            u32 flipbits = ~(chan->short_mode ? 0x4040 : 0x4000);
            u32 lfsr = chan->duty_counter;
            u32 l2b = lfsr;
            lfsr >>= 1;
            l2b = (l2b ^ lfsr) & 1;
            lfsr &= flipbits;
            lfsr |= l2b * ~flipbits;
            chan->duty_counter = lfsr;
            chan->polarity = (chan->duty_counter & 1) ^ 1;
        }
    }
}

static void tick_pulse_period(struct GB_APU *this, int cnum)
{
    struct GBSNDCHAN *chan = &this->channels[cnum];
    if (chan->on) {
        chan->period_counter++;
        if (chan->period_counter >= 0x7FF) {
            chan->period_counter = (i32)chan->period;
            chan->duty_counter = (chan->duty_counter + 1) & 7;
            chan->polarity = sq_duty[chan->wave_duty][chan->duty_counter];
        }
    }
}

void GB_APU_cycle(struct GB_APU* this)
{
    tick_pulse_period(this, 0);
    tick_pulse_period(this, 1);
    tick_wave_period_twice(this);
    tick_noise_period(this);
    this->clocks.divider_2048--;
    if (this->clocks.divider_2048 <= 0) {
        this->clocks.divider_2048 = 2048;
        this->clocks.frame_sequencer = (this->clocks.frame_sequencer + 1) & 7;
        switch(this->clocks.frame_sequencer) {
            case 6:
            case 2:
                tick_sweep(this, 0);
                [[fallthrough]];
            case 0:
            case 4:
                tick_length_timer(this, 0);
                tick_length_timer(this, 1);
                tick_length_timer(this, 2);
                tick_length_timer(this, 3);
                break;
            case 7:
                tick_env(this, 0);
                tick_env(this, 1);
                tick_env(this, 3);
                break;
        }
    }
}

float GB_APU_mix_sample(struct GB_APU* this, u32 is_debug)
{
    float output = 0;
    struct GBSNDCHAN *chan;
    if (!is_debug && (!this->ext_enable || !this->io.master_on)) {
        return 0;
    }

    for (u32 i = 0; i < 4; i++) {
        chan = &this->channels[i];
        switch(i) {
            case 3:
            case 0:
            case 1: {
                if (chan->on && chan->ext_enable) {
                    float intensity = (1.0f/15.0f) * (float)chan->vol;
                    float sample = ((float) (((i32) chan->polarity * -2) + 1) * intensity);
                    assert(sample >= -1.0f);
                    assert(sample <= 1.0f);
                    output += sample * .25f;
                }
                break;
            }
            case 2: {
                if (chan->on && chan->ext_enable && chan->dac_on) {
                    float sample = ((((float)chan->polarity) / 15.0f) * -2.0f) + 1.0f;
                    output += sample * .25f;
                }
            }
        }
    }
    return output;
}

float GB_APU_sample_channel(struct GB_APU* this, int cnum) {
    struct GBSNDCHAN *chan = &this->channels[cnum];
    float output = 0;

    switch(cnum) {
        case 3:
        case 0:
        case 1: {
            if (chan->on) {
                float intensity = (1.0f / 15.0f) * (float)chan->vol;
                output = ((float)((chan->polarity * 2) - 1) * intensity);
            }
            break; }
        case 2: {
            output = ((((float)chan->polarity) / 15.0f) * -2.0f) + 1.0f;
            break; }
    }
    return output;
}
