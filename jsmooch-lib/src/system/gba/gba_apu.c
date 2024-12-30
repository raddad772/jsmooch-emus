//
// Created by . on 12/28/24.
//

#include "gba_bus.h"
#include "gba_apu.h"

void GBA_APU_init(struct GBA*this)
{
    GB_APU_init(&this->apu.old);
}

u32 GBA_APU_read_IO(struct GBA*this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    return 0;
}

void GBA_APU_write_IO(struct GBA*this, u32 addr, u32 sz, u32 access, u32 val)
{

}
