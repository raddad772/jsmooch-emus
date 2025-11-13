//
// Created by . on 9/27/24.
//

#include "helpers/debugger/debugger.h"

#include "system/nes/nes.h"

#include "mapper.h"
#include "uxrom.h"

#define READONLY 1
#define READWRITE 0

void UXROM::remap(bool is_boot)
{
    bus->map_PRG16K( 0x8000, 0xBFFF, &bus->PRG_ROM, io.bank_num, READONLY);
    if (is_boot) {
        bus->map_PRG16K( 0xC000, 0xFFFF, &bus->PRG_ROM, bus->num_PRG_ROM_banks16K - 1, READONLY);
        bus->map_CHR8K(0x0000, 0x1FFF, &bus->CHR_RAM, 0, READWRITE);
    }
}

void UXROM::serialize(serialized_state &state)
{

#define S(x) Sadd(state, & x, sizeof( x))
    S(io.bank_num);
#undef S
}

void UXROM::deserialize(serialized_state &state)
{

#define L(x) Sload(state, & x, sizeof( x))
    L(io.bank_num);
#undef L
    remap(false);
}

void UXROM::reset()
{
    printf("\nUXROM Resetting, so remapping bus...");
    remap(true);
}

void UXROM::writecart(u32 addr, u32 val, u32 &do_write)
{

    do_write = 1;
    if (addr >= 0x8000) {
        io.bank_num = val % bus->num_PRG_ROM_banks16K;
        remap(false);
    }
}

u32 UXROM::readcart(u32 addr, u32 old_val, u32 has_effect, u32 &do_read)
{
    do_read = 1;
    return old_val;
}

void UXROM::setcart(NES_cart &cart)
{
    bus->ppu_mirror_mode = cart.header.mirroring;
    bus->PPU_mirror_set();
}

UXROM::UXROM(NES_bus *bus) : NES_mapper(bus)
{
    this->overrides_PPU = false;
}
