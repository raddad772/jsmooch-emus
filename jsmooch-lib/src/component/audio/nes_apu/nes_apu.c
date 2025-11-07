//
// Created by . on 9/8/24.
//

#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "nes_apu.h"
#include "helpers_c/serialize/serialize.h"

#define PULSE0 0
#define PULSE1 1
#define TRIANGLE 2
#define NOISE 3

static void quarter_frame(struct NES_APU *this);
static void half_frame(struct NES_APU *this);


static const i32 duty_levels[4][8] = {
        { 0, 0, 0, 0, 0, 0, 0, 1 },
        { 0, 0, 0, 0, 0, 0, 1, 1 },
        { 0, 0, 0, 0, 1, 1, 1, 1 },
        { 1, 1, 1, 1, 1, 1, 0, 0 }
};

void NES_APU_init(struct NES_APU* this)
{
    memset(this, 0, sizeof(struct NES_APU));
    for (u32 i = 0; i < 4; i++) {
        this->channels[i].ext_enable = 1;
        this->channels[i].number = i;
    }
    this->ext_enable = 1;
    this->dmc.ext_enable = 1;
    this->channels[3].first_clock = 0x4000;
    this->frame_counter.step_mod = 4;
    this->clocks.every_other = 1;
}

void NES_APU_reset(struct NES_APU* this)
{
    //this->channels[0].
}

static i32 length_counter_load_values[0x20] = {
        10, 254, 20, 2, 40, 4, 80, 6,
        160, 8, 60, 10, 14, 12, 26, 14,
        12, 16, 24, 18, 48, 20, 96, 22,
        192, 24, 72, 26, 16, 28, 32, 30
};

static void update_IF(struct NES_APU* this) {
    u32 old_val = this->frame_counter.IF || this->dmc.IF;

    this->frame_counter.IF = this->frame_counter.new_IF;
    this->dmc.IF = this->dmc.new_IF;

    u32 new_val = this->frame_counter.IF || this->dmc.IF;

    if (old_val != new_val) {
        if (new_val) this->IRQ_pin.just_set = 1;
        if (this->IRQ_pin.func != NULL) this->IRQ_pin.func(this->IRQ_pin.ptr, new_val);
    }
}

static i32 get_pulse_channel_output(struct NES_APU* this, u32 pc, u32 is_debug)
{
    struct NESSNDCHAN *c = &this->channels[pc];
    if (c->sweep.overflow) return 0;
    if (c->length_counter.overflow) return 0;
    if (c->period.reload < 8) return 0;
    if (c->length_counter.enabled == 0) return 0;

    if (!is_debug && !c->ext_enable) return 0;

    return c->vol * c->duty.current;
}

static i32 get_noise_channel_output(struct NES_APU* this, u32 is_debug)
{
    struct NESSNDCHAN *c = &this->channels[NOISE];
    if (!is_debug && !c->ext_enable) return 0;
    if (c->length_counter.overflow) return 0;
    if (c->length_counter.enabled == 0) return 0;

    return c->vol * c->output;
}

static i32 get_triangle_channel_output(struct NES_APU* this, u32 is_debug)
{
    struct NESSNDCHAN *c = &this->channels[TRIANGLE];
    if (is_debug && !c->ext_enable) return 0;
    if (c->length_counter.enabled == 0) return 0;
    if (c->length_counter.overflow) return 0;
    if (c->period.reload < 2) return 0;
    if (c->linear_counter.count == 0) return 0;
    if (!c->ext_enable && !is_debug) return 0;

    return (float)c->output;
}

static const i32 noise_period_values[16] = {4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068};

static void set_length_counter(struct NES_APU* this, u32 pc, u32 val)
{
    struct NESSNDCHAN *c = &this->channels[pc];
    if (c->length_counter.enabled) c->length_counter.count = length_counter_load_values[val & 0x1F];
    c->length_counter.overflow = 0;
    c->length_counter.count_write_value = val;
}

static void write_length_counter_enable(struct NES_APU* this, u32 pc, u32 val)
{
    struct NESSNDCHAN *c = &this->channels[pc];
    c->length_counter.enabled = val & 1;
    if (!c->length_counter.enabled) {
        c->length_counter.count = 0;
        c->length_counter.count_write_value = 0;
    }
}

void NES_APU_write_IO(struct NES_APU* this, u32 addr, u8 val)
{
    u32 pc = (addr >> 2) & 3;
    struct NESSNDCHAN *c;
    switch ((addr & 0x1F) | 0x4000) {
        case 0x4000:
        case 0x4004:
        case 0x400C:
            c = &this->channels[pc];
            c->duty.kind = (val >> 6) & 3;
            c->env_loop_or_length_counter_halt = (val >> 5) & 1;
            c->constant_volume = (val >> 4) & 1;
            c->volume_or_envelope = val & 15;
            break;
        case 0x4001:
        case 0x4005:
            c = &this->channels[pc];
            c->sweep.enabled = (val >> 7) & 1;
            c->sweep.period.reload = (val >> 4) & 7;
            c->sweep.negate = (val >> 3) & 1;
            c->sweep.shift = val & 7;
            break;
        case 0x4002:
        case 0x4006:
            c = &this->channels[pc];
            c->period.reload = (c->period.reload & 0x700) | val;
            break;
        case 0x4003:
        case 0x4007: // trigger pulse chan 1
        case 0x400F:
            c = &this->channels[pc];
            c->period.count = 0;
            set_length_counter(this, pc, (val >> 3) & 0x1F);
            c->vol = c->volume_or_envelope;
            c->envelope.start_flag = 1;
            if (addr != 0x400F) { // pulse channel specific actions
                c->period.reload = (c->period.reload & 0xFF) | ((val & 7) << 8);
                c->duty.counter = 0;
                c->duty.current = duty_levels[c->duty.kind][c->duty.counter];
                c->sweep.overflow = 0; // sweep mute flag
                c->sweep.reload = 1; // sweep reload flag
                //printf("\nTRIGGER PULSE%d LENGTH:%d tb:%d PERIOD:%d HC:%d cyc:%lld", pc, c->length_counter.count, (val >> 3) & 0x1F, c->period.reload, c->env_loop_or_length_counter_halt, *this->master_cycles);
            }
            /*else {
                printf("\nTRIGGER NOISE LENGTH:%d PERIOD:%d ENVORLHALT:%d cyc:%lld", c->length_counter.count, c->period.reload, c->env_loop_or_length_counter_halt, *this->master_cycles);
            }*/
            break;
        case 0x4008: // triangle
            c = &this->channels[TRIANGLE];
            c->linear_counter.enabled = (val >> 7) & 1;
            c->env_loop_or_length_counter_halt = c->linear_counter.enabled;
            c->linear_counter.reload = val & 0x7F;
            break;
        case 0x400A: //
            c = &this->channels[TRIANGLE];
            c->period.reload = (c->period.reload & 0x700) | val;
            break;
        case 0x400B: // triangle
            c = &this->channels[TRIANGLE];
            set_length_counter(this, TRIANGLE, (val >> 3) & 0x1F);
            c->period.reload = (c->period.reload & 0xFF) | ((val & 7) << 8);
            c->linear_counter.reload_flag = 1;
            break;
        case 0x400E: // noise
            c = &this->channels[NOISE];
            c->mode = (val >> 7) & 1;
            c->period.reload = noise_period_values[val & 15];
            break;
        case 0x4015: // status/channel enables
            write_length_counter_enable(this, PULSE0, val & 1);
            write_length_counter_enable(this, PULSE1, (val >> 1) & 1);
            write_length_counter_enable(this, TRIANGLE, (val >> 2) & 1);
            write_length_counter_enable(this, NOISE, (val >> 3) & 1);
            this->dmc.enabled = (val >> 4) & 1;
            this->frame_counter.new_IF = 0;
            update_IF(this);
            break;
        case 0x4017: // frame counter
            this->frame_counter.mode = (val >> 7) & 1;
            if (this->frame_counter.mode) {
                quarter_frame(this);
                half_frame(this);
            }
            this->frame_counter.interrupt_inhibit = (val >> 6) & 1;
            if (this->frame_counter.interrupt_inhibit) {
                this->frame_counter.IF = 0;
                update_IF(this);
            }
            break;
    }
}

static inline u8 lc_halted(struct NES_APU* this, u32 pc)
{
    /*
N/T/2/1 will read as 1 if the corresponding length counter has not been halted through either expiring or a write of 0 to the corresponding bit. For the triangle channel, the status of the linear counter is irrelevant.     */
    struct NESSNDCHAN *c = &this->channels[pc];
    if (c->length_counter.overflow) return 0;
    if (!c->length_counter.enabled) return 0;
    return 1;
}

u8 NES_APU_read_IO(struct NES_APU* this, u32 addr, u8 old_val, u32 has_effect)
{
    u32 pc = (addr >> 2) & 3;
    struct NESSNDCHAN *c;
    u8 r = 0;
    switch ((addr & 0x1F) | 0x4000) {
        case 0x4000:
        case 0x4004:
        case 0x400C:
            c = &this->channels[pc];
            r |= (c->duty.kind << 6);
            r |= (c->env_loop_or_length_counter_halt << 5);
            r |= (c->constant_volume << 4);
            r |= c->volume_or_envelope;
            return r;
        case 0x4001:
        case 0x4005:
            c = &this->channels[pc];
            r |= (c->sweep.enabled << 7);
            r |= (c->sweep.period.reload << 4);
            r |= (c->sweep.negate << 3);
            r |= c->sweep.shift;
            return r;
        case 0x4002:
        case 0x4006: // pulse timer lo
            c = &this->channels[pc];
            return c->period.reload & 0xFF;
        case 0x4003:
        case 0x4007:
        case 0x400F:
            c = &this->channels[pc];
            r |= (c->length_counter.count_write_value <<  3);
            r |= (c->period.reload >> 8);
            return r;
        case 0x4015:
            // if it hasn't been halted
            r |= lc_halted(this, PULSE0);
            r |= lc_halted(this, PULSE1) << 1;
            r |= lc_halted(this, TRIANGLE) << 2;
            r |= lc_halted(this, NOISE) << 3;
            r |= (this->dmc.bytes_remaining > 0) << 4;
            r |= (this->frame_counter.IF << 6);
            r |= (old_val & 0x20); // bit 5 is open bus
            if (!this->IRQ_pin.just_set) {
                this->frame_counter.IF = 0;
                update_IF(this);
            }
            //printf("\nREAD 2015 val:%02x cyc:%lld", r, *this->master_cycles);
            return r;
            // If an interrupt flag was set at the same moment of the read, it will read back as 1 but it will not be cleared.
    }
    //printf("\nRead APU %04x", addr);
    return old_val;
}

static const i32 tri_values[32] = {15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15};

static void clock_env(struct NES_APU *this, u32 pc)
{
    struct NESSNDCHAN *c = &this->channels[pc];
    if (c->envelope.start_flag) {
        c->envelope.start_flag = 0;
        c->envelope.period_count = c->volume_or_envelope;
        c->envelope.decay_level = 15;
    }
    else {
        if (c->envelope.period_count == 0) {
            c->envelope.period_count = c->volume_or_envelope;
            if (c->envelope.decay_level != 0) {
                c->envelope.decay_level--;
            }
            else if (c->env_loop_or_length_counter_halt) {
                c->envelope.decay_level = 15;
            }
            c->envelope.decay_level = (c->envelope.decay_level - 1) & 15;
        }
        c->envelope.period_count = (c->envelope.period_count - 1);
    }
    if (c->constant_volume) c->vol = c->volume_or_envelope;
    else c->vol = c->envelope.decay_level;
}

static void clock_linear_counter(struct NES_APU *this)
{
    struct NESSNDCHAN *c = &this->channels[TRIANGLE];
    if (c->linear_counter.reload_flag) {
        c->linear_counter.count = c->linear_counter.reload;
    }
    else if (c->linear_counter.count > 0) {
        c->linear_counter.count--;
    }
    c->linear_counter.reload_flag &= c->linear_counter.enabled;
}

static void clock_envs_and_linear_counters(struct NES_APU *this)
{
    // when looping, for pulse 1&2, env will go 0->15 infintely
    clock_env(this, PULSE0);
    clock_env(this, PULSE1);
    clock_env(this, NOISE);

    // Do triangle linear counter
    clock_linear_counter(this);
}

static inline void clock_sweep(struct NES_APU* this, u32 pc)
{
    struct NESSNDCHAN *c = &this->channels[pc];
    i32 v = (c->period.reload >> c->sweep.shift);
    if (c->sweep.negate) v = -v;
    if (v < 0 && pc == 0) v--; // chan0 does - x - 1, chan1 does not
    v += c->period.reload; // target value now
    if (v < 0) v = 0; // clamp to 0. this will silence channel due to reload < 8 later
    if (v > 0x7FF) { // silence channel due to overflow
        v = 0x800;
        c->sweep.overflow = 1;
    }
    //if ((c->number == 0) && (c->sweep.enabled))
    //printf("\nCLOCK SWEEP C0. SHIFT: %d, PERIOD COUNT: %d, PERIOD RELOAD: %d", c->sweep.shift, c->sweep.period.counter, c->sweep.period.reload);
    if (c->sweep.enabled && (c->sweep.shift != 0) && (c->sweep.period.counter <= 0)) { // If the divider's counter is zero, the sweep is enabled, the shift count is nonzero,
        if (!c->sweep.overflow) { // and the sweep unit is not muting the channel: The pulse's period is set to the target period.
            //  the sweep unit is disabled including because the shift count is zero, the pulse channel's period is never updated
            if ((c->sweep.enabled) && (c->sweep.shift != 0)) {
                //if (c->number == 0) printf("\nSET NEW FREQ:%d chan:%d", v, c->number);
                c->period.reload = v;
            }
        }
    }
    if ((c->sweep.period.counter <= 0) || (c->sweep.reload)) { // If the divider's counter is zero or the reload flag is true
        // The divider counter is set to P and the reload flag is cleared.
        c->sweep.period.counter = c->sweep.period.reload;
        c->sweep.reload = 0;
    }
    else {
        c->sweep.period.counter--;
        //if ((c->number == 0) && c->sweep.enabled) printf("\nSWEEP PERIOD COUNTER %d", c->sweep.period.counter);
    }
}

static void clock_length_counters_and_sweeps(struct NES_APU *this)
{
    clock_sweep(this, 0);
    clock_sweep(this, 1);
    for (u32 i = 0; i < 4; i++) {
        struct NESSNDCHAN *c = &this->channels[i];
        if (c->length_counter.enabled && !c->env_loop_or_length_counter_halt && !c->length_counter.overflow) {
            if (c->length_counter.count <= 0) {
                c->length_counter.overflow = 1;
                c->length_counter.count = 0;
            }
            else {
                c->length_counter.count--;
            }
        }
    }
}

static void clock_irq(struct NES_APU *this)
{
    if (!this->frame_counter.interrupt_inhibit) {
        this->frame_counter.new_IF = 1;
        update_IF(this);
    }
}

static void quarter_frame(struct NES_APU *this)
{
    clock_envs_and_linear_counters(this);

}

static void half_frame(struct NES_APU *this)
{
    clock_length_counters_and_sweeps(this);
}

static void clock_frame_counter(struct NES_APU* this)
{
    if (this->frame_counter.mode == 0) {
        switch(this->frame_counter.step) {
            case 0:
                quarter_frame(this);
                break;
            case 1:
                quarter_frame(this);
                half_frame(this);
                break;
            case 2:
                quarter_frame(this);
                break;
            case 3:
                quarter_frame(this);
                half_frame(this);
                clock_irq(this);
                break;
        }
    }
    else {
        switch(this->frame_counter.step) {
            case 0:
                quarter_frame(this);
                break;
            case 1:
                quarter_frame(this);
                half_frame(this);
                break;
            case 2:
                quarter_frame(this);
                break;
            case 4:
                quarter_frame(this);
                half_frame(this);
                break;
        }
    }
}

static void clock_triangle(struct NES_APU* this)
{
    struct NESSNDCHAN *c = &this->channels[TRIANGLE];
    if (c->period.count >= (c->period.reload+1)) {
        c->period.count = 0;

        if ((c->linear_counter.count >= 0) && (c->length_counter.count >= 0)) {
            c->duty.counter = (c->duty.counter + 1) & 31;
            c->output = tri_values[c->duty.counter];
        }
    }
    else c->period.count++;
}

    static void clock_noise(struct NES_APU* this)
{
    struct NESSNDCHAN *c = &this->channels[NOISE];
    c->period.count++;
    if (c->period.count >= c->period.reload) {
        c->period.count = 0;

        u32 ob = (c->mode ? (c->duty.current >> 6) : (c->duty.current >> 1)) & 1;
        ob ^= (c->duty.current & 1);
        c->duty.current >>= 1;
        c->duty.current &= 0x3FFF;
        c->duty.current |= (ob << 14) | (c->first_clock);
        c->first_clock = 0;
        assert(c->duty.current != 0);
        c->output = c->duty.current & 1;
    }
}


static void clock_pulse(struct NES_APU* this, u32 pc)
{
    struct NESSNDCHAN *c = &this->channels[pc];
    c->period.count++;
    if (c->period.count >= c->period.reload) {
        c->period.count = 0;
        c->duty.counter = (c->duty.counter - 1) & 7;
        c->duty.current = duty_levels[c->duty.kind][c->duty.counter];
    }
}

void NES_APU_cycle(struct NES_APU* this) {
    clock_triangle(this);
    clock_noise(this);
    this->clocks.counter_1 ^= 1;
    if (this->clocks.counter_1 == 0) {
        clock_pulse(this, PULSE0);
        clock_pulse(this, PULSE1);
    }
    this->clocks.counter_240hz--;
    if (this->clocks.counter_240hz <= 0) {
        this->clocks.counter_240hz = 7457; //22335;// + this->clocks.every_other;
        //this->clocks.counter_240hz = 3728; //22335;// + this->clocks.every_other;
        this->frame_counter.step = (this->frame_counter.step + 1) % this->frame_counter.step_mod;
        clock_frame_counter(this);
    }
    this->IRQ_pin.just_set = 0;
}

static const float div15 = 1.0f / 15.0f;

float NES_APU_mix_sample(struct NES_APU* this, u32 is_debug)
{
    float output = 0.0f;
    output += (float)get_pulse_channel_output(this, PULSE0, is_debug) * div15 * 0.2f;
    output += (float)get_pulse_channel_output(this, PULSE1, is_debug) * div15 * 0.2f;
    output += (float)get_triangle_channel_output(this, is_debug) * div15 * 0.2f;
    output += (float)get_noise_channel_output(this, is_debug) * div15 * 0.2f;
    return output;
}

float NES_APU_sample_channel(struct NES_APU* this, int cnum)
{
    switch(cnum) {
        case 0:
        case 1:
            return (float)get_pulse_channel_output(this, cnum, 1) * div15;
        case 2:
            return (float)get_triangle_channel_output(this, 1) * div15;
        case 3:
            return (float)get_noise_channel_output(this, 1) * div15;
    }
    return 0.0f;
}

void NES_APU_serialize(struct NES_APU *this, struct serialized_state *state)
{
#define S(x) Sadd(state, &(this-> x), sizeof(this-> x))
    S(channels[0]);
    S(channels[1]);
    S(channels[2]);
    S(channels[3]);
    S(dmc);
    S(io);
    S(frame_counter);
    S(clocks);
    S(IRQ_pin.just_set);
#undef S
}

void NES_APU_deserialize(struct NES_APU *this, struct serialized_state *state)
{
#define L(x) Sload(state, &(this-> x), sizeof(this-> x))
    L(channels[0]);
    L(channels[1]);
    L(channels[2]);
    L(channels[3]);
    L(dmc);
    L(io);
    L(frame_counter);
    L(clocks);
    L(IRQ_pin.just_set);
#undef L
}
