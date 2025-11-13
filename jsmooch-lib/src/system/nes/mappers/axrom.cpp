//
// Created by . on 9/29/24.
//


//
// Created by . on 9/27/24.
//

#include "system/nes/nes.h"

#include "mapper.h"
#include "axrom.h"

#define READONLY 1
#define READWRITE 0

void AXROM::remap()
{
    bus->map_PRG32K( 0x8000, 0xFFFF, &bus->PRG_ROM, io.PRG_bank, READONLY);
    bus->PPU_mirror_set();
}

void AXROM::serialize(serialized_state &state)
{
#define S(x) Sadd(state, &x, sizeof(x))
    S(io.PRG_bank);
    S(io.nametable);
#undef S
}

void AXROM::deserialize(serialized_state &state)
{
#define L(x) Sload(state, &x, sizeof(x))
    L(io.PRG_bank);
    L(io.nametable);
#undef L
    remap();
}

void AXROM::reset()
{
    printf("\nAXROM Resetting, so remapping bus...");
    io.PRG_bank = bus->num_PRG_ROM_banks32K - 1; // so like we're 7/8. = 14/16, 28/32.
    remap();

    bus->map_CHR8K(0x0000, 0x1FFF, &bus->CHR_RAM, 0, READWRITE);
}

void AXROM::writecart(u32 addr, u32 val, u32 &do_write)
{
    if (addr >= 0x8000) {
        io.PRG_bank = val & 7;
        io.nametable = (val >> 4) & 1;
        bus->ppu_mirror_mode = io.nametable ? PPUM_ScreenBOnly : PPUM_ScreenAOnly;
        remap();
    }
    do_write = 1;
}

u32 AXROM::readcart(u32 addr, u32 old_val, u32 has_effect, u32 &do_read)
{
    do_read = 1;
    return old_val;
}

void AXROM::setcart(NES_cart &cart)
{
    bus->ppu_mirror_mode = PPUM_ScreenAOnly;
    bus->PPU_mirror_set();
}

AXROM::AXROM(NES_bus *bus) : NES_mapper(bus) {
    this->overrides_PPU = false;;
}