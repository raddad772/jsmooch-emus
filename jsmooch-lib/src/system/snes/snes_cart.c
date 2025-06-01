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
    this->cart.header.sram_sizebit = rom[this->cart.header.offset + 0x28];
    this->cart.header.rom_sizebit = rom[this->cart.header.offset + 0x27];
    memset(this->cart.header.internal_name, 0, sizeof(this->cart.header.internal_name));
    memcpy(this->cart.header.internal_name, rom+this->cart.header.offset+0x10, 20);
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
    u32 sl = rom[this->cart.header.offset + 0x1C];
    if (sl > 0) {
        u32 expansion_flash_size = 1 << sl;
        printf("\nEXPANSION FLASH SZ: %d", expansion_flash_size);
        printf("\nWARN NOT SUPPORT!");
    }
}


#define NUM_LIKELY 6
static const u32 likely_opcodes[NUM_LIKELY] = {
        0x78, 0x18, 0x38, 0x9C, 0x4C, 0x5C
};

#define NUM_PLAUSIBLE 11
static const u32 plausible_opcodes[NUM_PLAUSIBLE] = {
        0xC2, 0xE2, 0xAD, 0xAE, 0xAC, 0xAF, 0xA9,
        0xA2, 0xA0, 0x20, 0x22
};

#define NUM_IMPLAUSIBLE 6
static const u32 implausible_opcodes[NUM_IMPLAUSIBLE] = {
        0x40, 0x60, 0x6B, 0xCD, 0xEC, 0xCC
};

#define NUM_TERRIBLE 5
static const u32 terrible_opcodes[NUM_TERRIBLE] = {
        0x00, 0x02, 0xDB, 0x42, 0xFF
};

static i32 score_header(struct SNES *this, u32 addr)
{
    /*
     * The main ideas behind this detection are from Ares.
     */
    if (this->cart.ROM.size < (addr + 0x50)) return 0;

    i32 score = 0;
    u8 *rom = this->cart.ROM.ptr;
    u16 reset_vector = rom[addr + 0x4C] | (rom[addr + 0x4D] << 8);
    // 0000-7FFF on lowest page is never ROM data
    if (reset_vector < 0x8000) return 0;

    // first instruction executed...
    u8 opcode = rom[(addr & ~0x7FFF) | (reset_vector & 0x7FFF)];
    for (u32 i = 0; i < NUM_LIKELY; i++) {
        if (opcode == likely_opcodes[i]) score += 8;
    }
    for (u32 i = 0; i < NUM_PLAUSIBLE; i++) {
        if (opcode == plausible_opcodes[i]) score += 4;
    }
    for (u32 i = 0; i < NUM_IMPLAUSIBLE; i++) {
        if (opcode == implausible_opcodes[i]) score -= 4;
    }
    for (u32 i = 0; i < NUM_TERRIBLE; i++) {
        if (opcode == terrible_opcodes[i]) score -= 8;
    }

    u32 complement = rom[addr + 0x2C] | (rom[addr + 0x2D] << 8);
    u32 checksum = rom[addr + 0x2E] | (rom[addr + 0x2F] << 8);
    if (((checksum + complement) & 0xFFFF) == 0xFFFF) score += 4;

    u32  map_mode = rom[addr + 0x25] & ~0x10;

    if ((addr == 0x7FB0) && (map_mode == 0x20)) score += 2;
    if ((addr == 0xFFB0) && (map_mode == 0x21)) score += 2;

    return score > 0 ? score : 0;
}

static u32 find_cart_header(struct SNES *this)
{
    u32 first_offset = 0x7F00;
    i32 lorom = score_header(this, 0x7FB0);
    i32 hirom = score_header(this, 0xFFB0);
    u32 ex_lo_rom = score_header(this, 0x407FB0);
    u32 ex_hi_rom = score_header(this, 0x40FFB0);

    if (ex_lo_rom) ex_lo_rom += 4;
    if (ex_hi_rom) ex_hi_rom += 4;

    u32 header_addr = 0x40FFB0;
    if ((lorom >= hirom) && (lorom >= ex_lo_rom) && (lorom >= ex_hi_rom)) header_addr = 0x7FB0;
    else if ((hirom >= ex_lo_rom) && (hirom >= ex_hi_rom)) header_addr = 0xFFB0;
    else if (ex_lo_rom >= ex_hi_rom) header_addr = 0xf07FB0;

    if (header_addr > 0x10000) {
        printf("\nEXHI OR EXLO, NOT SUPPORT!");
    }

    return header_addr;
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
    if (rom[this->cart.header.offset + 0x24] == 0)
        ver = 2;
    if (rom[this->cart.header.offset + 0x2A] == 0x33)
        ver = 3;
    printf("\nHeader version %d", ver);
    this->cart.header.version = ver;
    this->cart.header.hi_speed = (rom[this->cart.header.offset + 0x25] & 0x10) != 0;
    this->cart.header.mapping_mode = 0xF & rom[this->cart.header.offset + 0x25];
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