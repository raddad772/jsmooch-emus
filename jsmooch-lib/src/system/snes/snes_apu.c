//
// Created by . on 4/23/25.
//

#include "snes_apu.h"

#include "snes_bus.h"

// ratio for one to the other =


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

static inline void calculate_sample_addr(struct SNES *snes, struct SNES_APU_ch *ch)
{
    u16 brr_entry = ((snes->apu.dsp.io.DIR << 8) + (ch->io.SRCN << 2)) & 0xFFFF;
    ch->sample_data.start_addr = snes->apu.cpu.RAM[brr_entry++];
    ch->sample_data.start_addr |= snes->apu.cpu.RAM[brr_entry++] << 8;
    ch->sample_data.loop_addr = snes->apu.cpu.RAM[brr_entry++];
    ch->sample_data.loop_addr |= snes->apu.cpu.RAM[brr_entry] << 8;
}

static void calculate_sample_addrs(struct SNES *snes)
{
    for (u32 i = 0; i < 8; i++) {
        struct SNES_APU_ch *ch = &snes->apu.dsp.channel[i];
        calculate_sample_addr(snes, ch);
    }
}

//  ((0) + (4F * 8000)
// ((addr & !0x8000) + (bank & !0x80) * 0x8000
//  ((addr & 0x7FFF) + (bank & 0x7F) * 0x8000
static void BRR_decode(u8 *buf, struct SNES_APU_sample *dest, struct SNES_APU_filter *filter)
{
    dest->pos = 0;
    u8 header = *buf;
    buf++;
    if (dest->first_or_loop) {
        dest->first_or_loop = 0;
        header = 0;
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
            i32 nibble = (i32)(((i8)data & 0xF0) >> 4);
            data <<= 4;
            //nibble = SIGNe4to32(nibble);
            if (scale <= 12) {
                nibble <<= scale;
                nibble >>= 1;
            } else {
                nibble &= ~0x7ff;
            }
            switch(filter_num) {
                case 0:
                    break;
                case 1:
                    //s += filter->prev[0] * 0.46875
                    nibble += filter->prev[0] >> 1;
                    nibble += (-filter->prev[0]) >> 5;
                    break;

                case 2:
                    //s += filter->prev[0] * 0.953125 - filter->prev[1] * 0.46875
                    nibble += filter->prev[0];
                    nibble -= filter->prev[1];
                    nibble += filter->prev[1] >> 4;
                    nibble += (filter->prev[0] * -3) >> 6;
                    break;

                case 3:
                    nibble += filter->prev[0];
                    nibble -= filter->prev[1];
                    nibble += (filter->prev[0] * -13) >> 7;
                    nibble += (filter->prev[1] * 3) >> 4;
                    break;

            }
            if (nibble < -32768) nibble = -32768;
            if (nibble > 32767) nibble = 32767;
            nibble = (i16)((u16)nibble << 1);

            dest->decoded[tn++] = (i16)nibble;
            filter->prev[3] = filter->prev[2];
            filter->prev[2] = filter->prev[1];
            filter->prev[1] = filter->prev[0];
            filter->prev[0] = (i16)nibble;
        }
    }
}

static u8 read_voice(struct SNES *snes, struct SNES_APU_ch *ch, u8 param)
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
    struct SNES *snes = (struct SNES *)ptr;
    snes->apu.dsp.io.KON = 0;
}

static u8 read_DSP(void *ptr, u16 addr)
{
    struct SNES *snes = (struct SNES *)ptr;
    u32 ch_num = (addr >> 4) & 7;
    struct SNES_APU_ch *ch = &snes->apu.dsp.channel[ch_num];
    u32 param = addr & 15;
    if (param < 0x0A)
        return read_voice(snes, ch, param);

    struct SNES_APU_DSP *this = &snes->apu.dsp;
    switch (addr) {
        case 0x0c:
            return this->io.MVOLL;
        case 0x1C:
            return this->io.MVOLR;
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
    struct SNES *snes = (struct SNES *)ptr;
    struct SNES_APU_DSP *this = &snes->apu.dsp;

    s32 feedback = this->noise.level << 13 ^ this->noise.level << 14;
    this->noise.level = feedback & 0x4000 | this->noise.level >> 1;

    this->noise.next_update += this->noise.stride;
    this->noise.sch_id = scheduler_only_add_abs(&snes->scheduler, (i64)this->noise.next_update, 0, snes, &do_noise, &this->noise.sch_still);
}

static void ch_do_env(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *snes = (struct SNES *)ptr;
    struct SNES_APU_ch *ch = &snes->apu.dsp.channel[key];
    //printf("\nDO ENV %lld STATE:%d ATTENUATION:%d", key, ch->env.state, ch->env.attenuation);
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
        }
        if (ch->env.state == SDEM_decay && ch->env.attenuation <= ch->env.sustain_level) {
            ch->env.state = SDEM_sustain;
        }
    }
    else {
        if (!ch->io.GAIN.custom.custom_gain) {
            ch->env.attenuation = (ch->io.GAIN.direct.fixed_vol << 4);
            return;
        }
        rate = ch->io.GAIN.custom.gain_rate;
        switch(ch->io.GAIN.custom.gain_mode) {
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

    ch->env.stride = (long double)env_periods[rate] * snes->clock.apu.env.stride;
    ch->env.next_update += ch->env.stride;
    ch->env.sch_id = scheduler_only_add_abs(&snes->scheduler, (i64)ch->env.next_update, ch->num, snes, &ch_do_env, &ch->env.sch_still);
}

/*static void ch_do_sample(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *snes = (struct SNES *)ptr;
    struct SNES_APU_ch *ch = &snes->apu.dsp.channel[key];

    if (ch->ended) {
        return;
    }

    if (ch->sample_data.pos >= 15) { // sample has ended, or attack JUST started.
        if (ch->sample_data.end) {
            ch->sample_data.next_read_addr = ch->sample_data.loop_addr;
            if (!ch->sample_data.loop) {
                ch->ended = 1;
                return;
            }
        }

        assert(ch->sample_data.next_read_addr < (0xFFFF - 8));
        BRR_decode(snes->apu.cpu.RAM + ch->sample_data.next_read_addr, &ch->sample_data, &ch->filter);
        if (ch->sample_data.end) snes->apu.dsp.io.ENDX |= (1 << ch->num);
        ch->sample_data.next_read_addr = (ch->sample_data.next_read_addr + 9) & 0xFFFF;
    }

    ch->samples.head = (ch->samples.head + 1) & 3;
    i32 smp;
    if (snes->apu.dsp.io.NON & (1 << ch->num)) smp = (i16)(snes->apu.dsp.noise.level << 1);
    else smp = ch->sample_data.decoded[ch->sample_data.pos];
    smp = (smp * ch->env.attenuation) >> 11;
    ch->io.OUTX = (smp >> 7) & 0xFF;

    // smp = smp * vol / 128;
    i32 vol = (ch->io.VOLL + ch->io.VOLR) >> 1;

    ch->samples.data[ch->samples.head] = (i16)((smp * vol) >> 7);
    ch->sample_data.pos++;

    //ch->pitch.next_sample += ch->pitch.stride;
    //ch->pitch.sch_id = scheduler_only_add_abs(&snes->scheduler, (i64)ch->pitch.next_sample, key, snes, &ch_do_sample, &ch->pitch.sch_still);
}*/

static void schedule_env(struct SNES *snes, struct SNES_APU_ch *ch, u32 rate)
{
    // rate is # of 1mhz cycles so
    ch->env.stride = (long double)env_periods[rate] * snes->clock.apu.env.stride;
    ch->env.next_update = snes->clock.master_cycle_count + ch->env.stride;
    ch->env.sch_id = scheduler_only_add_abs(&snes->scheduler, (i64)ch->env.next_update, ch->num, snes, &ch_do_env, &ch->env.sch_still);
}


/*static inline void schedule_ch(struct SNES *snes, struct SNES_APU_ch *ch, u32 pitch)
{
    u32 hz = (pitch * 32000) / 0x1000;
    ch->pitch.stride = (long double)snes->clock.timing.second.master_cycles / (long double)hz;

    ch->pitch.next_sample = ((long double)snes->clock.master_cycle_count) + ch->pitch.stride;
    ch->pitch.sch_id = scheduler_only_add_abs(&snes->scheduler, (i64)ch->pitch.next_sample, ch->num, snes, &ch_do_sample, &ch->pitch.sch_still);
}*/

static void update_noise(struct SNES *snes)
{
    struct SNES_APU_DSP *this = &snes->apu.dsp;
    if (this->noise.sch_still) scheduler_delete_if_exist(&snes->scheduler, this->noise.sch_id);
    if (this->io.FLG.noise_freq) {
        this->noise.stride = (long double)env_periods[this->io.FLG.noise_freq] * snes->clock.apu.env.stride;
        this->noise.next_update = snes->clock.master_cycle_count + this->noise.stride;
        this->noise.sch_id = scheduler_only_add_abs(&snes->scheduler, (i64)this->noise.next_update, 0, snes, &do_noise, &this->noise.sch_still);
    }
}

static void update_envelope(struct SNES *snes, struct SNES_APU_ch *ch, u32 rate) {
    if (ch->env.sch_still) scheduler_delete_if_exist(&snes->scheduler, ch->env.sch_id);
    if (!ch->io.ADSR1.adsr_on && !ch->io.GAIN.custom.custom_gain) {
        ch->env.attenuation = ch->io.GAIN.direct.fixed_vol << 4;
        rate = 0;
    }
    if (rate) {
        schedule_env(snes, ch, rate);
    }
}

/*static void update_pitch(struct SNES *snes, struct SNES_APU_ch *ch, u32 old_pitch)
{
    u32 new_pitch = (ch->io.PITCHH << 8) | ch->io.PITCHL;
    if ((old_pitch == new_pitch) && (ch->pitch.sch_still)) return;
    if (ch->pitch.sch_still) scheduler_delete_if_exist(&snes->scheduler, ch->pitch.sch_id);
    if (new_pitch) {
        schedule_ch(snes, ch, new_pitch);
    }
}*/

static u32 calc_env_rate(struct SNES *snes, struct SNES_APU_ch *ch)
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

static void write_voice(struct SNES *snes, struct SNES_APU_ch * ch, u8 param, u8 val)
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

static void keyon(struct SNES *snes, u32 ch_num)
{
    struct SNES_APU_ch *ch = &snes->apu.dsp.channel[ch_num];
    ch->ended = 0;
    ch->pitch.counter = 0;
    ch->sample_data.pos = 15; // To force new block read
    ch->sample_data.next_read_addr = ch->sample_data.start_addr; // Start at start of sample

    ch->sample_data.end = 0; // Make sure we don't immediately end
    ch->sample_data.loop = 0;
    ch->sample_data.first_or_loop = 1;

    assert(ch->sample_data.next_read_addr < (0xFFFF - 8));
    BRR_decode(snes->apu.cpu.RAM + ch->sample_data.next_read_addr, &ch->sample_data, &ch->filter);
    if (ch->sample_data.end) snes->apu.dsp.io.ENDX |= (1 << ch->num);
    ch->sample_data.next_read_addr = (ch->sample_data.next_read_addr + 9) & 0xFFFF;

    ch->env.state = SDEM_attack;
    if (ch->io.ADSR1.adsr_on) ch->env.attenuation = 0;
    update_envelope(snes, ch, calc_env_rate(snes, ch));
}

static void keyoff(struct SNES *snes, u32 ch_num)
{
    struct SNES_APU_ch *ch = &snes->apu.dsp.channel[ch_num];
    ch->env.state = SDEM_release;
    update_envelope(snes, ch, calc_env_rate(snes, ch));
}

static void write_DSP(void *ptr, u16 addr, u8 val)
{
    struct SNES *snes = (struct SNES *)ptr;
    u32 ch_num = (addr >> 4) & 7;
    struct SNES_APU_ch *ch = &snes->apu.dsp.channel[ch_num];
    u32 param = addr & 15;

    if (param < 0x0A)
        return write_voice(snes, ch, param, val);

    struct SNES_APU_DSP *this = &snes->apu.dsp;
    switch (addr) {
        case 0x0c:
            this->io.MVOLL = val;
            return;
        case 0x1C:
            this->io.MVOLR = val;
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


void SNES_APU_init(struct SNES *snes)
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

void SNES_APU_delete(struct SNES *snes)
{
    SPC700_delete(&snes->apu.cpu);
}

void SNES_APU_reset(struct SNES *snes)
{
    SPC700_reset(&snes->apu.cpu);
}

static void CPU_cycle(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *snes = (struct SNES *)ptr;
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

static void cycle_SNES_channel(struct SNES *snes, struct SNES_APU_ch *ch)
{
    // do pitch updates!
    // this is called @ 32kHz
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
            ch->sample_data.first_or_loop = 1;
        }
        assert(ch->sample_data.next_read_addr < (0xFFFF - 8));
        BRR_decode(snes->apu.cpu.RAM + ch->sample_data.next_read_addr, &ch->sample_data, &ch->filter);
        if (ch->sample_data.end) snes->apu.dsp.io.ENDX |= (1 << ch->num);
        ch->sample_data.next_read_addr = (ch->sample_data.next_read_addr + 9) & 0xFFFF;
    }

    u32 sample_num = (ch->pitch.counter >> 12) & 15;
    i32 smp;
    if (snes->apu.dsp.io.NON & (1 << ch->num)) smp = snes->apu.dsp.noise.level;
    else smp = ch->sample_data.decoded[sample_num];
    smp = (smp * ch->env.attenuation) >> 11;
    ch->io.OUTX = (smp >> 7) & 0xFF;

    // smp = smp * vol / 128;
    i32 vol = (ch->io.VOLL + ch->io.VOLR) >> 1;

    ch->sample = (i16)((smp * vol) >> 7);
    //ch->sample_data.pos++;


    // ch_do_sample(snes, ch->num, snes->clock.master_cycle_count, 0);
    /*
Counter.Bit15-12 indicates the current sample (within a BRR block).
Counter.Bit11-3 are used as gaussian interpolation index.     */
}

static void DSP_cycle(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *snes = (struct SNES *)ptr;

    for (u32 i = 0; i < 8; i++) {
        struct SNES_APU_ch *ch = &snes->apu.dsp.channel[i];
        cycle_SNES_channel(snes, ch);
    }

    snes->clock.apu.sample.next += snes->clock.apu.sample.stride;
    scheduler_only_add_abs(&snes->scheduler, (i64)snes->clock.apu.sample.next, 0, snes, &DSP_cycle, NULL);
}

void SNES_APU_schedule_first(struct SNES *snes)
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

u32 SNES_APU_read(struct SNES *snes, u32 addr, u32 old, u32 has_effect)
{
    return snes->apu.cpu.io.CPUO[addr & 3];
}

void SNES_APU_write(struct SNES *snes, u32 addr, u32 val)
{
    snes->apu.cpu.io.CPUI[addr & 3] = val;
}


i16 SNES_APU_mix_sample(struct SNES_APU *this, u32 is_debug)
{
    i32 out = 0;
    if (!this->dsp.ext_enable || this->dsp.io.FLG.mute_all) return 0;

    for (u32 i = 0; i < 8; i++) {
        struct SNES_APU_ch *ch = &this->dsp.channel[i];
        if (!ch->ext_enable || !ch->env.attenuation || ch->ended) continue;

        i32 smp;
        smp = ch->sample;
        out += smp;
        if (out < -32768) out = -32768;
        if (out > 32767) out = 32767;
    }
    return out;
}