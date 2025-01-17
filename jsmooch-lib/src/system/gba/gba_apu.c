//
// Created by . on 12/28/24.
//
#include <string.h>
#include <stdlib.h>

#include "helpers/int.h"
#include "gba_bus.h"
#include "gba_apu.h"


static i32 sq_duty[4][8] = {
        { 0, 0, 0, 0, 0, 0, 0, 1 }, // 00 - 12.5%
        { 1, 0, 0, 0, 0, 0, 0, 1 }, // 01 - 25%
        { 1, 0, 0, 0, 0, 1, 1, 1 }, // 10 - 50%
        { 0, 1, 1, 1, 1, 1, 1, 0 }, // 11 - 75%
};

void GBA_APU_init(struct GBA*this)
{
    memset(&this->apu, 0, sizeof(this->apu));
    for (u32 i = 0; i < 4; i++) {
        this->apu.channels[i].ext_enable = 1;
        this->apu.channels[i].number = i;
    }
    for (u32 i = 0; i < 32; i++)
        this->apu.channels[3].samples[i] = 0;
    this->apu.fifo[0].ext_enable = 1;
    this->apu.fifo[1].ext_enable = 1;
    this->apu.ext_enable = 1;
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

static inline u8 read_NRx0(struct GBASNDCHAN *chan)
{
    u8 r = 0;
    if (chan->number == 2) {
        r |= chan->sample_bank_mode << 5;
        r |= chan->cur_sample_bank << 6;
        r |= chan->dac_on << 7;
    }
    else {
        r |= chan->sweep.pace << 4;
        r |= chan->sweep.direction << 3;
        r |= chan->sweep.individual_step;
    }
    return r;
}

static inline u8 read_NRx1(struct GBASNDCHAN *chan)
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
    NOGOHERE;
}

static inline u8 read_NRx2(struct GBASNDCHAN *chan)
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
        case 3:
            r |= chan->env.initial_vol << 4;
            r |= chan->env.direction << 3;
            r |= chan->env.period;
            return r;
    }
    NOGOHERE;
}

static inline u8 read_NRx3(struct GBASNDCHAN* chan)
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

static inline u8 read_NRx4(struct GBASNDCHAN* chan)
{
    return chan->length_enable << 6;
}

static inline void trigger_channel(struct GBASNDCHAN* chan) {
    chan->on = chan->dac_on;
    chan->env.on = 1;
    chan->period_counter = 0;
    switch (chan->number) {
        case 2:
            chan->length_counter = 256;
            chan->duty_counter = 0;
            chan->sample_sample_bank = chan->sample_bank_mode ? 0 : chan->cur_sample_bank;
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

static inline void write_NRx0(struct GBASNDCHAN* chan, u8 val)
{
    if (chan->number == 2) {
        chan->sample_bank_mode = (val >> 5) & 1;
        chan->cur_sample_bank = (val >> 6) & 1;
        chan->dac_on = (val >> 7) & 1;
        chan->on = chan->dac_on;
    }
    else {
        chan->sweep.pace = (val >> 4) & 7;
        chan->sweep.direction = (val >> 3) & 1;
        chan->sweep.individual_step = val & 7;

    }
}

static inline void write_NRx1(struct GBASNDCHAN* chan, u8 val)
{
    switch(chan->number) {
        case 0:
        case 1:
            chan->wave_duty = (val >> 6) & 3;
                    __attribute__ ((fallthrough));;
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

static inline void write_NRx2(struct GBASNDCHAN* chan, u8 val)
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
            return;
    }
}

static u32 noise_periods[8] = { 8, 16, 32, 48, 64, 80, 96, 112};

static inline void write_NRx3(struct GBASNDCHAN* chan, u8 val)
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

static inline void write_NRx4(struct GBASNDCHAN *chan, u8 val)
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

    if (val & 0x80) {
        trigger_channel(chan);
    }
}

static void write_wave_ram(struct GBA *this, u32 addr, u32 val)
{
    this->apu.channels[2].samples[((this->apu.channels[2].cur_sample_bank ^ 1) << 4) | addr] = val;
}

static u32 read_wave_ram(struct GBA *this, u32 addr)
{
    return this->apu.channels[2].samples[((this->apu.channels[2].cur_sample_bank ^ 1) << 4) | addr];

}

#define C0 &this->apu.channels[0]
#define C1 &this->apu.channels[1]
#define C2 &this->apu.channels[2]
#define C3 &this->apu.channels[3]

u32 GBA_APU_read_IO(struct GBA*this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    u32 v;
    switch(addr) {
        case SOUNDBIAS+0:
            return this->apu.io.sound_bias & 0xFF;
        case SOUNDBIAS+1:
            return this->apu.io.sound_bias >> 8;
        case SOUNDCNT_X+0:
            v = this->apu.channels[0].on;
            v |= this->apu.channels[1].on << 1;
            v |= this->apu.channels[2].on << 2;
            v |= this->apu.channels[3].on << 3;
            v |= this->apu.io.master_enable << 7;
            return v;
        case SOUND1CNT_L+0:
            return read_NRx0(C0);
        case SOUND1CNT_H+0:
            return read_NRx1(C0);
        case SOUND1CNT_H+1:
            return read_NRx2(C0);
        case SOUND1CNT_X+0:
            return read_NRx3(C0);
        case SOUND1CNT_X+1:
            return read_NRx4(C0);
        case SOUND2CNT_L+0:
            return read_NRx1(C1);
        case SOUND2CNT_L+1:
            return read_NRx2(C1);
        case SOUND2CNT_H+0:
            return read_NRx3(C1);
        case SOUND2CNT_H+1:
            return read_NRx3(C1);
        case SOUND3CNT_L+0:
            return read_NRx0(C2);
        case SOUND3CNT_H+0:
            return read_NRx1(C2);
        case SOUND3CNT_H+1:
            return read_NRx2(C2);
        case SOUND3CNT_X+0:
            return read_NRx3(C2);
        case SOUND3CNT_X+1:
            return read_NRx4(C2);
        case SOUND4CNT_L+0:
            return read_NRx1(C3);
        case SOUND4CNT_L+1:
            return read_NRx2(C3);
        case SOUND4CNT_H+0:
            return read_NRx3(C3);
        case SOUND4CNT_H+1:
            return read_NRx4(C3);

        case SOUNDCNT_H+0:
            v = this->apu.io.ch03_vol;
            v |= this->apu.fifo[0].vol << 2;
            v |= this->apu.fifo[1].vol << 3;
            return v;
        case SOUNDCNT_H+1:
            v = this->apu.fifo[0].enable_r;
            v |= this->apu.fifo[0].enable_l << 1;
            v |= this->apu.fifo[0].timer_id << 2;
            v |= this->apu.fifo[1].enable_r << 4;
            v |= this->apu.fifo[1].enable_l << 5;
            v |= this->apu.fifo[1].timer_id << 6;
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
            return read_wave_ram(this, addr & 15);

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
            return GBA_open_bus_byte(this, addr);
    }
    printf("\nWARN UNDONE READ FROM APU %08x", addr);
    return 0;
}

#undef C0
#undef C1
#undef C2
#undef C3

void GBA_APU_sound_FIFO(struct GBA *this, u32 num) {
    struct GBA_APU_FIFO *fifo = &this->apu.fifo[num];

    if (fifo->len > 0) {
        fifo->len--;
        fifo->sample = (i32)fifo->data[fifo->head];
        fifo->head = (fifo->head + 1) & 31;
    }

    // If we need more data...
    if (fifo->len <= 16) {
        struct GBA_DMA_ch *c = &this->dma[1 + num];
        if ((c->io.enable) && (c->io.start_timing == 3)) {
            GBA_dma_start(c, 1 + num, 1);
        }
    }
}

static void FIFO_reset(struct GBA *this, u32 fn)
{
    struct GBA_APU_FIFO *f = &this->apu.fifo[fn];
    f->needs_commit = 0;
    f->head = f->tail;
    f->len = 0;
    f->output_head = 0;
}

static void FIFO_commit(struct GBA *this, u32 fn)
{
    struct GBA_APU_FIFO *f = &this->apu.fifo[fn];
    if (!f->needs_commit) return;
    f->needs_commit = 0;
    f->tail = (f->tail + 4) & 31;
    f->len += 4;
    if (f->len > 32) {
        FIFO_reset(this, fn);
    }
}

static void FIFO_write(struct GBA* this, u32 fn, u32 which, u32 v)
{
    struct GBA_APU_FIFO *f = &this->apu.fifo[fn];
    if (f->len == 32) f->head = (f->head + 1) & 31; // Full buffer, push another byte?...
    else f->len++;
    f->data[f->tail] = v;
    f->tail = (f->tail + 1) & 31;
}


#define C0 &this->apu.channels[0], val
#define C1 &this->apu.channels[1], val
#define C2 &this->apu.channels[2], val
#define C3 &this->apu.channels[3], val

static void GBA_APU_write_IO8(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val)
{
    switch(addr) {
        case SOUND1CNT_L+0:
            write_NRx0(C0);
            return;
        case SOUND1CNT_H+0:
            write_NRx1(C0);
            return;
        case SOUND1CNT_H+1:
            write_NRx2(C0);
            return;
        case SOUND1CNT_X+0:
            write_NRx3(C0);
            return;
        case SOUND1CNT_X+1:
            write_NRx4(C0);
            return;
        case SOUND2CNT_L+0:
            write_NRx1(C1);
            return;
        case SOUND2CNT_L+1:
            write_NRx2(C1);
            return;
        case SOUND2CNT_H+0:
            write_NRx3(C1);
            return;
        case SOUND2CNT_H+1:
            write_NRx4(C1);
            return;
        case SOUND3CNT_L+0:
            write_NRx0(C2);
            return;
        case SOUND3CNT_H+0:
            write_NRx1(C2);
            return;
        case SOUND3CNT_H+1:
            write_NRx2(C2);
            return;
        case SOUND3CNT_X+0:
            write_NRx3(C2);
            return;
        case SOUND3CNT_X+1:
            write_NRx4(C2);
            return;
        case SOUND4CNT_L+0:
            write_NRx1(C3);
            return;
        case SOUND4CNT_L+1:
            write_NRx2(C3);
            return;
        case SOUND4CNT_H+0:
            write_NRx3(C3);
            return;
        case SOUND4CNT_H+1:
            write_NRx4(C3);
            return;
        case SOUNDBIAS+0:
            this->apu.io.sound_bias = (this->apu.io.sound_bias & 0xFF00) | val;
            return;
        case SOUNDBIAS+1:
            this->apu.io.sound_bias = (this->apu.io.sound_bias & 0xFF) | (val << 8);
            u32 v = (val >> 6) & 3;
            this->apu.io.bias_amplitude = v;
            switch(v) {
                case 0: this->apu.divider2.mask = 31; break; // /8 = 262khz. =16 = 128khz. =32 = 64khz. /64 = 32khz
                case 1: this->apu.divider2.mask = 15; break;
                case 2: this->apu.divider2.mask = 7; break;
                case 3: this->apu.divider2.mask = 3; break;
            }
            return;
        case SOUNDCNT_L+0:
            this->apu.io.ch03_vol_r = val & 7;
            this->apu.io.ch03_vol_l = (val >> 4) & 7;
            return;
        case SOUNDCNT_L+1:
            this->apu.channels[0].enable_r = (val >> 0) & 1;
            this->apu.channels[1].enable_r = (val >> 1) & 1;
            this->apu.channels[2].enable_r = (val >> 2) & 1;
            this->apu.channels[3].enable_r = (val >> 3) & 1;
            this->apu.channels[0].enable_l = (val >> 4) & 1;
            this->apu.channels[1].enable_l = (val >> 5) & 1;
            this->apu.channels[2].enable_l = (val >> 6) & 1;
            this->apu.channels[3].enable_l = (val >> 7) & 1;
            return;
        case SOUNDCNT_H+0:
            this->apu.io.ch03_vol = val & 3;
            this->apu.fifo[0].vol = (val >> 2) & 1;
            this->apu.fifo[1].vol = (val >> 3) & 1;
            return;
        case SOUNDCNT_H+1:
            this->apu.fifo[0].enable_l = val & 1;
            this->apu.fifo[0].enable_r = (val >> 1) & 1;
            this->apu.fifo[0].timer_id = (val >> 2) & 1;
            if ((val >> 3) & 1) FIFO_reset(this, 0);
            this->apu.fifo[1].enable_l = (val >> 4) & 1;
            this->apu.fifo[1].enable_r = (val >> 5) & 1;
            this->apu.fifo[1].timer_id = (val >> 6) & 1;
            if ((val >> 7) & 1) FIFO_reset(this, 0);
            return;
        case SOUNDCNT_X+0:
            this->apu.io.master_enable = (val >> 7) & 1;
            return;
        case SOUNDCNT_X+1:
            return;
        case FIFO_A_L+0: FIFO_write(this, 0, 0, val); return;
        case FIFO_A_L+1: FIFO_write(this, 0, 1, val); return;
        case FIFO_A_H+0: FIFO_write(this, 0, 2, val); return;
        case FIFO_A_H+1: FIFO_write(this, 0, 3, val); return;
        case FIFO_B_L+0: FIFO_write(this, 1, 0, val); return;
        case FIFO_B_L+1: FIFO_write(this, 1, 1, val); return;
        case FIFO_B_H+0: FIFO_write(this, 1, 2, val); return;
        case FIFO_B_H+1: FIFO_write(this, 1, 3, val); return;

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
            write_wave_ram(this, addr & 15, val);
            return;
    }
    printf("\nWARN UNDONE WRITE TO APU %08x", addr);
}
#undef C0
#undef C1
#undef C2
#undef C3

void GBA_APU_write_IO(struct GBA*this, u32 addr, u32 sz, u32 access, u32 val) {
    GBA_APU_write_IO8(this, addr, 1, access, val & 0xFF);
    if (sz >= 2) {
        GBA_APU_write_IO8(this, addr + 1, 1, access, (val >> 8) & 0xFF);
    }
    if (sz == 4) {
        GBA_APU_write_IO8(this, addr + 2, 1, access, (val >> 16) & 0xFF);
        GBA_APU_write_IO8(this, addr + 3, 1, access, (val >> 24) & 0xFF);
    }
    if (this->apu.fifo[0].needs_commit) FIFO_commit(this, 0);
    if (this->apu.fifo[1].needs_commit) FIFO_commit(this, 1);
}

float GB_APU_sample_channel_for_GBA(struct GBA_APU* this, int cnum) {
    struct GBASNDCHAN *chan = &this->channels[cnum];
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

float GBA_APU_sample_channel(struct GBA *this, u32 n)
{
    if (n < 4) return GB_APU_sample_channel_for_GBA(&this->apu, n);
    struct GBA_APU_FIFO *fifo = &this->apu.fifo[n - 4];
    i32 sample = fifo->output << fifo->vol;

    switch(this->apu.io.bias_amplitude) {
        case 0: break;
        case 1: sample &= ~1; break;
        case 2: sample &= ~3; break;
        case 3: sample &= ~7; break;
    }

    return (float)(sample << 7) / 32768.0f;
}

float GBA_APU_mix_sample(struct GBA*this, u32 is_debug)
{
    float s = 0.0f;
    if (this->apu.io.master_enable && (this->apu.ext_enable || is_debug)) {
        i32 left = 0, right = 0;
        for (u32 i = 0; i < 2; i++) {
            struct GBA_APU_FIFO *fifo = &this->apu.fifo[i];
            if (!fifo->ext_enable && !is_debug) continue;
            if (fifo->enable_l) {
                left += fifo->output << fifo->vol; // 8 to 9 bits
            }
            if (fifo->enable_r) {
                right += fifo->output << fifo->vol; // 8 to 9 bits
            }
        }
        left += this->apu.psg.output_l;
        right += this->apu.psg.output_r;
        // up to 11 bits possible now. Clamp!
        if (left < -1024) left = -1024;
        if (left > 1023) left = 1023;
        if (right < -1024) right = -1024;
        if (right > 1023) right = 1023;

        // Get rid of bits...
        switch(this->apu.io.bias_amplitude) {
            case 0: break;
            case 1: left &= ~1; right &= ~1; break;
            case 2: left &= ~3; right &= ~3; break;
            case 3: left &= ~7; right &= ~7; break;
        }

        i32 center = left + right;
        // 11 bits possible now! Because we're mixing left and right!
        // Now convert to float
        //s = (((float)(center + 1024)) / 2047.0f) - 1.0f;
        s = (float)(center << 5) / 32768.0f;
        //s = (((float)center) / 1024.0f);
        assert(s <= 1.0f);
        assert(s >= -1.0f);
    }
    return s;
}

static void tick_pulse_period(struct GBA_APU *this, int cnum)
{
    struct GBASNDCHAN *chan = &this->channels[cnum];
    if (chan->on) {
        chan->period_counter++;
        if (chan->period_counter >= 0x7FF) {
            chan->period_counter = (i32)chan->period;
            chan->duty_counter = (chan->duty_counter + 1) & 7;
            chan->polarity = sq_duty[chan->wave_duty][chan->duty_counter];
        }
    }
}

static void tick_wave_period_twice(struct GBA_APU *this) {
    struct GBASNDCHAN *chan = &this->channels[2];
    for (u32 i = 0; i < 2; i++) {
        if (chan->on) {
            chan->period_counter++;
            if (chan->period_counter >= 0x7FF) {
                chan->period_counter = chan->period;
                if ((chan->duty_counter & 1) == 0) {
                    chan->sample_sample_bank ^= chan->sample_bank_mode; // don't flip if mode = 0
                    u8 p = chan->samples[(chan->sample_sample_bank << 4) | (chan->duty_counter >> 1)];
                    chan->sample_buffer[0] = p >> 4;
                    chan->sample_buffer[1] = p & 15;
                }
                chan->duty_counter = (chan->duty_counter + 1) & 31;
                chan->polarity = chan->sample_buffer[chan->duty_counter & 1] >> chan->env.rshift;
            }
        }
    }
}

static void tick_noise_period(struct GBA_APU *this)
{
    struct GBASNDCHAN *chan = &this->channels[3];
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

static void tick_sweep(struct GBA_APU* this, u32 cnum)
{
    struct GBASNDCHAN *chan = &this->channels[cnum];
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

static void tick_length_timer(struct GBA_APU *this, u32 cnum)
{
    struct GBASNDCHAN *chan = &this->channels[cnum];
    if (chan->length_enable) {
        chan->length_counter = chan->length_counter - 1;
        if (chan->length_counter <= 0) {
            chan->on = 0;
            if (chan->number < 2) chan->duty_counter = 0;
        }
    }
}

static void tick_env(struct GBA_APU *this, u32 cnum)
{
    struct GBASNDCHAN *chan = &this->channels[cnum];
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

static void tick_psg(struct GBA* this)
{
    // @1MHz
    tick_pulse_period(&this->apu, 0);
    tick_pulse_period(&this->apu, 1);
    tick_wave_period_twice(&this->apu);
    tick_noise_period(&this->apu);
    if (this->apu.clocks.divider_2048 == 0) {
        this->apu.clocks.frame_sequencer = (this->apu.clocks.frame_sequencer + 1) & 7;
        switch(this->apu.clocks.frame_sequencer) {
            case 6:
            case 2:
                tick_sweep(&this->apu, 0);
                        __attribute__ ((fallthrough));;
            case 0:
            case 4:
                tick_length_timer(&this->apu, 0);
                tick_length_timer(&this->apu, 1);
                tick_length_timer(&this->apu, 2);
                tick_length_timer(&this->apu, 3);
                break;
            case 7:
                tick_env(&this->apu, 0);
                tick_env(&this->apu, 1);
                tick_env(&this->apu, 3);
                break;
        }
    }
    this->apu.clocks.divider_2048 = (this->apu.clocks.divider_2048 + 1) & 2047;

}

static void sample_psg(struct GBA* this)
{
    // So we want a 9-bit signed output?

    i32 left = 0, right = 0;
    struct GBASNDCHAN *chan;
    if (!(this->apu.ext_enable && this->apu.io.master_enable)) {
        this->apu.psg.output_l = 0;
        this->apu.psg.output_r = 0;
        return;
    }

    for (u32 i = 0; i < 4; i++) {
        chan = &this->apu.channels[i];
        switch(i) {
            case 3:
            case 0:
            case 1: {
                if (chan->on && chan->ext_enable) {
                    i32 intensity = chan->vol;
                    i32 sample = chan->polarity * intensity;
                    if (chan->enable_l) left += sample;
                    if (chan->enable_r) right += sample;
                }
                break;
            }
            case 2: {
                if (chan->on && chan->ext_enable && chan->dac_on) {
                    i32 sample = chan->polarity;
                    if (chan->enable_l) left += sample;
                    if (chan->enable_r) right += sample;
                }
            }
        }
    }
    // Current range: 0...60

    // Apply volumes...
    left = ((left * (1 + (i32)this->apu.io.ch03_vol_l)) << 1) >> (3 - this->apu.io.ch03_vol);
    right = ((right * (1 + (i32)this->apu.io.ch03_vol_r)) << 1) >> (3 - this->apu.io.ch03_vol);
// New range 0...960, so 10 bits unsigned, 11 bits signed?

    this->apu.psg.output_l = left;
    this->apu.psg.output_r = right;
}

void GBA_APU_cycle(struct GBA*this)
{
    // 2 MHz divider for PSG, /8 divider.

    // 64 CPU clocks per sample = 262kHz
    // and then divisor...
    if (this->apu.clocks.divider_16 == 0) { // /16=1Mhz
        // Tick PSG!
        tick_psg(this);

        // Further divide by more for samples...
        if (this->apu.divider2.counter == 0) {
            this->apu.fifo[0].output = this->apu.fifo[0].sample;
            this->apu.fifo[1].output = this->apu.fifo[1].sample;
            sample_psg(this);
        }
        this->apu.divider2.counter = (this->apu.divider2.counter + 1) & this->apu.divider2.mask;
    }
    this->apu.clocks.divider_16 = (this->apu.clocks.divider_16 + 1) & 15;
}
