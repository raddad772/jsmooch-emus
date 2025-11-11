//
// Created by . on 4/23/25.
//

#include "snes_apu.h"
#include "gauss_table.h"
#include "snes_bus.h"

// ratio for one to the other =
static void update_envelope(SNES *snes, SNES_APU_ch *ch, u32 rate);
static u32 calc_env_rate(SNES *snes, SNES_APU_ch *ch);


static const int env_periods[32] = {
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

static inline void calculate_sample_addr(SNES *snes, SNES_APU_ch *ch)
{
    u16 brr_entry = ((snes->apu.dsp.io.DIR << 8) + (ch->io.SRCN << 2)) & 0xFFFF;
    ch->sample_data.start_addr = snes->apu.cpu.RAM[brr_entry++];
    ch->sample_data.start_addr |= snes->apu.cpu.RAM[brr_entry++] << 8;
    ch->sample_data.loop_addr = snes->apu.cpu.RAM[brr_entry++];
    ch->sample_data.loop_addr |= snes->apu.cpu.RAM[brr_entry] << 8;
}

static void calculate_sample_addrs(SNES *snes)
{
    for (u32 i = 0; i < 8; i++) {
        struct SNES_APU_ch *ch = &snes->apu.dsp.channel[i];
        calculate_sample_addr(snes, ch);
    }
}

//  ((0) + (4F * 8000)
// ((addr & !0x8000) + (bank & !0x80) * 0x8000
//  ((addr & 0x7FFF) + (bank & 0x7F) * 0x8000
static void BRR_decode(u8 *buf, SNES_APU_sample *dest, SNES_APU_filter *filter)
{
    dest->cur_decode_buf ^= 1;
    dest->pos = 0;
    u8 header = *buf;
    buf++;
    if (dest->first_or_loop) {
        dest->first_or_loop = 0;
        header = 0x0;
    }
    u32 scale = (header >> 4) & 15; // BEFORE BRR decoding
    dest->loop = (header >> 1) & 1;
    dest->end = header & 1;
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
                nibble <<= scale;
                nibble >>= 1;
            } else {
                nibble &= ~0x7FF;
            }
            i32 prev1 = filter->prev[0] >> 1;
            i32 prev2 = filter->prev[1] >> 1;
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
            }
            if (nibble < -32768) nibble = -32768;
            if (nibble > 32767) nibble = 32767;
            nibble = (i16)(nibble * 2);

            dest->decoded[dest->cur_decode_buf][tn++] = (i16)(nibble);
            filter->prev[1] = filter->prev[0];
            filter->prev[0] = (i16)nibble;
        }
    }
}

static u8 read_voice(SNES *snes, SNES_APU_ch *ch, u8 param)
{
    switch(param) {
        case 0:
            return ch->io.VOLL;
        case 1:
            return ch->io.VOLR;
        case 2:
            return ch->io.PITCHL;
        case 3:
            return ch->io.PITCHH;
        case 4:
            return ch->io.SRCN;
        case 5:
            return ch->io.ADSR1.v;
        case 6:
            return ch->io.ADSR2.v;
        case 7:
            return ch->io.GAIN.v;
        case 8:
            // TODO: implement this. should set to upper 7 of 11 bit attenuation
            return ch->io.ENVX;
        case 9:
            // TODO: implement this. should set to upper 8 bits of current sample after env but before vxvol
            return ch->io.OUTX;

    }
    printf("\nMISSED READ VOICE ATTR %d CH:%d", param, ch->num);
    return 0;
}

static void clear_kon(void *ptr, u64 key, u64 timecode, u32 jitter)
{
    struct SNES *snes = (SNES *)ptr;
    snes->apu.dsp.io.KON = 0;
}

static u8 read_DSP(void *ptr, u16 addr)
{
    struct SNES *snes = (SNES *)ptr;
    u32 ch_num = (addr >> 4) & 7;
    struct SNES_APU_ch *ch = &snes->apu.dsp.channel[ch_num];
    u32 param = addr & 15;
    if (param < 0x0A)
        return read_voice(snes, ch, param);

    struct SNES_APU_DSP *this = &snes->apu.dsp;
    switch (addr) {
        case 0x0c:
            return this->io.MVOLL & 0xFF;
        case 0x1C:
            return this->io.MVOLR & 0xFF;
        case 0x2C:
            return this->io.EVOLL;
        case 0x3C:
            return this->io.EVOLR;
        case 0x4C:
            scheduler_only_add_abs(&snes->scheduler, snes->clock.master_cycle_count + (63.0 * snes->clock.apu.ratio), 0, snes, &clear_kon, NULL);
            return this->io.KON;
        case 0x5C:
            return this->io.KOFF;

        case 0x6C:
            return this->io.FLG.u;

        case 0x7C: {
            return this->io.ENDX;}

        case 0x0D:
            return this->io.EFB;
        case 0x1D:
            return this->io.unused;
        case 0x2D:
            return this->io.PMON;
        case 0x3D:
            return this->io.NON;
        case 0x4D:
            return this->io.EON;
        case 0x5D:
            return this->io.DIR;
        case 0x6D:
            return this->io.ESA;
        case 0x7D:
            return this->io.EDL;
        case 0x0F:
        case 0x1F:
        case 0x2F:
        case 0x3F:
        case 0x4F:
        case 0x5F:
        case 0x6F:
        case 0x7F:
            return this->io.FIR[ch_num];
    }
    printf("\nMISSED DSP READ TO %02x", addr);
    return 0;
}

static void do_noise(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *snes = (SNES *)ptr;
    struct SNES_APU_DSP *this = &snes->apu.dsp;

    s32 feedback = this->noise.level << 13 ^ this->noise.level << 14;
    this->noise.level = feedback & 0x4000 | this->noise.level >> 1;

    this->noise.next_update += this->noise.stride;
    this->noise.sch_id = scheduler_only_add_abs(&snes->scheduler, (i64)this->noise.next_update, 0, snes, &do_noise, &this->noise.sch_still);
}

static void ch_update_env(SNES *snes, SNES_APU_ch *ch)
{
    //printf("\nDO ENV %lld STATE:%d ATTENUATION:%d", key, ch->env.state, ch->env.attenuation);
    ch->env.counter++;
    if (ch->env.counter >= ch->env.current_period) {
        ch->env.counter = 0;
        if (ch->io.ADSR1.adsr_on && ch->env.state == SDEM_release && ch->env.attenuation == 0) {
            // No need to keep subtracting from 0
            //printf("\nEARLY EXIT ON RELEASE");
            return;
        }

        u32 rate = 0;
        i32 mod = 0;
        if (ch->io.ADSR1.adsr_on || ch->env.state == SDEM_release) {
            switch (ch->env.state) {
                case SDEM_attack:
                    rate = (ch->env.attack_rate * 2) + 1;
                    if (rate == 31) mod = 1024;
                    else mod = 32;
                    break;
                case SDEM_decay:
                    rate = (ch->env.decay_rate * 2) + 16;
                    mod = -(((ch->env.attenuation - 1) >> 8) + 1);
                    break;
                case SDEM_sustain:
                    rate = ch->env.sustain_rate;
                    mod = -(((ch->env.attenuation - 1) >> 8) + 1);
                    break;
                case SDEM_release:
                    rate = 31;
                    mod = -8;
                    break;
            }
            ch->env.attenuation += mod;

            // Update phase
            if (ch->env.state == SDEM_attack && ch->env.attenuation >= 0x7E0) {
                ch->env.attenuation = (ch->env.attenuation > 0x7FF) ? 0x7FFF : ch->env.attenuation;
                ch->env.state = SDEM_decay;
                update_envelope(snes, ch, calc_env_rate(snes, ch));
            }
            if (ch->env.state == SDEM_decay && ch->env.attenuation <= ch->env.sustain_level) {
                ch->env.state = SDEM_sustain;
                update_envelope(snes, ch, calc_env_rate(snes, ch));
            }
        } else {
            if (!ch->io.GAIN.custom.custom_gain) {
                ch->env.attenuation = (ch->io.GAIN.direct.fixed_vol << 4);
                return;
            }
            rate = ch->io.GAIN.custom.gain_rate;
            ch->env.current_period = env_periods[rate];
            switch (ch->io.GAIN.custom.gain_mode) {
                case 0:
                    mod = -32;
                    break;
                case 1:
                    mod = -(((ch->env.attenuation - 1) >> 8) + 1);
                    break;
                case 2:
                    mod = 32;
                    break;
                case 3:
                    mod = ch->env.attenuation < 0x600 ? 32 : 8;
                    break;
            }
            ch->env.attenuation += mod;
        }
        if (ch->env.attenuation < 0) ch->env.attenuation = 0;
        if (ch->env.attenuation > 0x7FF) ch->env.attenuation = 0x7FF;
    }
}

static void update_noise(SNES *snes)
{
    struct SNES_APU_DSP *this = &snes->apu.dsp;
    if (this->noise.sch_still) scheduler_delete_if_exist(&snes->scheduler, this->noise.sch_id);
    if (this->io.FLG.noise_freq) {
        this->noise.stride = (long double)env_periods[this->io.FLG.noise_freq] * snes->clock.apu.env.stride;
        this->noise.next_update = snes->clock.master_cycle_count + this->noise.stride;
        this->noise.sch_id = scheduler_only_add_abs(&snes->scheduler, (i64)this->noise.next_update, 0, snes, &do_noise, &this->noise.sch_still);
    }
}

static u32 calc_env_rate(SNES *snes, SNES_APU_ch *ch)
{
    if (ch->io.ADSR1.adsr_on || ch->env.state == SDEM_release) { // ADSR mode
        switch (ch->env.state) {
            case SDEM_attack:
                return (ch->env.attack_rate * 2) + 1;
            case SDEM_decay:
                return (ch->env.decay_rate * 2) + 16;
            case SDEM_sustain:
                return ch->env.sustain_rate;
            case SDEM_release:
                return 31;
        }
    }
    else if (ch->io.GAIN.custom.custom_gain) {
        return ch->io.GAIN.custom.gain_rate;
    }
    return 0;

}

static void update_envelope(SNES *snes, SNES_APU_ch *ch, u32 rate) {
    if (!ch->io.ADSR1.adsr_on && !ch->io.GAIN.custom.custom_gain) {
        ch->env.attenuation = ch->io.GAIN.direct.fixed_vol << 4;
        rate = 0;
    }
    ch->env.current_period = env_periods[rate];
}


static void write_voice(SNES *snes, SNES_APU_ch * ch, u8 param, u8 val)
{
    switch(param) {
        case 0:
            ch->io.VOLL = SIGNe8to32(val);
            return;
        case 1:
            ch->io.VOLR = SIGNe8to32(val);
            return;
        case 2: {
            u32 old_pitch = (ch->io.PITCHH << 8) | ch->io.PITCHL;
            ch->io.PITCHL = val;
            return; }
        case 3: {
            u32 old_pitch = (ch->io.PITCHH << 8) | ch->io.PITCHL;
            ch->io.PITCHH = val & 0x3F;
            return; }
        case 4:
            ch->io.SRCN = val;
            calculate_sample_addr(snes, ch);
            return;
        case 5:
            ch->io.ADSR1.v = val;
            ch->env.attack_rate = ch->io.ADSR1.attack_rate;
            ch->env.decay_rate = ch->io.ADSR1.decay_rate;
            update_envelope(snes, ch, calc_env_rate(snes, ch));
            return;
        case 6:
            ch->io.ADSR2.v = val;
            ch->env.sustain_rate = ch->io.ADSR2.sustain_level;
            ch->env.sustain_level = (ch->io.ADSR2.sustain_level + 1) << 8;
            update_envelope(snes, ch, calc_env_rate(snes, ch));
            return;
        case 7:
            ch->io.GAIN.v = val;
            update_envelope(snes, ch, calc_env_rate(snes, ch));
            return;
        case 8:
            ch->io.ENVX = val;
            return;
        case 9:
            ch->io.OUTX = val;
            return;
    }
    printf("\nMISSED VOICE WRITE TO %d: %02x", param, val);
}

static void keyon(SNES *snes, u32 ch_num)
{
    struct SNES_APU_ch *ch = &snes->apu.dsp.channel[ch_num];
    ch->ended = 0;
    ch->pitch.counter = 0;
    ch->env.counter = 0;
    ch->sample_data.pos = 15; // To force new block read
    ch->sample_data.next_read_addr = ch->sample_data.start_addr; // Start at start of sample

    ch->sample_data.end = 0; // Make sure we don't immediately end
    ch->sample_data.loop = 0;
    ch->sample_data.first_or_loop = 1;

    ch->pitch.counter = 0;

    assert(ch->sample_data.next_read_addr < (0xFFFF - 8));
    BRR_decode(snes->apu.cpu.RAM + ch->sample_data.next_read_addr, &ch->sample_data, &ch->filter);
    if (ch->sample_data.end) snes->apu.dsp.io.ENDX |= (1 << ch->num);
    ch->sample_data.next_read_addr = (ch->sample_data.next_read_addr + 9) & 0xFFFF;

    ch->env.state = SDEM_attack;
    if (ch->io.ADSR1.adsr_on || ch->io.GAIN.custom.custom_gain) ch->env.attenuation = 0;
    update_envelope(snes, ch, calc_env_rate(snes, ch));
}

static void keyoff(SNES *snes, u32 ch_num)
{
    struct SNES_APU_ch *ch = &snes->apu.dsp.channel[ch_num];
    ch->env.state = SDEM_release;
    update_envelope(snes, ch, calc_env_rate(snes, ch));
}

static void write_DSP(void *ptr, u16 addr, u8 val)
{
    struct SNES *snes = (SNES *)ptr;
    u32 ch_num = (addr >> 4) & 7;
    struct SNES_APU_ch *ch = &snes->apu.dsp.channel[ch_num];
    u32 param = addr & 15;

    if (param < 0x0A)
        return write_voice(snes, ch, param, val);

    struct SNES_APU_DSP *this = &snes->apu.dsp;
    switch (addr) {
        case 0x0c:
            this->io.MVOLL = SIGNe8to32(val);
            return;
        case 0x1C:
            this->io.MVOLR = SIGNe8to32(val);
            return;
        case 0x2C:
            this->io.EVOLL = val;
            return;
        case 0x3C:
            this->io.EVOLR = val;
            return;
        case 0x4C:
            this->io.KON = val;
            for (u32 i = 0; i < 8; i++) {
                if ((val >> i) & 1) keyon(snes, i);
            }
            return;
        case 0x5C:
            this->io.KOFF = val;
            for (u32 i = 0; i < 8; i++) {
                if ((val >> i) & 1) keyoff(snes, i);
            }
            return;
        case 0x6C:
            this->io.FLG.u = val;
            if (this->io.FLG.soft_reset) {
                this->io.FLG.soft_reset = 0;
                for (u32 i = 0; i < 8; i++) {
                    this->channel[i].env.attenuation = 0;
                    keyoff(snes, i);
                }
            }
            update_noise(snes);
            return;
        case 0x7C: {
            this->io.ENDX = 0;
            return; }
        case 0x0D:
            this->io.EFB = val;
            return;
        case 0x1D:
            this->io.unused = val;
            return;
        case 0x2D:
            this->io.PMON = val & 0xFE; // TODO: FF?
            if (this->io.PMON) {
                static int a = 0;
                if (a < 5) {
                    a++;
                    printf("\nWARN PMON ENABLED FOR S-DSP!");
                }
            }
            return;
        case 0x3D:
            this->io.NON = val;
            return;
        case 0x4D:
            this->io.EON = val;
            return;
        case 0x5D:
            this->io.DIR = val;
            calculate_sample_addrs(snes);
            return;
        case 0x6D:
            this->io.ESA = val;
            return;
        case 0x7D:
            this->io.EDL = val;
            return;
        case 0x0F:
        case 0x1F:
        case 0x2F:
        case 0x3F:
        case 0x4F:
        case 0x5F:
        case 0x6F:
        case 0x7F:
            this->io.FIR[ch_num] = val;
            return;
    }
    printf("\nMissed DSP write %02x to %02x:", addr, val);
}


void SNES_APU_init(SNES *snes)
{
    SPC700_init(&snes->apu.cpu, &snes->clock.master_cycle_count);
    snes->apu.cpu.read_dsp = &read_DSP;
    snes->apu.cpu.write_dsp = &write_DSP;
    snes->apu.cpu.read_ptr = snes;
    snes->apu.cpu.write_ptr = snes;
    for (u32 i = 0; i < 8; i++) {
        struct SNES_APU_ch *ch = &snes->apu.dsp.channel[i];
        ch->num = i;
        ch->ended = 1;
    }
}

void SNES_APU_delete(SNES *snes)
{
    SPC700_delete(&snes->apu.cpu);
}

void SNES_APU_reset(SNES *snes)
{
    SPC700_reset(&snes->apu.cpu);
}

static void CPU_cycle(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *snes = (SNES *)ptr;
    u64 cur = clock - jitter;

    SPC700_cycle(&snes->apu.cpu, 1);

    // TODO: skip - amounts when scheduling
    /*i32 num = 0 - snes->apu.cpu.cycles;
    if (num < 1) num = 1;
    snes->clock.apu.cycle.next += ((long double)num * snes->clock.apu.cycle.stride);
    snes->apu.cpu.cycles = 0;*/
    snes->clock.apu.cycle.next += snes->clock.apu.cycle.stride;
    scheduler_only_add_abs(&snes->scheduler, (i64)snes->clock.apu.cycle.next, 0, snes, &CPU_cycle, NULL);
}


static i16 gaussian_me_up(SNES *snes, SNES_APU_ch *ch)
{
    i32 sample_num = (i32)((ch->pitch.counter >> 12) & 15);
    u32 g_index = (ch->pitch.counter >> 4) & 0xFF;
    u32 bufn = ch->sample_data.cur_decode_buf;
    i32 new = ch->sample_data.decoded[bufn][sample_num];
    sample_num--; if (sample_num < 0) { sample_num = 15; bufn ^= 1; }
    i32 old = ch->sample_data.decoded[bufn][sample_num];
    sample_num--; if (sample_num < 0) { sample_num = 15; bufn ^= 1; }
    i32 older = ch->sample_data.decoded[bufn][sample_num];
    sample_num--; if (sample_num < 0) { sample_num = 15; bufn ^= 1; }
    i32 oldest = ch->sample_data.decoded[bufn][sample_num];

    i32 out = (gauss_table[0xFF-g_index] * oldest) >> 10;
    out += (gauss_table[0x1FF-g_index] * older) >> 10;
    out += (gauss_table[0x100+g_index] * old) >> 10;
    out += (gauss_table[g_index] * new) >> 10;
    out >>= 1;
    if (out < -16384) out = -16384;
    if (out > 16383) out = 16383;

    return out;
}

static void ch_update_sample(SNES *snes, SNES_APU_ch *ch)
{
    u32 step = (ch->io.PITCHH << 8) | ch->io.PITCHL;
    if (ch->num > 0 && snes->apu.dsp.io.PMON & (1 << ch->num)) {
        assert(0);
    }
    if (ch->ended) return;
    ch->pitch.counter += step;
    if (ch->pitch.counter > 0xFFFF) {
        // next BRR block
        ch->pitch.counter -= 0x10000;
        if (ch->sample_data.end) {
            ch->sample_data.next_read_addr = ch->sample_data.loop_addr;
            if (!ch->sample_data.loop) {
                ch->ended = 1;
                return;
            }
            //ch->sample_data.first_or_loop = 1;
        }
        assert(ch->sample_data.next_read_addr < (0xFFFF - 8));
        BRR_decode(snes->apu.cpu.RAM + ch->sample_data.next_read_addr, &ch->sample_data, &ch->filter);
        if (ch->sample_data.end) snes->apu.dsp.io.ENDX |= (1 << ch->num);
        ch->sample_data.next_read_addr = (ch->sample_data.next_read_addr + 9) & 0xFFFF;
    }

    i32 smp;
    if (snes->apu.dsp.io.NON & (1 << ch->num)) smp = snes->apu.dsp.noise.level;
    else smp = gaussian_me_up(snes, ch);
    smp = (smp * ch->env.attenuation) >> 11;
    ch->io.OUTX = (smp >> 7) & 0xFF;
    ch->output.debug = smp;

    // smp = smp * vol / 128;
    ch->output.l = (i16)((smp * ch->io.VOLL) >> 7);
    ch->output.r = (i16)((smp * ch->io.VOLR) >> 7);
    if (ch->output.l > 32767) ch->output.l = 32767;
    if (ch->output.l < -32768) ch->output.l = -32768;
    if (ch->output.r > 32767) ch->output.r = 32767;
    if (ch->output.r < -32768) ch->output.r = -32768;
}

static void mix_sample(SNES_APU *this)
{
    i32 out_l, out_r;
    out_l = out_r = 0;

    for (u32 i = 0; i < 8; i++) {
        struct SNES_APU_ch *ch = &this->dsp.channel[i];
        if (!ch->ext_enable || !ch->env.attenuation || ch->ended) continue;

        out_l += ch->output.l;
        out_r += ch->output.r;
        if (out_l < -32768) out_l = -32768;
        if (out_l > 32767) out_l = 32767;
        if (out_r < -32768) out_r = -32768;
        if (out_r > 32767) out_r = 32767;
    }
    out_l = (out_l * this->dsp.io.MVOLL) >> 7;
    out_r = (out_r * this->dsp.io.MVOLR) >> 7;
    if (out_l < -32768) out_l = -32768;
    if (out_l > 32767) out_l = 32767;
    if (out_r < -32768) out_r = -32768;
    if (out_r > 32767) out_r = 32767;
    if (!this->dsp.ext_enable || this->dsp.io.FLG.mute_all) {
        out_l = out_r = 0;
    }
    this->dsp.output.l = out_l;
    this->dsp.output.r = out_r;
}

static void DSP_cycle(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *snes = (SNES *)ptr;

    for (u32 i = 0; i < 8; i++) {
        struct SNES_APU_ch *ch = &snes->apu.dsp.channel[i];
        ch_update_env(snes, ch);
        ch_update_sample(snes, ch);
    }

    mix_sample(&snes->apu);

    snes->clock.apu.sample.next += snes->clock.apu.sample.stride;
    scheduler_only_add_abs(&snes->scheduler, (i64)snes->clock.apu.sample.next, 0, snes, &DSP_cycle, NULL);

    snes->apu.dsp.sample.func(snes->apu.dsp.sample.ptr, 0, clock, jitter);
}

void SNES_APU_schedule_first(SNES *snes)
{
    snes->clock.apu.cycle.next = snes->clock.apu.cycle.stride;
    snes->clock.apu.sample.next = snes->clock.apu.sample.stride;

    scheduler_only_add_abs(&snes->scheduler, (i64)snes->clock.apu.cycle.next, 0, snes, &CPU_cycle, NULL);
    scheduler_only_add_abs(&snes->scheduler, (i64)snes->clock.apu.sample.next, 0, snes, &DSP_cycle, NULL);
    for (u32 i = 0; i < 8; i++) {
        struct SNES_APU_ch *ch = &snes->apu.dsp.channel[i];
        ch->io.PITCHL = 0;
        ch->io.PITCHH = 0;
        ch->env.state = SDEM_release;
        ch->env.attenuation = 0;
        ch->io.ADSR1.adsr_on = 1;
        update_envelope(snes, ch, 0);
    }
}

u32 SNES_APU_read(SNES *snes, u32 addr, u32 old, u32 has_effect)
{
    return snes->apu.cpu.io.CPUO[addr & 3];
}

void SNES_APU_write(SNES *snes, u32 addr, u32 val)
{
    snes->apu.cpu.io.CPUI[addr & 3] = val;
}


