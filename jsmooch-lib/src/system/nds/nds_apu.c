//
// Created by . on 3/27/25.
//

#include <math.h>
#include <string.h>
#include "nds_bus.h"
#include "nds_apu.h"
#include "nds_regs.h"

typedef long double mf;


static i32 adpcm_index_tbl[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

static i16 adpcm_tbl[89] = {
0x0007, 0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E,
0x0010, 0x0011, 0x0013, 0x0015, 0x0017, 0x0019, 0x001C, 0x001F,
0x0022, 0x0025, 0x0029, 0x002D, 0x0032, 0x0037, 0x003C, 0x0042,
0x0049, 0x0050, 0x0058, 0x0061, 0x006B, 0x0076, 0x0082, 0x008F,
0x009D, 0x00AD, 0x00BE, 0x00D1, 0x00E6, 0x00FD, 0x0117, 0x0133,
0x0151, 0x0173, 0x0198, 0x01C1, 0x01EE, 0x0220, 0x0256, 0x0292,
0x02D4, 0x031C, 0x036C, 0x03C3, 0x0424, 0x048E, 0x0502, 0x0583,
0x0610, 0x06AB, 0x0756, 0x0812, 0x08E0, 0x09C3, 0x0ABD, 0x0BD0,
0x0CFF, 0x0E4C, 0x0FBA, 0x114C, 0x1307, 0x14EE, 0x1706, 0x1954,
0x1BDC, 0x1EA5, 0x21B6, 0x2515, 0x28CA, 0x2CDF, 0x315B, 0x364B,
0x3BB9, 0x41B2, 0x4843, 0x4F7E, 0x5771, 0x602F, 0x69CE, 0x7462,
0x7FFF };

static void buffer_pcm8(struct NDS *this, struct NDS_APU_CH *ch, u32 num)
{
    static int a = 1;
    if (a) {
        printf("\nWARN IMPLEMENT PCM8");
        a = 0;
    }

}

static void buffer_pcm16(struct NDS *this, struct NDS_APU_CH *ch, u32 num)
{
    // Since we move in 32-bit words, we move 2 samples at a time for PCM16.
    // We must read at least that many samples.
    i32 match_num = num;
    if (num & 1) num++;
    num >>= 1;
    for (u32 i = 0; i < num; i++) {
        u32 word = NDS_mainbus_read7(this, ch->status.addr, 4, 0, 0);
        ch->status.addr += 4;
        for (u32 j = 0; j < 2; j++) {
            match_num--;
            if (match_num < 0) {
                ch->status.sample_input_buffer.samples[ch->status.sample_input_buffer.tail] = word & 0xFFFF;
                ch->status.sample_input_buffer.tail = (ch->status.sample_input_buffer.tail + 1) & 15;
                ch->status.sample_input_buffer.len++;
            }
            else {
                ch->status.sample = word & 0xFFFF;
            }
            word >>= 16;
        }
    }
}

static void buffer_adpcm(struct NDS *this, struct NDS_APU_CH *ch, u32 num)
{
    static int a = 1;
    if (a) {
        printf("\nWARN IMPLEMENT ADPCM");
        a = 0;
    }

}

static void buffer_psg(struct NDS *this, struct NDS_APU_CH *ch, u32 num)
{
    if (!ch->has_psg) return;
    static int a = 1;
    if (a) {
        printf("\nWARN IMPLEMENT PSG");
        a = 0;
    }
}

static void pop_samples(struct NDS *this, struct NDS_APU_CH *ch, u32 num)
{
    if (num < 1) {
        printf("\nPOP 0 SAMPLES!?");
        return;
    }

    if (ch->status.sample_input_buffer.len > 0) {
        if (num >= ch->status.sample_input_buffer.len) { // case: we need as many or more samples than are in buffer
            // empty out that buffer!
            ch->status.sample = ch->status.sample_input_buffer.samples[ch->status.sample_input_buffer.tail];
            num -= ch->status.sample_input_buffer.len;
            ch->status.sample_input_buffer.len = ch->status.sample_input_buffer.head = ch->status.sample_input_buffer.tail = 0;
            if (num == 0) return;
        }
        else { // case: we need less samples than are in buffer
            // Empty out part of buffer and return!
            ch->status.sample_input_buffer.len -= num;
            ch->status.sample_input_buffer.head = (ch->status.sample_input_buffer.head + num) & 15;
            u32 nm1 = (ch->status.sample_input_buffer.head - 1) & 15;
            ch->status.sample = ch->status.sample_input_buffer.samples[nm1];
            return;
        }
    }

    switch(ch->latched.format) {
        case NDS_APU_FMT_pcm8:
            buffer_pcm8(this, ch, num);
            break;
        case NDS_APU_FMT_pcm16:
            buffer_pcm16(this, ch, num);
            break;
        case NDS_APU_FMT_ima_adpcm:
            buffer_adpcm(this, ch, num);
            break;
        case NDS_APU_FMT_psg:
            buffer_psg(this, ch, num);
            break;
    }
}

static void do_run_to_current(struct NDS *this, struct NDS_APU_CH *ch)
{
    if (!ch->status.playing) return;
    if (this->apu.buffer.status.total_samples < ch->status.next_timecode) return;

    while(ch->status.my_total_samples < this->apu.buffer.status.total_samples) {
        ch->status.sampling_counter += ch->status.freq;
        if (ch->status.sampling_counter >= 32768) {
            u64 num_to_pop = ch->status.sampling_counter / 32768;
            ch->status.sampling_counter %= 32768;
            pop_samples(this, ch, num_to_pop);
        }
        this->apu.buffer.samples[ch->status.mix_buffer_tail] += ch->status.sample;
        ch->status.mix_buffer_tail = (ch->status.mix_buffer_tail + 1) & (NDS_APU_MAX_SAMPLES - 1);
        ch->status.my_total_samples++;
    }

    ch->status.last_timecode = this->apu.buffer.status.total_samples;
    u64 num_to_wait = (32768 - ch->status.sampling_counter) / ch->status.freq;
    ch->status.next_timecode = ch->status.last_timecode + (num_to_wait ? num_to_wait : 1);
}

static void run_master_to_current(struct NDS *this, u64 cur_clock)
{
    if (cur_clock < this->apu.buffer.status.next_timecode) {
        return;
    }

    u64 num_clocks = cur_clock - this->apu.buffer.status.last_timecode;
    this->apu.buffer.status.last_timecode = cur_clock;

    this->apu.buffer.status.sampling_counter += num_clocks * 32768;
    u64 num_samples_to_play = this->apu.buffer.status.sampling_counter / this->clock.timing.arm7.hz;
    this->apu.buffer.status.sampling_counter -= this->clock.timing.arm7.hz * num_samples_to_play;
    this->apu.buffer.status.next_timecode = cur_clock + this->apu.buffer.status.sampling_counter;

    // Determine number of samples we need to catch up!
    if (num_samples_to_play < 1) {
        printf("\nWHAT UNDERRUN HERE WHY!?");
        return;
    }

    this->apu.buffer.status.total_samples += num_samples_to_play;
    this->apu.buffer.len += num_samples_to_play;

    for (u64 i = 0; i < num_samples_to_play; i++) {
        if (this->apu.buffer.len >= NDS_APU_MAX_SAMPLES) {
            printf("\nCRAP HERE YO!");
            return;
        }
        this->apu.buffer.samples[this->apu.buffer.tail] = 0;
        this->apu.buffer.tail = (this->apu.buffer.tail + 1) & (NDS_APU_MAX_SAMPLES - 1);
    }
}

static void run_to_current(struct NDS *this, struct NDS_APU_CH *ch, u64 cur_clock)
{
    run_master_to_current(this, cur_clock);
    if (ch) {
        do_run_to_current(this, ch);
    }
    else {
        for (u32 i = 0; i < 16; i++) {
            ch = &this->apu.ch[i];
            do_run_to_current(this, ch);
        }
    }
}

void NDS_APU_run_to_current(struct NDS *this)
{
    run_to_current(this, NULL, NDS_clock_current7(this));
}

static u32 apu_read8(struct NDS *this, u32 addr)
{
    u32 v;
    u32 chn;
    struct NDS_APU_CH *ch = NULL;
    if (addr < 0x04000500) {
        chn = (addr >> 4) & 15;
        ch = &this->apu.ch[chn];
        addr &= 0xFFFFFF0F;
    }
    switch(addr) {
        case R7_SOUNDxCNT+0:
            return ch->io.vol;
        case R7_SOUNDxCNT+1:
            v = ch->io.vol_div;
            v |= ch->io.hold << 7;
            return v;
        case R7_SOUNDxCNT+2:
            return ch->io.pan;
        case R7_SOUNDxCNT+3:
            v = ch->io.wave_duty;
            v |= ch->io.repeat_mode << 3;
            v |= ch->io.format << 5;
            v |= ch->status.playing << 7;
            return v;
        case R7_SOUNDCNT+0:
            return this->apu.io.master_vol;
        case R7_SOUNDCNT+1:
            v = this->apu.io.output_from_left;
            v |= this->apu.io.output_from_right << 2;
            v |= this->apu.io.output_ch1_to_mixer << 4;
            v |= this->apu.io.output_ch3_to_mixer << 5;
            v |= this->apu.io.master_vol << 7;
            return v;

        case R7_SOUNDBIAS+0:
            return this->apu.io.sound_bias & 0xFF;
        case R7_SOUNDBIAS+1:
            return (this->apu.io.sound_bias >> 8) & 3;

        case R7_SOUNDCNT+2:
        case R7_SOUNDCNT+3:
        case R7_SOUNDBIAS+2:
        case R7_SOUNDBIAS+3:
            return 0;

        case R7_SOUNDCAP0CNT:
        case R7_SOUNDCAP1CNT:
            return 0;
    }
    if (addr > 0x04000508) {
        static int a = 1;
        if (a) {
            printf("\nWARN: SND CAP READ!");
            a = 0;
        }
        return 0;
    }

    printf("\nUnhandled APU read %08x", addr);
    return 0;
}

static inline void latch_io_ch(struct NDS *this, struct NDS_APU_CH *ch)
{
    memcpy(&ch->latched, &ch->io, sizeof(struct NDS_APU_CH_params));
    ch->status.freq = this->clock.timing.arm7.hz_2 / ch->latched.period;
    if (ch->latched.period < 4) {
        ch->status.playing = 0;
        return;
    }
}

static void latch_io(struct NDS *this, struct NDS_APU_CH *ch) {
    if (!ch) {
        memcpy(&this->apu.latched, &this->apu.io, sizeof(struct NDS_APU_params));
        for (u32 i = 0; i < 15; i++) {
            ch = &this->apu.ch[i];
            latch_io_ch(this, ch);
        }
    } else
        latch_io_ch(this, ch);
}

static void calculate_ch(struct NDS *this, struct NDS_APU_CH *ch, u64 cur_clock)
{
    // Empty, off channel.
    if (!ch->latched.status) return;

    // Check if we WERE off but are now on
    u32 trigger = !ch->status.playing;


    if (trigger) {
        // trigger channel.
        printf("\nTRIGGER CH %d", ch->num);
        ch->status.playing = 1;
        // clear input data FIFO, read first ADPM header, setup PSG state, etc.
        ch->status.sample_input_buffer.head = ch->status.sample_input_buffer.tail = ch->status.sample_input_buffer.len = 0;
        ch->status.pos = 0;
        ch->status.addr = ch->latched.source_addr;
        switch(ch->latched.format ) {
            case NDS_APU_FMT_pcm8:
            case NDS_APU_FMT_pcm16:
                break;
            case NDS_APU_FMT_ima_adpcm:
                // TODO: this
                break;
            case NDS_APU_FMT_psg:
                // TODO: this
                break;
            default:
                break;
        }
        ch->status.sampling_counter = 0;
        ch->status.last_timecode = this->apu.buffer.status.total_samples;

        ch->status.next_timecode = ch->status.last_timecode + (32768 / ch->status.freq);
    }
}

static void calculate_ch_settings_based_on_current_values(struct NDS *this, struct NDS_APU_CH *ch, u64 cur_clock)
{
    if (!ch) {
        for (u32 i = 0; i < 16; i++) {
            ch = &this->apu.ch[i];
            calculate_ch(this, ch, cur_clock);
        }
    }
    else calculate_ch(this, ch, cur_clock);
}

static void change_params(struct NDS *this, struct NDS_APU_CH *ch)
{
    u64 cur_clock = NDS_clock_current7(this);
    run_to_current(this, ch, cur_clock);
    latch_io(this, ch);
    calculate_ch_settings_based_on_current_values(this, ch, cur_clock);
}

static void write_sndcnt_hi(struct NDS *this, struct NDS_APU_CH *ch, u32 val)
{
    // TODO: this. mostly for 0->1 edge of enable
    // if !old and new: calculate_ch_settngs_based_on_current_values();
    // else if old && new: change_params(this, ch);
    // else if old && !new: sample_off(this, ch) which will set output hold
    change_params(this, ch);
}

static void apu_write8(struct NDS *this, u32 addr, u32 val)
{
    u32 chn;
    struct NDS_APU_CH *ch = NULL;
    if (addr < 0x04000500) {
        chn = (addr >> 4) & 15;
        ch = &this->apu.ch[chn];
        addr &= 0xFFFFFF0F;
    }
    switch(addr) {
        case R7_SOUNDCNT+0:
            this->apu.io.master_vol = val & 0x7F;
            change_params(this, NULL);
            return;

        case R7_SOUNDCNT+1:
            this->apu.io.output_from_left = val & 3;
            this->apu.io.output_from_right = (val >> 2) & 3;
            this->apu.io.output_ch1_to_mixer = (val >> 4) & 1;
            this->apu.io.output_ch3_to_mixer = (val >> 5) & 1;
            this->apu.io.master_enable = (val >> 7) & 1;
            change_params(this, NULL);
            return;
        case R7_SOUNDxCNT+0:
            ch->io.vol = val & 0x7F;
            change_params(this, ch);
            return;
        case R7_SOUNDxCNT+1: {
            static const u32 sval[4] = {0, 1, 2, 4};
            ch->io.vol_div = val & 3;
            ch->io.vol_rshift = sval[ch->io.vol_div];
            ch->io.hold = (val >> 7) & 1;
            change_params(this, ch);
            return; }
        case R7_SOUNDxCNT+2: {
            val &= 0x7F;
            ch->io.pan = val;
            if (val == 64) {
                ch->io.left_pan = ch->io.right_pan = 63;
            }
            else {
                if (val < 64) {
                    ch->io.left_pan = 63;
                    ch->io.right_pan = val;
                }
                else {
                    ch->io.right_pan = 63;
                    ch->io.left_pan = 127 - val;
                }
            }
            change_params(this, ch);
            return; }

        case R7_SOUNDxCNT+3: {
            ch->io.wave_duty = val & 7;
            ch->io.repeat_mode = (val >> 3) & 3;
            ch->io.format = (val >> 5) & 3;
            ch->io.status = (val >> 7) & 1;
            change_params(this, ch);
            return; }

        case R7_SOUNDxSAD+0:
            ch->io.source_addr = (ch->io.source_addr & 0x07FFFF00) | val;
            return;

        case R7_SOUNDxSAD+1:
            ch->io.source_addr = (ch->io.source_addr & 0x07FF00FF) | (val << 8);
            return;

        case R7_SOUNDxSAD+2:
            ch->io.source_addr = (ch->io.source_addr & 0x0700FFFF) | (val << 16);
            return;

        case R7_SOUNDxSAD+3:
            ch->io.source_addr = (ch->io.source_addr & 0x00FFFFFF) | ((val & 7) << 24);
            change_params(this, ch);
            return;

        case R7_SOUNDxTMR+0:
            ch->io.period = (ch->io.period & 0xFF00) | val;
            return;

        case R7_SOUNDxTMR+1:
            ch->io.period = (ch->io.period & 0xFF) | (val << 8);
            change_params(this, ch);
            return;

        case R7_SOUNDxLEN+0:
            ch->io.len = (ch->io.len & 0xFF00) | val;
            return;

        case R7_SOUNDxLEN+1:
            ch->io.len = (ch->io.len & 0xFF) | (val << 8);
            change_params(this, ch);
            return;

        case R7_SOUNDBIAS+0:
            this->apu.io.sound_bias = (this->apu.io.sound_bias & 0x300) | val;
            return;

        case R7_SOUNDBIAS+1:
            this->apu.io.sound_bias = (this->apu.io.sound_bias & 0xFF) | ((val & 3) << 8);
            change_params(this, NULL);
            return;

        case R7_SOUNDxPNT+0:
            ch->io.loop_start_pos = (ch->io.loop_start_pos & 0xFF00) | val;
            return;

        case R7_SOUNDxPNT+1:
            ch->io.loop_start_pos = (ch->io.loop_start_pos & 0xFF) | (val << 8);
            change_params(this, ch);
            return;

        case R7_SOUNDCNT+2:
        case R7_SOUNDCNT+3:
        case R7_SOUNDxLEN+2:
        case R7_SOUNDxLEN+3:
        case R7_SOUNDBIAS+2:
        case R7_SOUNDBIAS+3:
            return;
    }
    if (addr > 0x04000508) {
        static int a = 1;
        if (a) {
            printf("\nWARN: SND CAP WRITE!");
            a = 0;
        }
        return;
    }
    printf("\nUnhandled APU write %08x: %02x", addr, val);
}

u32 NDS_APU_read(struct NDS *this, u32 addr, u32 sz, u32 access)
{
    u32 v = apu_read8(this, addr);
    if (sz >= 2) {
        v |= apu_read8(this, addr+1) << 8;
    }
    if (sz == 4) {
        v |= apu_read8(this, addr+2) << 16;
        v |= apu_read8(this, addr+3) << 24;
    }
    return v;
}

void NDS_APU_write(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    apu_write8(this, addr, val & 0xFF);
    if (sz >= 2) {
        apu_write8(this, addr+1, (val >> 8) & 0xFF);
    }
    if (sz == 4) {
        apu_write8(this, addr+2, (val >> 16) & 0xFF);
        apu_write8(this, addr+3, (val >> 24) & 0xFF);
    }
}

void NDS_APU_init(struct NDS *this)
{
    memset(&this->apu, 0, sizeof(this->apu));
    for (u32 i = 0; i < 16; i++) {
        struct NDS_APU_CH *ch = &this->apu.ch[i];
        ch->num = i;
        if ((i >= 8) && (i < 14)) {
            ch->has_psg = 1;
        }
        if (i >= 14) {
            ch->has_noise = 1;
        }
    }
}