//
// Created by Dave on 2/9/2024.
//


#include "stdio.h"
#include "string.h"

#include "sms_gg_mapper_sega.h"
#include "sms_gg.h"

void SMSGG_mapper_sega_init(struct SMSGG_mapper_sega* this, enum jsm_systems variant)
{
    buf_init(&this->ROM);
    buf_init(&this->BIOS);

    this->enable_RAM = 1;
    u32 is_SMS = (variant == SYS_SMS1) || (variant == SYS_SMS2);
    this->enable_bios = is_SMS ? 1 : 0;
    this->enable_cart = !this->enable_bios;

    this->cart = (struct SMSGG_cart) {
            .ram_80_enabled = 0,
            .ram_C0_enabled = 0,
            .num_ROM_banks = 0,
            .rom_shift = 0,
            .rom_bank_upper = 0,
            .rom_00_bank = 0,
            .rom_40_bank = 0x4000,
            .rom_80_bank = 0x8000
    };

    this->bios = (struct SMSGG_bios) {
        .rom_00_bank = 0,
        .rom_40_bank = 0x4000,
        .rom_80_bank = 0x8000,
        .num_banks = 1
    };
}

void SMSGG_mapper_sega_delete(struct SMSGG_mapper_sega* this)
{
    buf_delete(&this->ROM);
    buf_delete(&this->BIOS);
}

void SMSGG_mapper_sega_reset(struct SMSGG_mapper_sega* this)
{
    this->bios.rom_00_bank = 0;
    this->bios.rom_40_bank = 0x4000;
    this->bios.rom_80_bank = 0x8000;
    this->cart.rom_00_bank = 0;
    this->cart.rom_40_bank = 0x4000;
    this->cart.rom_80_bank = 0x8000;
    this->cart.ram_80_enabled = 0;
    this->cart.ram_C0_enabled = false;
    //this->enable_cart = (this->variant == SYS_GG) ? 1 : 0;
    this->enable_cart = 0;
    this->enable_bios = 1;
    memset(this->RAM, 0, 16384);
}

void SMSGG_mapper_sega_set_BIOS(struct SMSGG_mapper_sega* this, u32 to)
{
    this->enable_bios = to;
    if (this->BIOS.size == 0) this->enable_bios = 0;
}

u32 SMSGG_mapper_sega_bios_read(struct SMSGG_mapper_sega* this, u32 addr, u32 val) {
    if (this->BIOS.ptr == NULL) return val;
    if (addr < 0x400) return ((u8*)this->BIOS.ptr)[addr];
    if (addr < 0x4000) return ((u8*)this->BIOS.ptr)[this->bios.rom_00_bank | (addr & 0x3FFF)];
    if (addr < 0x8000) return ((u8*)this->BIOS.ptr)[this->bios.rom_40_bank | (addr & 0x3FFF)];
    if (addr < 0xC000) return ((u8*)this->BIOS.ptr)[this->bios.rom_80_bank | (addr & 0x3FFF)];
    return val;
}

u32 SMSGG_mapper_sega_ram_read(struct SMSGG_mapper_sega* this, u32 addr, u32 val)
{
    return this->RAM[addr & 0x1FFF];
}

u32 SMSGG_mapper_sega_cart_read(struct SMSGG_mapper_sega* this,u32 addr, u32 val)
{
    // DBG VER
    i32 r = -1;
    if (addr < 0x400) r = ((u8*)this->ROM.ptr)[addr];
    else if (addr < 0x4000) r = ((u8*)this->ROM.ptr)[this->cart.rom_00_bank | (addr & 0x3FFF)];
    else if (addr < 0x8000) r = ((u8*)this->ROM.ptr)[this->cart.rom_40_bank | (addr & 0x3FFF)];
    else if (addr < 0xC000) r = ((u8*)this->ROM.ptr)[this->cart.rom_80_bank | (addr & 0x3FFF)];
    if (r != -1) {
        //console.log('ROM READ', hex4(addr), hex2(r))
        //if (addr === 0x7FEF) dbg.break();
        return (u32)r;
    }

    // REAL VER
    /*if (addr < 0x400) return this->ROM[addr];
    if (addr < 0x4000) return this->ROM[this->cart.rom_00_bank | (addr & 0x3FFF)];
    if (addr < 0x8000) return this->ROM[this->cart.rom_40_bank | (addr & 0x3FFF)];
    if (addr < 0xC000) return this->ROM[this->cart.rom_80_bank | (addr & 0x3FFF)];*/
    return val;
    
}

u32 SMSGG_bus_read(struct SMSGG* bus, u32 addr, u32 val, u32 has_effect)
{
    struct SMSGG_mapper_sega* this = &bus->mapper;
    addr &= 0xFFFF;
    if ((addr >= 0xC000) && (this->enable_RAM)) val = SMSGG_mapper_sega_ram_read(this, addr, val);
    if ((this->variant != SYS_GG) && this->enable_bios) val = SMSGG_mapper_sega_bios_read(this, addr, val);
    if (this->enable_cart) val = SMSGG_mapper_sega_cart_read(this, addr, val);
    return val;
}

void SMSGG_mapper_sega_bios_write(struct SMSGG_mapper_sega* this, u32 addr, u32 val) {
    switch (addr) {
        case 0xFFFD: // ROM 0x0400-0x3FFF
            this->bios.rom_00_bank = (val % this->bios.num_banks) << 14;
            return;
        case 0xFFFE: // ROM 0x4000-0x7FFF
            this->bios.rom_40_bank = (val % this->bios.num_banks) << 14;
            return;
        case 0xFFFF: // ROM 0x8000-0xBFFF
            this->bios.rom_80_bank = (val % this->bios.num_banks) << 14;
            return;
    }
}

void SMSGG_mapper_sega_cart_write(struct SMSGG_mapper_sega* this, u32 addr, u32 val)
{
    switch(addr) {
        case 0xFFFC: // various stuff
            //this->cart.ram_
            this->cart.ram_C0_enabled = (val & 0x10) >> 4;
            this->cart.ram_80_enabled = (val & 0x08) >> 3;
            return;
        case 0xFFFD: // ROM 0x0400-0x3FFF
            //console.log('ROM CART PAGE0', val % this->cart.num_ROM_banks);
            this->cart.rom_00_bank = (val % this->cart.num_ROM_banks) << 14;
            return;
        case 0xFFFE: // ROM 0x4000-0x7FFF
            this->cart.rom_40_bank = (val % this->cart.num_ROM_banks) << 14;
            return;
        case 0xFFFF: // ROM 0x8000-0xBFFF
            //console.log('CART PAGE', this->cpu.trace_cycles, val % this->cart.num_ROM_banks, hex4((val % this->cart.num_ROM_banks) << 14));
            //if (val === 15) dbg.break();
            this->cart.rom_80_bank = (val % this->cart.num_ROM_banks) << 14;
            return;
    }
}

void SMSGG_mapper_sega_ram_write(struct SMSGG_mapper_sega* this, u32 addr, u32 val)
{
    this->RAM[addr & 0x1FFF] = (u8)val;
}

void SMSGG_bus_write(struct SMSGG* bus, u32 addr, u32 val)
{
    struct SMSGG_mapper_sega* this = &bus->mapper;
    addr &= 0xFFFF;
    if ((addr >= 0xC000) && this->enable_RAM) SMSGG_mapper_sega_ram_write(this, addr, val);
    if ((this->variant != SYS_GG) && this->enable_bios) SMSGG_mapper_sega_bios_write(this, addr, val);
    if ((this->variant == SYS_GG) && this->enable_bios && (addr < 0x400)) SMSGG_mapper_sega_bios_write(this, addr, val);
    if ((this->variant != SYS_GG) && this->enable_cart) SMSGG_mapper_sega_cart_write(this, addr, val);
    if ((this->variant == SYS_GG) && ((addr >= 0x400) || (!this->enable_bios))) SMSGG_mapper_sega_cart_write(this, addr, val);
}

void SMSGG_mapper_load_BIOS_from_RAM(struct SMSGG_mapper_sega* this, struct buf *BIOS)
{
    buf_copy(&this->BIOS, BIOS);
    this->enable_bios = 1;
    this->bios.num_banks = this->BIOS.size >> 14;
    if (this->bios.num_banks == 0) this->bios.num_banks = 1;
}

void SMSGG_mapper_load_ROM_from_RAM(struct SMSGG_mapper_sega* this, struct buf* inbuf)
{
    buf_copy(&this->ROM, inbuf);
    this->cart.num_ROM_banks = this->ROM.size / 0x4000;
    if (this->cart.num_ROM_banks == 0) this->cart.num_ROM_banks = 1;
    printf("\nLoaded ROM of size %ud banks %d", this->ROM.size, this->cart.num_ROM_banks);
}