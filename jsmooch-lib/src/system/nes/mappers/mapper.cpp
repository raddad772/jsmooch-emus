#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "../nes.h"
#include "mapper.h"

#include "nrom.h"
#include "system/nes/mappers/mmc3b_dxrom/mmc3b_dxrom.h"
#include "axrom.h"
#include "uxrom.h"
#include "cnrom_gnrom_jf11_jf14_color_dreams.h"
#include "mmc1_sxrom.h"
#include "vrc_2b_2e_4f.h"
#include "sunsoft_5_7.h"
#include "mmc5.h"

void NES_bus_init(struct NES_bus *this, struct NES* nes)
{
    memset(this, 0, sizeof(struct NES_bus));
    this->nes = nes;
    simplebuf8_init(&this->PRG_ROM);
    simplebuf8_init(&this->CHR_ROM);
    simplebuf8_init(&this->CHR_RAM);

    simplebuf8_init(&this->CPU_RAM);
    simplebuf8_init(&this->CIRAM);

    simplebuf8_allocate(&this->CPU_RAM, 0x800); // 2KB of CPU RAM
    simplebuf8_allocate(&this->CIRAM, 0x1000); // 4KB of PPU RAM (normally only 2KB is accessible)

    NES_memmap_init_empty(this->CPU_map, 0x0000, 0xFFFF, 13);
    NES_memmap_init_empty(this->PPU_map, 0x0000, 0x3FFF, 10);

    this->NES_audio_bias = 1.0f;
    this->mapper_audio_bias = 0.0f;
    NROM_init(this, nes);
    for (u32 addr = 0x2000; addr < 0x3FFF; addr += 0x400) {
        struct NES_memmap *m = &this->PPU_map[addr >> 10];
        m->empty = 0;
        m->buf = &this->CIRAM;
        m->read_only = 0;
        m->addr = addr;
        m->mask = 0x3FF;
        m->is_SRAM = 0;
        m->SRAM = NULL;
    }
}

void NES_bus_delete(struct NES_bus *this)
{
    if (this->destruct) this->destruct(this);
    this->destruct = NULL;
    this->a12_watch = NULL;
    this->cpu_cycle = NULL;
    this->reset = NULL;
    this->setcart = NULL;
    this->readcart = NULL;
    this->writecart = NULL;
    this->ptr = NULL;

    simplebuf8_delete(&this->PRG_ROM);
    simplebuf8_delete(&this->CHR_ROM);
    simplebuf8_delete(&this->CHR_RAM);
    simplebuf8_delete(&this->CPU_RAM);
    simplebuf8_delete(&this->CIRAM);

    NES_memmap_init_empty(this->CPU_map, 0x0000, 0xFFFF, 13);
    NES_memmap_init_empty(this->PPU_map, 0x0000, 0x3FFF, 10);

}

static enum NES_mappers iNES_mapper_to_my_mappers(u32 wh)
{
    enum NES_mappers which = NESM_UNKNOWN;
    switch(wh) {
        case 0: // no mapper
            which = NESM_NONE;
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

void NES_bus_set_which_mapper(struct NES_bus *this, u32 wh)
{
    if (this->ptr != NULL) {
        if (this->destruct) this->destruct(this->ptr);
        this->ptr = NULL;
        this->destruct = NULL;
    }

    enum NES_mappers which = iNES_mapper_to_my_mappers(wh);

    switch (which) {
        case NESM_UNKNOWN:
            printf("\nERROR UNKNOWN VALUE...");
            return;
        case NESM_NONE: // No mapper!
            NROM_init(this, this->nes);
            printf("\nNO MAPPER!");
            break;
        case NESM_MMC3b:
        case NESM_DXROM:
            MMC3b_init(this, this->nes, which);
            printf("\nMMC3B or DXROM");
            break;
        case NESM_MMC5:
            printf("\nMMC5! GOOD LUCK!");
            MMC5_init(this, this->nes);
            break;
        case NESM_AXROM:
            AXROM_init(this, this->nes);
            printf("\nAXROM");
            break;
        case NESM_CNROM:
        case NESM_GNROM:
        case NESM_COLOR_DREAMS:
        case NESM_JF11_JF14:
            printf("\nGNROM-like init!");
            GNROM_JF11_JF14_color_dreams_init(this, this->nes, which);
            break;
        case NESM_MMC1:
            SXROM_init(this, this->nes);
            printf("\nMMC1");
            break;
        case NESM_UXROM:
            UXROM_init(this, this->nes);
            printf("\nUXROM");
            break;
        case NESM_VRC4E_4F:
            VRC2B_4E_4F_init(this, this->nes, which);
            printf("\nVRC4");
            break;
        case NESM_SUNSOFT_5b:
        case NESM_SUNSOFT_7:
            sunsoft_5_7_init(this, this->nes, which);
            printf("\nSunsoft mapper");
            break;
        default:
            assert(1==2);
            printf("\nNO SUPPORTED MAPPER! %d", which);
            break;
    }

}

void NES_bus_CPU_cycle(struct NES *nes)
{
    if (nes->bus.cpu_cycle) nes->bus.cpu_cycle(&nes->bus);
}

u32 NES_bus_CPU_read(struct NES *nes, u32 addr, u32 old_val, u32 has_effect)
{
    if (addr < 0x2000)
        return nes->bus.CPU_RAM.ptr[addr & 0x7FF];
    if (addr < 0x4000)
        return NES_bus_PPU_read_regs(nes, addr, old_val, has_effect);
    if (addr < 0x4020)
        return NES_bus_CPU_read_reg(nes, addr, old_val, has_effect);

    u32 do_read = 1;
    u32 nv = nes->bus.readcart(&nes->bus, addr, old_val, has_effect, &do_read);
    if (!do_read) {
        return nv;
    }

    u32 v = NES_mmap_read(&nes->bus.CPU_map[addr >> 13], addr, old_val);
    return v;
}

void NES_bus_CPU_write(struct NES *nes, u32 addr, u32 val)
{
    if (addr < 0x2000) {
        nes->bus.CPU_RAM.ptr[addr & 0x7FF] = val;
        return;
    }

    u32 do_write = 1;
    nes->bus.writecart(&nes->bus, addr, val, &do_write); // Thanks to MMC4 this has to go here

    if (addr < 0x4000)
        return NES_bus_PPU_write_regs(nes, addr, val);
    if (addr < 0x4020)
        return NES_bus_CPU_write_reg(nes, addr, val);

    if (!do_write) {
        assert(1==0);
        return;
    }

    NES_mmap_write(&nes->bus.CPU_map[addr >> 13], addr, val);
}

u32 NES_PPU_read_effect(struct NES *nes, u32 addr)
{
    addr &= 0x3FFF;
    if (nes->bus.PPU_read_override) return nes->bus.PPU_read_override(&nes->bus, addr, 1);
    if (addr > 0x2000) addr = (addr & 0xFFF) | 0x2000;
    if (nes->bus.a12_watch) nes->bus.a12_watch(&nes->bus, addr);
    return NES_mmap_read(&nes->bus.PPU_map[addr >> 10], addr, 0);
}

u32 NES_PPU_read_noeffect(struct NES *nes, u32 addr)
{
    addr &= 0x3FFF;
    if (nes->bus.PPU_read_override) return nes->bus.PPU_read_override(&nes->bus, addr, 0);
    if (addr > 0x2000) addr = (addr & 0xFFF) | 0x2000;
    return NES_mmap_read(&nes->bus.PPU_map[addr >> 10], addr, 0);
}

void NES_PPU_write(struct NES *nes, u32 addr, u32 val)
{
    addr &= 0x3FFF;
    if (nes->bus.PPU_write_override) return nes->bus.PPU_write_override(&nes->bus, addr, val);
    if (addr > 0x2000) addr = (addr & 0xFFF) | 0x2000;
    NES_mmap_write(&nes->bus.PPU_map[addr >> 10], addr, val);
}

void NES_bus_reset(struct NES *nes)
{
    if (nes->bus.fake_PRG_RAM.ptr == NULL)
        nes->bus.fake_PRG_RAM.ptr = nes->bus.SRAM->data;
    if (nes->bus.reset) nes->bus.reset(&nes->bus);
}

void NES_bus_set_cart(struct NES *nes, struct NES_cart* cart, struct physical_io_device *pio)
{
    assert(nes->bus.setcart);
    struct NES_bus *this = &nes->bus;
    //printf("\nPRG ROM: %d bytes/%lld", cart->header.prg_rom_size, cart->PRG_ROM.size);
    simplebuf8_copy_from_buf(&this->PRG_ROM, &cart->PRG_ROM);
    this->num_PRG_ROM_banks8K = this->PRG_ROM.sz >> 13;
    this->num_PRG_ROM_banks16K = this->num_PRG_ROM_banks8K >> 1;
    this->num_PRG_ROM_banks32K = this->num_PRG_ROM_banks16K >> 1;
    if (this->num_PRG_ROM_banks8K == 0) this->num_PRG_ROM_banks8K = 1;
    if (this->num_PRG_ROM_banks16K == 0) this->num_PRG_ROM_banks16K = 1;
    if (this->num_PRG_ROM_banks32K == 0) this->num_PRG_ROM_banks32K = 1;

    printf("\nPRG ROM sz:%lld  banks:%d", this->PRG_ROM.sz, this->num_PRG_ROM_banks8K);

    if (cart->header.chr_rom_size > 0) {
        printf("\nCHR ROM: %d bytes/%lld", cart->header.chr_rom_size, cart->CHR_ROM.size);
        simplebuf8_copy_from_buf(&this->CHR_ROM, &cart->CHR_ROM);
        this->num_CHR_ROM_banks1K = this->CHR_ROM.sz >> 10;
        this->num_CHR_ROM_banks2K = this->CHR_ROM.sz >> 11;
        this->num_CHR_ROM_banks4K = this->CHR_ROM.sz >> 12;
        this->num_CHR_ROM_banks8K = this->CHR_ROM.sz >> 13;
        if (this->num_CHR_ROM_banks1K == 0) this->num_CHR_ROM_banks1K = 1;
        if (this->num_CHR_ROM_banks2K == 0) this->num_CHR_ROM_banks2K = 1;
        if (this->num_CHR_ROM_banks4K == 0) this->num_CHR_ROM_banks4K = 1;
        if (this->num_CHR_ROM_banks8K == 0) this->num_CHR_ROM_banks8K = 1;
    }
    if (cart->header.chr_ram_present) {
        simplebuf8_allocate(&this->CHR_RAM, cart->header.chr_ram_size);
        printf("\nCHR RAM: %lld bytes", this->CHR_RAM.sz);
        this->num_CHR_RAM_banks = this->CHR_RAM.sz >> 10;
    }
    this->SRAM = &pio->cartridge_port.SRAM;
    this->SRAM->requested_size = cart->header.prg_ram_size;
    this->SRAM->persistent = cart->header.battery_present;
    this->fake_PRG_RAM.sz = cart->header.prg_ram_size;
    this->fake_PRG_RAM.ptr = NULL;
    this->SRAM->dirty = 1;
    this->SRAM->ready_to_use = 0;

    this->setcart(&nes->bus, cart);
}

void NES_bus_a12_watch(struct NES *nes, u32 addr)
{
    if (nes->bus.a12_watch) nes->bus.a12_watch(&nes->bus, addr);
}


void NES_bus_map_PRG32K(struct NES_bus *bus, u32 range_start, u32 range_end, struct simplebuf8 *buf, u32 bank, u32 is_readonly)
{
    NES_memmap_map(bus->CPU_map, 13, range_start, range_end, buf, bank << 15, is_readonly, bus->nes->dbg.interface, 0, bus->SRAM);
}

void NES_bus_map_PRG16K(struct NES_bus *bus, u32 range_start, u32 range_end, struct simplebuf8 *buf, u32 bank, u32 is_readonly)
{
    NES_memmap_map(bus->CPU_map, 13, range_start, range_end, buf, bank << 14, is_readonly, bus->nes->dbg.interface, 0, bus->SRAM);
}

void NES_bus_map_PRG8K(struct NES_bus *bus, u32 range_start, u32 range_end, struct simplebuf8 *buf, u32 bank, u32 is_readonly)
{
    NES_memmap_map(bus->CPU_map, 13, range_start, range_end, buf, bank << 13, is_readonly, bus->nes->dbg.interface, 0, bus->SRAM);
}

void NES_bus_map_CHR1K(struct NES_bus *bus, u32 range_start, u32 range_end, struct simplebuf8 *buf, u32 bank, u32 is_readonly)
{
    NES_memmap_map(bus->PPU_map, 10, range_start, range_end, buf, bank << 10, is_readonly, NULL, 1, bus->SRAM);
}

void NES_bus_map_CHR2K(struct NES_bus *bus, u32 range_start, u32 range_end, struct simplebuf8 *buf, u32 bank, u32 is_readonly)
{
    NES_memmap_map(bus->PPU_map, 10, range_start, range_end, buf, bank << 11, is_readonly, NULL, 1, bus->SRAM);
}

void NES_bus_map_CHR4K(struct NES_bus *bus, u32 range_start, u32 range_end, struct simplebuf8 *buf, u32 bank, u32 is_readonly)
{
    NES_memmap_map(bus->PPU_map, 10, range_start, range_end, buf, bank << 12, is_readonly, NULL, 1, bus->SRAM);
}

void NES_bus_map_CHR8K(struct NES_bus *bus, u32 range_start, u32 range_end, struct simplebuf8 *buf, u32 bank, u32 is_readonly)
{
    NES_memmap_map(bus->PPU_map, 10, range_start, range_end, buf, bank << 13, is_readonly, NULL, 1, bus->SRAM);
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

static u32  NES_mirror_ppu_Bonly(u32 addr) {
    return 0x400 | (addr & 0x3FF);
}


void NES_bus_PPU_mirror_set(struct NES_bus *bus)
{
    switch(bus->ppu_mirror_mode) {
        case PPUM_Vertical:
            bus->ppu_mirror = &NES_mirror_ppu_vertical;
            break;
        case PPUM_Horizontal:
            bus->ppu_mirror = &NES_mirror_ppu_horizontal;
            break;
        case PPUM_FourWay:
            bus->ppu_mirror = &NES_mirror_ppu_four;
            break;
        case PPUM_ScreenAOnly:
            bus->ppu_mirror = &NES_mirror_ppu_Aonly;
            break;
        case PPUM_ScreenBOnly:
            bus->ppu_mirror = &NES_mirror_ppu_Bonly;
            break;
        default:
            assert(1==0);
    }
    for (u32 addr = 0x2000; addr < 0x3FFF; addr += 0x400) {
        struct NES_memmap *m = &bus->PPU_map[addr >> 10];
        m->offset = bus->ppu_mirror(addr);
        m->mask = 0x3FF;
        //printf("\nPPUmap %04x to offset %04x", addr, m->offset);
    }
}