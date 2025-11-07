//
// Created by . on 7/19/25.
//

#ifndef JSMOOCH_EMUS_HUC6280_PSG_H
#define JSMOOCH_EMUS_HUC6280_PSG_H

#include "helpers_c/debugger/debuggerdefs.h"
#include "helpers_c/int.h"


struct HUC6280_PSG {
    u8 LMAL, RMAL; // left/right main amplitude
    u8 SEL;
    u8 updates;
    u32 ext_enable;

    struct {
        u8 FREQ, TRG, CTL;
    } LFO;

    struct {
        u16 l, r;
    } out;

    struct HUC6280_PSG_ch {
        union UN16 FREQ;
        u32 num;
        u8 ON, DDA, AL; // AL = amplitude level
        i16 output;
        u16 output_l, output_r;
        u8 LAL, RAL; // left/right amplitude
        u8 WAVEDATA[32];
        u32 wavectr;
        struct {
            u32 E, FREQ;
            i32 COUNTER;
            u32 state;
            i32 output;
        } NOISE;
        i32 counter;
        u32 ext_enable;
    } ch[6];

};

void HUC6280_PSG_init(struct HUC6280_PSG *);
void HUC6280_PSG_delete(struct HUC6280_PSG *);
void HUC6280_PSG_reset(struct HUC6280_PSG *);

void HUC6280_PSG_write(struct HUC6280_PSG *, u32 addr, u8 val);
void HUC6280_PSG_cycle(struct HUC6280_PSG *);
u16 HUC6280_PSG_debug_ch_sample(struct HUC6280_PSG *, u32 num);

#endif //JSMOOCH_EMUS_HUC6280_PSG_H
