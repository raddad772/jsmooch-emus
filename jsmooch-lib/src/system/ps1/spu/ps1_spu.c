//
// Created by . on 2/20/25.
//

#include "ps1_spu.h"
#include "../ps1_bus.h"

void PS1_SPU_init(struct PS1 *this)
{

}

void PS1_SPU_write(struct PS1_SPU *this, u32 addr, u32 sz, u32 val)
{
    addr -= 0x1F801C00;
    addr <<= 1;
    assert(addr < 0x400);
    this->val[addr] = val & 0xFFFF;
    if (sz == 4) this->val[addr+1] = val >> 16;
}

u32 PS1_SPU_read(struct PS1_SPU *this, u32 addr, u32 sz, u32 has_effect)
{
    addr -= 0x1F801C00;
    addr <<= 1;
    assert(addr < 0x400);
    u32 r = this->val[addr];
    if (sz == 4) r |= (this->val[addr+1] << 16);
    return r;
}
