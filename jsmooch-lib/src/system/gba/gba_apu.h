//
// Created by . on 12/28/24.
//

#ifndef JSMOOCH_EMUS_GBA_APU_H
#define JSMOOCH_EMUS_GBA_APU_H

#include "helpers/int.h"
#include "component/audio/gb_apu/gb_apu.h"

struct GBA_APU {
    u32 ext_enable;
    struct GB_APU old;
    struct GBA_APU_FIFO {
        u32 timer_id;
    } fifo[2];

    struct {
        u32 wave_ram_size, wave_ram_bank, playback, sound_len;
    } chan[6];

    struct {
        u32 sound_bias;
    } io;
};

struct GBA;
void GBA_APU_init(struct GBA*);
u32 GBA_APU_read_IO(struct GBA*, u32 addr, u32 sz, u32 access, u32 has_effect);
void GBA_APU_write_IO(struct GBA*, u32 addr, u32 sz, u32 access, u32 val);


#endif //JSMOOCH_EMUS_GBA_APU_H
