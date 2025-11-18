//
// Created by . on 7/19/25.
//
#include <stdio.h>

#include "huc6280_psg.h"

void HUC6280_PSG_init(struct HUC6280_PSG *this)
{
    for (u32 i = 0; i < 6; i++) {
        this->ch[i].num = i;
        if (i > 4) this->ch[i].NOISE.state = 1;
        this->ch[i].ext_enable = 1;
    }
    this->ext_enable = 1;
}

void HUC6280_PSG_delete(struct HUC6280_PSG *this)
{

}

void HUC6280_PSG_reset(struct HUC6280_PSG *this)
{
    for (u32 i = 4; i < 6; i++) {
        this->ch[i].NOISE.state = 1;
    }
}

static void update_ch_output(struct HUC6280_PSG *this, struct HUC6280_PSG_ch *ch)
{
    const u8 volume_reduce[30] = { 255,214,180,151,127,107,90,76,64,53,45,38,32,27,22,19,16,13,11,9,8,6,5,4,4,3,2,2,2,1 };
    uint8_t reduction_l = (0xF - this->LMAL) * 2 + (0x1F - ch->AL) + (0xF - ch->LAL) * 2;
    uint8_t reduction_r = (0xF - this->RMAL) * 2 + (0x1F - ch->AL) + (0xF - ch->RAL) * 2;
    if(reduction_l > 29) {
        ch->output_l = 0;
    }
    else {
        ch->output_l = ch->output * volume_reduce[reduction_l];
    }
    if (reduction_r > 29) {
        ch->output_r = 0;
    }
    else {
        ch->output_r = ch->output * volume_reduce[reduction_r];
    }
}

static void update_DDA(struct HUC6280_PSG *this, struct HUC6280_PSG_ch *ch)
{
    this->updates = 1;
    ch->output = ch->WAVEDATA[ch->wavectr];
    update_ch_output(this, ch);
}

void HUC6280_PSG_write(struct HUC6280_PSG *this, u32 addr, u8 val)
{
    addr &= 15;
    struct HUC6280_PSG_ch *ch = &this->ch[this->SEL];
    switch(addr) {
        case 0: // channel select
            this->SEL = val & 7;
            if (this->SEL > 5) {
                //printf("\nWARN CH SEL %d", this->SEL);
            }
            return;
        case 1: // master vol left/right
            if (this->SEL < 6) {
                this->LMAL = (val >> 4) & 15;
                this->RMAL = val & 15;
            }
            return;
        // per-channel stuff
        case 2: //
            if (this->SEL < 6)
                ch->FREQ.lo = val;
            return;
        case 3:
            if (this->SEL < 6)
                ch->FREQ.hi = val & 15;
            return;
        case 4:
            if (this->SEL < 6) {
                ch->ON = (val >> 7) & 1;
                ch->DDA = (val >> 6) & 1;
                ch->AL = val & 0x1F;
                if (!ch->ON && !ch->DDA) {
                    ch->wavectr = 0;
                }
                if (ch->ON && ch->DDA) {
                    update_DDA(this, ch);
                }
            }
            return;
        case 5:
            if (this->SEL < 6) {
                ch->LAL = (val >> 4) & 15;
                ch->RAL = val & 15;
            }
            return;
        case 6:
            if (this->SEL < 6) {
                ch->WAVEDATA[ch->wavectr] = val & 0x1F;
                update_DDA(this, ch);
                if (!ch->ON) ch->wavectr = (ch->wavectr + 1) & 0x1F;
            }
            return;
        case 7: //
            if (this->SEL < 4) return;
            if (this->SEL > 5) return;
            ch->NOISE.E = (val >> 7) & 1;
            u32 a = val & 0x1F;
            ch->NOISE.FREQ = a == 0x1F ? 32 : (a ^ 0x1F) * 64;
            return;
        case 8:
            this->LFO.FREQ = val;
            return;
        case 9:
            this->LFO.TRG = (val >> 7) & 1;
            this->LFO.CTL = val & 3;
            return;
    }
}


static void clock_noise(struct HUC6280_PSG_ch *ch)
{
    ch->NOISE.COUNTER--;
    if (ch->NOISE.COUNTER < 0) {
        ch->NOISE.COUNTER = ch->NOISE.FREQ;
        u32 v = ch->NOISE.state;
        u32 bit = ((v >> 0) ^ (v >> 1) ^ (v >> 11) ^ (v >> 12) ^ (v >> 17)) & 1;
        ch->NOISE.output = (ch->NOISE.state & 1) ? 0x1F : 0;
        ch->NOISE.state >>= 1;
        ch->NOISE.state |= bit << 17;
    }
}

static void tick_ch(struct HUC6280_PSG *this, struct HUC6280_PSG_ch *ch)
{
    if (ch->num > 4) clock_noise(ch);
    ch->counter--;
    if (ch->counter < 0) {
        this->updates = 1;
        ch->counter = ch->FREQ.u;
        if (ch->num == 0) {
            int shift = ((this->LFO.CTL & 0x03) - 1) * 2;
            i32 a = (i32)this->ch[1].output << shift;
            ch->counter = (ch->counter + a) & 0xFFF;
        }
        if (ch->ON) {
            if (ch->NOISE.E) { // NOISE!
                ch->output = ch->NOISE.output;
            }
            else {
                if (!ch->DDA) { // DDA Mode
                    ch->wavectr = (ch->wavectr + 1) & 31;
                }
                ch->output = ch->WAVEDATA[ch->wavectr];
            }
        }
        else {
            ch->output = 0;
        }
        update_ch_output(this, ch);
    }
}

static void mix_sample(struct HUC6280_PSG *this)
{
    this->out.l = 0;
    this->out.r = 0;
    for (u32 i = 0; i < 6; i++) {
        struct HUC6280_PSG_ch *ch = &this->ch[i];
        if (ch->ext_enable) {
            this->out.l += ch->output_l;
            this->out.r += ch->output_r;
        }
    }
    this->updates = 0;
}

void HUC6280_PSG_cycle(struct HUC6280_PSG *this)
{
    for (u32 i = 0; i < 6; i++) {
        struct HUC6280_PSG_ch *ch = &this->ch[i];
        tick_ch(this, ch);
    }
    if (this->updates) mix_sample(this);
    // TODO: LFO
}

u16 HUC6280_PSG_debug_ch_sample(struct HUC6280_PSG *this, u32 num)
{
    struct HUC6280_PSG_ch *ch = &this->ch[num];
    u16 r = (ch->output_l + ch->output_r) + 0x100;
    if (r == 0x100) r = 0;
    return r;
}

