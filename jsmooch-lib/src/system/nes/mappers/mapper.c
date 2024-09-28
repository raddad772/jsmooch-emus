#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "../nes.h"
#include "mapper.h"

#include "nrom.h"

static void init_memmap_empty(struct NES_bus *this)
{
    for (u32 i = 0; i < 0x10000; i += 0x2000) {
        struct NES_memmap *m = &this->CPU_map[i >> 13];
        m->empty = 1;
        m->addr = i;
        m->offset = 0;
        m->read_only = 1;
        m->buf = NULL;
    }

    for (u32 i = 0; i < 0x3000; i += 0x400) {
        struct NES_memmap *m = &this->PPU_map[i >> 10];
        m->empty = 1;
        m->addr = i;
        m->offset = 0;
        m->read_only = 1;
        m->buf = NULL;
    }
}

void NES_bus_init(struct NES_bus *this, struct NES* nes)
{
    printf("\nNES BUS INIT!");
    memset(this, 0, sizeof(struct NES_bus));
    this->nes = nes;
    simplebuf8_init(&this->PRG_RAM);
    simplebuf8_init(&this->PRG_ROM);
    simplebuf8_init(&this->CHR_ROM);
    simplebuf8_init(&this->CHR_RAM);

    simplebuf8_init(&this->CPU_RAM);
    simplebuf8_init(&this->CIRAM);

    simplebuf8_allocate(&this->CPU_RAM, 0x800); // 2KB of CPU RAM
    simplebuf8_allocate(&this->CIRAM, 0x800); // 2KB of PPU RAM

    init_memmap_empty(this);
    NROM_init(this, nes);

    for (u32 addr = 0x2000; addr < 0x3FFF; addr += 0x400) {
        struct NES_memmap *m = &this->PPU_map[addr >> 10];
        m->empty = 0;
        m->buf = &this->CIRAM;
        m->read_only = 0;
        m->addr = addr;
        m->mask = 0x3FF;
    }
}

void NES_bus_delete(struct NES_bus *this)
{
    assert(this->destruct);
    this->destruct(this);
    this->destruct = NULL;
    this->a12_watch = NULL;
    this->cpu_cycle = NULL;
    this->reset = NULL;
    this->setcart = NULL;
    this->readcart = NULL;
    this->writecart = NULL;
    this->ptr = NULL;

    // TODO: mapper delete first
    simplebuf8_delete(&this->PRG_RAM);
    simplebuf8_delete(&this->PRG_ROM);
    simplebuf8_delete(&this->CHR_ROM);
    simplebuf8_delete(&this->CHR_RAM);
    simplebuf8_delete(&this->CPU_RAM);
    simplebuf8_delete(&this->CIRAM);

    init_memmap_empty(this);
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
        case 7: // AXROM
            which = NESM_AXROM;
            break;
        case 23: // VRC4
            which = NESM_VRC4E_4F;
            break;
        case 69: // JXROM/Sunsoft 7M and 5M
            which = NESM_SUNSOFT_57;
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
            //NES_mapper_MMC3b_init(this, this->nes);
            printf("\nMBC1");
            break;
        case NESM_AXROM:
            //NES_mapper_AXROM_init(this, this->nes);
            printf("\nAXROM");
            break;
        case NESM_CNROM:
            //NES_mapper_CNROM_init(this, this->nes);
            printf("\nCNROM");
            break;
        case NESM_DXROM:
            //NES_mapper_DXROM_init(this, this->nes);
            printf("\nDXROM");
            break;
        case NESM_MMC1:
            //NES_mapper_MMC1_init(this, this->nes);
            printf("\nMMC1");
            break;
        case NESM_UXROM:
            //NES_mapper_UXROM_init(this, this->nes);
            printf("\nUXROM");
            break;
        case NESM_VRC4E_4F:
            //NES_mapper_VRC2B_4E_4F_init(this, this->nes);
            printf("\nVRC4");
            break;
        case NESM_SUNSOFT_57:
            //NES_mapper_sunsoft_57_init(this, this->nes, NES_mapper_sunsoft_5b);
            printf("\nSunsoft mapper");
            break;
        default:
            printf("\nNO SUPPORTED MAPPER! %d", which);
            break;
    }

}

void NES_bus_CPU_cycle(struct NES *nes)
{
    if (nes->bus.cpu_cycle) nes->bus.cpu_cycle(&nes->bus);
}

static inline u32 mmap_read(struct NES_memmap *this, u32 addr, u32 old_val)
{
    if (this->empty) return old_val;
    assert(this->buf);
    assert(this->buf->ptr);
    return this->buf->ptr[((addr & this->mask) + this->offset) % this->buf->sz];
}

static inline void mmap_write(struct NES_memmap *this, u32 addr, u32 val)
{
    if (this->read_only || this->empty) return;
    assert(this->buf);
    assert(this->buf->ptr);
    this->buf->ptr[((addr & this->mask) + this->offset) % this->buf->sz] = val;
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
    if (!do_read) return nv;

    return mmap_read(&nes->bus.CPU_map[addr >> 13], addr, old_val);
}

void NES_bus_CPU_write(struct NES *nes, u32 addr, u32 val)
{
    if (addr < 0x2000) {
        nes->bus.CPU_RAM.ptr[addr & 0x7FF] = val;
        return;
    }
    if (addr < 0x4000)
        return NES_bus_PPU_write_regs(nes, addr, val);
    if (addr < 0x4020)
        return NES_bus_CPU_write_reg(nes, addr, val);

    u32 do_write = 1;
    nes->bus.writecart(&nes->bus, addr, val, &do_write);
    if (!do_write) return;

    mmap_write(&nes->bus.CPU_map[addr >> 13], addr, val);
}

u32 NES_PPU_read_effect(struct NES *nes, u32 addr)
{
    addr &= 0x3FFF;
    return mmap_read(&nes->bus.PPU_map[addr >> 10], addr, 0);
}

u32 NES_PPU_read_noeffect(struct NES *nes, u32 addr)
{
    addr &= 0x3FFF;
    return mmap_read(&nes->bus.PPU_map[addr >> 10], addr, 0);
}

void NES_PPU_write(struct NES *nes, u32 addr, u32 val)
{
    addr &= 0x2FFF;
    mmap_write(&nes->bus.PPU_map[addr >> 10], addr, val);
}

void NES_bus_reset(struct NES *nes)
{
    if (nes->bus.reset) nes->bus.reset(&nes->bus);
}

void NES_bus_set_cart(struct NES *nes, struct NES_cart* cart)
{
    assert(nes->bus.setcart);
    printf("\nPRG ROM: %d bytes/%lld", cart->header.prg_rom_size, cart->PRG_ROM.size);
    simplebuf8_copy_from_buf(&nes->bus.PRG_ROM, &cart->PRG_ROM);
    if (cart->header.chr_rom_size > 0) {
        printf("\nCHR ROM: %d bytes/%lld", cart->header.chr_rom_size, cart->CHR_ROM.size);
        simplebuf8_copy_from_buf(&nes->bus.CHR_ROM, &cart->CHR_ROM);
    }
    if (cart->header.chr_ram_present) {
        simplebuf8_allocate(&nes->bus.CHR_RAM, cart->header.chr_ram_size);
        printf("\nCHR RAM: %lld bytes", nes->bus.CHR_RAM.sz);
    }
    if (cart->header.prg_ram_size > 0) {
        simplebuf8_allocate(&nes->bus.PRG_RAM, cart->header.prg_ram_size);
        printf("\nPRG RAM: %lld bytes", nes->bus.PRG_RAM.sz);
    }

    nes->bus.setcart(&nes->bus, cart);
}

void NES_bus_a12_watch(struct NES *nes, u32 addr)
{
    if (nes->bus.a12_watch) nes->bus.a12_watch(&nes->bus, addr);
}

static void set_bm(struct NES_memmap *mmap, u32 shift, u32 range_start, u32 range_end, struct simplebuf8* buf, u32 offset, u32 is_readonly)
{
    u32 range_size = 1 << shift;
    printf("\nRANGE SIZE %04x", range_size);
    for (u32 addr = range_start; addr < range_end; addr += range_size) {
        struct NES_memmap *m = mmap + (addr >> shift);
        printf("\nPAGE:%d ADDR:%04x OFFSET:%05x", addr >> shift, addr, offset);
        m->offset = offset;
        m->buf = buf;
        m->empty = 0;
        m->read_only = is_readonly;
        m->addr = addr;
        m->mask = range_size - 1;
        printf("\nMASK! %04x", m->mask);

        offset = (offset + range_size) % buf->sz;
    }
}


void NES_bus_map_PRG8K(struct NES_bus *bus, u32 range_start, u32 range_end, struct simplebuf8 *buf, u32 offset, u32 is_readonly)
{
    printf("\n\nMAP PRG");
    set_bm(bus->CPU_map, 13, range_start, range_end, buf, offset, is_readonly);
}

void NES_bus_map_CHR1K(struct NES_bus *bus, u32 range_start, u32 range_end, struct simplebuf8 *buf, u32 offset, u32 is_readonly)
{
    printf("\n\nMAP CHR");
    set_bm(bus->PPU_map, 10, range_start, range_end, buf, offset, is_readonly);
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
        printf("\nPPUmap %d", addr >> 10);
        struct NES_memmap *m = &bus->PPU_map[addr >> 10];
        m->offset = bus->ppu_mirror(addr);
        m->mask = 0x3FF;
    }
}