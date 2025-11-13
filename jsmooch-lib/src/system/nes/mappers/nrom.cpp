//
// Created by . on 9/27/24.
//

#include "helpers/debugger/debugger.h"
#include "system/nes/nes.h"

#include "mapper.h"
#include "nrom.h"

#define READONLY 1
#define READWRITE 0

void NROM::remap()
{
    bus->map_PRG32K( 0x8000, 0xFFFF, &bus->PRG_ROM, 0, READONLY);
    bus->map_CHR1K(0x0000, 0x1FFF, &bus->CHR_ROM, 0, READONLY);
    bus->PPU_mirror_set();
}

void NROM::serialize(serialized_state &state)
{

}

void NROM::deserialize(serialized_state &state)
{

}

void NROM::reset()
{
    printf("\nNROM Resetting, so remapping bus...");
    remap();
}

void NROM::writecart(u32 addr, u32 val, u32 &do_write)
{
    do_write = 1;
}

u32 NROM::readcart(u32 addr, u32 old_val, u32 has_effect, u32 &do_read)
{
    do_read = 1;
    return old_val;
}

void NROM::setcart(NES_cart &cart)
{
    bus->ppu_mirror_mode = cart.header.mirroring;
}

NROM::NROM(NES_bus *bus) : NES_mapper(bus)
{
    this->overrides_PPU = false;
}
