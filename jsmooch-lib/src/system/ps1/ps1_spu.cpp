//
// Created by . on 2/20/25.
//
#include <cassert>
#include "ps1_spu.h"
#include "ps1_bus.h"

namespace PS1 {
void SPU::write(u32 addr, u32 sz, u32 wval)
{
    addr -= 0x1F801C00;
    addr <<= 1;
    assert(addr < 0x400);
    val[addr] = wval & 0xFFFF;
    if (sz == 4) val[addr+1] = wval >> 16;
}

u32 SPU::read(u32 addr, u32 sz, bool has_effect)
{
    addr -= 0x1F801C00;
    addr <<= 1;
    assert(addr < 0x400);
    u32 r = val[addr];
    if (sz == 4) r |= (val[addr+1] << 16);
    return r;
}
}