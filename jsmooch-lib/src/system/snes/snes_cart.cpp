//
// Created by . on 4/21/25.
//
#include <cmath>

#include "snes_cart.h"
#include "snes_bus.h"
#include "snes_mem.h"
namespace SNES {

void cart::read_ver1_header()
{
    auto *rom = static_cast<u8 *>(ROM.ptr);
    header.sram_sizebit = rom[header.offset + 0x28];
    header.rom_sizebit = rom[header.offset + 0x27];
    memset(header.internal_name, 0, sizeof(header.internal_name));
    memcpy(header.internal_name, rom+header.offset+0x10, 20);
}

void cart::read_ver2_header()
{
    read_ver1_header();
}

void cart::read_ver3_header()
{
    read_ver1_header();
    //  FFBCh  Expansion FLASH Size (1 SHL n) Kbytes (used in JRA PAT)
    //  FFBDh  Expansion RAM Size (1 SHL n) Kbytes (in GSUn games) (without battery?
    auto *rom = static_cast<u8 *>(ROM.ptr);
    u32 sl = rom[header.offset + 0x1C];
    if (sl > 0) {
        const u32 expansion_flash_size = 1 << sl;
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

i32 cart::score_header(const u32 addr) const
{
    /*
     * The main ideas behind this detection are from Ares.
     */
    if (ROM.size < (addr + 0x50)) return 0;

    i32 score = 0;
    auto *rom = static_cast<u8 *>(ROM.ptr);
    const u16 reset_vector = rom[addr + 0x4C] | (rom[addr + 0x4D] << 8);
    // 0000-7FFF on lowest page is never ROM data
    if (reset_vector < 0x8000) return 0;

    // first instruction executed...
    u8 opcode = rom[(addr & ~0x7FFF) | (reset_vector & 0x7FFF)];
    for (unsigned int likely_opcode : likely_opcodes) {
        if (opcode == likely_opcode) score += 8;
    }
    for (unsigned int plausible_opcode : plausible_opcodes) {
        if (opcode == plausible_opcode) score += 4;
    }
    for (unsigned int implausible_opcode : implausible_opcodes) {
        if (opcode == implausible_opcode) score -= 4;
    }
    for (unsigned int terrible_opcode : terrible_opcodes) {
        if (opcode == terrible_opcode) score -= 8;
    }

    const u32 complement = rom[addr + 0x2C] | (rom[addr + 0x2D] << 8);
    const u32 checksum = rom[addr + 0x2E] | (rom[addr + 0x2F] << 8);
    if (((checksum + complement) & 0xFFFF) == 0xFFFF) score += 4;

    const u32  map_mode = rom[addr + 0x25] & ~0x10;

    if ((addr == 0x7FB0) && (map_mode == 0x20)) score += 2;
    if ((addr == 0xFFB0) && (map_mode == 0x21)) score += 2;

    return score > 0 ? score : 0;
}

u32 cart::find_cart_header()
{
    u32 first_offset = 0x7F00;
    const i32 lorom = score_header(0x7FB0);
    const i32 hirom = score_header(0xFFB0);
    u32 ex_lo_rom = score_header(0x407FB0);
    u32 ex_hi_rom = score_header(0x40FFB0);

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

u32 cart::load_ROM_from_RAM(char* fil, u64 fil_sz, physical_io_device &pio)
{
    const u32 SMCheader_size = fil_sz % 1024;
    if (SMCheader_size != 0) {
        printf("\nSMC header found! Removing %d bytes!", SMCheader_size);
        fil_sz -= SMCheader_size;
        fil += SMCheader_size;
    }
    ROM.allocate(fil_sz);
    memcpy(ROM.ptr, fil, fil_sz);
    header.offset = find_cart_header();
    printf("\nCART HEADER DETECTED AT %06x", header.offset);

    printf("\n\nLoading SNES cart, size %ld bytes", ROM.size);
    u32 ver = 1;
    u8 *rom = static_cast<u8 *>(ROM.ptr);
    if (rom[header.offset + 0x24] == 0)
        ver = 2;
    if (rom[header.offset + 0x2A] == 0x33)
        ver = 3;
    printf("\nHeader version %d", ver);
    header.version = ver;
    header.hi_speed = (rom[header.offset + 0x25] & 0x10) != 0;
    header.mapping_mode = 0xF & rom[header.offset + 0x25];
    printf("\nMapping mode 0x%02x", header.mapping_mode);
    switch(header.mapping_mode) {
        case 0:
            header.lorom = 1;
            printf("\nLOROM!");
            break;
        case 1:
            header.hirom = 1;
            printf("\nHIROM!");
            break;
        default:
            printf("\nNOT SUPPORT MAP MODE %d", header.mapping_mode);
            break;
    }

    u32 num_address_lines = static_cast<u32>(ceil(log2(static_cast<double>(fil_sz))));
    switch(ver) {
        case 1:
            read_ver1_header();
            break;
        case 2:
            read_ver2_header();
            break;
        case 3:
            read_ver3_header();
            break;
    }
    header.sram_size = (1 << header.sram_sizebit) * 1024;
    header.sram_mask = header.sram_size - 1;
    printf("\nInternal name %s", header.internal_name);
    printf("\nSRAM size %d bytes", header.sram_size);

    SRAM = &pio.cartridge_port.SRAM;
    persistent_store *sram = SRAM;
    sram->persistent = 1;
    sram->requested_size = header.sram_size;
    sram->fill_value = 0xFF;

    snes->mem.cart_inserted();

    return 0;
}
}