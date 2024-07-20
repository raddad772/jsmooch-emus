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
    printf("\nYM2612 write addr:%d val:%d", addr, val);
}

void ym2612_reset(struct ym2612* this)
{

}

u8 ym2612_read(struct ym2612* this, u32 addr, u32 old, u32 has_effect)
{
    printf("\nYM2612 read addr:%d", addr);
    return 0xFF;
}

//genesis_z80_ym2612_read