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
        case 0x5F7018: { // Alternate status / Device control

        }
        case 0x5F7080: {// Data / Data
        }
        case 0x5F7084: {// Error / Features
        }
        case 0x5F7088: {// Interrupt reason / sector count
        }
        case 0x5F708C: {// Sector Number/Sector Number
        }
        case 0x5F7090: {// Byte control lo / byte control lo
        }
        case 0x5F7094: {// Byte control hi / byte control hi
        }
        case 0x5F7098: {// Drive select / drive select
        }
        case 0x5F709C: {// Status / command
        }
        default: {
            printf("\nUnhandled GDR reg write %02x val %04x", reg, val);
            return;
        }
    }
}

u32 GDROM_read(struct DC* this, u32 reg, u32 bits)
{

}