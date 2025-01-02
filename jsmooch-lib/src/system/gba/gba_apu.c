//
// Created by . on 12/28/24.
//

#include "gba_bus.h"
#include "gba_apu.h"

void GBA_APU_init(struct GBA*this)
{
    GB_APU_init(&this->apu.old);
}

u32 GBA_APU_read_IO(struct GBA*this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    return 0;
}

void GBA_APU_write_IO(struct GBA*this, u32 addr, u32 sz, u32 access, u32 val)
{
    switch(addr) {
        case 0x04000070:
            this->apu.chan[2].wave_ram_size = (val >> 5) ? 64 : 32;
            this->apu.chan[2].wave_ram_bank = (val >> 6) & 1;
            this->apu.chan[2].playback = (val >> 7) & 1;
            return;
        case 0x04000071:
            return;
        case 0x04000072:
            this->apu.chan[2].sound_len = val;
            return;
    }
}

void GBA_APU_cycle(struct GBA*this)
{
    // 64 CPU clocks per sample
}
