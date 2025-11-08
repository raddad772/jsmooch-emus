//
// Created by Dave on 2/5/2024.
//

#include <cstdio>
#include <cstring>
#include <cassert>

#include "nes.h"
#include "nes_cart.h"

u32 NES_cart::read_ines1_header(const char *fil, size_t fil_sz)
{
    printf("\niNES version 1 header found");
    header.prg_rom_size = 16384 * fil[4];
    header.chr_rom_size = 8192 * fil[5];
    header.chr_ram_present = header.chr_rom_size == 0;
    if (header.chr_ram_present) header.chr_ram_size = 8192;
    header.mirroring = static_cast<NES_PPU_mirror_modes>(fil[6] & 1);
    if (header.mirroring == 0) header.mirroring = PPUM_Horizontal;
    else header.mirroring = PPUM_Vertical;

    header.battery_present = (fil[6] & 2) >> 1;
    header.trainer_present = (fil[6] & 4) >> 2;
    header.four_screen_mode = (fil[6] & 8) >> 3;
    header.mapper_number = (fil[6] >> 4) | (fil[7] & 0xF0);
    printf("\nMAPPER %d", header.mapper_number);
    header.prg_ram_size = fil[8] != 0 ? fil[8] * 8192 : 8192;
    header.nes_timing = (fil[9] & 1) ? NES_PAL : NES_NTSC;
    return true;
}

u32 NES_cart::read_ines2_header(const char *fil, size_t fil_sz)
{
    printf("iNES version 2 header found");
    u32 prgrom_msb = fil[9] & 0x0F;
    u32 chrrom_msb = (fil[9] & 0xF0) >> 4;
    if (prgrom_msb == 0x0F) {
        u32 E = (fil[4] & 0xFC) >> 2;
        u32 M = fil[4] & 0x03;
        header.prg_rom_size = (1 << E) * ((M*2)+1);
    } else {
        header.prg_rom_size = ((prgrom_msb << 8) | fil[4]) * 16384;
    }
    printf("PRGROM found: %dkb", (header.prg_rom_size / 1024));

    if (chrrom_msb == 0x0F) {
        u32 E = (fil[5] & 0xFC) >> 2;
        u32 M = fil[5] & 0x03;
        header.chr_rom_size = (1 << E) * ((M*2)+1);
    } else {
        header.chr_rom_size = ((chrrom_msb << 8) | fil[5]) * 8192;
    }
    printf("CHR ROM found: %d", header.chr_rom_size);

    header.mirroring = static_cast<NES_PPU_mirror_modes>(fil[6] & 1);
    if (header.mirroring == 0) header.mirroring = PPUM_Horizontal;
    else header.mirroring = PPUM_Vertical;

    header.battery_present = (fil[6] & 2) >> 1;
    header.trainer_present = (fil[6] & 4) >> 2;
    header.four_screen_mode = (fil[6] & 8) >> 3;
    header.mapper_number = (fil[6] >> 4) | (fil[7] & 0xF0) | ((fil[8] & 0x0F) << 8);
    header.submapper = fil[8] >> 4;

    u32 prg_shift = fil[10] & 0x0F;
    u32 prgnv_shift = fil[10] >> 4;
    if (prg_shift != 0) header.prg_ram_size = 64 << prg_shift;
    if (prgnv_shift != 0) header.prg_nvram_size = 64 << prgnv_shift;

    u32 chr_shift = fil[11] & 0x0F;
    u32 chrnv_shift = fil[11] >> 4;
    if (chr_shift != 0) header.chr_ram_size = 64 << chr_shift;
    if (chrnv_shift != 0) header.chr_nvram_size = 64 << chrnv_shift;
    switch(fil[12] & 3) {
        case 0:
            header.nes_timing = NES_NTSC;
            break;
        case 1:
            header.nes_timing = NES_PAL;
            break;
        case 2:
            printf("WTF even is this");
            header.nes_timing = NES_NTSC;
            break;
        case 3:
            header.nes_timing = NES_DENDY;
            break;
    }
    return true;
}

void NES_cart::read_ROM_RAM(const char* inp, size_t inp_size)
{
    PRG_ROM.allocate(header.prg_rom_size);
    CHR_ROM.allocate(header.chr_rom_size);

    if ((header.prg_rom_size + header.chr_rom_size) > inp_size) {
        printf("\nBUFFER UNDERRUN!");
        assert(1==0);
        return;
    }

    memcpy(PRG_ROM.ptr, inp, header.prg_rom_size);
    if (header.chr_rom_size > 0)
        memcpy(CHR_ROM.ptr, inp+(header.prg_rom_size), header.chr_rom_size);

}

u32 NES_cart::load_ROM_from_RAM(char* fil, u64 fil_sz)
{

    if ((fil[0] != 0x4E) || (fil[1] != 0x45) ||
        (fil[2] != 0x53) || (fil[3] != 0x1A)) {
        printf("\nBad iNES header");
        return false;
    }
    u32 worked = true;
    if ((fil[7] & 12) == 8)
        worked = read_ines2_header(fil, fil_sz);
    else
        worked = read_ines1_header(fil, fil_sz);
    if (!worked) return false;

    u32 header_size = 16 + (header.trainer_present ? 512 : 0);
    read_ROM_RAM(fil+header_size, fil_sz-header_size);
    valid = worked;
    return worked;
}