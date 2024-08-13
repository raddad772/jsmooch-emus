#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gb_bus.h"
#include "gb_clock.h"
#include "gb_cpu.h"
#include "mappers/mapper.h"
#include "helpers/debugger/debugger.h"

void GB_bus_init(struct GB_bus* this, struct GB_clock* clock) {
	this->cart = NULL;
	this->mapper = NULL;
	this->ppu = NULL;
	this->cpu = NULL;
	this->BIOS = NULL;
    this->clock = clock;

    cvec_ptr_init(&this->dbg.event.view);
    cvec_ptr_init(&this->dbg.event.VRAM_write);

    u32 i;
    for (i = 0; i < (8192 * 8); i++) {
        this->generic_mapper.WRAM[i] = 0;
    }

    for (i = 0; i < 128; i++) {
        this->generic_mapper.HRAM[i] = 0;
    }

    for (i = 0; i < 16384; i++) {
        this->generic_mapper.VRAM[i] = 0;
    }

    this->generic_mapper.VRAM_bank_offset = 0;
    this->generic_mapper.WRAM_bank_offset = 0x1000;
    this->generic_mapper.BIOS_big = 0;
}

void GB_bus_delete(struct GB_bus *this)
{
    if (this->BIOS != NULL) {
        free(this->BIOS);
        this->BIOS = NULL;
    }
}

void GB_generic_mapper_reset(struct GB_bus *this)
{
    this->generic_mapper.VRAM_bank_offset = 0;
    this->generic_mapper.WRAM_bank_offset = 0x1000;
}

void GB_bus_reset(struct GB_bus *this)
{
    GB_generic_mapper_reset(this);
}

inline u32 GB_bus_PPU_read(struct GB_bus* this, u32 addr)
{
    if ((addr < 0x8000) || (addr >= 0xC000)) {
        printf("WAIT WHAT BAD READ? %d", addr);
        return 0xFF;
    }
    return this->generic_mapper.VRAM[(addr - 0x8000) & 0x3FFF];
}

inline void GB_bus_CPU_write(struct GB_bus* this, u32 addr, u32 val) {
    if ((addr >= 0x4000) && (addr < 0x4020)) {
        printf("W! %d %d", addr, val);
    }
    if ((addr >= 0xE000) && (addr < 0xFE00)) addr -= 0x2000;  // Mirror WRAM

    if ((addr >= 0x8000) && (addr < 0xA000)) { // VRAM
        debugger_report_event(this->dbg.event.view, this->dbg.event.VRAM_write);
        this->generic_mapper.VRAM[(addr & 0x1FFF) + this->generic_mapper.VRAM_bank_offset] = (u8)val;
        return;
    }

    if ((addr >= 0xC000) && (addr < 0xD000)) { // WRAM lo bank
        this->generic_mapper.WRAM[addr & 0xFFF] = (u8)val;
        return;
    }
    if ((addr >= 0xD000) && (addr < 0xE000)) { // WRAM hi bank
        this->generic_mapper.WRAM[(addr & 0xFFF) + this->generic_mapper.WRAM_bank_offset] = val;
        return;
    }
    if ((addr >= 0xFE00) && (addr < 0xFF00)) { // OAM
        this->write_OAM(this, addr, val);
        return;
    }
    if ((addr >= 0xFF00) && (addr < 0xFF80)) {// registers
        this->write_IO(this, addr, val);
        return;
    }
    if ((addr >= 0xFF80) && (addr < 0xFFFF)) { // HRAM always accessible
        this->generic_mapper.HRAM[addr - 0xFF80] = (u8)val;
        return;
    }
    if (addr == 0xFFFF) {  // 0xFFFF register
        this->write_IO(this, addr, val);
        return;
    }
    this->mapper->CPU_write(this->mapper, addr, val);
}

inline u32 GB_bus_CPU_read(struct GB_bus *this, u32 addr, u32 val, u32 has_effect) {
    if ((addr >= 0xE000) && (addr < 0xFE00)) addr -= 0x2000; // Mirror WRAM
    if (this->clock->bootROM_enabled) {
        if (addr < 0x100) {
            return this->BIOS[addr];
        }
        if (this->generic_mapper.BIOS_big && (addr >= 0x200) && (addr < 0x900))
            return this->BIOS[addr - 0x100];
    }

    if ((addr >= 0x8000) && (addr < 0xA000)) { // VRAM, banked
        //if (this->clock.CPU_can_VRAM)
        return this->generic_mapper.VRAM[(addr & 0x1FFF) + this->generic_mapper.VRAM_bank_offset];
        return 0xFF;
    }

    if ((addr >= 0xC000) && (addr < 0xD000)) // WRAM lo bank
        return this->generic_mapper.WRAM[addr & 0xFFF];
    if ((addr >= 0xD000) && (addr < 0xE000)) // WRAM hi bank
        return this->generic_mapper.WRAM[(addr & 0xFFF) + this->generic_mapper.WRAM_bank_offset];
    if ((addr >= 0xFE00) && (addr < 0xFF00)) // OAM
        return this->read_OAM(this, addr);
    if ((addr >= 0xFF00) && (addr < 0xFF80)) // registers
        return this->read_IO(this, addr, val);//, has_effect);
    if ((addr >= 0xFF80) && (addr < 0xFFFF)) // HRAM always accessible
        return this->generic_mapper.HRAM[addr - 0xFF80];
    if (addr == 0xFFFF)
        return this->read_IO(this, 0xFFFF, val);//, has_effect); // 0xFFFF register

    return this->mapper->CPU_read(this->mapper, addr, val, has_effect);
}

void GB_bus_set_cart(struct GB_bus* this, struct GB_cart* cart)
{
    this->cart = cart;
}

void GB_bus_set_BIOS(struct GB_bus* this, u8 *BIOS, u32 sz)
{
    if (this->BIOS != NULL) {
        free(this->BIOS);
        this->BIOS = NULL;
    }
    this->BIOS = malloc(sz);
    memcpy(this->BIOS, BIOS, sz);
    this->generic_mapper.BIOS_big = sz > 256;
}