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

#define READONLY 1
#define READWRITE 0

void GNROM_JF11_JF14_color_dreams::remap()
{
    bus->map_PRG32K( 0x8000, 0xFFFF, &bus->PRG_ROM, io.PRG_bank_num, READONLY);
    bus->map_CHR8K(0x0000, 0x1FFF, &bus->CHR_ROM, io.CHR_bank_num, READONLY);
}

void GNROM_JF11_JF14_color_dreams::serialize(serialized_state &state)
{
#define S(x) Sadd(state, &x, sizeof(x))
    S(io.CHR_bank_num);
    S(io.PRG_bank_num);
#undef S
}

void GNROM_JF11_JF14_color_dreams::deserialize(serialized_state &state)
{
#define L(x) Sload(state, &x, sizeof(x))
    L(io.CHR_bank_num);
    L(io.PRG_bank_num);
#undef L
    remap();
}

void GNROM_JF11_JF14_color_dreams::reset()
{
    printf("\nGNROM/JF11/JF14/Color Dreams Resetting, so remapping bus...");
    bus->PPU_mirror_set();
    io.CHR_bank_num = 0;
    io.PRG_bank_num = 0;
    remap();
}

void GNROM_JF11_JF14_color_dreams::writecart(u32 addr, u32 val, u32 &do_write)
{
    do_write = 1;
    //u32 dirty_RAM = 0;
    switch(kind) {
        case NESM_JF11_JF14:
            if ((addr >= 0x6000) && (addr <= 0x7FFF)) {
                //dirty_RAM = io.PRG_bank_num != ((val >> 4) & 3);
                io.PRG_bank_num = (val >> 4) & 3;
                io.CHR_bank_num = val & 15;
                remap();
            }
            break;
        case NESM_GNROM:
            if (addr >= 0x8000) {
                //dirty_RAM = io.PRG_bank_num != ((val >> 4) & 3);
                io.PRG_bank_num = (val >> 4) & 3;
                io.CHR_bank_num = val & 3;
                remap();
            }
            break;
        case NESM_CNROM:
            if (addr >= 0x8000) {
                io.CHR_bank_num = val % bus->num_CHR_ROM_banks8K;
                remap();
            }
            break;
        case NESM_COLOR_DREAMS:
            if (addr >= 0x8000) {
                //dirty_RAM = io.PRG_bank_num != (val & 3);
                io.PRG_bank_num = val & 3;
                io.CHR_bank_num = (val >> 4) & 15;
                remap();
            }
        default:
            assert(1==2);
    }
}

u32 GNROM_JF11_JF14_color_dreams::readcart(u32 addr, u32 old_val, u32 has_effect, u32 &do_read)
{
    do_read = 1;
    return old_val;
}

void GNROM_JF11_JF14_color_dreams::setcart(NES_cart &cart)
{
    bus->ppu_mirror_mode = cart.header.mirroring ^ 1;
}

GNROM_JF11_JF14_color_dreams::GNROM_JF11_JF14_color_dreams(NES_bus *bus, NES_mappers in_kind) : NES_mapper(bus), kind(in_kind)
{
    this->overrides_PPU = false;
}


