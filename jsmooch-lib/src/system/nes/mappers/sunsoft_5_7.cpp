//
// Created by . on 9/27/24.
//

#include <cstdlib>
#include <cassert>

// TODO: verify all mappers dirty memory appropriately
#include "helpers/debugger/debugger.h"
#include "../nes.h"

#include "mapper.h"
#include "sunsoft_5_7.h"

#define THISM sunsoft_5_7 *th = static_cast<sunsoft_5_7 *>(bus->ptr)

struct sunsoft_5_7 {
    NES *nes;

    u32 bank_mask;
    u32 has_sound;

    enum NES_mappers kind;
    struct {
        u32 counter;
        u32 output;
        u32 enabled;
        u32 counter_enabled;
    } irq;
    struct {
        struct {
            u32 banks[4];
        } cpu;
        struct {
            u32 banks[8];
        } ppu;
        u32 reg;
        u32 wram_enabled, wram_mapped;

        struct {
            u32 reg;
            u32 write_enabled;
        } audio;
    } io;

};

#define READONLY 1
#define READWRITE 0

static void remap(NES_mapper *bus, u32 boot)
{
    THISM;

    if (boot) {
        th->io.cpu.banks[0] = 0;
        th->io.cpu.banks[1] = 1;
        th->io.cpu.banks[2] = 2;
        th->io.cpu.banks[3] = 3;
        th->io.wram_enabled = 0;
        th->io.wram_mapped = 0;

        NES_bus_map_PRG8K(bus, 0xE000, 0xFFFF, &bus->PRG_ROM, bus->num_PRG_ROM_banks8K - 1, READONLY);
    }

    if (th->io.wram_mapped && th->io.wram_enabled)
        NES_bus_map_PRG8K(bus, 0x6000, 0x7FFF, &bus->fake_PRG_RAM, 0, READWRITE);
    else
        NES_bus_map_PRG8K(bus, 0x6000, 0x7FFF, &bus->PRG_ROM, th->io.cpu.banks[0], READONLY);

    NES_bus_map_PRG8K(bus, 0x8000, 0x9FFF, &bus->PRG_ROM, th->io.cpu.banks[1], READONLY);
    NES_bus_map_PRG8K(bus, 0xA000, 0xBFFF, &bus->PRG_ROM, th->io.cpu.banks[2], READONLY);
    NES_bus_map_PRG8K(bus, 0xC000, 0xDFFF, &bus->PRG_ROM, th->io.cpu.banks[3], READONLY);

    NES_bus_map_CHR1K(bus, 0x0000, 0x03FF, &bus->CHR_ROM, th->io.ppu.banks[0], READONLY);
    NES_bus_map_CHR1K(bus, 0x0400, 0x07FF, &bus->CHR_ROM, th->io.ppu.banks[1], READONLY);
    NES_bus_map_CHR1K(bus, 0x0800, 0x0BFF, &bus->CHR_ROM, th->io.ppu.banks[2], READONLY);
    NES_bus_map_CHR1K(bus, 0x0C00, 0x0FFF, &bus->CHR_ROM, th->io.ppu.banks[3], READONLY);
    NES_bus_map_CHR1K(bus, 0x1000, 0x13FF, &bus->CHR_ROM, th->io.ppu.banks[4], READONLY);
    NES_bus_map_CHR1K(bus, 0x1400, 0x17FF, &bus->CHR_ROM, th->io.ppu.banks[5], READONLY);
    NES_bus_map_CHR1K(bus, 0x1800, 0x1BFF, &bus->CHR_ROM, th->io.ppu.banks[6], READONLY);
    NES_bus_map_CHR1K(bus, 0x1C00, 0x1FFF, &bus->CHR_ROM, th->io.ppu.banks[7], READONLY);
}

static void serialize(NES_mapper *bus, serialized_state &state)
{
    THISM;
#define S(x) Sadd(state, &th-> x, sizeof(th-> x))
    S(irq);
    S(io);
#undef S
}

static void deserialize(NES_mapper *bus, serialized_state &state)
{
    THISM;
#define L(x) Sload(state, &th-> x, sizeof(th-> x))
    L(irq);
    L(io);
#undef L
    remap(bus, 0);
}

static void sunsoft_5_7_destruct(NES_mapper *bus)
{

}

static void sunsoft_5_7_reset(NES_mapper *bus)
{
    printf("\nsunsoft_5_7 Resetting, so remapping bus...");
    remap(bus, 1);
}

static void write_reg(NES_mapper* bus, u32 val)
{
    THISM;
    if (th->io.reg < 8) {
        th->io.ppu.banks[th->io.reg] = val;
        u32 rng_start = 0x400 * th->io.reg;
        NES_bus_map_CHR1K(bus, rng_start, rng_start + 0x3FF, &bus->CHR_ROM, val, READONLY);
        return;
    }

    switch(th->io.reg) {
        case 8:
            th->io.cpu.banks[0] = val;
            th->io.wram_mapped = (val >> 6) & 1;
            th->io.wram_enabled = (val >> 7) & 1;
            if (th->io.wram_mapped && th->io.wram_enabled)
                NES_bus_map_PRG8K(bus, 0x6000, 0x7FFF, &bus->fake_PRG_RAM, 0, READWRITE);
            else
                NES_bus_map_PRG8K(bus, 0x6000, 0x7FFF, &bus->PRG_ROM, th->io.cpu.banks[0], READONLY);
            break;
        case 9:
        case 10:
        case 11: {
            u32 r = th->io.reg - 8;
            th->io.cpu.banks[r] = val;
            u32 rng_start = 0x6000 + (0x2000 * r);
            NES_bus_map_PRG8K(bus, rng_start, rng_start + 0x1FFF, &bus->PRG_ROM, th->io.cpu.banks[r], READONLY);
            break; }
        case 12:
            switch(val & 3) {
                case 0:
                    bus->ppu_mirror_mode = PPUM_Vertical;
                    break;
                case 1:
                    bus->ppu_mirror_mode = PPUM_Horizontal;
                    break;
                case 2:
                    bus->ppu_mirror_mode = PPUM_ScreenAOnly;
                    break;
                case 3:
                    bus->ppu_mirror_mode = PPUM_ScreenBOnly;
                    break;
                default: break;
            }
            NES_bus_PPU_mirror_set(bus);
            break;
        case 13: // IRQ control
            th->irq.enabled = val & 1;
            th->irq.counter_enabled = (val >> 7) & 1;
            th->irq.output = 0;
            bus->nes->cpu.notify_IRQ(0, 1);
            break;
        case 14: // IRQ counter low
            th->irq.counter = (th->irq.counter & 0xFF00) | val;
            break;
        case 15:
            th->irq.counter = (th->irq.counter & 0xFF) | (val << 8);
            break;
        default: break;
    }
}

static void write_audio_reg(NES_mapper* bus, u32 val)
{
    THISM;
    printf("\nwrite A%d: %02x", th->io.audio.reg, val);
}


static void sunsoft_5_7_writecart(NES_mapper *bus, u32 addr, u32 val, u32 *do_write)
{
    *do_write = 1;
    THISM;
    if (addr < 0x8000) return;
    switch(addr & 0xE000) {
        case 0x8000:
            th->io.reg = val & 15;
            break;
        case 0xA000:
            write_reg(bus, val);
            break;
        case 0xC000:
            th->io.audio.reg = val & 15;
            th->io.audio.write_enabled = (val >> 4) == 0;
            break;
        case 0xE000:
            write_audio_reg(bus, val);
            break;
        default: break;
    }
}

static u32 sunsoft_5_7_readcart(NES_mapper *bus, u32 addr, u32 old_val, u32 has_effect, u32 *do_read)
{
    *do_read = 1;
    return old_val;
}

static void sunsoft_5_7_setcart(NES_mapper *bus, NES_cart *cart)
{
    bus->ppu_mirror_mode = cart->header.mirroring;
    NES_bus_PPU_mirror_set(bus);
}

static void sunsoft_5_7_cpucycle(NES_mapper *bus)
{
    THISM;
    if (th->irq.enabled && th->irq.counter_enabled) {
        th->irq.counter = (th->irq.counter - 1) & 0xFFFF;
        if (th->irq.counter == 0xFFFF) {
            th->irq.output = 1;
            bus->nes->cpu.notify_IRQ(1, 1);
        }
    }
}

void sunsoft_5_7_init(NES_mapper *bus, NES *nes, enum NES_mappers kind)
{
    if (bus->ptr != nullptr) free(bus->ptr);
    bus->ptr = malloc(sizeof(sunsoft_5_7));
    THISM;

    th->nes = nes;
    th->kind = kind;

    bus->NES_audio_bias = 0.5f;
    bus->mapper_audio_bias = 0.5f;

    switch(kind) {
        case NESM_SUNSOFT_5b:
            th->has_sound = 1;
            break;
        case NESM_SUNSOFT_7:
            th->has_sound = 0;
            break;
        default:
            assert(1==2);
    }

    th->io.wram_enabled = th->io.wram_mapped = 0;
    th->irq.enabled = 0;


    bus->destruct = &sunsoft_5_7_destruct;
    bus->reset = &sunsoft_5_7_reset;
    bus->writecart = &sunsoft_5_7_writecart;
    bus->readcart = &sunsoft_5_7_readcart;
    bus->setcart = &sunsoft_5_7_setcart;
    bus->cpu_cycle = &sunsoft_5_7_cpucycle;
    bus->a12_watch = nullptr;
    bus->serialize = &serialize;
    bus->deserialize = &deserialize;
}