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
    u8 *rom = (u8 *)this->cart.ROM.ptr;
    this->cart.header.sram_sizebit = rom[this->cart.header.offset + 0xD8];
    this->cart.header.rom_sizebit = rom[this->cart.header.offset + 0xD7];
    memset(this->cart.header.internal_name, 0, sizeof(this->cart.header.internal_name));
    memcpy(this->cart.header.internal_name, rom+this->cart.header.offset+0xC0, 20);
}

static void read_ver3_header(struct SNES *this)
{
    printf("\nWARN VER3 HEADER NOT SUPPORT!");
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
    this->cart.header.offset = 0x7F00;

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
    this->cart.header.mapping_mode = 0x2F & rom[this->cart.header.offset + 0xD5];
    printf("\nMapping mode 0x%02x", this->cart.header.mapping_mode);
    if (this->cart.header.mapping_mode == 0x20) this->cart.header.lorom = 1;
    printf("\nLOROM? %d", this->cart.header.lorom);
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