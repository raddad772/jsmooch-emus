//
// Created by Dave on 2/5/2024.
//

#include "stdio.h"
#include "string.h"
#include "assert.h"

#include "nes.h"
#include "nes_cart.h"

void NES_cart_init(struct NES_cart* this, struct NES* nes)
{
    buf_init(&this->CHR_ROM);
    buf_init(&this->PRG_ROM);
    this->nes = nes;
    
    this->header.mapper_number = 0;
    this->header.nes_timing = 0;
    this->header.submapper = 0;
    this->header.mirroring = 0;
    this->header.battery_present = 0;
    this->header.trainer_present = 0;
    this->header.four_screen_mode = 0;
    this->header.chr_ram_present = 0;
    this->header.chr_rom_size = 0;
    this->header.prg_rom_size = 0;
    this->header.prg_ram_size = 0;
    this->header.prg_nvram_size = 0;
    this->header.chr_ram_size = 0;
    this->header.chr_nvram_size = 0;

    this->valid = 0;
}

void NES_cart_delete(struct NES_cart* this)
{
    buf_delete(&this->CHR_ROM);
    buf_delete(&this->PRG_ROM);
}

u32 read_ines1_header(struct NES_cart* this, const char *fil, size_t fil_sz)
{
    printf("\niNES version 1 header found");
    this->header.prg_rom_size = 16384 * fil[4];
    this->header.chr_rom_size = 8192 * fil[5];
    this->header.chr_ram_present = this->header.chr_rom_size == 0;
    if (this->header.chr_ram_present) this->header.chr_ram_size = 8192;
    this->header.mirroring = fil[6] & 1;
    if (this->header.mirroring == 0) this->header.mirroring = PPUM_Horizontal;
    else this->header.mirroring = PPUM_Vertical;

    this->header.battery_present = (fil[6] & 2) >> 1;
    this->header.trainer_present = (fil[6] & 4) >> 2;
    this->header.four_screen_mode = (fil[6] & 8) >> 3;
    this->header.mapper_number = (fil[6] >> 4) | (fil[7] & 0xF0);
    printf("\nMAPPER %d", this->header.mapper_number);
    this->header.prg_ram_size = fil[8] != 0 ? fil[8] * 8192 : 8192;
    this->header.nes_timing = (fil[9] & 1) ? NES_PAL : NES_NTSC;
    return true;
}

u32 read_ines2_header(struct NES_cart* this, const char *fil, size_t fil_sz)
{
    printf("iNES version 2 header found");
    u32 prgrom_msb = fil[9] & 0x0F;
    u32 chrrom_msb = (fil[9] & 0xF0) >> 4;
    if (prgrom_msb == 0x0F) {
        u32 E = (fil[4] & 0xFC) >> 2;
        u32 M = fil[4] & 0x03;
        this->header.prg_rom_size = (1 << E) * ((M*2)+1);
    } else {
        this->header.prg_rom_size = ((prgrom_msb << 8) | fil[4]) * 16384;
    }
    printf("PRGROM found: %dkb", (this->header.prg_rom_size / 1024));

    if (chrrom_msb == 0x0F) {
        u32 E = (fil[5] & 0xFC) >> 2;
        u32 M = fil[5] & 0x03;
        this->header.chr_rom_size = (1 << E) * ((M*2)+1);
    } else {
        this->header.chr_rom_size = ((chrrom_msb << 8) | fil[5]) * 8192;
    }
    printf("CHR ROM found: %d", this->header.chr_rom_size);

    this->header.mirroring = fil[6] & 1;
    if (this->header.mirroring == 0) this->header.mirroring = PPUM_Horizontal;
    else this->header.mirroring = PPUM_Vertical;

    this->header.battery_present = (fil[6] & 2) >> 1;
    this->header.trainer_present = (fil[6] & 4) >> 2;
    this->header.four_screen_mode = (fil[6] & 8) >> 3;
    this->header.mapper_number = (fil[6] >> 4) | (fil[7] & 0xF0) | ((fil[8] & 0x0F) << 8);
    this->header.submapper = fil[8] >> 4;

    u32 prg_shift = fil[10] & 0x0F;
    u32 prgnv_shift = fil[10] >> 4;
    if (prg_shift != 0) this->header.prg_ram_size = 64 << prg_shift;
    if (prgnv_shift != 0) this->header.prg_nvram_size = 64 << prgnv_shift;

    u32 chr_shift = fil[11] & 0x0F;
    u32 chrnv_shift = fil[11] >> 4;
    if (chr_shift != 0) this->header.chr_ram_size = 64 << chr_shift;
    if (chrnv_shift != 0) this->header.chr_nvram_size = 64 << chrnv_shift;
    switch(fil[12] & 3) {
        case 0:
            this->header.nes_timing = NES_NTSC;
            break;
        case 1:
            this->header.nes_timing = NES_PAL;
            break;
        case 2:
            printf("WTF even is this");
            this->header.nes_timing = NES_NTSC;
            break;
        case 3:
            this->header.nes_timing = NES_DENDY;
            break;
    }
    return true;
}

void read_ROM_RAM(struct NES_cart* this, char* inp, size_t inp_size)
{
    buf_allocate(&this->PRG_ROM, this->header.prg_rom_size);
    buf_allocate(&this->CHR_ROM, this->header.chr_rom_size);

    if ((this->header.prg_rom_size + this->header.chr_rom_size) > inp_size) {
        printf("\nBUFFER UNDERRUN!");
        assert(1==0);
        return;
    }

    memcpy(this->PRG_ROM.ptr, inp, this->header.prg_rom_size);
    if (this->header.chr_rom_size > 0)
        memcpy(this->CHR_ROM.ptr, inp+(this->header.prg_rom_size), this->header.chr_rom_size);

}

u32 NES_cart_load_ROM_from_RAM(struct NES_cart* this, char* fil, u64 fil_sz)
{

    if ((fil[0] != 0x4E) || (fil[1] != 0x45) ||
        (fil[2] != 0x53) || (fil[3] != 0x1A)) {
        printf("\nBad iNES header");
        return false;
    }
    u32 worked = true;
    if ((fil[7] & 12) == 8)
        worked = read_ines2_header(this, fil, fil_sz);
    else
        worked = read_ines1_header(this, fil, fil_sz);
    if (!worked) return false;

    u32 header_size = 16 + (this->header.trainer_present ? 512 : 0);
    read_ROM_RAM(this, fil+header_size, fil_sz-header_size);
    this->valid = worked;
    return worked;
}