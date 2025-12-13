//
// Created by . on 3/27/25.
//

#include <cmath>
#include <cstring>
#include "nds_bus.h"
#include "nds_apu.h"
#include "nds_regs.h"

namespace NDS::APU {
static constexpr i32 ima_index_table[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

static constexpr i16 ima_step_table[89] = {
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

void MCH::disable()
{
    if (scheduled) {
        apu->scheduler->delete_if_exist(schedule_id);
    }
    if (!io.hold) sample = 0;
    io.status = 0;
}

u32 core::read8(u32 addr)
{
    u32 v;
    u32 chn = 0;
    if (addr < 0x04000500) {
        chn = (addr >> 4) & 15;
        addr &= 0xFFFFFF0F;
    }
    const MCH &ch = CH[chn];
    switch(addr) {
        case R7_SOUNDxCNT+0:
            return ch.io.vol;
        case R7_SOUNDxCNT+1:
            v = ch.io.vol_div;
            v |= ch.io.hold << 7;
            return v;
        case R7_SOUNDxCNT+2:
            return ch.io.pan;
        case R7_SOUNDxCNT+3:
            v = ch.io.wave_duty;
            v |= ch.io.repeat_mode << 3;
            v |= ch.io.format << 5;
            v |= ch.io.status << 7;
            return v;
        case R7_SOUNDCNT+0:
            return io.master_vol;
        case R7_SOUNDCNT+1:
            v = io.output_from_left;
            v |= io.output_from_right << 2;
            v |= io.output_ch1_to_mixer << 4;
            v |= io.output_ch3_to_mixer << 5;
            v |= io.master_vol << 7;
            return v;

        case R7_SOUNDBIAS+0:
            return io.sound_bias & 0xFF;
        case R7_SOUNDBIAS+1:
            return (io.sound_bias >> 8) & 3;

        case R7_SOUNDCAP0CNT:
        case R7_SOUNDCAP1CNT: {
            u32 num = addr & 1;
            SOUNDCAP &cp = soundcap[num];
            v = cp.ctrl_src;
            v |= cp.cap_source << 1;
            v |= cp.repeat_mode << 2;
            v |= cp.format << 3;
            v |= cp.status << 7;
            return v; }

        case R7_SOUNDCAP0DAD+0: return soundcap[0].dest_addr & 0xFF;
        case R7_SOUNDCAP0DAD+1: return (soundcap[0].dest_addr >> 8) & 0xFF;
        case R7_SOUNDCAP0DAD+2: return (soundcap[0].dest_addr >> 16) & 0xFF;
        case R7_SOUNDCAP0DAD+3: return (soundcap[0].dest_addr >> 24) & 0xFF;
        case R7_SOUNDCAP1DAD+0: return soundcap[1].dest_addr & 0xFF;
        case R7_SOUNDCAP1DAD+1: return (soundcap[1].dest_addr >> 8) & 0xFF;
        case R7_SOUNDCAP1DAD+2: return (soundcap[1].dest_addr >> 16) & 0xFF;
        case R7_SOUNDCAP1DAD+3: return (soundcap[1].dest_addr >> 24) & 0xFF;

        case R7_SOUNDCNT+2:
        case R7_SOUNDCNT+3:
        case R7_SOUNDBIAS+2:
        case R7_SOUNDBIAS+3:
            return 0;
        default:
    }
    printf("\nUnhandled APU read %08x", addr);
    return 0;
}

void MCH::calc_loop_start()
{
    switch(io.format) {
        case FMT_pcm8:
            status.real_loop_start_pos = io.loop_start_pos * 4;
            break;
        case FMT_pcm16:
            status.real_loop_start_pos = io.loop_start_pos * 2;
            break;
        case FMT_ima_adpcm:
            status.real_loop_start_pos = (io.loop_start_pos - 1) * 8;
            break;
        case FMT_psg:
            status.real_loop_start_pos = INT32_MAX;
            break;
    }
}

void MCH::update_len()
{
    // TODO: more of this
    switch(io.format) {
        case FMT_pcm8:
            io.sample_len = 4 * io.len;
            break;
        case FMT_pcm16:
            io.sample_len = 2 * io.len;
            break;
        case FMT_ima_adpcm:
            io.sample_len = 8 * (io.len - 1);
            break;
        case FMT_psg:
            break;
    }
}

void MCH::run_pcm8()
{
    sample = static_cast<i16>(NDS::core::mainbus_read7(apu->bus, io.source_addr + (status.pos & 0xFFFFFFFC), 1, 0, false) << 8);
    status.pos++;
}

void MCH::run_pcm16()
{
    // addr, sz, access, effect
    sample = NDS::core::mainbus_read7(apu->bus, io.source_addr + ((status.pos >> 1) << 2), 2, 0, false);
    status.pos++;
}

void MCH::run_ima_adpcm()
{
    if ((status.pos & 7) == 0) {
        adpcm.data = NDS::core::mainbus_read7(apu->bus, io.source_addr + ((status.pos >> 3) << 2) + 4, 4, 0, false);
    }
    const u32 nibble = adpcm.data & 15;
    adpcm.data >>= 4;

    const i32 diff = static_cast<i32>(ima_step_table[adpcm.tbl_idx] * (((nibble & 7) << 1) | 1) >> 3);

    if (nibble & 8) {
        adpcm.sample -= diff;
        if (adpcm.sample < -32767) adpcm.sample = -32767;
    } else {
        adpcm.sample += diff;
        if (adpcm.sample > 32767) adpcm.sample = 32767;
    }
    sample = static_cast<i16>(adpcm.sample);

    adpcm.tbl_idx += ima_index_table[nibble & 7];
    if (adpcm.tbl_idx < 0) adpcm.tbl_idx = 0;
    if (adpcm.tbl_idx > 88) adpcm.tbl_idx = 88;

    status.pos++;
}

void MCH::run_psg()
{
    sample = static_cast<i16>(((status.pos++ > io.wave_duty) * 65535) - 32768);
    status.pos++;
}

void MCH::run_noise()
{
    const u32 b1 = lfsr & 1;
    const u32 xorby = b1 * 0x6000;
    lfsr = (lfsr >> 1) ^ xorby;
    sample = static_cast<i16>((static_cast<i32>(b1) * 65535) - 32768);
    status.pos++;
}

void core::run_cap(u32 cap_num)
{
    SOUNDCAP &cp = soundcap[cap_num];
    if (!cp.status) return;
    u32 target_ch = (cap_num << 1) + 1;
    MCH &ch = CH[target_ch];

    i32 smp = ch.sample;
    static int a = 1;
    if (a) {
        printf("\nWARN IMPLEMENT SOUND CAP");
        a = 0;
    }
}

void MCH::run(void *ptr, u64 ch_num, u64 cur_clock, u32 jitter)
{
    auto &ch = *static_cast<MCH *>(ptr);
    if (!ch.io.status) return;

    u64 real_clock = cur_clock - jitter;

    // Get sample
    switch(ch.io.format) {
        case FMT_pcm8:
            ch.run_pcm8();
            break;
        case FMT_pcm16:
            ch.run_pcm16();
            break;
        case FMT_ima_adpcm:
            ch.run_ima_adpcm();
            break;
        case FMT_psg:
            if (ch.has_psg) ch.run_psg();
            if (ch.has_noise) ch.run_noise();
            break;
    }
    if (ch.io.format != FMT_psg) {
        if (ch.status.pos >= ch.io.sample_len) {
            if (ch.io.repeat_mode == RM_loop_infinite) {
                ch.status.pos = ch.status.real_loop_start_pos;
                if (ch.io.format == FMT_ima_adpcm) {
                    ch.adpcm.tbl_idx = ch.adpcm.loop_tbl_idx;
                    ch.adpcm.sample = ch.adpcm.loop_sample;
                }
            }
            else { // TODO: supposed to disable after the last sample finishes?
                ch.disable();
                return;
            }
        }
    }

    ch.schedule_id = ch.apu->scheduler->only_add_abs(real_clock + ch.status.sampling_interval, ch.num, &ch, &MCH::run, &ch.scheduled);
    if (ch.has_cap) ch.apu->run_cap(ch.num >> 1);
}

void MCH::change_freq()
{
    u32 new_interval = (0x10000 - io.period) << 1;
    if (new_interval != status.sampling_interval) {
        if (scheduled) {
            apu->scheduler->delete_if_exist(schedule_id);
            schedule_id = apu->scheduler->only_add_abs(apu->bus->clock.current7() + new_interval, num, this, &MCH::run, &scheduled);
        }
        status.sampling_interval = new_interval;
    }
}

void MCH::probe_trigger(u32 old_status)
{
    if (old_status == io.status) return;

    if (old_status) { // Disable channel!
        disable();
        return;
    }

    // Trigger channel!
    // TODO: supposed to wait 1-3 0 samples
    if (io.format == FMT_ima_adpcm) {
        u32 adpcm_header = NDS::core::mainbus_read7(apu->bus, io.source_addr, 4, 0, false);

        adpcm.sample = sample = static_cast<i16>(adpcm_header & 0xFFFF);
        adpcm.tbl_idx = (adpcm_header >> 16) & 0x7F;
        if (adpcm.tbl_idx < 0) adpcm.tbl_idx = 0;
        if (adpcm.tbl_idx > 88) adpcm.tbl_idx = 88;

        adpcm.loop_sample = sample;
        adpcm.loop_tbl_idx = adpcm.tbl_idx;
    }

    status.pos = 0;

    sample = 0;
    schedule_id = apu->scheduler->only_add_abs(apu->bus->clock.current7() + status.sampling_interval, num, this, &MCH::run, &scheduled);
    lfsr = 0x7FFF;
}

void core::write8(u32 addr, u32 val, MCH &ch)
{
    switch(addr) {
        case R7_SOUNDCNT+0:
            io.master_vol = val & 0x7F;
            return;

        case R7_SOUNDCNT+1:
            io.output_from_left = static_cast<APU_OF>(val & 3);
            io.output_from_right = static_cast<APU_OF>((val >> 2) & 3);
            io.output_ch1_to_mixer = (val >> 4) & 1;
            io.output_ch3_to_mixer = (val >> 5) & 1;
            io.master_enable = (val >> 7) & 1;
            return;
        case R7_SOUNDxCNT+0:
            ch.io.vol = ch.io.real_vol = val & 0x7F;
            if (ch.io.real_vol == 127) ch.io.real_vol = 128;
            return;
        case R7_SOUNDxCNT+1: {
            static constexpr u32 sval[4] = {0, 1, 2, 4};
            ch.io.vol_div = val & 3;
            ch.io.vol_rshift = sval[ch.io.vol_div];
            ch.io.hold = (val >> 7) & 1;
            return; }
        case R7_SOUNDxCNT+2: {
            val &= 0x7F;
            ch.io.pan = val;
            // (0..127=left..right) (64=half volume on both speakers)
            // 0 = 100% on left, 0% on right
            // 127 = 100% on right, 0% on left
            // 0...127 right
            // 127 - 0...127 left
            if (val == 64) {
                ch.io.left_pan = ch.io.right_pan = 64;
            }
            else {
                ch.io.right_pan = ch.io.pan;
                ch.io.left_pan = 127 - ch.io.pan;
            }
            return; }

        case R7_SOUNDxCNT+3: {
            u32 old_enable = ch.io.status;
            ch.io.wave_duty = val & 7;
            ch.io.repeat_mode = static_cast<REPEAT_MODE>((val >> 3) & 3);
            ch.io.format = static_cast<FMT>((val >> 5) & 3);
            ch.io.status = (val >> 7) & 1;
            ch.update_len();
            ch.calc_loop_start();
            ch.probe_trigger(old_enable);
            return; }

        case R7_SOUNDxSAD+0:
            ch.io.source_addr = (ch.io.source_addr & 0x07FFFF00) | val;
            return;

        case R7_SOUNDxSAD+1:
            ch.io.source_addr = (ch.io.source_addr & 0x07FF00FF) | (val << 8);
            return;

        case R7_SOUNDxSAD+2:
            ch.io.source_addr = (ch.io.source_addr & 0x0700FFFF) | (val << 16);
            return;

        case R7_SOUNDxSAD+3:
            ch.io.source_addr = (ch.io.source_addr & 0x00FFFFFF) | ((val & 7) << 24);
            return;

        case R7_SOUNDxTMR+0:
            ch.io.period = (ch.io.period & 0xFF00) | val;
            ch.change_freq();
            return;

        case R7_SOUNDxTMR+1:
            ch.io.period = (ch.io.period & 0xFF) | (val << 8);
            ch.change_freq();
            return;

        case R7_SOUNDxLEN+0:
            ch.io.len = (ch.io.len & 0xFF00) | val;
            ch.update_len();
            return;

        case R7_SOUNDxLEN+1:
            ch.io.len = (ch.io.len & 0xFF) | (val << 8);
            ch.update_len();
            return;

        case R7_SOUNDBIAS+0:
            io.sound_bias = (io.sound_bias & 0x300) | val;
            return;

        case R7_SOUNDBIAS+1:
            io.sound_bias = (io.sound_bias & 0xFF) | ((val & 3) << 8);
            return;

        case R7_SOUNDxPNT+0:
            ch.io.loop_start_pos = (ch.io.loop_start_pos & 0xFF00) | val;
            ch.calc_loop_start();
            return;

        case R7_SOUNDxPNT+1:
            ch.io.loop_start_pos = (ch.io.loop_start_pos & 0xFF) | (val << 8);
            ch.calc_loop_start();
            return;

        case R7_SOUNDCAP0CNT:
        case R7_SOUNDCAP1CNT: {
            u32 num = addr & 1;
            SOUNDCAP &cp = soundcap[num];
            u32 old_status = cp.status;
            cp.ctrl_src = val & 1;
            cp.cap_source = static_cast<SOUNDCAP_SOURCE>((val >> 1) & 1);
            cp.repeat_mode = static_cast<SOUNDCAP_REPEAT>((val >> 2) & 1);
            cp.format = static_cast<SOUNDCAP_FORMAT>((val >> 3) & 1);
            cp.status = (val >> 7) & 1;
            if (!old_status && cp.status) {
                cp.pos = 0;
            }
            return; }

        case R7_SOUNDCAP0DAD+0:
        case R7_SOUNDCAP1DAD+0: {
            u32 num = (addr >> 4) & 1;
            SOUNDCAP &cp = soundcap[num];
            cp.dest_addr = (cp.dest_addr & 0x07FFFF00) | val;
            return; }

        case R7_SOUNDCAP0DAD+1:
        case R7_SOUNDCAP1DAD+1: {
            u32 num = (addr >> 4) & 1;
            SOUNDCAP &cp = soundcap[num];
            cp.dest_addr = (cp.dest_addr & 0x07FF00FF) | (val << 8);
            return; }

        case R7_SOUNDCAP0DAD+2:
        case R7_SOUNDCAP1DAD+2: {
            u32 num = (addr >> 4) & 1;
            SOUNDCAP &cp = soundcap[num];
            cp.dest_addr = (cp.dest_addr & 0x0700FFFF) | (val << 16);
            return; }

        case R7_SOUNDCAP0DAD+3:
        case R7_SOUNDCAP1DAD+3: {
            u32 num = (addr >> 4) & 1;
            SOUNDCAP &cp = soundcap[num];
            cp.dest_addr = (cp.dest_addr & 0x00FFFFFF) | ((val & 7) << 24);
            return; }

        case R7_SOUNDCAP0LEN+0:
        case R7_SOUNDCAP1LEN+0: {
            u32 num = (addr >> 4) & 1;
            SOUNDCAP &cp = soundcap[num];
            cp.len_words = (cp.len_words & 0xFF00) | val;
            cp.len_bytes = cp.len_words << 2;
            return; }

        case R7_SOUNDCAP0LEN+1:
        case R7_SOUNDCAP1LEN+1: {
            u32 num = (addr >> 4) & 1;
            SOUNDCAP &cp = soundcap[num];
            cp.len_words = (cp.len_words & 0xFF) | (val << 8);
            cp.len_bytes = cp.len_words << 2;
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

u32 core::read(u32 addr, u8 sz, u8 access)
{
    u32 v = read8(addr);
    if (sz >= 2) {
        v |= read8(addr+1) << 8;
    }
    if (sz == 4) {
        v |= read8(addr+2) << 16;
        v |= read8(addr+3) << 24;
    }
    return v;
}

void core::write(u32 addr, u8 sz, u8 access, u32 val)
{
    u32 chn=0;
    if (addr < 0x04000500) {
        chn = (addr >> 4) & 15;
        addr &= 0xFFFFFF0F;
    }
    MCH &ch = CH[chn];

    switch(addr) {
        case R7_SOUNDxTMR:
            if (sz == 1) ch.io.period = (val & 0xFF00) | val;
            else ch.io.period = val & 0xFFFF;
            ch.change_freq();
            if (sz == 4) write(addr+2, 2, access, val >> 16);
            return;
    }

    write8(addr, val & 0xFF, ch);
    if (sz >= 2) {
        write8(addr+1, (val >> 8) & 0xFF, ch);
    }
    if (sz == 4) {
        write8(addr+2, (val >> 16) & 0xFF, ch);
        write8(addr+3, (val >> 24) & 0xFF, ch);
    }
}

core::core(NDS::core *parent, scheduler_t *scheduler_in) : scheduler(scheduler_in), bus(parent) {
    // MUST be done AFTER NDS_clock_init, which should be first init.
    for (u32 i = 0; i < 16; i++) {
        MCH &ch = CH[i];
        if ((i == 1) || (i == 3)) ch.has_cap = true;
        if ((i >= 8) && (i < 14)) {
            ch.has_psg = true;
        }
        if (i >= 14) {
            ch.has_noise = true;
        }
    }

    sample_cycles = (static_cast<long double>(bus->clock.timing.arm7.hz) / 32768.0f);
    next_sample = sample_cycles;
}

void core::master_sample_callback(void *ptr, u64 nothing, u64 cur_clock, u32 jitter)
{
    //printf("\nMASTER SAMPLE CALLBACK!");
     auto *th = static_cast<core *>(ptr);
    /*if (!io.master_enable) {
        buffer.samples[buffer.tail] = 0;
        buffer.tail = (buffer.tail + 1) & (NDS_APU_MAX_SAMPLES - 1);
        buffer.len++;

        total_samples += 1.0;
        next_sample = total_samples * sample_cycles;
        scheduler_only_add_abs((i64)next_sample, 0, this, &NDS_master_sample_callback, nullptr);
        return;
    }*/

    i32 left = 0, right = 0;
    i32 spkr = 0;
    for (auto &ch : th->CH) {
        //i32 smp = ch.sample; //
        i32 smp = ((static_cast<i32>(ch.sample) * static_cast<i32>(ch.io.real_vol)) >> 7) >> ch.io.vol_rshift;
        // Current range, 16 bits.
        //spkr += smp;
        left += (smp * ch.io.left_pan) >> 7;
        right += (smp * ch.io.right_pan) >> 7;
    }
    //spkr >>= 6;
    // >> 7 for master_vol divide, and another 6 for 16->10bit
    left = (left * static_cast<i32>(th->io.master_vol)) >> 13;
    right = (right * static_cast<i32>(th->io.master_vol)) >> 13;
    //if (spkr < -1024) spkr = -1024;
    //if (spkr > 1023) spkr = 1023;
    if (left < -512) left = -512;
    if (left > 511) left = 511;
    if (right < -512) right = -512;
    if (right > 511) right = 511;
    if (th->buffer.len >= NDS_APU_MAX_SAMPLES) {
        printf("\nERROR SOUND OVERRUN!");
    }
    else {
        th->buffer.samples2[th->buffer.tail] = left;
        th->buffer.tail = (th->buffer.tail + 1) & (NDS_APU_MAX_SAMPLES - 1);
        th->buffer.samples2[th->buffer.tail] = right;
        th->buffer.tail = (th->buffer.tail + 1) & (NDS_APU_MAX_SAMPLES - 1);
        th->buffer.len++;
    }

    th->right_output = right;
    th->left_output = left;
    th->total_samples += 1.0;

    th->next_sample = th->total_samples * th->sample_cycles;
    th->scheduler->only_add_abs(static_cast<i64>(th->next_sample), 0, th, &master_sample_callback, nullptr);
}
}