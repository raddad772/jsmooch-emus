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
    switch(addr) {
        case 0x04000088:
            return this->apu.io.sound_bias & 0xFF;
        case 0x04000089:
            return this->apu.io.sound_bias >> 8;
    }
    //printf("\nWARN UNDONE READ FROM APU %08x", addr);
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
        case 0x04000088:
            this->apu.io.sound_bias = (this->apu.io.sound_bias & 0xFF00) | val;
            return;
        case 0x04000089:
            this->apu.io.sound_bias = (this->apu.io.sound_bias & 0xFF) | (val << 8);
            return;
    }
    //printf("\nWARN UNDONE WRITE TO APU %08x", addr);
}

void GBA_APU_cycle(struct GBA*this)
{
    // 64 CPU clocks per sample
}
