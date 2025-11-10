//
// Created by . on 9/27/24.
//

#include "helpers/debugger/debugger.h"

#include "system/nes/nes.h"

#include "mapper.h"
#include "uxrom.h"

#define THISM UXROM *th = static_cast<UXROM *>(bus->ptr)

struct UXROM {
    NES *nes;

    struct {
        u32 bank_num;
    } io;
};

#define READONLY 1
#define READWRITE 0

static void remap(NES_mapper *bus, u32 is_boot)
{
    THISM;
    NES_bus_map_PRG16K(bus, 0x8000, 0xBFFF, &bus->PRG_ROM, th->io.bank_num, READONLY);
    if (is_boot) {
        NES_bus_map_PRG16K(bus, 0xC000, 0xFFFF, &bus->PRG_ROM, bus->num_PRG_ROM_banks16K - 1, READONLY);
        NES_bus_map_CHR8K(bus, 0x0000, 0x1FFF, &bus->CHR_RAM, 0, READWRITE);
    }
}

static void serialize(NES_mapper *bus, serialized_state &state)
{
    THISM;
#define S(x) Sadd(state, &th-> x, sizeof(th-> x))
    S(io.bank_num);
#undef S
}

static void deserialize(NES_mapper *bus, serialized_state &state)
{
    THISM;
#define L(x) Sload(state, &th-> x, sizeof(th-> x))
    L(io.bank_num);
#undef L
    remap(bus, 0);
}

static void UXROM_destruct(NES_mapper *bus)
{

}

static void UXROM_reset(NES_mapper *bus)
{
    printf("\nUXROM Resetting, so remapping bus...");
    remap(bus, 1);
}

static void UXROM_writecart(NES_mapper *bus, u32 addr, u32 val, u32 *do_write)
{
    THISM;
    *do_write = 1;
    if (addr >= 0x8000) {
        th->io.bank_num = val % bus->num_PRG_ROM_banks16K;
        remap(bus, 0);
    }
}

static u32 UXROM_readcart(NES_mapper *bus, u32 addr, u32 old_val, u32 has_effect, u32 *do_read)
{
    *do_read = 1;
    return old_val;
}

static void UXROM_setcart(NES_mapper *bus, NES_cart *cart)
{
    bus->ppu_mirror_mode = cart->header.mirroring;
    NES_bus_PPU_mirror_set(bus);
}

void UXROM_init(NES_mapper *bus, NES *nes)
{
    if (bus->ptr != nullptr) free(bus->ptr);
    bus->ptr = malloc(sizeof(UXROM));
    THISM;

    th->io.bank_num = 0;

    th->nes = nes;

    bus->destruct = &UXROM_destruct;
    bus->reset = &UXROM_reset;
    bus->writecart = &UXROM_writecart;
    bus->readcart = &UXROM_readcart;
    bus->setcart = &UXROM_setcart;
    bus->cpu_cycle = nullptr;
    bus->a12_watch = nullptr;
    bus->serialize = &serialize;
    bus->deserialize = &deserialize;

}