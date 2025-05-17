//
// Created by . on 4/23/25.
//

#include "snes_apu.h"

#include "snes_bus.h"

// ratio for one to the other =


static inline void calculate_sample_addr(struct SNES *snes, struct SNES_APU_ch *ch)
{
    u16 brr_entry = ((snes->apu.dsp.io.DIR << 8) + (ch->io.SRCN << 4)) & 0xFFFF;
    ch->sample_data.start_addr = snes->apu.cpu.RAM[brr_entry++];
    ch->sample_data.start_addr |= snes->apu.cpu.RAM[brr_entry++] << 8;
    ch->sample_data.loop_addr = snes->apu.cpu.RAM[brr_entry++];
    ch->sample_data.loop_addr |= snes->apu.cpu.RAM[brr_entry++] << 8;
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
    u32 left_shift = (header >> 4) & 15; // BEFORE BRR decoding
    dest->loop = (header >> 1) & 1;
    dest->end = header & 1;
    u32 filter_num = (header >> 2) & 3;
    if (left_shift > 12) {
        printf("\nLEFT SHIFT BAD %d", left_shift);
        left_shift = 0;
    }
    u32 tn = 0;
    for (u32 i = 0; i < 8; i++) {
        u8 data = *buf;
        buf++;
        for (u32 num = 0; num < 2; num++) {
            i32 nibble = (data & 0xF0) >> 4;
            nibble = SIGNe4to32(nibble);
            nibble <<= left_shift;
            static const int filter_s0_mul[4] = { 0, 15, 31, 115};
            static const int filter_s0_shift[4] = { 0, 4, 5, 6};
            static const int filter_s1_mul[4] = { 0, 0, 15, 13};
            nibble += ((filter->prev[0] * filter_s0_mul[filter_num]) >> filter_s0_shift[filter_num]) + ((filter->prev[1] * filter_s1_mul[filter_num]) >> 4);
            if (nibble < -32768) nibble = -32768;
            if (nibble > 32767) nibble = 32767;

            dest->decoded[tn] = nibble;
            filter->prev[1] = filter->prev[0];
            filter->prev[0] = nibble;

            data <<= 4;
            tn++;
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
            return ch->io.ADSR1;
        case 6:
            return ch->io.ADSR2;
        case 7:
            return ch->io.GAIN;
        case 8:
            return ch->io.ENVX;
        case 9:
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
            u32 r = this->channel[0].sample_data.end;
            r |= this->channel[1].sample_data.end << 1;
            r |= this->channel[2].sample_data.end << 2;
            r |= this->channel[3].sample_data.end << 3;
            r |= this->channel[4].sample_data.end << 4;
            r |= this->channel[5].sample_data.end << 5;
            r |= this->channel[6].sample_data.end << 6;
            r |= this->channel[7].sample_data.end << 7;
            return r;}

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

static void ch_do_sample(void *ptr, u64 key, u64 clock, u32 jitter)
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
        ch->sample_data.next_read_addr = (ch->sample_data.next_read_addr + 9) & 0xFFFF;
    }

    ch->samples.head = (ch->samples.head + 1) & 3;
    ch->samples.data[ch->samples.head] = ch->sample_data.decoded[ch->sample_data.pos] >> 3; // TODO: add ADSR etc.;;
    ch->sample_data.pos++;

    ch->pitch.next_sample += ch->pitch.stride;
    ch->sch_id = scheduler_only_add_abs(&snes->scheduler, (i64)ch->pitch.next_sample, key, snes, &ch_do_sample, &ch->sch_still);
}

static inline void schedule_ch(struct SNES *snes, struct SNES_APU_ch *ch, u32 pitch)
{
    ch->pitch.stride = pitch * snes->clock.apu.sample.pitch_ratio;
    ch->pitch.next_sample = ((long double)snes->clock.master_cycle_count) + ch->pitch.stride;
    ch->sch_id = scheduler_only_add_abs(&snes->scheduler, (i64)ch->pitch.next_sample, ch->num, snes, &ch_do_sample, &ch->sch_still);
}

static void update_pitch(struct SNES *snes, struct SNES_APU_ch *ch, u32 old_pitch)
{
    u32 new_pitch = (ch->io.PITCHH << 8) | ch->io.PITCHL;
    if ((old_pitch == new_pitch) && (ch->sch_still)) return;
    if (ch->sch_still) scheduler_delete_if_exist(&snes->scheduler, ch->sch_id);
    if (new_pitch) {
        schedule_ch(snes, ch, new_pitch);
    }
}

static void write_voice(struct SNES *snes, struct SNES_APU_ch * ch, u8 param, u8 val)
{
    switch(param) {
        case 0:
            ch->io.VOLL = val;
            return;
        case 1:
            ch->io.VOLR = val;
            return;
        case 2: {
            u32 old_pitch = (ch->io.PITCHH << 8) | ch->io.PITCHL;
            ch->io.PITCHL = val;
            update_pitch(snes, ch, old_pitch);
            return; }
        case 3: {
            u32 old_pitch = (ch->io.PITCHH << 8) | ch->io.PITCHL;
            ch->io.PITCHH = val & 0x3F;
            update_pitch(snes, ch, old_pitch);
            return; }
        case 4:
            ch->io.SRCN = val;
            calculate_sample_addr(snes, ch);
            return;
        case 5:
            ch->io.ADSR1 = val;
            return;
        case 6:
            ch->io.ADSR2 = val;
            return;
        case 7:
            ch->io.GAIN = val;
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
    ch->sample_data.pos = 15; // To force new block read
    ch->sample_data.next_read_addr = ch->sample_data.start_addr; // Start at start of sample
    ch->sample_data.end = 0; // Make sure we don't immediately end
    update_pitch(snes, ch, (ch->io.PITCHH << 8) | ch->io.PITCHL);

    // TODO: set envelope to attack
}

static void keyoff(struct SNES *snes, u32 ch_num)
{
    struct SNES_APU_ch *ch = &snes->apu.dsp.channel[ch_num];
    // TODO: set envelope to release
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
                printf("\nWARN SOFT RESET DSP");
            }
            printf("\nMUTE ALL? %d", this->io.FLG.mute_all);
            return;
        case 0x0D:
            this->io.EFB = val;
            return;
        case 0x1D:
            this->io.unused = val;
            return;
        case 0x2D:
            this->io.PMON = val & 0xFE; // TODO: FF?
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
    // TODO: envelope etc.
}

static void DSP_cycle(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *snes = (struct SNES *)ptr;


    for (u32 i = 0; i < 8; i++) {
        struct SNES_APU_ch *ch = &snes->apu.dsp.channel[i];
        cycle_SNES_channel(snes, ch);
    }

    snes->clock.apu.cycle.next += snes->clock.apu.cycle.stride;
    scheduler_only_add_abs(&snes->scheduler, (i64)snes->clock.apu.cycle.next, 0, snes, &CPU_cycle, NULL);
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
        update_pitch(snes, ch, 1000);
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
    //if (!this->dsp.ext_enable || this->dsp.io.FLG.mute_all) return 0;


    for (u32 i = 0; i < 8; i++) {
        struct SNES_APU_ch *ch = &this->dsp.channel[i];
        //if (!ch->ext_enable) continue;

        out += ch->samples.data[ch->samples.head];
    }
    if (out < -32768) out = -32768;
    if (out > 32767) out = 32767;
    return out;
}