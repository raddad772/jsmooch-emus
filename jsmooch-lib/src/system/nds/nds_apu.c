//
// Created by . on 3/27/25.
//

#include <math.h>
#include <string.h>
#include "nds_bus.h"
#include "nds_apu.h"
#include "nds_regs.h"

static const i32 ima_index_table[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

static const i16 ima_step_table[89] = {
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
0x3BB9, 0x41B2, 0x4844, 0x4F7E, 0x5771, 0x602F, 0x69CE, 0x7462,
0x7FFF
};

static void disable_ch(struct NDS *this, struct NDS_APU_CH *ch)
{
    if (ch->scheduled) {
        scheduler_delete_if_exist(&this->scheduler, ch->schedule_id);
    }
    if (!ch->io.hold) ch->sample = 0;
    ch->io.status = 0;
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
            v |= ch->io.status << 7;
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

        case R7_SOUNDCAP0CNT:
        case R7_SOUNDCAP1CNT: {
            u32 num = addr & 1;
            struct NDS_APU_SOUNDCAP *cp = &this->apu.soundcap[num];
            v = cp->ctrl_src;
            v |= cp->cap_source << 1;
            v |= cp->repeat_mode << 2;
            v |= cp->format << 3;
            v |= cp->status << 7;
            return v; }

        case R7_SOUNDCAP0DAD+0: return this->apu.soundcap[0].dest_addr & 0xFF;
        case R7_SOUNDCAP0DAD+1: return (this->apu.soundcap[0].dest_addr >> 8) & 0xFF;
        case R7_SOUNDCAP0DAD+2: return (this->apu.soundcap[0].dest_addr >> 16) & 0xFF;
        case R7_SOUNDCAP0DAD+3: return (this->apu.soundcap[0].dest_addr >> 24) & 0xFF;
        case R7_SOUNDCAP1DAD+0: return this->apu.soundcap[1].dest_addr & 0xFF;
        case R7_SOUNDCAP1DAD+1: return (this->apu.soundcap[1].dest_addr >> 8) & 0xFF;
        case R7_SOUNDCAP1DAD+2: return (this->apu.soundcap[1].dest_addr >> 16) & 0xFF;
        case R7_SOUNDCAP1DAD+3: return (this->apu.soundcap[1].dest_addr >> 24) & 0xFF;

        case R7_SOUNDCNT+2:
        case R7_SOUNDCNT+3:
        case R7_SOUNDBIAS+2:
        case R7_SOUNDBIAS+3:
            return 0;

    }
    printf("\nUnhandled APU read %08x", addr);
    return 0;
}

static void calc_loop_start(struct NDS *this, struct NDS_APU_CH *ch)
{
    switch(ch->io.format) {
        case NDS_APU_FMT_pcm8:
            ch->status.real_loop_start_pos = ch->io.loop_start_pos * 4;
            break;
        case NDS_APU_FMT_pcm16:
            ch->status.real_loop_start_pos = ch->io.loop_start_pos * 2;
            break;
        case NDS_APU_FMT_ima_adpcm:
            ch->status.real_loop_start_pos = (ch->io.loop_start_pos - 1) * 8;
            break;
        case NDS_APU_FMT_psg:
            ch->status.real_loop_start_pos = INT32_MAX;
            break;
    }
}

static void update_len(struct NDS *this, struct NDS_APU_CH *ch)
{
    // TODO: more of this
    switch(ch->io.format) {
        case NDS_APU_FMT_pcm8:
            ch->io.sample_len = 4 * ch->io.len;
            break;
        case NDS_APU_FMT_pcm16:
            ch->io.sample_len = 2 * ch->io.len;
            break;
        case NDS_APU_FMT_ima_adpcm:
            ch->io.sample_len = 8 * (ch->io.len - 1);
            break;
        case NDS_APU_FMT_psg:
            break;
    }
}

static void run_pcm8(struct NDS *this, struct NDS_APU_CH *ch)
{
    ch->sample = (i16)(NDS_mainbus_read7(this, ch->io.source_addr + (ch->status.pos & 0xFFFFFFFC), 1, 0, 0) << 8);
    ch->status.pos++;
}

static void run_pcm16(struct NDS *this, struct NDS_APU_CH *ch)
{
    // addr, sz, access, effect
    ch->sample = NDS_mainbus_read7(this, ch->io.source_addr + ((ch->status.pos >> 1) << 2), 2, 0, 0);
    ch->status.pos++;
}

static void run_ima_adpcm(struct NDS *this, struct NDS_APU_CH *ch)
{
    if ((ch->status.pos & 7) == 0) {
        ch->adpcm.data = NDS_mainbus_read7(this, ch->io.source_addr + ((ch->status.pos >> 3) << 2) + 4, 4, 0, 0);
    }
    u32 nibble = ch->adpcm.data & 15;
    ch->adpcm.data >>= 4;

    i32 diff = (i32)(ima_step_table[ch->adpcm.tbl_idx] * (((nibble & 7) << 1) | 1) >> 3);

    if (nibble & 8) {
        ch->adpcm.sample -= diff;
        if (ch->adpcm.sample < -32767) ch->adpcm.sample = -32767;
    } else {
        ch->adpcm.sample += diff;
        if (ch->adpcm.sample > 32767) ch->adpcm.sample = 32767;
    }
    ch->sample = (i16)ch->adpcm.sample;

    ch->adpcm.tbl_idx += ima_index_table[nibble & 7];
    if (ch->adpcm.tbl_idx < 0) ch->adpcm.tbl_idx = 0;
    if (ch->adpcm.tbl_idx > 88) ch->adpcm.tbl_idx = 88;

    ch->status.pos++;
}

static void run_psg(struct NDS *this, struct NDS_APU_CH *ch)
{
    ch->sample = (i16)((i32)(((ch->status.pos++) > ch->io.wave_duty) * 65535) - 32768);
    ch->status.pos++;
}

static void run_noise(struct NDS *this, struct NDS_APU_CH *ch)
{
    u32 b1 = ch->lfsr & 1;
    u32 xorby = b1 * 0x6000;
    ch->lfsr = (ch->lfsr >> 1) ^ xorby;
    ch->sample = (i16)((((i32)b1) * 65535) - 32768);
    ch->status.pos++;
}

static void run_cap(struct NDS *this, u32 cap_num)
{
    struct NDS_APU_SOUNDCAP *cp = &this->apu.soundcap[cap_num];
    if (!cp->status) return;
    u32 target_ch = (cap_num << 1) + 1;
    struct NDS_APU_CH *ch = &this->apu.ch[target_ch];

    i32 smp = ch->sample;
    static int a = 1;
    if (a) {
        printf("\nWARN IMPLEMENT SOUND CAP");
        a = 0;
    }
}

static void run_channel(void *ptr, u64 ch_num, u64 cur_clock, u32 jitter)
{
    struct NDS *this = (struct NDS *)ptr;
    struct NDS_APU_CH *ch = &this->apu.ch[ch_num];

    if (!ch->io.status) return;

    u64 real_clock = cur_clock - jitter;

    // Get sample
    switch(ch->io.format) {
        case NDS_APU_FMT_pcm8:
            run_pcm8(this, ch);
            break;
        case NDS_APU_FMT_pcm16:
            run_pcm16(this, ch);
            break;
        case NDS_APU_FMT_ima_adpcm:
            run_ima_adpcm(this, ch);
            break;
        case NDS_APU_FMT_psg:
            if (ch->has_psg) run_psg(this, ch);
            if (ch->has_noise) run_noise(this, ch);
            break;
    }
    if (ch->io.format != NDS_APU_FMT_psg) {
        if (ch->status.pos >= ch->io.sample_len) {
            if (ch->io.repeat_mode == NDS_APU_RM_loop_infinite) {
                ch->status.pos = ch->status.real_loop_start_pos;
                if (ch->io.format == NDS_APU_FMT_ima_adpcm) {
                    ch->adpcm.tbl_idx = ch->adpcm.loop_tbl_idx;
                    ch->adpcm.sample = ch->adpcm.loop_sample;
                }
            }
            else { // TODO: supposed to disable after the last sample finishes?
                disable_ch(this, ch);
                return;
            }
        }
    }

    ch->schedule_id = scheduler_only_add_abs(&this->scheduler, real_clock + ch->status.sampling_interval, ch->num, this, &run_channel, &ch->scheduled);
    if (ch->has_cap) run_cap(this, ch->num >> 1);
}

static void change_freq(struct NDS *this, struct NDS_APU_CH *ch)
{
    u32 new_interval = (0x10000 - ch->io.period) << 1;
    if (new_interval != ch->status.sampling_interval) {
        if (ch->scheduled) {
            scheduler_delete_if_exist(&this->scheduler, ch->schedule_id);
            ch->schedule_id = scheduler_only_add_abs(&this->scheduler, NDS_clock_current7(this) + new_interval, ch->num, this, &run_channel, &ch->scheduled);
        }
        ch->status.sampling_interval = new_interval;
    }
}


static void probe_trigger(struct NDS *this, struct NDS_APU_CH *ch, u32 old_status)
{
    if (old_status == ch->io.status) return;

    if (old_status) { // Disable channel!
        disable_ch(this, ch);
        return;
    }

    // Trigger channel!
    // TODO: supposed to wait 1-3 0 samples
    if (ch->io.format == NDS_APU_FMT_ima_adpcm) {
        u32 adpcm_header = NDS_mainbus_read7(this, ch->io.source_addr, 4, 0, 0);

        ch->adpcm.sample = ch->sample = (i16)(adpcm_header & 0xFFFF);
        ch->adpcm.tbl_idx = (adpcm_header >> 16) & 0x7F;
        if (ch->adpcm.tbl_idx < 0) ch->adpcm.tbl_idx = 0;
        if (ch->adpcm.tbl_idx > 88) ch->adpcm.tbl_idx = 88;

        ch->adpcm.loop_sample = ch->sample;
        ch->adpcm.loop_tbl_idx = ch->adpcm.tbl_idx;
    }

    ch->status.pos = 0;

    ch->sample = 0;
    ch->schedule_id = scheduler_only_add_abs(&this->scheduler, NDS_clock_current7(this) + ch->status.sampling_interval, ch->num, this, &run_channel, &ch->scheduled);
    ch->lfsr = 0x7FFF;
}

static void apu_write8(struct NDS *this, u32 addr, u32 val, struct NDS_APU_CH *ch)
{
    switch(addr) {
        case R7_SOUNDCNT+0:
            this->apu.io.master_vol = val & 0x7F;
            return;

        case R7_SOUNDCNT+1:
            this->apu.io.output_from_left = val & 3;
            this->apu.io.output_from_right = (val >> 2) & 3;
            this->apu.io.output_ch1_to_mixer = (val >> 4) & 1;
            this->apu.io.output_ch3_to_mixer = (val >> 5) & 1;
            this->apu.io.master_enable = (val >> 7) & 1;
            return;
        case R7_SOUNDxCNT+0:
            ch->io.vol = ch->io.real_vol = val & 0x7F;
            if (ch->io.real_vol == 127) ch->io.real_vol = 128;
            return;
        case R7_SOUNDxCNT+1: {
            static const u32 sval[4] = {0, 1, 2, 4};
            ch->io.vol_div = val & 3;
            ch->io.vol_rshift = sval[ch->io.vol_div];
            ch->io.hold = (val >> 7) & 1;
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
            if (ch->io.right_pan == 63) ch->io.right_pan = 64;
            if (ch->io.left_pan == 63) ch->io.left_pan = 64;
            return; }

        case R7_SOUNDxCNT+3: {
            u32 old_enable = ch->io.status;
            ch->io.wave_duty = val & 7;
            ch->io.repeat_mode = (val >> 3) & 3;
            ch->io.format = (val >> 5) & 3;
            ch->io.status = (val >> 7) & 1;
            update_len(this, ch);
            calc_loop_start(this, ch);
            probe_trigger(this, ch, old_enable);
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
            return;

        case R7_SOUNDxTMR+0:
            ch->io.period = (ch->io.period & 0xFF00) | val;
            change_freq(this, ch);
            return;

        case R7_SOUNDxTMR+1:
            ch->io.period = (ch->io.period & 0xFF) | (val << 8);
            change_freq(this, ch);
            return;

        case R7_SOUNDxLEN+0:
            ch->io.len = (ch->io.len & 0xFF00) | val;
            update_len(this, ch);
            return;

        case R7_SOUNDxLEN+1:
            ch->io.len = (ch->io.len & 0xFF) | (val << 8);
            update_len(this, ch);
            return;

        case R7_SOUNDBIAS+0:
            this->apu.io.sound_bias = (this->apu.io.sound_bias & 0x300) | val;
            return;

        case R7_SOUNDBIAS+1:
            this->apu.io.sound_bias = (this->apu.io.sound_bias & 0xFF) | ((val & 3) << 8);
            return;

        case R7_SOUNDxPNT+0:
            ch->io.loop_start_pos = (ch->io.loop_start_pos & 0xFF00) | val;
            calc_loop_start(this, ch);
            return;

        case R7_SOUNDxPNT+1:
            ch->io.loop_start_pos = (ch->io.loop_start_pos & 0xFF) | (val << 8);
            calc_loop_start(this, ch);
            return;

        case R7_SOUNDCAP0CNT:
        case R7_SOUNDCAP1CNT: {
            u32 num = addr & 1;
            struct NDS_APU_SOUNDCAP *cp = &this->apu.soundcap[num];
            u32 old_status = cp->status;
            cp->ctrl_src = val & 1;
            cp->cap_source = (val >> 1) & 1;
            cp->repeat_mode = (val >> 2) & 1;
            cp->format = (val >> 3) & 1;
            cp->status = (val >> 7) & 1;
            if (!old_status && cp->status) {
                cp->pos = 0;
            }
            return; }

        case R7_SOUNDCAP0DAD+0:
        case R7_SOUNDCAP1DAD+0: {
            u32 num = (addr >> 4) & 1;
            struct NDS_APU_SOUNDCAP *cp = &this->apu.soundcap[num];
            cp->dest_addr = (cp->dest_addr & 0x07FFFF00) | val;
            return; }

        case R7_SOUNDCAP0DAD+1:
        case R7_SOUNDCAP1DAD+1: {
            u32 num = (addr >> 4) & 1;
            struct NDS_APU_SOUNDCAP *cp = &this->apu.soundcap[num];
            cp->dest_addr = (cp->dest_addr & 0x07FF00FF) | (val << 8);
            return; }

        case R7_SOUNDCAP0DAD+2:
        case R7_SOUNDCAP1DAD+2: {
            u32 num = (addr >> 4) & 1;
            struct NDS_APU_SOUNDCAP *cp = &this->apu.soundcap[num];
            cp->dest_addr = (cp->dest_addr & 0x0700FFFF) | (val << 16);
            return; }

        case R7_SOUNDCAP0DAD+3:
        case R7_SOUNDCAP1DAD+3: {
            u32 num = (addr >> 4) & 1;
            struct NDS_APU_SOUNDCAP *cp = &this->apu.soundcap[num];
            cp->dest_addr = (cp->dest_addr & 0x00FFFFFF) | ((val & 7) << 24);
            return; }

        case R7_SOUNDCAP0LEN+0:
        case R7_SOUNDCAP1LEN+0: {
            u32 num = (addr >> 4) & 1;
            struct NDS_APU_SOUNDCAP *cp = &this->apu.soundcap[num];
            cp->len_words = (cp->len_words & 0xFF00) | val;
            cp->len_bytes = cp->len_words << 2;
            return; }

        case R7_SOUNDCAP0LEN+1:
        case R7_SOUNDCAP1LEN+1: {
            u32 num = (addr >> 4) & 1;
            struct NDS_APU_SOUNDCAP *cp = &this->apu.soundcap[num];
            cp->len_words = (cp->len_words & 0xFF) | (val << 8);
            cp->len_bytes = cp->len_words << 2;
            return; }

        case R7_SOUNDCNT+2:
        case R7_SOUNDCNT+3:
        case R7_SOUNDxLEN+2:
        case R7_SOUNDxLEN+3:
        case R7_SOUNDBIAS+2:
        case R7_SOUNDBIAS+3:
            return;
    }
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
    u32 chn;
    struct NDS_APU_CH *ch = NULL;
    if (addr < 0x04000500) {
        chn = (addr >> 4) & 15;
        ch = &this->apu.ch[chn];
        addr &= 0xFFFFFF0F;
    }

    switch(addr) {
        case R7_SOUNDxTMR:
            if (sz == 1) ch->io.period = (val & 0xFF00) | val;
            else ch->io.period = val & 0xFFFF;
            change_freq(this, ch);
            if (sz == 4) NDS_APU_write(this, addr+2, 2, access, val >> 16);
            return;
    }

    apu_write8(this, addr, val & 0xFF, ch);
    if (sz >= 2) {
        apu_write8(this, addr+1, (val >> 8) & 0xFF, ch);
    }
    if (sz == 4) {
        apu_write8(this, addr+2, (val >> 16) & 0xFF, ch);
        apu_write8(this, addr+3, (val >> 24) & 0xFF, ch);
    }
}

void NDS_APU_init(struct NDS *this) {
    // MUST be done AFTER NDS_clock_init, which should be first init.
    memset(&this->apu, 0, sizeof(this->apu));
    for (u32 i = 0; i < 16; i++) {
        struct NDS_APU_CH *ch = &this->apu.ch[i];
        ch->num = i;
        if ((i == 1) || (i == 3)) ch->has_cap = 1;
        if ((i >= 8) && (i < 14)) {
            ch->has_psg = 1;
        }
        if (i >= 14) {
            ch->has_noise = 1;
        }
    }

    this->apu.sample_cycles = ((long double) this->clock.timing.arm7.hz / 32768.0f);
    this->apu.next_sample = this->apu.sample_cycles;
    this->apu.io.master_enable = 1;
}

void NDS_master_sample_callback(void *ptr, u64 nothing, u64 cur_clock, u32 jitter)
{
    //printf("\nMASTER SAMPLE CALLBACK!");
    struct NDS *this = (struct NDS *)ptr;
    /*if (!this->apu.io.master_enable) {
        this->apu.buffer.samples[this->apu.buffer.tail] = 0;
        this->apu.buffer.tail = (this->apu.buffer.tail + 1) & (NDS_APU_MAX_SAMPLES - 1);
        this->apu.buffer.len++;

        this->apu.total_samples += 1.0;
        this->apu.next_sample = this->apu.total_samples * this->apu.sample_cycles;
        scheduler_only_add_abs(&this->scheduler, (i64)this->apu.next_sample, 0, this, &NDS_master_sample_callback, NULL);
        return;
    }*/

    i32 left = 0, right = 0;
    i32 spkr = 0;
    for (u32 chn = 0; chn < 16; chn++) {
        struct NDS_APU_CH *ch = &this->apu.ch[chn];
        //i32 smp = ch->sample; //
        i32 smp = ((ch->sample * (i32)ch->io.real_vol) >> 7) >> ch->io.vol_rshift;
        // Current range, 16 bits.
        spkr += smp;

        left += (smp * ch->io.left_pan) >> 6;
        right += (smp * ch->io.right_pan) >> 6;
    }
    spkr >>= 6;
    if (spkr < -1024) spkr = -1024;
    if (spkr > 1023) spkr = 1023;
    if (left <= -32768) left = -32768;
    if (left > 32767) left = 32767;
    if (right <= -32768) right = -32768;
    if (right > 32767) right = 32767;
    if (this->apu.buffer.len >= NDS_APU_MAX_SAMPLES) {
        printf("\nERROR SOUND OVERRUN!");
    }
    else {
        this->apu.buffer.samples[this->apu.buffer.tail] = spkr;
        this->apu.buffer.tail = (this->apu.buffer.tail + 1) & (NDS_APU_MAX_SAMPLES - 1);
        this->apu.buffer.len++;
    }

    this->apu.right_output = right;
    this->apu.left_output = left;
    this->apu.total_samples += 1.0;

    this->apu.next_sample = this->apu.total_samples * this->apu.sample_cycles;
    scheduler_only_add_abs(&this->scheduler, (i64)this->apu.next_sample, 0, this, &NDS_master_sample_callback, NULL);
}