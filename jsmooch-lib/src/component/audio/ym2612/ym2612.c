//
// Created by . on 7/19/24.
//

#include <stdio.h>
#include <string.h>
#include "helpers/debug.h"
#include "ym2612.h"

void ym2612_init(struct ym2612* this)
{
    memset(this, 0, sizeof(*this));
}

void ym2612_delete(struct ym2612* this)
{

}

void ym2612_write(struct ym2612* this, u32 addr, u8 val)
{
    addr &= 3;
    addr |= 0x4000;
    /*case 0x4000: return opn2.writeAddress(0 << 8 | data);
    case 0x4001: return opn2.writeData(data);
    case 0x4002: return opn2.writeAddress(1 << 8 | data);
    case 0x4003: return opn2.writeData(data);*/
    printf("\nYM2612 write addr:%d val:%d", addr, val);
}

void ym2612_reset(struct ym2612* this)
{

}

u8 ym2612_read(struct ym2612* this, u32 addr, u32 old, u32 has_effect)
{
    return this->timer_a.line | (this->timer_b.line << 1);
}

//genesis_z80_ym2612_read