//
// Created by . on 2/20/25.
//
#include <cassert>
#include "ps1_spu.h"
#include "ps1_bus.h"

namespace PS1 {
void SPU::write(u32 addr, u8 sz, u32 wval)
{
    addr &= 0xFFFFFFFE;
    addr -= 0x1F801C00;
    //addr <<= 1;
    assert(addr < 0x400);
    RAM[addr] = wval & 0xFFFF;
    if (sz == 4) RAM[addr+1] = wval >> 16;
}

u32 SPU::read(u32 addr, u8 sz, bool has_effect)
{
    addr -= 0x1F801C00;
    //addr <<= 1;
    assert(addr < 0x400);
    u32 r = RAM[addr];
    if (sz == 4) r |= (RAM[addr+1] << 16);
    return r;
}
}