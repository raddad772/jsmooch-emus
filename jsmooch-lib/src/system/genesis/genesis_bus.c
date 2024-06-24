//
// Created by . on 6/1/24.
//

#include "genesis_bus.h"

#define RETUL16or8(thing) ((UDS && LDS) ? *(u16 *)(thing) : RETUL8(thing))
#define RETUL8(thing) *(((u8 *)(thing)) + (UDS ? 1 : 0))

#define SETUL16or8(thing, tw)   if (UDS && LDS) *(u16 *)(thing) = tw; else SETUL8(thing, tw)
#define SETUL8(thing, tw) *(((u8 *)(thing)) + (UDS ? 1 : 0)) = tw

u16 genesis_bus_read(struct genesis* this, u32 addr, u32 UDS, u32 LDS, u16 old)
{
    u16 v;
    if (addr < 0x400000)
        return genesis_cart_read(&this->cart, addr, UDS, LDS);
    if (addr >= 0xE00000)
        return this->RAM[(addr & 0xFFFF)>>1];

    return old;
}

// LDS = bits 0-7
// UDS = bits 8-15
void genesis_bus_write(struct genesis* this, u32 addr, u32 UDS, u32 LDS, u16 val)
{
    u8 *dst;

    if (addr >= 0xE00000) {
        dst = (u8 *)&this->RAM[(addr & 0xFFFF) >> 1];
        if (UDS) dst[1] = (val & 0xFF00) >> 8;
        if (LDS) dst[0] = (val & 0xFF);
        return;
    }
}

void genesis_cycle_m68k(struct genesis* this)
{
    M68k_cycle(&this->m68k);
    if (this->m68k.pins.AS && (!this->m68k.pins.DTACK) && (this->m68k.pins.UDS || this->m68k.pins.LDS)) {
        if (!this->m68k.pins.RW) { // read
            this->m68k.pins.DTACK = 1;
            genesis_bus_read(this, this->m68k.pins.Addr, this->m68k.pins.UDS, this->m68k.pins.LDS, this->m68k.pins.D);
            // TODO: do read here
        }
        else { // write
            this->m68k.pins.DTACK = 1;
            genesis_bus_write(this, this->m68k.pins.Addr, this->m68k.pins.UDS, this->m68k.pins.LDS, this->m68k.pins.D);
        }
    }
}

void genesis_cycle_z80(struct genesis* this)
{
    Z80_cycle(&this->z80);
}

void genesis_cycle_vdp(struct genesis* this)
{

}
