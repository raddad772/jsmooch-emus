//
// Created by . on 9/29/24.
//


//
// Created by . on 9/27/24.
//

#include "system/nes/nes.h"

#include "mapper.h"
#include "axrom.h"

#define THISM struct NROM *th = (NROM *)bus->ptr

struct NROM {
    struct NES *nes;
    // Literally no state is needed for this actually...

    struct {
        u32 PRG_bank;
        u32 nametable;
    } io;

};

#define READONLY 1
#define READWRITE 0

static void remap(NES_mapper *bus)
{
    THISM;
    NES_bus_map_PRG32K(bus, 0x8000, 0xFFFF, &bus->PRG_ROM, th->io.PRG_bank, READONLY);
    NES_bus_PPU_mirror_set(bus);
}

static void serialize(NES_mapper *bus, serialized_state &state)
{
    THISM;
#define S(x) Sadd(state, &th-> x, sizeof(th-> x))
    S(io.PRG_bank);
    S(io.nametable);
#undef S
}

static void deserialize(NES_mapper *bus, serialized_state &state)
{
    THISM;
#define L(x) Sload(state, &th-> x, sizeof(th-> x))
    L(io.PRG_bank);
    L(io.nametable);
#undef L
    remap(bus);
}

static void AXROM_destruct(NES_mapper *bus)
{

}

static void AXROM_reset(NES_mapper *bus)
{
    THISM;
    printf("\nAXROM Resetting, so remapping bus...");
    th->io.PRG_bank = bus->num_PRG_ROM_banks32K - 1; // so like we're 7/8. = 14/16, 28/32.
    remap(bus);

    NES_bus_map_CHR8K(bus, 0x0000, 0x1FFF, &bus->CHR_RAM, 0, READWRITE);
}

static void AXROM_writecart(NES_mapper *bus, u32 addr, u32 val, u32 *do_write)
{
    THISM;
    if (addr >= 0x8000) {
        th->io.PRG_bank = val & 7;
        th->io.nametable = (val >> 4) & 1;
        bus->ppu_mirror_mode = th->io.nametable ? PPUM_ScreenBOnly : PPUM_ScreenAOnly;
        remap(bus);
    }
    *do_write = 1;
}

static u32 AXROM_readcart(NES_mapper *bus, u32 addr, u32 old_val, u32 has_effect, u32 *do_read)
{
    *do_read = 1;
    return old_val;
}

static void AXROM_setcart(NES_mapper *bus, NES_cart *cart)
{
    bus->ppu_mirror_mode = PPUM_ScreenAOnly;
    NES_bus_PPU_mirror_set(bus);
}

void AXROM_init(NES_mapper *bus, NES *nes)
{
    if (bus->ptr != nullptr) free(bus->ptr);
    bus->ptr = malloc(sizeof(NROM));
    THISM;

    th->nes = nes;

    bus->destruct = &AXROM_destruct;
    bus->reset = &AXROM_reset;
    bus->writecart = &AXROM_writecart;
    bus->readcart = &AXROM_readcart;
    bus->setcart = &AXROM_setcart;
    bus->cpu_cycle = nullptr;
    bus->a12_watch = nullptr;
    bus->serialize = &serialize;
    bus->deserialize = &deserialize;
}