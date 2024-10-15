//
// Created by . on 9/27/24.
//

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// TODO: verify all mappers dirty memory appropriately
#include "helpers/debugger/debugger.h"
#include "../nes.h"

#include "mapper.h"
#include "sunsoft_5_7.h"

#define THISM struct sunsoft_5_7 *this = (struct sunsoft_5_7 *)bus->ptr

struct sunsoft_5_7 {
    struct NES *nes;

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

static void remap(struct NES_bus *bus, u32 boot)
{
    THISM;

    if (boot) {
        this->io.cpu.banks[0] = 0;
        this->io.cpu.banks[1] = 1;
        this->io.cpu.banks[2] = 2;
        this->io.cpu.banks[3] = 3;
        this->io.wram_enabled = 0;
        this->io.wram_mapped = 0;

        NES_bus_map_PRG8K(bus, 0xE000, 0xFFFF, &bus->PRG_ROM, bus->num_PRG_ROM_banks8K - 1, READONLY);
    }

    if (this->io.wram_mapped && this->io.wram_enabled)
        NES_bus_map_PRG8K(bus, 0x6000, 0x7FFF, &bus->PRG_RAM, 0, READWRITE);
    else
        NES_bus_map_PRG8K(bus, 0x6000, 0x7FFF, &bus->PRG_ROM, this->io.cpu.banks[0], READONLY);

    NES_bus_map_PRG8K(bus, 0x8000, 0x9FFF, &bus->PRG_ROM, this->io.cpu.banks[1], READONLY);
    NES_bus_map_PRG8K(bus, 0xA000, 0xBFFF, &bus->PRG_ROM, this->io.cpu.banks[2], READONLY);
    NES_bus_map_PRG8K(bus, 0xC000, 0xDFFF, &bus->PRG_ROM, this->io.cpu.banks[3], READONLY);

    NES_bus_map_CHR1K(bus, 0x0000, 0x03FF, &bus->CHR_ROM, this->io.ppu.banks[0], READONLY);
    NES_bus_map_CHR1K(bus, 0x0400, 0x07FF, &bus->CHR_ROM, this->io.ppu.banks[1], READONLY);
    NES_bus_map_CHR1K(bus, 0x0800, 0x0BFF, &bus->CHR_ROM, this->io.ppu.banks[2], READONLY);
    NES_bus_map_CHR1K(bus, 0x0C00, 0x0FFF, &bus->CHR_ROM, this->io.ppu.banks[3], READONLY);
    NES_bus_map_CHR1K(bus, 0x1000, 0x13FF, &bus->CHR_ROM, this->io.ppu.banks[4], READONLY);
    NES_bus_map_CHR1K(bus, 0x1400, 0x17FF, &bus->CHR_ROM, this->io.ppu.banks[5], READONLY);
    NES_bus_map_CHR1K(bus, 0x1800, 0x1BFF, &bus->CHR_ROM, this->io.ppu.banks[6], READONLY);
    NES_bus_map_CHR1K(bus, 0x1C00, 0x1FFF, &bus->CHR_ROM, this->io.ppu.banks[7], READONLY);
}


static void sunsoft_5_7_destruct(struct NES_bus *bus)
{

}

static void sunsoft_5_7_reset(struct NES_bus *bus)
{
    printf("\nsunsoft_5_7 Resetting, so remapping bus...");
    remap(bus, 1);
}

static void write_reg(struct NES_bus* bus, u32 val)
{
    THISM;
    if (this->io.reg < 8) {
        this->io.ppu.banks[this->io.reg] = val;
        u32 rng_start = 0x400 * this->io.reg;
        NES_bus_map_CHR1K(bus, rng_start, rng_start + 0x3FF, &bus->CHR_ROM, val, READONLY);
        return;
    }

    switch(this->io.reg) {
        case 8:
            this->io.cpu.banks[0] = val;
            this->io.wram_mapped = (val >> 6) & 1;
            this->io.wram_enabled = (val >> 7) & 1;
            if (this->io.wram_mapped && this->io.wram_enabled)
                NES_bus_map_PRG8K(bus, 0x6000, 0x7FFF, &bus->PRG_RAM, 0, READWRITE);
            else
                NES_bus_map_PRG8K(bus, 0x6000, 0x7FFF, &bus->PRG_ROM, this->io.cpu.banks[0], READONLY);
            break;
        case 9:
        case 10:
        case 11: {
            u32 r = this->io.reg - 8;
            this->io.cpu.banks[r] = val;
            u32 rng_start = 0x6000 + (0x2000 * r);
            NES_bus_map_PRG8K(bus, rng_start, rng_start + 0x1FFF, &bus->PRG_ROM, this->io.cpu.banks[r], READONLY);
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
            }
            NES_bus_PPU_mirror_set(bus);
            break;
        case 13: // IRQ control
            this->irq.enabled = val & 1;
            this->irq.counter_enabled = (val >> 7) & 1;
            this->irq.output = 0;
            r2A03_notify_IRQ(&bus->nes->cpu, 0, 1);
            break;
        case 14: // IRQ counter low
            this->irq.counter = (this->irq.counter & 0xFF00) | val;
            break;
        case 15:
            this->irq.counter = (this->irq.counter & 0xFF) | (val << 8);
            break;
    }
}

static void write_audio_reg(struct NES_bus* bus, u32 val)
{
    THISM;
    printf("\nwrite A%d: %02x", this->io.audio.reg, val);
}


static void sunsoft_5_7_writecart(struct NES_bus *bus, u32 addr, u32 val, u32 *do_write)
{
    *do_write = 1;
    THISM;
    if (addr < 0x8000) return;
    switch(addr & 0xE000) {
        case 0x8000:
            this->io.reg = val & 15;
            break;
        case 0xA000:
            write_reg(bus, val);
            break;
        case 0xC000:
            this->io.audio.reg = val & 15;
            this->io.audio.write_enabled = (val >> 4) == 0;
            break;
        case 0xE000:
            write_audio_reg(bus, val);
            break;
    }
}

static u32 sunsoft_5_7_readcart(struct NES_bus *bus, u32 addr, u32 old_val, u32 has_effect, u32 *do_read)
{
    *do_read = 1;
    return old_val;
}

static void sunsoft_5_7_setcart(struct NES_bus *bus, struct NES_cart *cart)
{
    bus->ppu_mirror_mode = cart->header.mirroring;
    NES_bus_PPU_mirror_set(bus);
}

static void sunsoft_5_7_cpucycle(struct NES_bus *bus)
{
    THISM;
    if (this->irq.enabled && this->irq.counter_enabled) {
        this->irq.counter = (this->irq.counter - 1) & 0xFFFF;
        if (this->irq.counter == 0xFFFF) {
            this->irq.output = 1;
            r2A03_notify_IRQ(&bus->nes->cpu, 1, 1);
        }
    }
}

void sunsoft_5_7_init(struct NES_bus *bus, struct NES *nes, enum NES_mappers kind)
{
    if (bus->ptr != NULL) free(bus->ptr);
    bus->ptr = malloc(sizeof(struct sunsoft_5_7));
    struct sunsoft_5_7 *this = (struct sunsoft_5_7*)bus->ptr;

    this->nes = nes;
    this->kind = kind;

    bus->NES_audio_bias = 0.5f;
    bus->mapper_audio_bias = 0.5f;

    switch(kind) {
        case NESM_SUNSOFT_5b:
            this->has_sound = 1;
            break;
        case NESM_SUNSOFT_7:
            this->has_sound = 0;
            break;
        default:
            assert(1==2);
    }

    this->io.wram_enabled = this->io.wram_mapped = 0;
    this->irq.enabled = 0;


    bus->destruct = &sunsoft_5_7_destruct;
    bus->reset = &sunsoft_5_7_reset;
    bus->writecart = &sunsoft_5_7_writecart;
    bus->readcart = &sunsoft_5_7_readcart;
    bus->setcart = &sunsoft_5_7_setcart;
    bus->cpu_cycle = &sunsoft_5_7_cpucycle;
    bus->a12_watch = NULL;
}