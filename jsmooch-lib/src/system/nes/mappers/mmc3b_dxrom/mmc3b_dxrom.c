//
// Created by . on 9/27/24.
//
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "../../nes.h"
#include "a12_watcher.h"
#include "mmc3b_dxrom.h"

#include "helpers_c/debugger/debugger.h"

#define THISM struct MMC3b *this = (struct MMC3b *)bus->ptr

struct MMC3b {
    struct NES *nes;

    struct NES_a12_watcher a12_watcher;

    enum NES_mappers kind;

    u32 has_IRQ;
    u32 has_mirroring_control;
    u32 fourway;
    struct {
        u32 rC000;
        u32 bank_select;
        u32 bank[8];
    } regs;

    struct {
        u32 ROM_bank_mode;
        u32 CHR_mode;
        u32 PRG_mode;
    } status;

    struct {
        u32 enable, reload;
        i32 counter;
    } irq;
};

#define READONLY 1
#define READWRITE 0

static void remap(struct NES_bus *bus, u32 is_boot)
{
    THISM;

    if (is_boot) {
        // PRG RAM
        NES_bus_map_PRG8K(bus, 0x6000, 0x7FFF, &bus->fake_PRG_RAM, 0, READWRITE);

        NES_bus_map_PRG8K(bus, 0xE000, 0xFFFF, &bus->PRG_ROM, bus->num_PRG_ROM_banks8K - 1, READONLY);

        // Map CHR ROM to basics
        NES_bus_map_CHR1K(bus, 0x0000, 0x1FFF, &bus->CHR_ROM, 0, READONLY);
    }

    if (this->status.PRG_mode == 0) {
        // 0 = 8000-9FFF swappable,
        //     C000-DFFF fixed to second-last bank
        // 1 = 8000-9FFF fixed to second-last bank
        //     C000-DFFF swappable
        NES_bus_map_PRG8K(bus, 0x8000, 0x9FFF, &bus->PRG_ROM, this->regs.bank[6], READONLY);
        NES_bus_map_PRG8K(bus, 0xC000, 0xDFFF, &bus->PRG_ROM, bus->num_PRG_ROM_banks8K - 2, READONLY);
    }
    else {
        NES_bus_map_PRG8K(bus, 0x8000, 0x9FFF, &bus->PRG_ROM, bus->num_PRG_ROM_banks8K - 2, READONLY);
        NES_bus_map_PRG8K(bus, 0xC000, 0xDFFF, &bus->PRG_ROM, this->regs.bank[6], READONLY);
    }

    NES_bus_map_PRG8K(bus, 0xA000, 0xBFFF, &bus->PRG_ROM, this->regs.bank[7], READONLY);

    if (this->status.CHR_mode == 0) {
        NES_bus_map_CHR1K(bus, 0x0000, 0x07FF, &bus->CHR_ROM, this->regs.bank[0] & 0xFE, READONLY);
        NES_bus_map_CHR1K(bus, 0x0800, 0x0FFF, &bus->CHR_ROM, this->regs.bank[1] & 0xFE, READONLY);
        NES_bus_map_CHR1K(bus, 0x1000, 0x13FF, &bus->CHR_ROM, this->regs.bank[2], READONLY);
        NES_bus_map_CHR1K(bus, 0x1400, 0x17FF, &bus->CHR_ROM, this->regs.bank[3], READONLY);
        NES_bus_map_CHR1K(bus, 0x1800, 0x1BFF, &bus->CHR_ROM, this->regs.bank[4], READONLY);
        NES_bus_map_CHR1K(bus, 0x1C00, 0x1FFF, &bus->CHR_ROM, this->regs.bank[5], READONLY);
    }
    else {
        NES_bus_map_CHR1K(bus, 0x0000, 0x03FF, &bus->CHR_ROM, this->regs.bank[2], READONLY);
        NES_bus_map_CHR1K(bus, 0x0400, 0x07FF, &bus->CHR_ROM, this->regs.bank[3], READONLY);
        NES_bus_map_CHR1K(bus, 0x0800, 0x0BFF, &bus->CHR_ROM, this->regs.bank[4], READONLY);
        NES_bus_map_CHR1K(bus, 0x0C00, 0x0FFF, &bus->CHR_ROM, this->regs.bank[5], READONLY);
        NES_bus_map_CHR1K(bus, 0x1000, 0x17FF, &bus->CHR_ROM, this->regs.bank[0] & 0xFE, READONLY);
        NES_bus_map_CHR1K(bus, 0x1800, 0x1FFF, &bus->CHR_ROM, this->regs.bank[1] & 0xFE, READONLY);
    }
    debugger_interface_dirty_mem(bus->nes->dbg.interface, NESMEM_CPUBUS, 0x8000, 0xFFFF);
}

static void remap_PPU(struct NES_bus *bus)
{
    NES_bus_PPU_mirror_set(bus);
}

static void serialize(struct NES_bus *bus, struct serialized_state *state)
{
     THISM;
#define S(x) Sadd(state, &this-> x, sizeof(this-> x))
     S(a12_watcher.last_cycle);
     S(a12_watcher.delay);
     S(a12_watcher.cycles_down);
     S(has_IRQ);
     S(has_mirroring_control);
     S(fourway);
     S(regs);
     S(status);
     S(irq);
#undef S
}

static void deserialize(struct NES_bus *bus, struct serialized_state *state)
{
    THISM;
#define L(x) Sload(state, &this-> x, sizeof(this-> x))
    L(a12_watcher.last_cycle);
    L(a12_watcher.delay);
    L(a12_watcher.cycles_down);
    L(has_IRQ);
    L(has_mirroring_control);
    L(fourway);
    L(regs);
    L(status);
    L(irq);
#undef L
    remap(bus, 0);
    remap_PPU(bus);
}

static void MMC3b_destruct(struct NES_bus *bus)
{

}

static void MMC3b_reset(struct NES_bus *bus)
{
    remap(bus, 1);
    remap_PPU(bus);
}

static void MMC3b_writecart(struct NES_bus *bus, u32 addr, u32 val, u32 *do_write)
{
    THISM;
    *do_write = 1;

    switch(addr & 0xE001) {
        case 0x8000: // Bank select
            this->regs.bank_select = (val & 7);
            this->status.PRG_mode = (val & 0x40) >> 6;
            this->status.CHR_mode = (val & 0x80) >> 7;
            break;
        case 0x8001: // Bank data
            this->regs.bank[this->regs.bank_select] = val;
            remap(bus, 0);
            break;
        case 0xA000:
            if (this->has_mirroring_control && !this->fourway) {
                bus->ppu_mirror_mode = val & 1 ? PPUM_Horizontal : PPUM_Vertical;
                remap_PPU(bus);
            }
            break;
        case 0xA001:
            break;
        case 0xC000:
            this->regs.rC000 = val;
            break;
        case 0xC001:
            if (this->has_IRQ) {
                this->irq.counter = 0;
                this->irq.reload = 1;
            }
            break;
        case 0xE000:
            if (this->has_IRQ) {
                this->irq.enable = 0;
                r2A03_notify_IRQ(&bus->nes->cpu, 0, 1);
            }
            break;
        case 0xE001:
            if (this->has_IRQ) {
                this->irq.enable = 1;
            }
            break;
    }
}

static u32 MMC3b_readcart(struct NES_bus *bus, u32 addr, u32 old_val, u32 has_effect, u32 *do_read)
{
    *do_read = 1;
    return old_val;
}

static void MMC3b_setcart(struct NES_bus *bus, struct NES_cart *cart)
{
    THISM;
    bus->ppu_mirror_mode = cart->header.mirroring ^ 1;
    if (cart->header.four_screen_mode) {
        printf("\nFOUR SCREEN MODE!");
        bus->ppu_mirror_mode = PPUM_FourWay;
        this->fourway = 1;
    }
    NES_bus_PPU_mirror_set(bus);
}

static void MMC3b_a12_watch(struct NES_bus *bus, u32 addr)
{
    THISM;
    if (!this->has_IRQ) return;
    if (a12_watcher_update(&this->a12_watcher, addr) == A12_RISE) {
        if ((this->irq.counter == 0) || (this->irq.reload)) {
            this->irq.counter = (i32)this->regs.rC000;
        } else {
            this->irq.counter--;
            if (this->irq.counter < 0) this->irq.counter = 0;
        }

        if ((this->irq.counter == 0) && this->irq.enable) {
            r2A03_notify_IRQ(&bus->nes->cpu, 1, 1);
        }
        this->irq.reload = 0;
    }

}

void MMC3b_init(struct NES_bus *bus, struct NES *nes, enum NES_mappers kind)
{
    if (bus->ptr != NULL) free(bus->ptr);
    bus->ptr = malloc(sizeof(struct MMC3b));
    struct MMC3b *this = (struct MMC3b*)bus->ptr;
    memset(this, 0, sizeof(struct MMC3b));

    this->nes = nes;
    this->kind = kind;
    this->fourway = 0;

    switch(kind) {
        case NESM_MMC3b:
            this->has_IRQ = 1;
            this->has_mirroring_control = 1;
            break;
        case NESM_DXROM:
            this->has_IRQ = 0;
            this->has_mirroring_control = 0;
            break;
        default:
            assert(1==2);
            break;
    }

    bus->destruct = &MMC3b_destruct;
    bus->reset = &MMC3b_reset;
    bus->writecart = &MMC3b_writecart;
    bus->readcart = &MMC3b_readcart;
    bus->setcart = &MMC3b_setcart;
    bus->cpu_cycle = NULL;
    bus->a12_watch = &MMC3b_a12_watch;
    bus->serialize = &serialize;
    bus->deserialize = &deserialize;

    a12_watcher_init(&this->a12_watcher, &nes->clock);

    // Map "something" to ROM here
    NES_bus_map_PRG8K(bus, 0x8000, 0xFFFF, &bus->PRG_ROM, 0, READONLY);
}