//
// Created by . on 4/23/25.
//

#include <cassert>

#include "snes_apu.h"
#include "gauss_table.h"
#include "snes_bus.h"

// ratio for one to the other =

namespace SNES::APU {

constexpr int env_periods[32] = {
        0x7FFFFFFF, 2048, 1536,
        1280, 1024, 768,
        640, 512, 384,
        320, 256, 192,
        160, 128, 96,
        80, 64, 48,
        40, 32, 24,
        20, 16, 12,
        10, 8, 6,
        5, 4, 3,
        2, 1
};

void ch::calculate_sample_addr()
{
    u16 brr_entry = ((apu->dsp.io.DIR << 8) + (io.SRCN << 2)) & 0xFFFF;
    sample_data.start_addr = apu->cpu.RAM[brr_entry++];
    sample_data.start_addr |= apu->cpu.RAM[brr_entry++] << 8;
    sample_data.loop_addr = apu->cpu.RAM[brr_entry++];
    sample_data.loop_addr |= apu->cpu.RAM[brr_entry] << 8;
}

void core::calculate_sample_addrs()
{
    for (auto & ch : dsp.channel) {
        ch.calculate_sample_addr();
    }
}

//  ((0) + (4F * 8000)
// ((addr & !0x8000) + (bank & !0x80) * 0x8000
//  ((addr & 0x7FFF) + (bank & 0x7F) * 0x8000
static void BRR_decode(const u8 *buf, sample &dest, filter &filter)
{
    dest.cur_decode_buf ^= 1;
    dest.pos = 0;
    u8 header = *buf;
    buf++;
    if (dest.first_or_loop) {
        dest.first_or_loop = 0;
        header = 0x0;
    }
    u32 scale = (header >> 4) & 15; // BEFORE BRR decoding
    dest.loop = (header >> 1) & 1;
    dest.end = header & 1;
    u32 filter_num = (header >> 2) & 3;
    u32 tn = 0;
    for (u32 i = 0; i < 8; i++) {
        u8 data = *buf;
        buf++;
        for (u32 num = 0; num < 2; num++) {
            i32 nibble = (i32)((data & 0xF0) >> 4);
            data <<= 4;
            nibble = SIGNe4to32(nibble);
            if (scale <= 12) {
                nibble <<= static_cast<i32>(scale);
                nibble >>= 1;
            } else {
                nibble &= ~0x7FF;
            }
            i32 prev1 = filter.prev[0] >> 1;
            i32 prev2 = filter.prev[1] >> 1;
            switch(filter_num) {
                case 0:
                    break;
                case 1:
                    nibble += prev1 + (-prev1 >> 4);
                    break;
                case 2:
                    nibble += (prev1 << 1) + ((-((prev1 << 1) + prev1)) >> 5) - prev2 + (prev2 >> 4);
                    break;
                case 3:
                    nibble += (prev1 << 1) + ((-(prev1 + (prev1 << 2) + (prev1 << 3))) >> 6) - prev2 +
                              (((prev2 << 1) + prev2) >> 4);
                    break;
                default: // clang-tidy sigh
            }
            if (nibble < -32768) nibble = -32768;
            if (nibble > 32767) nibble = 32767;
            nibble = static_cast<i16>(nibble * 2);

            dest.decoded[dest.cur_decode_buf][tn++] = static_cast<i16>(nibble);
            filter.prev[1] = filter.prev[0];
            filter.prev[0] = static_cast<i16>(nibble);
        }
    }
}

u8 ch::read_voice(u8 param) const
{
    switch(param) {
        case 0:
            return io.VOLL;
        case 1:
            return io.VOLR;
        case 2:
            return io.PITCHL;
        case 3:
            return io.PITCHH;
        case 4:
            return io.SRCN;
        case 5:
            return io.ADSR1.v;
        case 6:
            return io.ADSR2.v;
        case 7:
            return io.GAIN.v;
        case 8:
            // TODO: implement this. should set to upper 7 of 11 bit attenuation
            return io.ENVX;
        case 9:
            // TODO: implement this. should set to upper 8 bits of current sample after env but before vxvol
            return io.OUTX;
        default:
    }
    printf("\nMISSED READ VOICE ATTR %d CH:%d", param, num);
    return 0;
}

static void clear_kon(void *ptr, u64 key, u64 timecode, u32 jitter)
{
    auto *snes = static_cast<SNES::core *>(ptr);
    snes->apu.dsp.io.KON = 0;
}

static u8 readDSP(void *ptr, u16 addr)
{
    auto *snes = static_cast<SNES::core *>(ptr);
    u32 ch_num = (addr >> 4) & 7;
    ch &ch = snes->apu.dsp.channel[ch_num];
    u32 param = addr & 15;
    if (param < 0x0A)
        return ch.read_voice(param);

    switch (addr) {
        case 0x0c:
            return snes->apu.dsp.io.MVOLL & 0xFF;
        case 0x1C:
            return snes->apu.dsp.io.MVOLR & 0xFF;
        case 0x2C:
            return snes->apu.dsp.io.EVOLL;
        case 0x3C:
            return snes->apu.dsp.io.EVOLR;
        case 0x4C:
            snes->scheduler.only_add_abs(static_cast<i64>(static_cast<long double>(snes->clock.master_cycle_count) + (63.0 * snes->clock.apu.ratio)), 0, snes, &clear_kon, nullptr);
            return snes->apu.dsp.io.KON;
        case 0x5C:
            return snes->apu.dsp.io.KOFF;

        case 0x6C:
            return snes->apu.dsp.io.FLG.u;

        case 0x7C: {
            return snes->apu.dsp.io.ENDX;}

        case 0x0D:
            return snes->apu.dsp.io.EFB;
        case 0x1D:
            return snes->apu.dsp.io.unused;
        case 0x2D:
            return snes->apu.dsp.io.PMON;
        case 0x3D:
            return snes->apu.dsp.io.NON;
        case 0x4D:
            return snes->apu.dsp.io.EON;
        case 0x5D:
            return snes->apu.dsp.io.DIR;
        case 0x6D:
            return snes->apu.dsp.io.ESA;
        case 0x7D:
            return snes->apu.dsp.io.EDL;
        case 0x0F:
        case 0x1F:
        case 0x2F:
        case 0x3F:
        case 0x4F:
        case 0x5F:
        case 0x6F:
        case 0x7F:
            return snes->apu.dsp.io.FIR[ch_num];
        default:
    }
    printf("\nMISSED DSP READ TO %02x", addr);
    return 0;
}

static void do_noise(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *snes = static_cast<SNES::core *>(ptr);

    const s32 feedback = snes->apu.dsp.noise.level << 13 ^ snes->apu.dsp.noise.level << 14;
    snes->apu.dsp.noise.level = static_cast<i16>(feedback & 0x4000 | snes->apu.dsp.noise.level >> 1);

    snes->apu.dsp.noise.next_update += snes->apu.dsp.noise.stride;
    snes->apu.dsp.noise.sch_id = snes->scheduler.only_add_abs(static_cast<i64>(snes->apu.dsp.noise.next_update), 0, snes, &do_noise, &snes->apu.dsp.noise.sch_still);
}

void ch::update_env()
{
    //printf("\nDO ENV %lld STATE:%d ATTENUATION:%d", key, env.state, env.attenuation);
    env.counter++;
    if (env.counter >= env.current_period) {
        env.counter = 0;
        if (io.ADSR1.adsr_on && env.state == APU_envelope::ADSR_state::release && env.attenuation == 0) {
            // No need to keep subtracting from 0
            //printf("\nEARLY EXIT ON RELEASE");
            return;
        }

        u32 rate = 0;
        i32 mod = 0;
        if (io.ADSR1.adsr_on || env.state == APU_envelope::ADSR_state::release) {
            switch (env.state) {
                case APU_envelope::ADSR_state::attack:
                    rate = (env.attack_rate * 2) + 1;
                    if (rate == 31) mod = 1024;
                    else mod = 32;
                    break;
                case APU_envelope::ADSR_state::decay:
                    rate = (env.decay_rate * 2) + 16;
                    mod = -(((env.attenuation - 1) >> 8) + 1);
                    break;
                case APU_envelope::ADSR_state::sustain:
                    rate = env.sustain_rate;
                    mod = -(((env.attenuation - 1) >> 8) + 1);
                    break;
                case APU_envelope::ADSR_state::release:
                    rate = 31;
                    mod = -8;
                    break;
            }
            env.attenuation += mod;

            // Update phase
            if (env.state == APU_envelope::ADSR_state::attack && env.attenuation >= 0x7E0) {
                env.attenuation = (env.attenuation > 0x7FF) ? 0x7FFF : env.attenuation;
                env.state = APU_envelope::ADSR_state::decay;
                change_envelope(calc_env_rate());
            }
            if (env.state == APU_envelope::ADSR_state::decay && env.attenuation <= env.sustain_level) {
                env.state = APU_envelope::ADSR_state::sustain;
                change_envelope(calc_env_rate());
            }
        } else {
            if (!io.GAIN.custom.custom_gain) {
                env.attenuation = (io.GAIN.direct.fixed_vol << 4);
                return;
            }
            rate = io.GAIN.custom.gain_rate;
            env.current_period = env_periods[rate];
            switch (io.GAIN.custom.gain_mode) {
                case 0:
                    mod = -32;
                    break;
                case 1:
                    mod = -(((env.attenuation - 1) >> 8) + 1);
                    break;
                case 2:
                    mod = 32;
                    break;
                case 3:
                    mod = env.attenuation < 0x600 ? 32 : 8;
                    break;
                default:
            }
            env.attenuation += mod;
        }
        if (env.attenuation < 0) env.attenuation = 0;
        if (env.attenuation > 0x7FF) env.attenuation = 0x7FF;
    }
}

void DSP::update_noise()
{
    if (noise.sch_still) apu->snes->scheduler.delete_if_exist(noise.sch_id);
    if (io.FLG.noise_freq) {
        noise.stride = static_cast<long double>(env_periods[io.FLG.noise_freq]) * apu->snes->clock.apu.env.stride;
        noise.next_update = static_cast<long double>(apu->snes->clock.master_cycle_count) + noise.stride;
        noise.sch_id = apu->snes->scheduler.only_add_abs(static_cast<i64>(noise.next_update), 0, apu->snes, &do_noise, &noise.sch_still);
    }
}

u32 ch::calc_env_rate() const
{
    if (io.ADSR1.adsr_on || env.state == APU_envelope::ADSR_state::release) { // ADSR mode
        switch (env.state) {
            case APU_envelope::ADSR_state::attack:
                return (env.attack_rate * 2) + 1;
            case APU_envelope::ADSR_state::decay:
                return (env.decay_rate * 2) + 16;
            case APU_envelope::ADSR_state::sustain:
                return env.sustain_rate;
            case APU_envelope::ADSR_state::release:
                return 31;
        }
    }
    else if (io.GAIN.custom.custom_gain) {
        return io.GAIN.custom.gain_rate;
    }
    return 0;

}

void ch::change_envelope(u32 rate) {
    if (!io.ADSR1.adsr_on && !io.GAIN.custom.custom_gain) {
        env.attenuation = io.GAIN.direct.fixed_vol << 4;
        rate = 0;
    }
    env.current_period = env_periods[rate];
}


void ch::write_voice(u8 param, u8 val)
{
    switch(param) {
        case 0:
            io.VOLL = SIGNe8to32(val);
            return;
        case 1:
            io.VOLR = SIGNe8to32(val);
            return;
        case 2: {
            //u32 old_pitch = (io.PITCHH << 8) | io.PITCHL;
            io.PITCHL = val;
            return; }
        case 3: {
            //u32 old_pitch = (io.PITCHH << 8) | io.PITCHL;
            io.PITCHH = val & 0x3F;
            return; }
        case 4:
            io.SRCN = val;
            calculate_sample_addr();
            return;
        case 5:
            io.ADSR1.v = val;
            env.attack_rate = io.ADSR1.attack_rate;
            env.decay_rate = io.ADSR1.decay_rate;
            change_envelope(calc_env_rate());
            return;
        case 6:
            io.ADSR2.v = val;
            env.sustain_rate = io.ADSR2.sustain_level;
            env.sustain_level = (io.ADSR2.sustain_level + 1) << 8;
            change_envelope(calc_env_rate());
            return;
        case 7:
            io.GAIN.v = val;
            change_envelope(calc_env_rate());
            return;
        case 8:
            io.ENVX = val;
            return;
        case 9:
            io.OUTX = val;
            return;
        default:
    }
    printf("\nMISSED VOICE WRITE TO %d: %02x", param, val);
}

void ch::keyon()
{
    ended = 0;
    pitch.counter = 0;
    env.counter = 0;
    sample_data.pos = 15; // To force new block read
    sample_data.next_read_addr = sample_data.start_addr; // Start at start of sample

    sample_data.end = 0; // Make sure we don't immediately end
    sample_data.loop = 0;
    sample_data.first_or_loop = 1;

    pitch.counter = 0;

    assert(sample_data.next_read_addr < (0xFFFF - 8));
    BRR_decode(apu->cpu.RAM + sample_data.next_read_addr, sample_data, filter);
    if (sample_data.end)apu->dsp.io.ENDX |= (1 << num);
    sample_data.next_read_addr = (sample_data.next_read_addr + 9) & 0xFFFF;

    env.state = APU_envelope::ADSR_state::attack;
    if (io.ADSR1.adsr_on || io.GAIN.custom.custom_gain) env.attenuation = 0;
    change_envelope(calc_env_rate());
}

void ch::keyoff()
{
    env.state = APU_envelope::ADSR_state::release;
    change_envelope(calc_env_rate());
}

static void writeDSP(void *ptr, u16 addr, u8 val)
{
    auto *snes = static_cast<SNES::core *>(ptr);
    u32 ch_num = (addr >> 4) & 7;
    ch &ch = snes->apu.dsp.channel[ch_num];
    u32 param = addr & 15;

    if (param < 0x0A)
        return ch.write_voice(param, val);

    DSP *th = &snes->apu.dsp;
    switch (addr) {
        case 0x0c:
            th->io.MVOLL = SIGNe8to32(val);
            return;
        case 0x1C:
            th->io.MVOLR = SIGNe8to32(val);
            return;
        case 0x2C:
            th->io.EVOLL = val;
            return;
        case 0x3C:
            th->io.EVOLR = val;
            return;
        case 0x4C: {
            th->io.KON = val;
            for (u32 i = 0; i < 8; i++) {
                if ((val >> i) & 1) snes->apu.dsp.channel[i].keyon();
            }
            return;
        }
        case 0x5C:
            th->io.KOFF = val;
            for (u32 i = 0; i < 8; i++) {
                if ((val >> i) & 1) snes->apu.dsp.channel[i].keyoff();
            }
            return;
        case 0x6C:
            th->io.FLG.u = val;
            if (th->io.FLG.soft_reset) {
                th->io.FLG.soft_reset = 0;
                for (auto & mch : th->channel) {
                    mch.env.attenuation = 0;
                    mch.keyoff();
                }
            }
            th->update_noise();
            return;
        case 0x7C: {
            th->io.ENDX = 0;
            return; }
        case 0x0D:
            th->io.EFB = val;
            return;
        case 0x1D:
            th->io.unused = val;
            return;
        case 0x2D:
            th->io.PMON = val & 0xFE; // TODO: FF?
            if (th->io.PMON) {
                static int a = 0;
                if (a < 5) {
                    a++;
                    printf("\nWARN PMON ENABLED FOR S-DSP!");
                }
            }
            return;
        case 0x3D:
            th->io.NON = val;
            return;
        case 0x4D:
            th->io.EON = val;
            return;
        case 0x5D:
            th->io.DIR = val;
            snes->apu.calculate_sample_addrs();
            return;
        case 0x6D:
            th->io.ESA = val;
            return;
        case 0x7D:
            th->io.EDL = val;
            return;
        case 0x0F:
        case 0x1F:
        case 0x2F:
        case 0x3F:
        case 0x4F:
        case 0x5F:
        case 0x6F:
        case 0x7F:
            th->io.FIR[ch_num] = val;
            return;
        default:
    }
    printf("\nMissed DSP write %02x to %02x:", addr, val);
}

DSP::DSP(APU::core *apu_in) : apu(apu_in), channel{} {
    for (u32 i = 0; i < 8; i++) {
        auto &ch = channel[i];
        ch.apu = apu_in;
        ch.num = i;
        ch.ended = 1;
    }
};

core::core(SNES::core *parent, u64 *clock_ptr) : snes(parent), cpu(clock_ptr), dsp(this)
{
    cpu.read_dsp = &readDSP;
    cpu.write_dsp = &writeDSP;
    cpu.read_ptr = snes;
    cpu.write_ptr = snes;
}

void core::reset()
{
    cpu.reset();
}

static void CPU_cycle(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *snes = static_cast<SNES::core *>(ptr);
    //u64 cur = clock - jitter;

    snes->apu.cpu.cycle(1);

    // TODO: skip - amounts when scheduling
    /*i32 num = 0 - snes->apu.cpu.cycles;
    if (num < 1) num = 1;
    snes->clock.apu.cycle.next += ((long double)num * snes->clock.apu.cycle.stride);
    snes->apu.cpu.cycles = 0;*/
    snes->clock.apu.cycle.next += snes->clock.apu.cycle.stride;
    snes->scheduler.only_add_abs(static_cast<i64>(snes->clock.apu.cycle.next), 0, snes, &CPU_cycle, nullptr);
}


i16 ch::gaussian_me_up() const {
    i32 sample_num = static_cast<i32>((pitch.counter >> 12) & 15);
    const u32 g_index = (pitch.counter >> 4) & 0xFF;
    u32 bufn = sample_data.cur_decode_buf;
    const i32 mnew = sample_data.decoded[bufn][sample_num];
    sample_num--; if (sample_num < 0) { sample_num = 15; bufn ^= 1; }
    const i32 old = sample_data.decoded[bufn][sample_num];
    sample_num--; if (sample_num < 0) { sample_num = 15; bufn ^= 1; }
    const i32 older = sample_data.decoded[bufn][sample_num];
    sample_num--; if (sample_num < 0) { sample_num = 15; bufn ^= 1; }
    const i32 oldest = sample_data.decoded[bufn][sample_num];

    i32 out = (gauss_table[0xFF-g_index] * oldest) >> 10;
    out += (gauss_table[0x1FF-g_index] * older) >> 10;
    out += (gauss_table[0x100+g_index] * old) >> 10;
    out += (gauss_table[g_index] * mnew) >> 10;
    out >>= 1;
    if (out < -16384) out = -16384;
    if (out > 16383) out = 16383;

    return static_cast<i16>(out);
}

void ch::update_sample()
{
    u32 step = (io.PITCHH << 8) | io.PITCHL;
    if (num > 0 && apu->dsp.io.PMON & (1 << num)) {
        assert(0);
    }
    if (ended) return;
    pitch.counter += step;
    if (pitch.counter > 0xFFFF) {
        // next BRR block
        pitch.counter -= 0x10000;
        if (sample_data.end) {
            sample_data.next_read_addr = sample_data.loop_addr;
            if (!sample_data.loop) {
                ended = 1;
                return;
            }
            //sample_data.first_or_loop = 1;
        }
        assert(sample_data.next_read_addr < (0xFFFF - 8));
        BRR_decode(apu->cpu.RAM + sample_data.next_read_addr, sample_data, filter);
        if (sample_data.end) apu->dsp.io.ENDX |= (1 << num);
        sample_data.next_read_addr = (sample_data.next_read_addr + 9) & 0xFFFF;
    }

    i32 smp;
    if (apu->dsp.io.NON & (1 << num)) smp = apu->dsp.noise.level;
    else smp = gaussian_me_up();
    smp = (smp * env.attenuation) >> 11;
    io.OUTX = (smp >> 7) & 0xFF;
    output.debug = static_cast<i16>(smp);

    // smp = smp * vol / 128;
    output.l = static_cast<i16>((smp * io.VOLL) >> 7);
    output.r = static_cast<i16>((smp * io.VOLR) >> 7);
    if (output.l > 32767) output.l = 32767;
    if (output.l < -32768) output.l = -32768;
    if (output.r > 32767) output.r = 32767;
    if (output.r < -32768) output.r = -32768;
}

void core::mix_sample()
{
    i32 out_r;
    i32 out_l = out_r = 0;

    for (auto & ch : dsp.channel) {
        if (!ch.ext_enable || !ch.env.attenuation || ch.ended) continue;

        out_l += ch.output.l;
        out_r += ch.output.r;
        if (out_l < -32768) out_l = -32768;
        if (out_l > 32767) out_l = 32767;
        if (out_r < -32768) out_r = -32768;
        if (out_r > 32767) out_r = 32767;
    }
    out_l = (out_l * dsp.io.MVOLL) >> 7;
    out_r = (out_r * dsp.io.MVOLR) >> 7;
    if (out_l < -32768) out_l = -32768;
    if (out_l > 32767) out_l = 32767;
    if (out_r < -32768) out_r = -32768;
    if (out_r > 32767) out_r = 32767;
    if (!dsp.ext_enable || dsp.io.FLG.mute_all) {
        out_l = out_r = 0;
    }
    dsp.output.l = out_l;
    dsp.output.r = out_r;
}

static void DSP_cycle(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *snes = static_cast<SNES::core *>(ptr);

    for (auto & ch : snes->apu.dsp.channel) {
        ch.update_env();
        ch.update_sample();
    }

    snes->apu.mix_sample();

    snes->clock.apu.sample.next += snes->clock.apu.sample.stride;
    snes->scheduler.only_add_abs(static_cast<i64>(snes->clock.apu.sample.next), 0, snes, &DSP_cycle, nullptr);

    snes->apu.dsp.sample.func(snes->apu.dsp.sample.ptr, 0, clock, jitter);
}

void core::schedule_first()
{
    snes->clock.apu.cycle.next = snes->clock.apu.cycle.stride;
    snes->clock.apu.sample.next = snes->clock.apu.sample.stride;

    snes->scheduler.only_add_abs(static_cast<i64>(snes->clock.apu.cycle.next), 0, snes, &CPU_cycle, nullptr);
    snes->scheduler.only_add_abs(static_cast<i64>(snes->clock.apu.sample.next), 0, snes, &DSP_cycle, nullptr);
    for (auto & ch : dsp.channel) {
        ch.io.PITCHL = 0;
        ch.io.PITCHH = 0;
        ch.env.state = ch::APU_envelope::ADSR_state::release;
        ch.env.attenuation = 0;
        ch.io.ADSR1.adsr_on = 1;
        ch.change_envelope(0);
    }
}

u32 core::read(u32 addr, u32 old, bool has_effect) const {
    return cpu.io.CPUO[addr & 3];
}

void core::write(u32 addr, u32 val)
{
    cpu.io.CPUI[addr & 3] = val;
}


}