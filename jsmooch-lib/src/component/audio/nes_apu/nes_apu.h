//
// Created by . on 9/8/24.
//

#ifndef JSMOOCH_EMUS_NES_APU_H
#define JSMOOCH_EMUS_NES_APU_H

#include "helpers/int.h"

struct NES_APU {
    u32 ext_enable;
    
    struct NESSNDCHAN {
        u32 ext_enable;
        u32 number;

        u32 duty, env_loop_or_length_counter_halt, constant_volume, volume_or_envelope;
        u32 timer, length_counter_load;
        struct {
            u32 enabled, period, negate, shift;
        } sweep;


    } channels[5];

    struct {
        u32 step5_mode;
    } io;

    struct {
        u32 counter_1;
        i32 counter_240hz;
    } clocks;
};

void NES_APU_init(struct NES_APU* this);
void NES_APU_write_IO(struct NES_APU* this, u32 addr, u8 val);
u8 NES_APU_read_IO(struct NES_APU* this, u32 addr, u8 old_val, u32 has_effect);
void NES_APU_cycle(struct NES_APU* this);
float NES_APU_mix_sample(struct NES_APU* this, u32 is_debug);
float NES_APU_sample_channel(struct NES_APU* this, int cnum);
void NES_APU_reset(struct NES_APU* this);

#endif //JSMOOCH_EMUS_NES_APU_H
