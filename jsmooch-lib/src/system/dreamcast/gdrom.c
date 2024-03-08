//
// Created by David Schneider on 3/2/24.
//

#include "stdio.h"
#include "gdrom.h"

void GDROM_write(struct DC* this, u32 reg, u32 val, u32 bits)
{
    printf("\nGDROM write! %08x %02x %04x %d cycle:%llu", reg | 0x5F7400, reg, val, bits, this->sh4.trace_cycles);
    switch(reg | 0x5F7400) {
        // read / write
        default: {
            printf("\nUnhandled GDR reg write %02x val %04x", reg, val);
            return;
        }
    }
}

u32 GDROM_read(struct DC* this, u32 reg, u32 bits)
{
    return 0;
}