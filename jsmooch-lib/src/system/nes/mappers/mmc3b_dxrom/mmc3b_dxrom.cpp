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
#include "helpers/serialize/serialize.h"

#include "helpers/debugger/debugger.h"

#define THISM MMC3b *mp = static_cast<MMC3b *>(bus->ptr)

struct MMC3b {
    NES *nes;

    explicit MMC3b(NES *innes) : nes(innes), a12_watcher(&nes->clock) {};

    NES_a12_watcher a12_watcher;

    NES_mappers kind = NESM_MMC3b;

    u32 has_IRQ{};
    u32 has_mirroring_control{};
    u32 fourway{};
    struct {
        u32 rC000{};
        u32 bank_select{};
        u32 bank[8]{};
    } regs{};

    struct {
        u32 ROM_bank_mode{};
        u32 CHR_mode{};
        u32 PRG_mode{};
    } status{};

    struct {
        u32 enable{}, reload{};
        i32 counter{};
    } irq{};
};

#define READONLY 1
#define READWRITE 0

static void remap(NES_mapper *bus, u32 is_boot)
{
    THISM;

    if (is_boot) {
        // PRG RAM
        NES_bus_map_PRG8K(bus, 0x6000, 0x7FFF, &bus->fake_PRG_RAM, 0, READWRITE);

        NES_bus_map_PRG8K(bus, 0xE000, 0xFFFF, &bus->PRG_ROM, bus->num_PRG_ROM_banks8K - 1, READONLY);

        // Map CHR ROM to basics
        NES_bus_map_CHR1K(bus, 0x0000, 0x1FFF, &bus->CHR_ROM, 0, READONLY);
    }

    if (mp->status.PRG_mode == 0) {
        // 0 = 8000-9FFF swappable,
        //     C000-DFFF fixed to second-last bank
        // 1 = 8000-9FFF fixed to second-last bank
        //     C000-DFFF swappable
        NES_bus_map_PRG8K(bus, 0x8000, 0x9FFF, &bus->PRG_ROM, mp->regs.bank[6], READONLY);
        NES_bus_map_PRG8K(bus, 0xC000, 0xDFFF, &bus->PRG_ROM, bus->num_PRG_ROM_banks8K - 2, READONLY);
    }
    else {
        NES_bus_map_PRG8K(bus, 0x8000, 0x9FFF, &bus->PRG_ROM, bus->num_PRG_ROM_banks8K - 2, READONLY);
        NES_bus_map_PRG8K(bus, 0xC000, 0xDFFF, &bus->PRG_ROM, mp->regs.bank[6], READONLY);
    }

    NES_bus_map_PRG8K(bus, 0xA000, 0xBFFF, &bus->PRG_ROM, mp->regs.bank[7], READONLY);

    if (mp->status.CHR_mode == 0) {
        NES_bus_map_CHR1K(bus, 0x0000, 0x07FF, &bus->CHR_ROM, mp->regs.bank[0] & 0xFE, READONLY);
        NES_bus_map_CHR1K(bus, 0x0800, 0x0FFF, &bus->CHR_ROM, mp->regs.bank[1] & 0xFE, READONLY);
        NES_bus_map_CHR1K(bus, 0x1000, 0x13FF, &bus->CHR_ROM, mp->regs.bank[2], READONLY);
        NES_bus_map_CHR1K(bus, 0x1400, 0x17FF, &bus->CHR_ROM, mp->regs.bank[3], READONLY);
        NES_bus_map_CHR1K(bus, 0x1800, 0x1BFF, &bus->CHR_ROM, mp->regs.bank[4], READONLY);
        NES_bus_map_CHR1K(bus, 0x1C00, 0x1FFF, &bus->CHR_ROM, mp->regs.bank[5], READONLY);
    }
    else {
        NES_bus_map_CHR1K(bus, 0x0000, 0x03FF, &bus->CHR_ROM, mp->regs.bank[2], READONLY);
        NES_bus_map_CHR1K(bus, 0x0400, 0x07FF, &bus->CHR_ROM, mp->regs.bank[3], READONLY);
        NES_bus_map_CHR1K(bus, 0x0800, 0x0BFF, &bus->CHR_ROM, mp->regs.bank[4], READONLY);
        NES_bus_map_CHR1K(bus, 0x0C00, 0x0FFF, &bus->CHR_ROM, mp->regs.bank[5], READONLY);
        NES_bus_map_CHR1K(bus, 0x1000, 0x17FF, &bus->CHR_ROM, mp->regs.bank[0] & 0xFE, READONLY);
        NES_bus_map_CHR1K(bus, 0x1800, 0x1FFF, &bus->CHR_ROM, mp->regs.bank[1] & 0xFE, READONLY);
    }
    debugger_interface_dirty_mem(bus->nes->dbg.interface, NESMEM_CPUBUS, 0x8000, 0xFFFF);
}

static void remap_PPU(NES_mapper *bus)
{
    NES_bus_PPU_mirror_set(bus);
}

static void serialize(NES_mapper *bus, serialized_state &state)
{
     THISM;
#define S(x) Sadd(state, &mp-> x, sizeof(mp-> x))
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

static void deserialize(NES_mapper *bus, serialized_state &state)
{
    THISM;
#define L(x) Sload(state, &mp-> x, sizeof(mp-> x))
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

static void MMC3b_destruct(NES_mapper *bus)
{

}

static void MMC3b_reset(NES_mapper *bus)
{
    remap(bus, 1);
    remap_PPU(bus);
}

static void MMC3b_writecart(NES_mapper *bus, u32 addr, u32 val, u32 *do_write)
{
    THISM;
    *do_write = 1;

    switch(addr & 0xE001) {
        case 0x8000: // Bank select
            mp->regs.bank_select = (val & 7);
            mp->status.PRG_mode = (val & 0x40) >> 6;
            mp->status.CHR_mode = (val & 0x80) >> 7;
            break;
        case 0x8001: // Bank data
            mp->regs.bank[mp->regs.bank_select] = val;
            remap(bus, 0);
            break;
        case 0xA000:
            if (mp->has_mirroring_control && !mp->fourway) {
                bus->ppu_mirror_mode = val & 1 ? PPUM_Horizontal : PPUM_Vertical;
                remap_PPU(bus);
            }
            break;
        case 0xA001:
            break;
        case 0xC000:
            mp->regs.rC000 = val;
            break;
        case 0xC001:
            if (mp->has_IRQ) {
                mp->irq.counter = 0;
                mp->irq.reload = 1;
            }
            break;
        case 0xE000:
            if (mp->has_IRQ) {
                mp->irq.enable = 0;
                bus->nes->cpu.notify_IRQ(0, 1);
            }
            break;
        case 0xE001:
            if (mp->has_IRQ) {
                mp->irq.enable = 1;
            }
            break;
    }
}

static u32 MMC3b_readcart(NES_mapper *bus, u32 addr, u32 old_val, u32 has_effect, u32 *do_read)
{
    *do_read = 1;
    return old_val;
}

static void MMC3b_setcart(NES_mapper *bus, NES_cart *cart)
{
    THISM;
    bus->ppu_mirror_mode = cart->header.mirroring ^ 1;
    if (cart->header.four_screen_mode) {
        printf("\nFOUR SCREEN MODE!");
        bus->ppu_mirror_mode = PPUM_FourWay;
        mp->fourway = 1;
    }
    NES_bus_PPU_mirror_set(bus);
}

static void MMC3b_a12_watch(NES_mapper *bus, u32 addr)
{
    THISM;
    if (!mp->has_IRQ) return;
    if (mp->a12_watcher.update(addr) == A12_RISE) {
        if ((mp->irq.counter == 0) || (mp->irq.reload)) {
            mp->irq.counter = static_cast<i32>(mp->regs.rC000);
        } else {
            mp->irq.counter--;
            if (mp->irq.counter < 0) mp->irq.counter = 0;
        }

        if ((mp->irq.counter == 0) && mp->irq.enable) {
            bus->nes->cpu.notify_IRQ(1, 1);
        }
        mp->irq.reload = 0;
    }

}

void MMC3b_init(NES_mapper *bus, NES *nes, enum NES_mappers kind)
{
    if (bus->ptr != nullptr) free(bus->ptr);
    bus->ptr = malloc(sizeof(MMC3b));
    MMC3b *mp = static_cast<MMC3b *>(bus->ptr);
    new(mp) MMC3b(nes);

    mp->nes = nes;

    switch(kind) {
        case NESM_MMC3b:
            mp->has_IRQ = 1;
            mp->has_mirroring_control = 1;
            break;
        case NESM_DXROM:
            mp->has_IRQ = 0;
            mp->has_mirroring_control = 0;
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
    bus->cpu_cycle = nullptr;
    bus->a12_watch = &MMC3b_a12_watch;
    bus->serialize = &serialize;
    bus->deserialize = &deserialize;

    // Map "something" to ROM here
    NES_bus_map_PRG8K(bus, 0x8000, 0xFFFF, &bus->PRG_ROM, 0, READONLY);
}