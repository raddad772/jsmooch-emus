//
// Created by . on 9/27/24.
//

#include <cstdlib>
#include <cstdio>
#include <cassert>

#include "helpers/debugger/debugger.h"
#include "../nes.h"

#include "mapper.h"
#include "vrc_2b_2e_4f.h"

#define THISM VRC24 *th = static_cast<VRC24 *>(bus->ptr)

struct VRC24 {
    NES *nes;
    NES_mappers kind;

};

#define READONLY 1
#define READWRITE 0

void VRC2B_4E_4F::remap(bool boot)
{

    if (boot) {
        // VRC2 maps last c0 and e0 to last 2 banks
        io.vrc4.banks_swapped = 0;
        //bus->map_PRG8K( 0x6000, 0x7FFF, &bus->PRG_RAM, 0, READWRITE);

        bus->map_PRG8K( 0xE000, 0xFFFF, &bus->PRG_ROM, bus->num_PRG_ROM_banks8K - 1, READONLY);
    }

    bus->map_PRG8K( 0x6000, 0x7FFF, io.wram_enabled ? &bus->fake_PRG_RAM : nullptr, 0, READWRITE);

    if (is_vrc4 && io.vrc4.banks_swapped) {
        bus->map_PRG8K( 0x8000, 0x9FFF, &bus->PRG_ROM, bus->num_PRG_ROM_banks8K - 2, READONLY);
        bus->map_PRG8K( 0xA000, 0xBFFF, &bus->PRG_ROM, io.cpu.banka0, READONLY);
        bus->map_PRG8K( 0xC000, 0xDFFF, &bus->PRG_ROM, io.cpu.bank80, READONLY);
    } else {
        // VRC2
        bus->map_PRG8K( 0x8000, 0x9FFF, &bus->PRG_ROM, io.cpu.bank80, READONLY);
        bus->map_PRG8K( 0xA000, 0xBFFF, &bus->PRG_ROM, io.cpu.banka0, READONLY);
        bus->map_PRG8K( 0xC000, 0xDFFF, &bus->PRG_ROM, bus->num_PRG_ROM_banks8K - 2, READONLY);
    }

    bus->map_CHR1K(0x0000, 0x03FF, &bus->CHR_ROM, io.ppu.banks[0], READONLY);
    bus->map_CHR1K(0x0400, 0x07FF, &bus->CHR_ROM, io.ppu.banks[1], READONLY);
    bus->map_CHR1K(0x0800, 0x0BFF, &bus->CHR_ROM, io.ppu.banks[2], READONLY);
    bus->map_CHR1K(0x0C00, 0x0FFF, &bus->CHR_ROM, io.ppu.banks[3], READONLY);
    bus->map_CHR1K(0x1000, 0x13FF, &bus->CHR_ROM, io.ppu.banks[4], READONLY);
    bus->map_CHR1K(0x1400, 0x17FF, &bus->CHR_ROM, io.ppu.banks[5], READONLY);
    bus->map_CHR1K(0x1800, 0x1BFF, &bus->CHR_ROM, io.ppu.banks[6], READONLY);
    bus->map_CHR1K(0x1C00, 0x1FFF, &bus->CHR_ROM, io.ppu.banks[7], READONLY);
}

void VRC2B_4E_4F::serialize(serialized_state &state)
{
#define S(x) Sadd(state, & x, sizeof( x))
    S(irq);
    S(io);
#undef S
}

void VRC2B_4E_4F::deserialize(serialized_state &state)
{
#define L(x) Sload(state, & x, sizeof( x))
    L(irq);
    L(io);
#undef L
    remap(false);
}

void VRC2B_4E_4F::set_ppu_lo(u32 bank, u32 val)
{

    u32 b = io.ppu.banks[bank];
    if (is_vrc2a) b <<= 1;

    b = (b & 0x1F0) | (val & 0x0F);

    if (is_vrc2a) b >>= 1;
    io.ppu.banks[bank] = b;
    u32 range_start = bank << 10;
    bus->map_CHR1K(range_start, range_start + 0x3FF, &bus->CHR_ROM, b, READONLY);
}

void VRC2B_4E_4F::set_ppu_hi(u32 bank, u32 val)
{

    u32 b = io.ppu.banks[bank];
    if (is_vrc2a) b <<= 1;
    if (is_vrc4) val = (val & 0x1F) << 4;
    else val = (val & 0x0F) << 4;
    b = (b & 0x0F) | val;
    if (is_vrc2a) b >>= 1;
    io.ppu.banks[bank] = b;

    u32 range_start = bank << 10;
    bus->map_CHR1K(range_start, range_start + 0x3FF, &bus->CHR_ROM, b, READONLY);
}

void VRC2B_4E_4F::reset()
{

    printf("\nVRC24 Resetting, so remapping bus...");
    io.cpu.banka0 = 0;
    io.cpu.bank80 = 0;
    remap(true);
    bus->PPU_mirror_set();
}

static inline u32 mess_up_addr(u32 addr) {
    // Thanks Mesen! NESdev is not correct!
    u32 A0 = addr & 0x01;
    u32 A1 = (addr >> 1) & 0x01;

    //VRC4e
    A0 |= (addr >> 2) & 0x01;
    A1 |= (addr >> 3) & 0x01;
    return (addr & 0xFF00) | (A1 << 1) | A0;
}

void VRC2B_4E_4F::writecart(u32 addr, u32 val, u32 &do_write)
{
    do_write = 1;
    if ((addr >= 0x6000) && (addr < 0x8000)) {
        if (!is_vrc4) {
            io.latch60 = val & 1;
        }
        return;
    }
    if (addr < 0x8000) return;
    addr = mess_up_addr(addr) & 0xF00F;

    switch(addr) {
        case 0x8000:
        case 0x8001:
        case 0x8002:
        case 0x8003:
        case 0x8004:
        case 0x8005:
        case 0x8006: {
            u32 old80 = io.cpu.bank80;
            io.cpu.bank80 = val & 0x1F;
            if (old80 != io.cpu.bank80) {
                if (is_vrc4 && io.vrc4.banks_swapped) {
                    bus->map_PRG8K( 0xC000, 0xDFFF, &bus->PRG_ROM, io.cpu.bank80, READONLY);
                } else {
                    // VRC2
                    bus->map_PRG8K( 0x8000, 0x9FFF, &bus->PRG_ROM, io.cpu.bank80, READONLY);
                }
            }
            return; }
        case 0x9000:
        case 0x9001:
        case 0x9002:
        case 0x9003:
        case 0x9004:
        case 0x9005:
        case 0x9006: {
            if (is_vrc4 && (addr == 0x9002)) {
                // wram control
                u32 oe = io.wram_enabled;
                io.wram_enabled = (val & 1);
                if (oe != io.wram_enabled) {
                    bus->map_PRG8K( 0x6000, 0x7FFF, io.wram_enabled ? &bus->fake_PRG_RAM : nullptr, 0, READWRITE);
                }


                // swap mode
                io.vrc4.banks_swapped = (val & 2) >> 1;
            }
            if (is_vrc4 && (addr == 0x9003)) {
                return;
            }

            if (is_vrc4) val &= 3;
            else val &= 1;
            // 0 vertical 1 horizontal 2 a 3 b
            u32 om = bus->ppu_mirror_mode;
            switch(val) {
                case 0: bus->ppu_mirror_mode = PPUM_Vertical; break;
                case 1: bus->ppu_mirror_mode = PPUM_Horizontal; break;
                case 2: bus->ppu_mirror_mode = PPUM_ScreenAOnly; break;
                case 3: bus->ppu_mirror_mode = PPUM_ScreenBOnly; break;
                default: break;
            }
            if (om != bus->ppu_mirror_mode) bus->PPU_mirror_set();
            return; }
        case 0xA000:
        case 0xA001:
        case 0xA002:
        case 0xA003:
        case 0xA004:
        case 0xA005:
        case 0xA006: {
            u32 oldao = io.cpu.banka0;
            io.cpu.banka0 = val & 0x1F;
            if (oldao != io.cpu.banka0)
                bus->map_PRG8K( 0xA000, 0xBFFF, &bus->PRG_ROM, io.cpu.banka0, READONLY);
            return; }
        case 0xF000: // IRQ latch low 4
            if (!is_vrc4) return;
            irq.reload = (irq.reload & 0xF0) | (val & 0x0F);
            return;
        case 0xF001: // IRQ latch hi 4
            if (!is_vrc4) return;
            irq.reload = (irq.reload & 0x0F) | ((val & 0x0F) << 4);
            return;
        case 0xF002: // IRQ control
            if (!is_vrc4) return;
            irq.prescaler = 341;
            if (val & 2) irq.counter = irq.reload;
            irq.cycle_mode = (val & 4) >> 2;
            irq.enable = (val & 2) >> 1;
            irq.enable_after_ack = (val & 1);
            return;
        case 0xF003: // IRQ ack
            if (!is_vrc4) return;
            irq.enable = irq.enable_after_ack;
            bus->nes->cpu.notify_IRQ(0, 1);
            return;
        default: break;
    }

    // Thanks Messen! NESdev wiki was wrong here...
    if ((addr >= 0xB000) && (addr <= 0xE006)) {
        u32 rn = ((((addr >> 12) & 0x07) - 3) << 1) + ((addr >> 1) & 0x01);
        if ((addr & 1) == 0) {
            //The other reg contains the low 4 bits
            set_ppu_lo(rn, val);
        } else {
            //One reg contains the high 5 bits
            set_ppu_hi(rn, val);
        }
    }
}

u32 VRC2B_4E_4F::readcart(u32 addr, u32 old_val, u32 has_effect, u32 &do_read)
{
    do_read = 1;
    return old_val;
}

void VRC2B_4E_4F::setcart(NES_cart &cart)
{
    bus->ppu_mirror_mode = cart.header.mirroring;

    io.wram_enabled = bus->nes->cart.header.prg_ram_size > 0;
}

void VRC2B_4E_4F::cpu_cycle()
{
    if (is_vrc4 && irq.enable) {
        irq.prescaler -= 3;
        if (irq.cycle_mode || ((irq.prescaler <= 0) && !irq.cycle_mode)) {
            if (irq.counter == 0xFF) {
                irq.counter = irq.reload;
                bus->nes->cpu.notify_IRQ(1, 1);
            } else {
                irq.counter++;
            }
            irq.prescaler += 341;
        }
    }
}

VRC2B_4E_4F::VRC2B_4E_4F(NES_bus *bus, NES_mappers in_kind) : NES_mapper(bus), kind(in_kind)
{
    this->overrides_PPU = false;

    switch(kind) {
        case NESM_VRC4E_4F:
            is_vrc2a = false;
            is_vrc4 = true;
            break;
        default:
            assert(1==2);
    }

    for (u32 i = 0; i < 8; i++) {
        io.ppu.banks[i] = i;
    }
}