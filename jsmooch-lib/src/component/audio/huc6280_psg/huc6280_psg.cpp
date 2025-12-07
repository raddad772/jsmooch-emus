//
// Created by . on 7/19/25.
//
#include "huc6280_psg.h"

namespace HUC6280::PSG {

core::core() : channels{CH(this, 0), CH(this, 1), CH(this, 2), CH(this, 3), CH(this, 4), CH(this, 5)}
{
}

void core::reset() {

}

void core::update_ch_output(CH &ch)
{
    const u8 volume_reduce[30] = { 255,214,180,151,127,107,90,76,64,53,45,38,32,27,22,19,16,13,11,9,8,6,5,4,4,3,2,2,2,1 };
    uint8_t reduction_l = (0xF - LMAL) * 2 + (0x1F - ch.AL) + (0xF - ch.LAL) * 2;
    uint8_t reduction_r = (0xF - RMAL) * 2 + (0x1F - ch.AL) + (0xF - ch.RAL) * 2;
    if(reduction_l > 29) {
        ch.output_l = 0;
    }
    else {
        ch.output_l = ch.output * volume_reduce[reduction_l];
    }
    if (reduction_r > 29) {
        ch.output_r = 0;
    }
    else {
        ch.output_r = ch.output * volume_reduce[reduction_r];
    }
}

void core::update_DDA(CH &ch)
{
    updates = true;
    ch.output = ch.WAVEDATA[ch.wavectr];
    update_ch_output(ch);
}

void core::write(u16 addr, u8 val)
{
    addr &= 15;
    CH &ch = channels[SEL];
    switch(addr) {
        case 0: // channel select
            SEL = val & 7;
            if (SEL > 5) {
                //printf("\nWARN CH SEL %d", SEL);
            }
            return;
        case 1: // master vol left/right
            if (SEL < 6) {
                LMAL = (val >> 4) & 15;
                RMAL = val & 15;
            }
            return;
        // per-channel stuff
        case 2: //
            if (SEL < 6)
                ch.FREQ.lo = val;
            return;
        case 3:
            if (SEL < 6)
                ch.FREQ.hi = val & 15;
            return;
        case 4:
            if (SEL < 6) {
                ch.ON = (val >> 7) & 1;
                ch.DDA = (val >> 6) & 1;
                ch.AL = val & 0x1F;
                if (!ch.ON && !ch.DDA) {
                    ch.wavectr = 0;
                }
                if (ch.ON && ch.DDA) {
                    update_DDA(ch);
                }
            }
            return;
        case 5:
            if (SEL < 6) {
                ch.LAL = (val >> 4) & 15;
                ch.RAL = val & 15;
            }
            return;
        case 6:
            if (SEL < 6) {
                ch.WAVEDATA[ch.wavectr] = val & 0x1F;
                update_DDA(ch);
                if (!ch.ON) ch.wavectr = (ch.wavectr + 1) & 0x1F;
            }
            return;
        case 7: {
            //
            if (SEL < 4 || SEL > 5) return;
            ch.NOISE.E = (val >> 7) & 1;
            u32 a = val & 0x1F;
            ch.NOISE.FREQ = a == 0x1F ? 32 : (a ^ 0x1F) * 64;
            return; }
        case 8:
            LFO.FREQ = val;
            return;
        case 9:
            LFO.TRG = (val >> 7) & 1;
            LFO.CTL = val & 3;
            return;
        default:
    }
}


void CH::clock_noise()
{
    NOISE.COUNTER--;
    if (NOISE.COUNTER < 0) {
        NOISE.COUNTER = NOISE.FREQ;
        const u32 v = NOISE.state;
        const u32 bit = ((v >> 0) ^ (v >> 1) ^ (v >> 11) ^ (v >> 12) ^ (v >> 17)) & 1;
        NOISE.output = (NOISE.state & 1) ? 0x1F : 0;
        NOISE.state >>= 1;
        NOISE.state |= bit << 17;
    }
}

bool CH::tick()
{
    bool updates = false;
    if (num > 4) clock_noise();
    counter--;
    if (counter < 0) {
        updates = true;
        counter = FREQ.u;
        if (num == 0) {
            int shift = ((psg->LFO.CTL & 0x03) - 1) * 2;
            i32 a = static_cast<i32>(psg->channels[1].output) << shift;
            counter = (counter + a) & 0xFFF;
        }
        if (ON) {
            if (NOISE.E) { // NOISE!
                output = static_cast<i16>(NOISE.output);
            }
            else {
                if (!DDA) { // DDA Mode
                    wavectr = (wavectr + 1) & 31;
                }
                output = WAVEDATA[wavectr];
            }
        }
        else {
            output = 0;
        }
        psg->update_ch_output(*this);
    }
    return updates;
}

void core::mix_sample()
{
    out.l = 0;
    out.r = 0;
    for (auto &ch : channels) {
        if (ch.ext_enable) {
            out.l += ch.output_l;
            out.r += ch.output_r;
        }
    }
    updates = false;
}

void core::cycle()
{
    for (auto &ch : channels) {
        updates |=  ch.tick();
    }
    if (updates) mix_sample();
    // TODO: LFO
}

u16 CH::debug_sample() const
{
    u16 r = (output_l + output_r) + 0x100;
    if (r == 0x100) r = 0;
    return r;
}

}