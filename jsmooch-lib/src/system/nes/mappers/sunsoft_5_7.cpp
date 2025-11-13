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

#define READONLY 1
#define READWRITE 0

void sunsoft_5_7::remap(bool boot)
{
    if (boot) {
        io.cpu.banks[0] = 0;
        io.cpu.banks[1] = 1;
        io.cpu.banks[2] = 2;
        io.cpu.banks[3] = 3;
        io.wram_enabled = 0;
        io.wram_mapped = 0;

        bus->map_PRG8K( 0xE000, 0xFFFF, &bus->PRG_ROM, bus->num_PRG_ROM_banks8K - 1, READONLY);
    }

    if (io.wram_mapped && io.wram_enabled)
        bus->map_PRG8K( 0x6000, 0x7FFF, &bus->fake_PRG_RAM, 0, READWRITE);
    else
        bus->map_PRG8K( 0x6000, 0x7FFF, &bus->PRG_ROM, io.cpu.banks[0], READONLY);

    bus->map_PRG8K( 0x8000, 0x9FFF, &bus->PRG_ROM, io.cpu.banks[1], READONLY);
    bus->map_PRG8K( 0xA000, 0xBFFF, &bus->PRG_ROM, io.cpu.banks[2], READONLY);
    bus->map_PRG8K( 0xC000, 0xDFFF, &bus->PRG_ROM, io.cpu.banks[3], READONLY);

    bus->map_CHR1K(0x0000, 0x03FF, &bus->CHR_ROM, io.ppu.banks[0], READONLY);
    bus->map_CHR1K(0x0400, 0x07FF, &bus->CHR_ROM, io.ppu.banks[1], READONLY);
    bus->map_CHR1K(0x0800, 0x0BFF, &bus->CHR_ROM, io.ppu.banks[2], READONLY);
    bus->map_CHR1K(0x0C00, 0x0FFF, &bus->CHR_ROM, io.ppu.banks[3], READONLY);
    bus->map_CHR1K(0x1000, 0x13FF, &bus->CHR_ROM, io.ppu.banks[4], READONLY);
    bus->map_CHR1K(0x1400, 0x17FF, &bus->CHR_ROM, io.ppu.banks[5], READONLY);
    bus->map_CHR1K(0x1800, 0x1BFF, &bus->CHR_ROM, io.ppu.banks[6], READONLY);
    bus->map_CHR1K(0x1C00, 0x1FFF, &bus->CHR_ROM, io.ppu.banks[7], READONLY);
}

void sunsoft_5_7::serialize(serialized_state &state)
{
#define S(x) Sadd(state, & x, sizeof( x))
    S(irq);
    S(io);
#undef S
}

void sunsoft_5_7::deserialize(serialized_state &state)
{
#define L(x) Sload(state, & x, sizeof( x))
    L(irq);
    L(io);
#undef L
    remap(false);
}

void sunsoft_5_7::reset()
{
    printf("\nsunsoft_5_7 Resetting, so remapping bus...");
    remap(true);
}

void sunsoft_5_7::write_reg(u32 val)
{
    if (io.reg < 8) {
        io.ppu.banks[io.reg] = val;
        u32 rng_start = 0x400 * io.reg;
        bus->map_CHR1K(rng_start, rng_start + 0x3FF, &bus->CHR_ROM, val, READONLY);
        return;
    }

    switch(io.reg) {
        case 8:
            io.cpu.banks[0] = val;
            io.wram_mapped = (val >> 6) & 1;
            io.wram_enabled = (val >> 7) & 1;
            if (io.wram_mapped && io.wram_enabled)
                bus->map_PRG8K( 0x6000, 0x7FFF, &bus->fake_PRG_RAM, 0, READWRITE);
            else
                bus->map_PRG8K( 0x6000, 0x7FFF, &bus->PRG_ROM, io.cpu.banks[0], READONLY);
            break;
        case 9:
        case 10:
        case 11: {
            u32 r = io.reg - 8;
            io.cpu.banks[r] = val;
            u32 rng_start = 0x6000 + (0x2000 * r);
            bus->map_PRG8K( rng_start, rng_start + 0x1FFF, &bus->PRG_ROM, io.cpu.banks[r], READONLY);
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
            bus->PPU_mirror_set();
            break;
        case 13: // IRQ control
            irq.enabled = val & 1;
            irq.counter_enabled = (val >> 7) & 1;
            irq.output = 0;
            bus->nes->cpu.notify_IRQ(0, 1);
            break;
        case 14: // IRQ counter low
            irq.counter = (irq.counter & 0xFF00) | val;
            break;
        case 15:
            irq.counter = (irq.counter & 0xFF) | (val << 8);
            break;
        default: break;
    }
}

void sunsoft_5_7::write_audio_reg(u32 val)
{
    printf("\nwrite A%d: %02x", io.audio.reg, val);
}

void sunsoft_5_7::writecart(u32 addr, u32 val, u32 &do_write)
{
    do_write = 1;
    if (addr < 0x8000) return;
    switch(addr & 0xE000) {
        case 0x8000:
            io.reg = val & 15;
            break;
        case 0xA000:
            write_reg(val);
            break;
        case 0xC000:
            io.audio.reg = val & 15;
            io.audio.write_enabled = (val >> 4) == 0;
            break;
        case 0xE000:
            write_audio_reg(val);
            break;
        default: break;
    }
}

u32 sunsoft_5_7::readcart(u32 addr, u32 old_val, u32 has_effect, u32 &do_read)
{
    do_read = 1;
    return old_val;
}

void sunsoft_5_7::setcart(NES_cart &cart)
{
    bus->ppu_mirror_mode = cart.header.mirroring;
    bus->PPU_mirror_set();
}

void sunsoft_5_7::cpu_cycle()
{
    if (irq.enabled && irq.counter_enabled) {
        irq.counter = (irq.counter - 1) & 0xFFFF;
        if (irq.counter == 0xFFFF) {
            irq.output = 1;
            bus->nes->cpu.notify_IRQ(1, 1);
        }
    }
}

sunsoft_5_7::sunsoft_5_7(NES_bus *bus, NES_mappers in_kind) : NES_mapper(bus), kind(in_kind)
{
    this->overrides_PPU = false;
    bus->NES_audio_bias = 0.5f;
    bus->mapper_audio_bias = 0.5f;
    switch(kind) {
        case NESM_SUNSOFT_5b:
            has_sound = 1;
            break;
        case NESM_SUNSOFT_7:
            has_sound = 0;
            break;
        default:
            assert(1==2);
    }
}