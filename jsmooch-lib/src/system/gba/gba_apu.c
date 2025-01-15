//
// Created by . on 12/28/24.
//

#include "gba_bus.h"
#include "gba_apu.h"

void GBA_APU_init(struct GBA*this)
{
    GB_APU_init(&this->apu.old);
}

#define SOUNDBIAS 0x04000088
#define FIFO_A_L 0x040000A0
#define FIFO_A_H 0x040000A2
#define FIFO_B_L 0x040000A4
#define FIFO_B_H 0x040000A6
#define SOUNDCNT_L 0x04000080
#define SOUNDCNT_H 0x04000082
#define SOUNDCNT_X 0x04000084
#define SOUND3CNT_L 0x04000070
#define SOUND3CNT_H 0x04000072

u32 GBA_APU_read_IO(struct GBA*this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    u32 v;
    switch(addr) {
        case SOUNDBIAS+0:
            return this->apu.io.sound_bias & 0xFF;
        case SOUNDBIAS+1:
            return this->apu.io.sound_bias >> 8;
        case SOUND3CNT_L+0:
            v = (this->apu.chan[2].wave_ram_size == 64 ? 1 : 0) << 5;
            v |= this->apu.chan[2].wave_ram_bank << 6;
            v |= this->apu.chan[2].playback << 7;
            return v;
        case SOUND3CNT_L+1:
            return 0;
        case SOUND3CNT_H+0:
            return 0;
        case SOUND3CNT_H+1:
            v = this->apu.chan[2].snd_vol << 5;
            v |= this->apu.chan[2].force_vol << 7;
            return v;

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

void GBA_APU_sound_FIFO(struct GBA *this, u32 num) {
    struct GBA_APU_FIFO *fifo = &this->apu.fifo[num];

    // Move sound FIFO sample
    /*if ((fifo->output_head & 3) == 3) { // See if we can advance to next word
        if (fifo->len >= 4) {
            fifo->len -= 4;
            fifo->output_head = fifo->head;
            fifo->head = (fifo->head + 4) & 31;
        }
        else {
            printf("\nGBA FIFO UNDERFLOW!");
        }
    } else fifo->output_head++; // Advance within current word
    fifo->sample = SIGNe8to32(fifo->data[fifo->output_head]);*/

    //fifo->sample = SIGNe8to32(fifo->data[fifo->head]);
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

static void GBA_APU_write_IO8(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val)
{
    switch(addr) {
        case SOUND3CNT_L+0:
            this->apu.chan[2].wave_ram_size = (val >> 5) ? 64 : 32;
            this->apu.chan[2].wave_ram_bank = (val >> 6) & 1;
            this->apu.chan[2].playback = (val >> 7) & 1;
            return;
        case SOUND3CNT_L+1:
            return;
        case SOUND3CNT_H+0:
            this->apu.chan[2].sound_len = val;
            return;
        case SOUND3CNT_H+1:
            this->apu.chan[2].snd_vol = (val >> 5) & 3;
            this->apu.chan[2].force_vol = (val >> 7) & 1;
            return;
        case SOUNDBIAS+0:
            this->apu.io.sound_bias = (this->apu.io.sound_bias & 0xFF00) | val;
            return;
        case SOUNDBIAS+1:
            this->apu.io.sound_bias = (this->apu.io.sound_bias & 0xFF) | (val << 8);
            u32 v = (val >> 6) & 3;
            this->apu.io.bias_amplitude = v;
            switch(v) {
                case 0: this->apu.divider2.mask = 63; break; // /8 = 262khz. =16 = 128khz. =32 = 64khz. /64 = 32khz
                case 1: this->apu.divider2.mask = 31; break;
                case 2: this->apu.divider2.mask = 15; break;
                case 3: this->apu.divider2.mask = 7; break;
            }
            return;
        case SOUNDCNT_L+0:
            this->apu.io.ch03_vol_r = val & 7;
            this->apu.io.ch03_vol_l = (val >> 4) & 7;
            return;
        case SOUNDCNT_L+1:
            this->apu.chan[0].enable_r = (val >> 0) & 1;
            this->apu.chan[1].enable_r = (val >> 1) & 1;
            this->apu.chan[2].enable_r = (val >> 2) & 1;
            this->apu.chan[3].enable_r = (val >> 3) & 1;
            this->apu.chan[0].enable_l = (val >> 4) & 1;
            this->apu.chan[1].enable_l = (val >> 5) & 1;
            this->apu.chan[2].enable_l = (val >> 6) & 1;
            this->apu.chan[3].enable_l = (val >> 7) & 1;
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
    }
    printf("\nWARN UNDONE WRITE TO APU %08x", addr);
}


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

float GBA_APU_sample_channel(struct GBA *this, u32 n)
{
    if (n < 4) return 0.0f;
    struct GBA_APU_FIFO *fifo = &this->apu.fifo[n - 4];
    i32 sample = fifo->output << fifo->vol;

    switch(this->apu.io.bias_amplitude) {
        case 0: break;
        case 1: sample &= ~1; break;
        case 2: sample &= ~3; break;
        case 3: sample &= ~7; break;
    }

    /*if (sample < fifo->sample_min) {
        printf("\nNEW MIN! %d!", sample);
        fifo->sample_min = sample;
    }

    if (sample > fifo->sample_max) {
        printf("\nNEW MAX! %d!", sample);
        fifo->sample_max = sample;
    }*/

    return (float)(sample << 7) / 32768.0f;
    //return s;
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

void GBA_APU_cycle(struct GBA*this)
{
    // 2 MHz divider for PSG, /8 divider.

    // 64 CPU clocks per sample = 262kHz
    // and then divisor...
    if (this->apu.divider1.counter == 0) {
        // Tick PSG!

        // Further divide by more for samples...
        if (this->apu.divider2.counter == 0) {
            this->apu.fifo[0].output = this->apu.fifo[0].sample;
            this->apu.fifo[1].output = this->apu.fifo[1].sample;
        }
        this->apu.divider2.counter = (this->apu.divider2.counter + 1) & this->apu.divider2.mask;

    }
    this->apu.divider1.counter = (this->apu.divider1.counter + 1) & 7;
}
