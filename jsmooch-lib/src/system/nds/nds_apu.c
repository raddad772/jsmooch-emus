//
// Created by . on 3/27/25.
//

#include <string.h>
#include "nds_bus.h"
#include "nds_apu.h"
#include "nds_regs.h"

static void do_run_to_current(struct NDS *this, struct NDS_APU_CH *ch, u64 cur_clock)
{

}

static void run_to_current(struct NDS *this, struct NDS_APU_CH *ch, u64 cur_clock)
{
    if (ch) do_run_to_current(this, ch, cur_clock);
    else {
        for (u32 i = 0; i < 16; i++) {
            do_run_to_current(this, ch, cur_clock);
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
    }

    printf("\nUnhandled APU read %08x", addr);
    return 0;
}

/* So, each cycle, add 32768 to a counter.
 * When counter >= arm7.hz, -= arm7.hz, new sample.
 */

static void latch_io(struct NDS *this, struct NDS_APU_CH *ch) {
    if (!ch) {
        for (u32 i = 0; i < 15; i++) {
            ch = &this->apu.ch[i];
            memcpy(&ch->latched, &ch->io, sizeof(struct NDS_APU_CH_params));
        }
        memcpy(&this->apu.latched, &this->apu.io, sizeof(struct NDS_APU_params));
    } else
        memcpy(&ch->latched, &ch->io, sizeof(struct NDS_APU_CH_params));
}

static void calculate_ch(struct NDS *this, struct NDS_APU_CH *ch, u64 cur_clock)
{
    // Empty, off channel.
    if (!ch->latched.status) return;

    // calculate when next sample will be
    u64 cycles_per_sample = (this->clock.timing.arm7.hz >> 1) / ch->latched.period;

    ch->status.next_timecode = ch->status.last_sample.timecode + cycles_per_sample;
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
            ch->io.vol_rshift = sval[val & 3];
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
            write_sndcnt_hi(this, ch, val);
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
    printf("\nUnhandled APU write %08x: %02x", addr, val);
}

u32 NDS_APU_read(struct NDS *this, u32 addr, u32 sz)
{
    u32 v = apu_read8(this, addr);
    if (sz >= 2) {
        v |= apu_read8(this, addr+1) << 8;
    }
    if (sz == 4) {
        v |= apu_read8(this, addr+2) << 16;
        v |= apu_read8(this, addr+3) << 24;
    }

}

void NDS_APU_write(struct NDS *this, u32 addr, u32 sz, u32 val)
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