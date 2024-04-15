//
// Created by RadDad772 on 4/14/24.
//

#include "m6532.h"

void M6532_init(struct M6532* this)
{
    // TODO
}

void M6532_reset(struct M6532* this)
{
    // TODO
}

static u8 M6532_io_read(struct M6532* this, u32 addr, u32 val)
{
    switch((addr & 7) | 0x280) {
        case 0x280: // SWCHA: port A, input or output R/W
            return 0;
        case 0x281: // SWACNT: port A DDR R/W
            return 0;
        case 0x282: // SWCHB: port B, input or output R/W
            return 0;
        case 0x283: // SWBCNT: port B DDR R/W
            return 0;
        case 0x284: // INTIM: timer output (read-only)
        case 0x286:
            return 0;
        case 0x285: // INSTAT: timer status. read-only and undocumented
        case 0x287:
            return 0;
    }
    return 0;
}

static void M6532_io_write(struct M6532* this, u32 addr, u32 val)
{
    switch((addr & 7) | 0x280) {
        case 0x280: // SWCHA: port A, input or output R/W
            return;
        case 0x281: // SWACNT: port A DDR R/W
            return;
        case 0x282: // SWCHB: port B, input or output R/W
            return;
        case 0x283: // SWBCNT: port B DDR R/W
            return;
        case 0x284: // TIM1T: set 1-clock interval
            return;
        case 0x285: // TIM8T: set 8-clock interval
            return;
        case 0x286: // TIM64T: set 64-clock interval
            return;
        case 0x287: // T1024T: set 1024-clock interval
            return;
    }
}

void M6532_bus_cycle(struct M6532* this, u32 addr, u32 *data, u32 rw)
{
    addr &= 0x2FF;
    if (addr & 0x200) { // 280... switches
        if (rw == 0)
            *data = M6532_io_read(this, addr, *data);
        else
            M6532_io_write(this, addr, *data);
    }
    else { // RAM!
        if (rw == 0)
            *data = this->RAM[addr & 0x7F];
        else
            this->RAM[addr & 0x7F] = *data;
    }
}
