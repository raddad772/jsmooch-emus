//
// Created by RadDad772 on 4/14/24.
//

#include <string.h>

#include "m68000.h"
#include "m68000_internal.h"

void M68k_init(struct M68k* this)
{
    memset(this, 0, sizeof(*this));
}

void M68k_delete(struct M68k* this)
{

}

// big-endian processor. so bytes are Hi, Lo
// in a 16-bit read, the Upper will be addr 0, the Lower will be addr 1

// addr = 0, UDS = 1, LDS = 0
// addr = 1, UDS = 0, LDS = 1


#define ADDRAND1 { this->pins.LDS = (this->bus_cycle.addr & 1); this->pins.UDS = this->pins.LDS ^ 1; }

void M68k_bus_cycle_read(struct M68k* this)
{
    switch(this->bus_cycle.TCU) {
        case 0:
            // TODO: set FCs
            this->pins.RW = 0;
            this->bus_cycle.done = 0;
            if (this->state == M68kS_read8) {
                ADDRAND1;
            }
            else
                this->pins.LDS = this->pins.UDS = 1;
            break;
        case 1:
            this->pins.Addr = this->bus_cycle.addr & 0xFFFFFE;
            break;
        case 2:
            this->pins.AS = 1;
            break;
        case 3:
            break;
        case 4:
            if (!this->pins.DTACK) // insert wait state
                this->bus_cycle.TCU--;
        case 5:
        case 6:
            break;
        case 7: // latch data and de-assert pins
            this->pins.DTACK = 0;
            this->pins.AS = 0;
            if (this->state == M68kS_read8)
                this->bus_cycle.data = this->pins.D[this->bus_cycle.addr & 1];
            else {
                this->bus_cycle.data = ((this->pins.D[0] << 8) & 0xFF00) | (this->pins.D[1] & 0xFF);
            }
            this->pins.Addr = 0;
            this->pins.LDS = this->pins.UDS = 0;
            this->bus_cycle.done = 1;
            this->bus_cycle.TCU = -1;
            break;
    }
    this->bus_cycle.TCU++;
}

void M68k_bus_cycle_write(struct M68k* this)
{
    switch(this->bus_cycle.TCU) {
        case 0:
            // TODO: set FCs
            this->pins.RW = 0;
            this->bus_cycle.done = 0;
            break;
        case 1:
            this->pins.Addr = this->bus_cycle.addr & 0xFFFFFE;
            break;
        case 2:
            this->pins.AS = 1;
            this->pins.RW = 1;
            break;
        case 3: {
            u32 d;
            if (this->state == M68kS_write8)
                if (this->bus_cycle.addr & 1) this->pins.D[this->bus_cycle.addr & 1] = this->bus_cycle.data;
            else {
                this->pins.D[0] = (this->bus_cycle.data >> 8) & 0xFF;
                this->pins.D[1] = this->bus_cycle.data & 0xFF;
            }

            // technically this happens at rising edge of next cycle, buuuut, we're doing it here
            if (this->state == M68kS_write8) {
                ADDRAND1;
            }
            else
                this->pins.LDS = this->pins.UDS = 1;
            break; }
        case 4:
            // This is annoying
            if (!this->pins.DTACK) // insert wait state
                this->bus_cycle.TCU--;
        case 5:
        case 6:
            break;
        case 7: // latch data and de-assert pins
            this->pins.DTACK = 0;
            this->pins.AS = 0;
            this->pins.Addr = 0;
            this->pins.LDS = this->pins.UDS = 0;
            this->bus_cycle.done = 1;
            this->bus_cycle.TCU = -1;
            break;
    }
    this->bus_cycle.TCU++;
}



static void M68k_decode(struct M68k* this)
{
    u32 IRD = this->regs.IRD;

}

void M68k_cycle(struct M68k* this)
{
    if (this->state == M68kS_decode) {
        M68k_decode(this);
    }
    if (this->state == M68kS_exec) {
        this->instruction.func(this);
        // Todo handle finishing bus cycles etc.

    }
    if (this->state < M68kS_decode) {
        this->bus_cycle.func(this);
    }
}

#define MF(x) &M68K_ins_##x

void M68k_reset(struct M68k* this)
{
    this->state = M68kS_exec;
    this->instruction.done = 0;
    this->instruction.func = MF(RESET);
}


