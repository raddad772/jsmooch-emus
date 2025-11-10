//
// Created by . on 9/30/24.
//

#include <cstdlib>
#include <cassert>

#include "helpers/debugger/debugger.h"

#include "../nes.h"

#include "mapper.h"
#include "cnrom_gnrom_jf11_jf14_color_dreams.h"

#define THISM GNROM *th = static_cast<GNROM *>(bus->ptr)

struct GNROM {
    NES *nes;
    NES_mappers kind;

    struct {
        u32 CHR_bank_num;
        u32 PRG_bank_num;
    } io;
};

#define READONLY 1
#define READWRITE 0

static void remap(NES_mapper *bus)
{
    THISM;
    NES_bus_map_PRG32K(bus, 0x8000, 0xFFFF, &bus->PRG_ROM, th->io.PRG_bank_num, READONLY);
    NES_bus_map_CHR8K(bus, 0x0000, 0x1FFF, &bus->CHR_ROM, th->io.CHR_bank_num, READONLY);
}

static void serialize(NES_mapper *bus, serialized_state &state)
{
    THISM;
#define S(x) Sadd(state, &th-> x, sizeof(th-> x))
    S(io.CHR_bank_num);
    S(io.PRG_bank_num);
#undef S
}

static void deserialize(NES_mapper *bus, serialized_state &state)
{
    THISM;
#define L(x) Sload(state, &th-> x, sizeof(th-> x))
    L(io.CHR_bank_num);
    L(io.PRG_bank_num);
#undef L
    remap(bus);
}

static void GNROM_destruct(NES_mapper *bus)
{

}

static void GNROM_reset(NES_mapper *bus)
{
    printf("\nGNROM/JF11/JF14/Color Dreams Resetting, so remapping bus...");
    THISM;
    NES_bus_PPU_mirror_set(bus);
    th->io.CHR_bank_num = 0;
    th->io.PRG_bank_num = 0;
    remap(bus);
}

static void GNROM_writecart(NES_mapper *bus, u32 addr, u32 val, u32 *do_write)
{
    *do_write = 1;
    THISM;
    //u32 dirty_RAM = 0;
    switch(th->kind) {
        case NESM_JF11_JF14:
            if ((addr >= 0x6000) && (addr <= 0x7FFF)) {
                //dirty_RAM = th->io.PRG_bank_num != ((val >> 4) & 3);
                th->io.PRG_bank_num = (val >> 4) & 3;
                th->io.CHR_bank_num = val & 15;
                remap(bus);
            }
            break;
        case NESM_GNROM:
            if (addr >= 0x8000) {
                //dirty_RAM = th->io.PRG_bank_num != ((val >> 4) & 3);
                th->io.PRG_bank_num = (val >> 4) & 3;
                th->io.CHR_bank_num = val & 3;
                remap(bus);
            }
            break;
        case NESM_CNROM:
            if (addr >= 0x8000) {
                th->io.CHR_bank_num = val % bus->num_CHR_ROM_banks8K;
                remap(bus);
            }
            break;
        case NESM_COLOR_DREAMS:
            if (addr >= 0x8000) {
                //dirty_RAM = th->io.PRG_bank_num != (val & 3);
                th->io.PRG_bank_num = val & 3;
                th->io.CHR_bank_num = (val >> 4) & 15;
                remap(bus);
            }
        default:
            assert(1==2);
    }
}

static u32 GNROM_readcart(NES_mapper *bus, u32 addr, u32 old_val, u32 has_effect, u32 *do_read)
{
    *do_read = 1;
    return old_val;
}

static void GNROM_setcart(NES_mapper *bus, NES_cart *cart)
{
    bus->ppu_mirror_mode = cart->header.mirroring ^ 1;
}

void GNROM_JF11_JF14_color_dreams_init(NES_mapper *bus, NES *nes, enum NES_mappers kind)
{
    if (bus->ptr != nullptr) free(bus->ptr);
    bus->ptr = malloc(sizeof(GNROM));
    THISM;

    th->nes = nes;
    th->kind = kind;

    th->io.CHR_bank_num = 0;
    th->io.PRG_bank_num = 0;

    bus->destruct = &GNROM_destruct;
    bus->reset = &GNROM_reset;
    bus->writecart = &GNROM_writecart;
    bus->readcart = &GNROM_readcart;
    bus->setcart = &GNROM_setcart;
    bus->cpu_cycle = nullptr;
    bus->a12_watch = nullptr;
    bus->serialize = &serialize;
    bus->deserialize = &deserialize;

}