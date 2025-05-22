//
// Created by . on 4/21/25.
//
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "snes_cart.h"
#include "snes_bus.h"
#include "snes_mem.h"

void SNES_cart_init(struct SNES *this)
{
    buf_init(&this->cart.ROM);
}

void SNES_cart_delete(struct SNES *this)
{
    buf_delete(&this->cart.ROM);
}

static void read_ver1_header(struct SNES *this)
{
    u8 *rom = (u8 *)this->cart.ROM.ptr;
    this->cart.header.sram_sizebit = rom[this->cart.header.offset + 0xD8];
    this->cart.header.rom_sizebit = rom[this->cart.header.offset + 0xD7];
    memset(this->cart.header.internal_name, 0, sizeof(this->cart.header.internal_name));
    memcpy(this->cart.header.internal_name, rom+this->cart.header.offset+0xC0, 20);
}

static void read_ver2_header(struct SNES *this)
{
    read_ver1_header(this);
}

static void read_ver3_header(struct SNES *this)
{
    read_ver1_header(this);
    //  FFBCh  Expansion FLASH Size (1 SHL n) Kbytes (used in JRA PAT)
    //  FFBDh  Expansion RAM Size (1 SHL n) Kbytes (in GSUn games) (without battery?
    u8 *rom = (u8 *)this->cart.ROM.ptr;
    u32 sl = rom[this->cart.header.offset + 0xBC];
    if (sl > 0) {
        u32 expansion_flash_size = 1 << sl;
        printf("\nEXPANSION FLASH SZ: %d", expansion_flash_size);
        printf("\nWARN NOT SUPPORT!");
    }
}


static u32 check_offset(struct SNES *this, u32 offset)
{
    u8 *r = this->cart.ROM.ptr;
    r += offset + 0xC0;
    for (u32 i = 0; i < 20; i++) {
        if (*r == 0) {
            return 0;
        }
        r++;
    }
    return 1;
}

static u32 find_cart_header(struct SNES *this)
{
    u32 first_offset = 0x7F00;
    // check for 0x20 non-0 bytes
    if (check_offset(this, 0x7F00)) return 0x7F00;
    if (check_offset(this, 0xFF00)) return 0xFF00;
    return 0x7F00;
}

u32 SNES_cart_load_ROM_from_RAM(struct SNES* this, char* fil, u64 fil_sz, struct physical_io_device *pio)
{
    u32 SMCheader_size = fil_sz % 1024;
    if (SMCheader_size != 0) {
        printf("\nSMC header found! Removing %d bytes!", SMCheader_size);
        fil_sz -= SMCheader_size;
        fil += SMCheader_size;
    }
    buf_allocate(&this->cart.ROM, fil_sz);
    memcpy(this->cart.ROM.ptr, fil, fil_sz);
    this->cart.header.offset = find_cart_header(this);
    printf("\nCART HEADER DETECTED AT %06x", this->cart.header.offset);

    printf("\n\nLoading SNES cart, size %lld bytes", this->cart.ROM.size);
    u32 ver = 1;
    u8 *rom = (u8 *)this->cart.ROM.ptr;
    if (rom[this->cart.header.offset + 0xD4] == 0)
        ver = 2;
    if (rom[this->cart.header.offset + 0xDA] == 0x33)
        ver = 3;
    printf("\nHeader version %d", ver);
    this->cart.header.version = ver;
    this->cart.header.hi_speed = (rom[this->cart.header.offset + 0xD5] & 0x10) != 0;
    this->cart.header.mapping_mode = 0xF & rom[this->cart.header.offset + 0xD5];
    printf("\nMapping mode 0x%02x", this->cart.header.mapping_mode);
    switch(this->cart.header.mapping_mode) {
        case 0:
            this->cart.header.lorom = 1;
            printf("\nLOROM!");
            break;
        case 1:
            this->cart.header.hirom = 1;
            printf("\nHIROM!");
            break;
        default:
            printf("\nNOT SUPPORT MAP MODE %d", this->cart.header.mapping_mode);
            break;
    }

    u32 num_address_lines = (u32)ceil(log2((double)fil_sz));
    switch(ver) {
        case 1:
            read_ver1_header(this);
            break;
        case 2:
            read_ver2_header(this);
            break;
        case 3:
            read_ver3_header(this);
            break;
    }
    this->cart.header.sram_size = (1 << this->cart.header.sram_sizebit) * 1024;
    this->cart.header.sram_mask = this->cart.header.sram_size - 1;
    printf("\nInternal name %s", this->cart.header.internal_name);
    printf("\nSRAM size %d bytes", this->cart.header.sram_size);

    this->cart.SRAM = &pio->cartridge_port.SRAM;
    struct persistent_store *sram = this->cart.SRAM;
    sram->persistent = 1;
    sram->requested_size = this->cart.header.sram_size;
    sram->fill_value = 0xFF;

    SNES_mem_cart_inserted(this);

    return 0;
}