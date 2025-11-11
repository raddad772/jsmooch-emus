//
// Created by RadDad772 on 4/14/24.
//

#include "m6532.h"

void M6532_init(M6532* this)
{
    M6532_reset(this);
}

void M6532_reset(M6532* this)
{
    // TODO
    this->timer.counter = 0;
    this->timer.cycle_mask = 0;
    this->timer.highspeed_mode = 0;
    this->timer.reload_val = 0;
    this->timer.cycle = 0;

    this->io.underflow_since_timnnt = 0;
    this->io.underflow_since_instat = 0;
}

static u8 M6532_io_read(M6532* this, u32 addr, u32 val)
{
    u32 t;
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
            this->timer.highspeed_mode = 0;
            return this->timer.counter;
        case 0x285: // INSTAT: timer status. read-only and undocumented
        case 0x287:
            t = this->io.underflow_since_instat << 16;
            this->io.underflow_since_instat = 0;
            return (this->io.underflow_since_timnnt << 7) | t;
    }
    return 0;
}

void M6532_cycle(M6532* this)
{
    u32 do_decrement = 0;
    if (this->timer.highspeed_mode) do_decrement = 1;
    else {
        this->timer.cycle = (this->timer.cycle + 1) & this->timer.cycle_mask;
        do_decrement = this->timer.cycle == 0;
    }
    if (do_decrement) {
        this->timer.counter = (this->timer.counter - 1) & 0xFF;
        if (this->timer.counter == 0xFF) {// Underflow
            this->io.underflow_since_instat = 1;
            this->io.underflow_since_timnnt = 1;
            this->timer.highspeed_mode = 1;
        }
    }
}

static void M6532_io_write(M6532* this, u32 addr, u32 val)
{
    u32 vmask;
    switch((addr & 7) | 0x280) {
        case 0x280: // SWCHA: port A, input or output R/W
            vmask = this->io.SWACNT;
            this->io.SWCHA = (this->io.SWCHA & (~vmask)) | (val & vmask);
            return;
        case 0x281: // SWACNT: port A DDR R/W
            this->io.SWACNT = val;
            return;
        case 0x282: // SWCHB: port B, input or output R/W
            vmask = this->io.SWBCNT;
            this->io.SWCHB = (this->io.SWCHB & (~vmask)) | (val & vmask);
            return;
        case 0x283: // SWBCNT: port B DDR R/W
            this->io.SWBCNT = val;
            return;
        case 0x284: // TIM1T: set 1-clock interval
            this->timer.counter = val & 0xFF;
            this->timer.reload_val = val & 0xFF;
            this->timer.highspeed_mode = 0;
            this->timer.cycle_mask = 0;
            this->timer.cycle = 0;

            this->io.underflow_since_timnnt = 0;
            return;
        case 0x285: // TIM8T: set 8-clock interval
            this->timer.counter = val & 0xFF;
            this->timer.reload_val = val & 0xFF;
            this->timer.highspeed_mode = 0;
            this->timer.cycle_mask = 7;
            this->timer.cycle = 7;

            this->io.underflow_since_timnnt = 0;
            return;
        case 0x286: // TIM64T: set 64-clock interval
            this->timer.counter = val & 0xFF;
            this->timer.reload_val = val & 0xFF;
            this->timer.highspeed_mode = 0;
            this->timer.cycle_mask = 63;
            this->timer.cycle = 63;

            this->io.underflow_since_timnnt = 0;
            return;
        case 0x287: // T1024T: set 1024-clock interval
            this->timer.counter = val & 0xFF;
            this->timer.reload_val = val & 0xFF;
            this->timer.highspeed_mode = 0;
            this->timer.cycle_mask = 1023;
            this->timer.cycle = 1023;

            this->io.underflow_since_timnnt = 0;
            return;
    }
}

void M6532_bus_cycle(M6532* this, u32 addr, u32 *data, u32 rw)
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
