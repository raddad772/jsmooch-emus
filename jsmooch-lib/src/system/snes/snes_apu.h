//
// Created by . on 4/23/25.
//

#ifndef JSMOOCH_EMUS_SNES_APU_H
#define JSMOOCH_EMUS_SNES_APU_H

#include "component/cpu/spc700/spc700.h"

struct SNES_APU_sample {
    i16 decoded[16];
    u8 pos;
    u8 filter_pos;
    u8 loop, end;

    u16 start_addr, loop_addr;
    u16 next_read_addr;
};

struct SNES_APU_filter {
    i32 prev[2];
};

struct SNES_APU {
    struct SPC700 cpu;
    struct SNES_APU_DSP {
    u32 ext_enable;
        struct SNES_APU_ch {
            u32 num, ext_enable;

            struct {
                i16 data[4];
                u32 head;
            } samples;

            struct {
                long double next_sample, stride;
                u32 counter;
            } pitch;

            struct {
                u8 VOLL, VOLR, PITCHL, PITCHH, SRCN, ADSR1, ADSR2, GAIN, ENVX, OUTX;
            } io;

            struct SNES_APU_sample sample_data;
            struct SNES_APU_filter filter;

            u8 ended;

            u64 sch_id;
            u32 sch_still;
        } channel[8];

        struct {
            u8 MVOLL, MVOLR, EVOLL, EVOLR, KON, KOFF, ESA, EDL, FIR[8];
            u8 EFB, unused, PMON, NON, EON, DIR;
            union {
                struct {
                    u8 noise_freq : 5;
                    u8 disable_echo_write : 1;
                    u8 mute_all : 1;
                    u8 soft_reset : 1;
                };
                u8 u;
            } FLG;
        } io;

        struct {
            struct {
                u64 sch_id;
                u32 still_sched;
            } kon_clear, koff_clear;
        } scheduling;
    } dsp;
};

struct SNES;
void SNES_APU_init(struct SNES *);
void SNES_APU_delete(struct SNES *);
void SNES_APU_reset(struct SNES *);
void SNES_APU_schedule_first(struct SNES *);
u32 SNES_APU_read(struct SNES *, u32 addr, u32 old, u32 has_effect);
void SNES_APU_write(struct SNES *, u32 addr, u32 val);
i16 SNES_APU_mix_sample(struct SNES_APU *, u32 is_debug);

#endif //JSMOOCH_EMUS_SNES_APU_H
