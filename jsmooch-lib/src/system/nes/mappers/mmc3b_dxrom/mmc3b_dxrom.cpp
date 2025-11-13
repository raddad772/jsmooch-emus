//
// Created by . on 9/27/24.
//
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cassert>

#include "../../nes.h"
#include "a12_watcher.h"
#include "mmc3b_dxrom.h"
#include "helpers/serialize/serialize.h"

#include "helpers/debugger/debugger.h"

#define READONLY 1
#define READWRITE 0

void MMC3b_DXROM::remap(bool is_boot)
{
    if (is_boot) {
        // PRG RAM
        bus->map_PRG8K( 0x6000, 0x7FFF, &bus->fake_PRG_RAM, 0, READWRITE);

        bus->map_PRG8K( 0xE000, 0xFFFF, &bus->PRG_ROM, bus->num_PRG_ROM_banks8K - 1, READONLY);

        // Map CHR ROM to basics
        bus->map_CHR1K(0x0000, 0x1FFF, &bus->CHR_ROM, 0, READONLY);
    }

    if (status.PRG_mode == 0) {
        // 0 = 8000-9FFF swappable,
        //     C000-DFFF fixed to second-last bank
        // 1 = 8000-9FFF fixed to second-last bank
        //     C000-DFFF swappable
        bus->map_PRG8K( 0x8000, 0x9FFF, &bus->PRG_ROM, regs.bank[6], READONLY);
        bus->map_PRG8K( 0xC000, 0xDFFF, &bus->PRG_ROM, bus->num_PRG_ROM_banks8K - 2, READONLY);
    }
    else {
        bus->map_PRG8K( 0x8000, 0x9FFF, &bus->PRG_ROM, bus->num_PRG_ROM_banks8K - 2, READONLY);
        bus->map_PRG8K( 0xC000, 0xDFFF, &bus->PRG_ROM, regs.bank[6], READONLY);
    }

    bus->map_PRG8K( 0xA000, 0xBFFF, &bus->PRG_ROM, regs.bank[7], READONLY);

    if (status.CHR_mode == 0) {
        bus->map_CHR1K(0x0000, 0x07FF, &bus->CHR_ROM, regs.bank[0] & 0xFE, READONLY);
        bus->map_CHR1K(0x0800, 0x0FFF, &bus->CHR_ROM, regs.bank[1] & 0xFE, READONLY);
        bus->map_CHR1K(0x1000, 0x13FF, &bus->CHR_ROM, regs.bank[2], READONLY);
        bus->map_CHR1K(0x1400, 0x17FF, &bus->CHR_ROM, regs.bank[3], READONLY);
        bus->map_CHR1K(0x1800, 0x1BFF, &bus->CHR_ROM, regs.bank[4], READONLY);
        bus->map_CHR1K(0x1C00, 0x1FFF, &bus->CHR_ROM, regs.bank[5], READONLY);
    }
    else {
        bus->map_CHR1K(0x0000, 0x03FF, &bus->CHR_ROM, regs.bank[2], READONLY);
        bus->map_CHR1K(0x0400, 0x07FF, &bus->CHR_ROM, regs.bank[3], READONLY);
        bus->map_CHR1K(0x0800, 0x0BFF, &bus->CHR_ROM, regs.bank[4], READONLY);
        bus->map_CHR1K(0x0C00, 0x0FFF, &bus->CHR_ROM, regs.bank[5], READONLY);
        bus->map_CHR1K(0x1000, 0x17FF, &bus->CHR_ROM, regs.bank[0] & 0xFE, READONLY);
        bus->map_CHR1K(0x1800, 0x1FFF, &bus->CHR_ROM, regs.bank[1] & 0xFE, READONLY);
    }
    debugger_interface_dirty_mem(bus->nes->dbg.interface, NESMEM_CPUBUS, 0x8000, 0xFFFF);
}

void MMC3b_DXROM::remap_PPU()
{
    bus->PPU_mirror_set();
}

void MMC3b_DXROM::serialize(serialized_state &state)
{

#define S(x) Sadd(state, & x, sizeof( x))
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

void MMC3b_DXROM::deserialize(serialized_state &state)
{

#define L(x) Sload(state, & x, sizeof( x))
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
    remap(false);
    remap_PPU();
}

void MMC3b_DXROM::reset()
{
    remap(true);
    remap_PPU();
}

void MMC3b_DXROM::writecart(u32 addr, u32 val, u32 &do_write)
{
    do_write = 1;

    switch(addr & 0xE001) {
        case 0x8000: // Bank select
            regs.bank_select = (val & 7);
            status.PRG_mode = (val & 0x40) >> 6;
            status.CHR_mode = (val & 0x80) >> 7;
            break;
        case 0x8001: // Bank data
            regs.bank[regs.bank_select] = val;
            remap(false);
            break;
        case 0xA000:
            if (has_mirroring_control && !fourway) {
                bus->ppu_mirror_mode = val & 1 ? PPUM_Horizontal : PPUM_Vertical;
                remap_PPU();
            }
            break;
        case 0xA001:
            break;
        case 0xC000:
            regs.rC000 = val;
            break;
        case 0xC001:
            if (has_IRQ) {
                irq.counter = 0;
                irq.reload = 1;
            }
            break;
        case 0xE000:
            if (has_IRQ) {
                irq.enable = 0;
                bus->nes->cpu.notify_IRQ(0, 1);
            }
            break;
        case 0xE001:
            if (has_IRQ) {
                irq.enable = 1;
            }
            break;
        default:
    }
}

u32 MMC3b_DXROM::readcart(u32 addr, u32 old_val, u32 has_effect, u32 &do_read)
{
    do_read = 1;
    return old_val;
}

void MMC3b_DXROM::setcart(NES_cart &cart)
{

    bus->ppu_mirror_mode = cart.header.mirroring ^ 1;
    if (cart.header.four_screen_mode) {
        printf("\nFOUR SCREEN MODE!");
        bus->ppu_mirror_mode = PPUM_FourWay;
        fourway = 1;
    }
    bus->PPU_mirror_set();
}

void MMC3b_DXROM::a12_watch(u32 addr)
{

    if (!has_IRQ) return;
    if (a12_watcher.update(addr) == A12_RISE) {
        if ((irq.counter == 0) || (irq.reload)) {
            irq.counter = static_cast<i32>(regs.rC000);
        } else {
            irq.counter--;
            if (irq.counter < 0) irq.counter = 0;
        }

        if ((irq.counter == 0) && irq.enable) {
            bus->nes->cpu.notify_IRQ(1, 1);
        }
        irq.reload = 0;
    }
}

MMC3b_DXROM::MMC3b_DXROM(NES_bus *bus, NES_mappers in_kind) : NES_mapper(bus), kind(in_kind), a12_watcher(&bus->nes->clock)
{
    this->overrides_PPU = false;
    switch(in_kind) {
        case NESM_DXROM:
        case NESM_MMC3b:
            has_IRQ = 1;
            has_mirroring_control = 1;
            break;
        /*case NESM_DXROM:
            has_IRQ = 1;
            has_mirroring_control = 0;
            break;*/
        default:
            printf("\nERROR!?");
            assert(1==2);
            break;
    }

    // Map "something" to ROM here
    bus->map_PRG8K( 0x8000, 0xFFFF, &bus->PRG_ROM, 0, READONLY);
}