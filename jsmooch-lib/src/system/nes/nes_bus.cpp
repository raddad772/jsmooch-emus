//
// Created by . on 11/13/25.
//

#include <cstring>
#include <cassert>

#include "nes.h"
#include "nes_bus.h"
#include "mappers/mapper.h"
#include "mappers/nes_memmap.h"
#include "mappers/axrom.h"
#include "mappers/cnrom_gnrom_jf11_jf14_color_dreams.h"
#include "mappers/mmc1_sxrom.h"
#include "mappers/mmc5.h"
#include "mappers/nrom.h"
#include "mappers/sunsoft_5_7.h"
#include "mappers/uxrom.h"
#include "mappers/vrc_2b_2e_4f.h"
#include "mappers/mmc3b_dxrom/mmc3b_dxrom.h"

NES_bus::NES_bus(NES *nes) : nes(nes) {
    NES_memmap_init_empty(CPU_map, 0x0000, 0xFFFF, 13);
    NES_memmap_init_empty(PPU_map, 0x0000, 0x3FFF, 10);
    for (u32 addr = 0x2000; addr < 0x3FFF; addr += 0x400) {
        NES_memmap *m = &PPU_map[addr >> 10];
        m->empty = 0;
        m->buf = &CIRAM;
        m->read_only = 0;
        m->addr = addr;
        m->mask = 0x3FF;
        m->is_SRAM = 0;
        m->SRAM = nullptr;
    }

    mapper = static_cast<NES_mapper *>(new NROM(this));
    which = NESM_NROM;
};

NES_bus::~NES_bus()
{
    delete mapper;
    NES_memmap_init_empty(CPU_map, 0x0000, 0xFFFF, 13);
    NES_memmap_init_empty(PPU_map, 0x0000, 0x3FFF, 10);
}

static NES_mappers iNES_mapper_to_my_mappers(u32 wh)
{
    NES_mappers which = NESM_UNKNOWN;
    switch(wh) {
        case 0: // no mapper
            which = NESM_NROM;;
            break;
        case 1: // MMC1
            which = NESM_MMC1;
            break;
        case 2: // UxROM
            which = NESM_UXROM;
            break;
        case 3: // CNROM
            which = NESM_CNROM;
            break;
        case 4: // MMC3
            which = NESM_MMC3b;
            break;
        case 5:
            which = NESM_MMC5;
            break;
        case 7: // AXROM
            which = NESM_AXROM;
            break;
        case 11: // Color Dreams
            which = NESM_COLOR_DREAMS;
            break;
        case 23: // VRC4
            which = NESM_VRC4E_4F;
            break;
        case 66:
            which = NESM_GNROM;
            break;
        case 69: // JXROM/Sunsoft 7M and 5M
            which = NESM_SUNSOFT_5b;
            break;
        case 140:
            which = NESM_JF11_JF14;
            break;
        case 206: // DXROM
            which = NESM_DXROM;
            break;
        default:
            printf("Unknown mapper number dawg! %d", wh);
            which = NESM_UNKNOWN;
            break;
    }
    return which;
}

void NES_bus::set_which_mapper(u32 wh)
{
    delete mapper;
    which = iNES_mapper_to_my_mappers(wh);
    switch (which) {
        case NESM_UNKNOWN:
            printf("\nERROR UNKNOWN VALUE...");
            return;
        case NESM_NROM: // No mapper!
            mapper = static_cast<NES_mapper *>(new NROM(this));
            printf("\nNO MAPPER!");
            break;
        case NESM_MMC3b:
        case NESM_DXROM:
            mapper = static_cast<NES_mapper *>(new MMC3b_DXROM(this, which));
            printf("\nMMC3B or DXROM");
            break;
        case NESM_MMC5:
            printf("\nMMC5! GOOD LUCK!");
            mapper = static_cast<NES_mapper *>(new MMC5(this));
            break;
        case NESM_AXROM:
            mapper = static_cast<NES_mapper *>(new AXROM(this));
            printf("\nAXROM");
            break;
        case NESM_CNROM:
        case NESM_GNROM:
        case NESM_COLOR_DREAMS:
        case NESM_JF11_JF14:
            printf("\nGNROM-like init!");
            mapper = static_cast<NES_mapper *>(new GNROM_JF11_JF14_color_dreams(this, which));
            break;
        case NESM_MMC1:
            mapper = static_cast<NES_mapper *>(new SXROM(this));
            printf("\nMMC1");
            break;
        case NESM_UXROM:
            mapper = static_cast<NES_mapper *>(new UXROM(this));
            printf("\nUXROM");
            break;
        case NESM_VRC4E_4F:
            mapper = static_cast<NES_mapper *>(new VRC2B_4E_4F(this, which));
            printf("\nVRC4");
            break;
        case NESM_SUNSOFT_5b:
        case NESM_SUNSOFT_7:
            mapper = static_cast<NES_mapper *>(new sunsoft_5_7(this, which));
            printf("\nSunsoft mapper");
            break;
        default:
            assert(1==2);
            printf("\nNO SUPPORTED MAPPER! %d", which);
            break;
    }
}

u32 NES_bus::CPU_read(u32 addr, u32 old_val, u32 has_effect)
{
    if (addr < 0x2000)
        return nes->bus.CPU_RAM.ptr[addr & 0x7FF];
    if (addr < 0x4000)
        return nes->ppu.read_regs(addr, old_val, has_effect);
    if (addr < 0x4020)
        return nes->cpu.read_reg(addr, old_val, has_effect);

    u32 do_read = 1;
    u32 nv = mapper->readcart(addr, old_val, has_effect, do_read);
    if (!do_read) {
        return nv;
    }

    u32 v = nes->bus.CPU_map[addr >> 13].read(addr, old_val);
    return v;
}

void NES_bus::CPU_write(u32 addr, u32 val)
{
    if (addr < 0x2000) {
        nes->bus.CPU_RAM.ptr[addr & 0x7FF] = val;
        return;
    }

    u32 do_write = 1;
    mapper->writecart(addr, val, do_write); // Thanks to MMC4 this has to go here

    if (addr < 0x4000)
        return nes->ppu.write_regs(addr, val);
    if (addr < 0x4020)
        return nes->cpu.write_reg(addr, val);

    if (!do_write) {
        assert(1==0);
        return;
    }

    CPU_map[addr >> 13].write(addr, val);
}

u32 NES_bus::PPU_read_effect(u32 addr)
{
    addr &= 0x3FFF;
    if (mapper->overrides_PPU) return mapper->PPU_read_override(addr, 1);
    if (addr > 0x2000) addr = (addr & 0xFFF) | 0x2000;
    mapper->a12_watch(addr);
    return PPU_map[addr >> 10].read(addr, 0);
}

u32 NES_bus::PPU_read_noeffect(u32 addr)
{
    addr &= 0x3FFF;
    if (mapper->overrides_PPU) return mapper->PPU_read_override(addr, 0);
    if (addr > 0x2000) addr = (addr & 0xFFF) | 0x2000;
    return PPU_map[addr >> 10].read(addr, 0);
}

void NES_bus::PPU_write(u32 addr, u32 val)
{
    addr &= 0x3FFF;
    if (mapper->overrides_PPU) return mapper->PPU_write_override(addr, val);
    if (addr > 0x2000) addr = (addr & 0xFFF) | 0x2000;
    PPU_map[addr >> 10].write(addr, val);
}

void NES_bus::do_reset()
{
    if (fake_PRG_RAM.ptr == nullptr)
        fake_PRG_RAM.ptr = static_cast<u8 *>(SRAM->data);
    mapper->reset();
}

void NES_bus::set_cart(physical_io_device &pio)
{
    //NES_mapper *mp = &nes->bus;
    //printf("\nPRG ROM: %d bytes/%lld", nes->cart.header.prg_rom_size, nes->cart.PRG_ROM.size);
    PRG_ROM.copy_from_buf(nes->cart.PRG_ROM);
    num_PRG_ROM_banks8K = PRG_ROM.sz >> 13;
    num_PRG_ROM_banks16K = num_PRG_ROM_banks8K >> 1;
    num_PRG_ROM_banks32K = num_PRG_ROM_banks16K >> 1;
    if (num_PRG_ROM_banks8K == 0) num_PRG_ROM_banks8K = 1;
    if (num_PRG_ROM_banks16K == 0) num_PRG_ROM_banks16K = 1;
    if (num_PRG_ROM_banks32K == 0) num_PRG_ROM_banks32K = 1;

    printf("\nPRG ROM sz:%ld  banks:%d", PRG_ROM.sz, num_PRG_ROM_banks8K);

    if (nes->cart.header.chr_rom_size > 0) {
        printf("\nCHR ROM: %d bytes/%ld", nes->cart.header.chr_rom_size, nes->cart.CHR_ROM.size);
        CHR_ROM.copy_from_buf(nes->cart.CHR_ROM);
        num_CHR_ROM_banks1K = CHR_ROM.sz >> 10;
        num_CHR_ROM_banks2K = CHR_ROM.sz >> 11;
        num_CHR_ROM_banks4K = CHR_ROM.sz >> 12;
        num_CHR_ROM_banks8K = CHR_ROM.sz >> 13;
        if (num_CHR_ROM_banks1K == 0) num_CHR_ROM_banks1K = 1;
        if (num_CHR_ROM_banks2K == 0) num_CHR_ROM_banks2K = 1;
        if (num_CHR_ROM_banks4K == 0) num_CHR_ROM_banks4K = 1;
        if (num_CHR_ROM_banks8K == 0) num_CHR_ROM_banks8K = 1;
    }
    if (nes->cart.header.chr_ram_present) {
        CHR_RAM.allocate(nes->cart.header.chr_ram_size);
        printf("\nCHR RAM: %ld bytes", CHR_RAM.sz);
        num_CHR_RAM_banks = CHR_RAM.sz >> 10;
    }
    SRAM = &pio.cartridge_port.SRAM;
    SRAM->requested_size = nes->cart.header.prg_ram_size;
    SRAM->persistent = nes->cart.header.battery_present;
    fake_PRG_RAM.sz = nes->cart.header.prg_ram_size;
    fake_PRG_RAM.ptr = nullptr;
    SRAM->dirty = true;
    SRAM->ready_to_use = 0;

    mapper->setcart(nes->cart);
}


void NES_bus::map_PRG32K(u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly)
{
    NES_memmap_map(CPU_map, 13, range_start, range_end, buf, bank << 15, is_readonly, nes->dbg.interface, 0, SRAM);
}

void NES_bus::map_PRG16K(u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly)
{
    NES_memmap_map(CPU_map, 13, range_start, range_end, buf, bank << 14, is_readonly, nes->dbg.interface, 0, SRAM);
}

void NES_bus::map_PRG8K(u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly)
{
    NES_memmap_map(CPU_map, 13, range_start, range_end, buf, bank << 13, is_readonly, nes->dbg.interface, 0, SRAM);
}

void NES_bus::map_CHR1K(u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly)
{
    NES_memmap_map(PPU_map, 10, range_start, range_end, buf, bank << 10, is_readonly, nullptr, 1, SRAM);
}

void NES_bus::map_CHR2K(u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly)
{
    NES_memmap_map(PPU_map, 10, range_start, range_end, buf, bank << 11, is_readonly, nullptr, 1, SRAM);
}

void NES_bus::map_CHR4K(u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly)
{
    NES_memmap_map(PPU_map, 10, range_start, range_end, buf, bank << 12, is_readonly, nullptr, 1, SRAM);
}

void NES_bus::map_CHR8K(u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly)
{
    NES_memmap_map(PPU_map, 10, range_start, range_end, buf, bank << 13, is_readonly, nullptr, 1, SRAM);
}

static u32 NES_mirror_ppu_four(u32 addr) {
    return addr & 0xFFF;
}

static u32 NES_mirror_ppu_vertical(u32 addr) {
    return (addr & 0x0400) | (addr & 0x03FF);
}

static u32 NES_mirror_ppu_horizontal(u32 addr) {
    return ((addr >> 1) & 0x0400) | (addr & 0x03FF);
}

static u32 NES_mirror_ppu_Aonly(u32 addr) {
    return addr & 0x3FF;
}

static u32 NES_mirror_ppu_Bonly(u32 addr) {
    return 0x400 | (addr & 0x3FF);
}


void NES_bus::PPU_mirror_set()
{
    switch(ppu_mirror_mode) {
        case PPUM_Vertical:
            ppu_mirror = &NES_mirror_ppu_vertical;
            break;
        case PPUM_Horizontal:
            ppu_mirror = &NES_mirror_ppu_horizontal;
            break;
        case PPUM_FourWay:
            ppu_mirror = &NES_mirror_ppu_four;
            break;
        case PPUM_ScreenAOnly:
            ppu_mirror = &NES_mirror_ppu_Aonly;
            break;
        case PPUM_ScreenBOnly:
            ppu_mirror = &NES_mirror_ppu_Bonly;
            break;
        default:
            assert(1==0);
    }
    for (u32 addr = 0x2000; addr < 0x3FFF; addr += 0x400) {
        NES_memmap *m = &PPU_map[addr >> 10];
        m->offset = ppu_mirror(addr);
        m->mask = 0x3FF;
        //printf("\nPPUmap %04x to offset %04x", addr, m->offset);
    }
}