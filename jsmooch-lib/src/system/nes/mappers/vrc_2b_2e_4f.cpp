//
// Created by . on 9/27/24.
//

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "helpers/debugger/debugger.h"
#include "../nes.h"

#include "mapper.h"
#include "vrc_2b_2e_4f.h"

#define THISM struct VRC24 *this = (struct VRC24 *)bus->ptr

struct VRC24 {
    struct NES *nes;
    enum NES_mappers kind;

    u32 is_vrc4;
    u32 is_vrc2a;

    struct {
        u32 cycle_mode;
        u32 enable;
        u32 enable_after_ack;
        u32 reload;
        u32 prescaler;
        u32 counter;
    } irq;
    struct {
        u32 wram_enabled;
        u32 latch60;
        struct {
            u32 banks_swapped;
        } vrc4;
        struct {
            u32 banks[8];
        } ppu;
        struct {
            u32 bank80;
            u32 banka0;
        } cpu;
    } io;
};

#define READONLY 1
#define READWRITE 0

static void remap(struct NES_mapper *bus, u32 boot)
{
    THISM;
    if (boot) {
        // VRC2 maps last c0 and e0 to last 2 banks
        this->io.vrc4.banks_swapped = 0;
        //NES_bus_map_PRG8K(bus, 0x6000, 0x7FFF, &bus->PRG_RAM, 0, READWRITE);

        NES_bus_map_PRG8K(bus, 0xE000, 0xFFFF, &bus->PRG_ROM, bus->num_PRG_ROM_banks8K - 1, READONLY);
    }

    NES_bus_map_PRG8K(bus, 0x6000, 0x7FFF, this->io.wram_enabled ? &bus->fake_PRG_RAM : NULL, 0, READWRITE);

    if (this->is_vrc4 && this->io.vrc4.banks_swapped) {
        NES_bus_map_PRG8K(bus, 0x8000, 0x9FFF, &bus->PRG_ROM, bus->num_PRG_ROM_banks8K - 2, READONLY);
        NES_bus_map_PRG8K(bus, 0xA000, 0xBFFF, &bus->PRG_ROM, this->io.cpu.banka0, READONLY);
        NES_bus_map_PRG8K(bus, 0xC000, 0xDFFF, &bus->PRG_ROM, this->io.cpu.bank80, READONLY);
    } else {
        // VRC2
        NES_bus_map_PRG8K(bus, 0x8000, 0x9FFF, &bus->PRG_ROM, this->io.cpu.bank80, READONLY);
        NES_bus_map_PRG8K(bus, 0xA000, 0xBFFF, &bus->PRG_ROM, this->io.cpu.banka0, READONLY);
        NES_bus_map_PRG8K(bus, 0xC000, 0xDFFF, &bus->PRG_ROM, bus->num_PRG_ROM_banks8K - 2, READONLY);
    }

    NES_bus_map_CHR1K(bus, 0x0000, 0x03FF, &bus->CHR_ROM, this->io.ppu.banks[0], READONLY);
    NES_bus_map_CHR1K(bus, 0x0400, 0x07FF, &bus->CHR_ROM, this->io.ppu.banks[1], READONLY);
    NES_bus_map_CHR1K(bus, 0x0800, 0x0BFF, &bus->CHR_ROM, this->io.ppu.banks[2], READONLY);
    NES_bus_map_CHR1K(bus, 0x0C00, 0x0FFF, &bus->CHR_ROM, this->io.ppu.banks[3], READONLY);
    NES_bus_map_CHR1K(bus, 0x1000, 0x13FF, &bus->CHR_ROM, this->io.ppu.banks[4], READONLY);
    NES_bus_map_CHR1K(bus, 0x1400, 0x17FF, &bus->CHR_ROM, this->io.ppu.banks[5], READONLY);
    NES_bus_map_CHR1K(bus, 0x1800, 0x1BFF, &bus->CHR_ROM, this->io.ppu.banks[6], READONLY);
    NES_bus_map_CHR1K(bus, 0x1C00, 0x1FFF, &bus->CHR_ROM, this->io.ppu.banks[7], READONLY);
}

static void serialize(struct NES_mapper *bus, struct serialized_state *state)
{
    THISM;
#define S(x) Sadd(state, &this-> x, sizeof(this-> x))
    S(irq);
    S(io);
#undef S
}

static void deserialize(struct NES_mapper *bus, struct serialized_state *state)
{
    THISM;
#define L(x) Sload(state, &this-> x, sizeof(this-> x))
    L(irq);
    L(io);
#undef L
    remap(bus, 0);
}

static void set_ppu_lo(struct NES_mapper *bus, u32 bank, u32 val)
{
    THISM;
    u32 b = this->io.ppu.banks[bank];
    if (this->is_vrc2a) b <<= 1;

    b = (b & 0x1F0) | (val & 0x0F);

    if (this->is_vrc2a) b >>= 1;
    this->io.ppu.banks[bank] = b;
    u32 range_start = bank << 10;
    NES_bus_map_CHR1K(bus, range_start, range_start + 0x3FF, &bus->CHR_ROM, b, READONLY);
}

void set_ppu_hi(struct NES_mapper* bus, u32 bank, u32 val)
{
    THISM;
    u32 b = this->io.ppu.banks[bank];
    if (this->is_vrc2a) b <<= 1;
    if (this->is_vrc4) val = (val & 0x1F) << 4;
    else val = (val & 0x0F) << 4;
    b = (b & 0x0F) | val;
    if (this->is_vrc2a) b >>= 1;
    this->io.ppu.banks[bank] = b;

    u32 range_start = bank << 10;
    NES_bus_map_CHR1K(bus, range_start, range_start + 0x3FF, &bus->CHR_ROM, b, READONLY);
}



static void VRC24_destruct(struct NES_mapper *bus)
{

}

static void VRC24_reset(struct NES_mapper *bus)
{
    THISM;
    printf("\nVRC24 Resetting, so remapping bus...");
    this->io.cpu.banka0 = 0;
    this->io.cpu.bank80 = 0;
    remap(bus, 1);
    NES_bus_PPU_mirror_set(bus);
}

static u32 mess_up_addr(u32 addr) {
    // Thanks Mesen! NESdev is not correct!
    u32 A0 = addr & 0x01;
    u32 A1 = (addr >> 1) & 0x01;

    //VRC4e
    A0 |= (addr >> 2) & 0x01;
    A1 |= (addr >> 3) & 0x01;
    return (addr & 0xFF00) | (A1 << 1) | A0;
}


static void VRC24_writecart(struct NES_mapper *bus, u32 addr, u32 val, u32 *do_write)
{
    THISM;
    *do_write = 1;
    if ((addr >= 0x6000) && (addr < 0x8000)) {
        if (!this->is_vrc4) {
            this->io.latch60 = val & 1;
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
            u32 old80 = this->io.cpu.bank80;
            this->io.cpu.bank80 = val & 0x1F;
            if (old80 != this->io.cpu.bank80) {
                if (this->is_vrc4 && this->io.vrc4.banks_swapped) {
                    NES_bus_map_PRG8K(bus, 0xC000, 0xDFFF, &bus->PRG_ROM, this->io.cpu.bank80, READONLY);
                } else {
                    // VRC2
                    NES_bus_map_PRG8K(bus, 0x8000, 0x9FFF, &bus->PRG_ROM, this->io.cpu.bank80, READONLY);
                }
            }
            return; }
        case 0x9000:
        case 0x9001:
        case 0x9002:
        case 0x9003:
        case 0x9004:
        case 0x9005:
        case 0x9006:
            if (this->is_vrc4 && (addr == 0x9002)) {
                // wram control
                u32 oe = this->io.wram_enabled;
                this->io.wram_enabled = (val & 1);
                if (oe != this->io.wram_enabled) {
                    NES_bus_map_PRG8K(bus, 0x6000, 0x7FFF, this->io.wram_enabled ? &bus->fake_PRG_RAM : NULL, 0, READWRITE);
                }


                // swap mode
                this->io.vrc4.banks_swapped = (val & 2) >> 1;
            }
            if (this->is_vrc4 && (addr == 0x9003)) {
                return;
            }

            if (this->is_vrc4) val &= 3;
            else val &= 1;
            // 0 vertical 1 horizontal 2 a 3 b
            u32 om = bus->ppu_mirror_mode;
            switch(val) {
                case 0: bus->ppu_mirror_mode = PPUM_Vertical; break;
                case 1: bus->ppu_mirror_mode = PPUM_Horizontal; break;
                case 2: bus->ppu_mirror_mode = PPUM_ScreenAOnly; break;
                case 3: bus->ppu_mirror_mode = PPUM_ScreenBOnly; break;
            }
            if (om != bus->ppu_mirror_mode) NES_bus_PPU_mirror_set(bus);
            return;
        case 0xA000:
        case 0xA001:
        case 0xA002:
        case 0xA003:
        case 0xA004:
        case 0xA005:
        case 0xA006: {
            u32 oldao = this->io.cpu.banka0;
            this->io.cpu.banka0 = val & 0x1F;
            if (oldao != this->io.cpu.banka0)
                NES_bus_map_PRG8K(bus, 0xA000, 0xBFFF, &bus->PRG_ROM, this->io.cpu.banka0, READONLY);
            return; }
        case 0xF000: // IRQ latch low 4
            if (!this->is_vrc4) return;
            this->irq.reload = (this->irq.reload & 0xF0) | (val & 0x0F);
            return;
        case 0xF001: // IRQ latch hi 4
            if (!this->is_vrc4) return;
            this->irq.reload = (this->irq.reload & 0x0F) | ((val & 0x0F) << 4);
            return;
        case 0xF002: // IRQ control
            if (!this->is_vrc4) return;
            this->irq.prescaler = 341;
            if (val & 2) this->irq.counter = this->irq.reload;
            this->irq.cycle_mode = (val & 4) >> 2;
            this->irq.enable = (val & 2) >> 1;
            this->irq.enable_after_ack = (val & 1);
            return;
        case 0xF003: // IRQ ack
            if (!this->is_vrc4) return;
            this->irq.enable = this->irq.enable_after_ack;
            r2A03_notify_IRQ(&bus->nes->cpu, 0, 1);
            return;
    }

    // Thanks Messen! NESdev wiki was wrong here...
    if ((addr >= 0xB000) && (addr <= 0xE006)) {
        u32 rn = ((((addr >> 12) & 0x07) - 3) << 1) + ((addr >> 1) & 0x01);
        u32 lowBits = (addr & 0x01) == 0;
        if (lowBits) {
            //The other reg contains the low 4 bits
            set_ppu_lo(bus, rn, val);
        } else {
            //One reg contains the high 5 bits
            set_ppu_hi(bus, rn, val);
        }
    }
}

static u32 VRC24_readcart(struct NES_mapper *bus, u32 addr, u32 old_val, u32 has_effect, u32 *do_read)
{
    *do_read = 1;
    return old_val;
}

static void VRC24_setcart(struct NES_mapper *bus, struct NES_cart *cart)
{
    bus->ppu_mirror_mode = cart->header.mirroring;
    THISM;
    this->io.wram_enabled = bus->nes->cart.header.prg_ram_size > 0;
}

static void VRC24_cpucycle(struct NES_mapper *bus)
{
    THISM;
    if (this->irq.enable) {
        this->irq.prescaler -= 3;
        if (this->irq.cycle_mode || ((this->irq.prescaler <= 0) && !this->irq.cycle_mode)) {
            if (this->irq.counter == 0xFF) {
                this->irq.counter = this->irq.reload;
                r2A03_notify_IRQ(&bus->nes->cpu, 1, 1);
            } else {
                this->irq.counter++;
            }
            this->irq.prescaler += 341;
        }
    }
}

void VRC2B_4E_4F_init(struct NES_mapper *bus, struct NES *nes, enum NES_mappers kind)
{
    if (bus->ptr != NULL) free(bus->ptr);
    bus->ptr = malloc(sizeof(struct VRC24));
    struct VRC24 *this = (struct VRC24*)bus->ptr;

    this->nes = nes;
    this->kind = kind;

    bus->cpu_cycle = NULL;

    switch(kind) {
        case NESM_VRC4E_4F:
            this->is_vrc2a = false;
            this->is_vrc4 = true;
            bus->cpu_cycle = &VRC24_cpucycle;
            break;
        default:
            assert(1==2);
    }

    for (u32 i = 0; i < 8; i++) {
        this->io.ppu.banks[i] = i;
    }
    this->irq.cycle_mode = 0;
    this->irq.enable = 0;
    this->irq.enable_after_ack = 0;
    this->irq.reload = 0;
    this->irq.prescaler = 341;
    this->irq.counter = 0;
    this->io.vrc4.banks_swapped = 0;
    this->io.wram_enabled = 0;

    bus->destruct = &VRC24_destruct;
    bus->reset = &VRC24_reset;
    bus->writecart = &VRC24_writecart;
    bus->readcart = &VRC24_readcart;
    bus->setcart = &VRC24_setcart;
    bus->a12_watch = NULL;
    bus->serialize = &serialize;
    bus->deserialize = &deserialize;
}