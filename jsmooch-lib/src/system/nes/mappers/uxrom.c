//
// Created by Dave on 2/7/2024.
//

#include "stdlib.h"
#include "stdio.h"

#include "uxrom.h"

#include "../nes.h"
#include "../nes_ppu.h"
#include "../nes_cpu.h"
#include "helpers/debugger/debugger.h"

#define MTHIS struct NES_mapper_UXROM* this = (struct NES_mapper_UXROM*)mapper->ptr
#define NTHIS struct NES_mapper_UXROM* this = (struct NES_mapper_UXROM*)nes->bus.ptr

u32 NM_UXROM_CPU_read(struct NES* nes, u32 addr, u32 val, u32 has_effect)
{
    NTHIS;
    addr &= 0xFFFF;
    if (addr < 0x2000)
        return this->CPU_RAM[addr & 0x7FF];
    if (addr < 0x3FFF)
        return NES_bus_PPU_read_regs(nes, addr, val, has_effect);
    if (addr < 0x4020)
        return NES_bus_CPU_read_reg(nes, addr, val, has_effect);
    if (addr < 0x8000) return val;
    if (addr < 0xC000) return ((u8*)this->PRG_ROM.ptr)[(addr - 0x8000) + this->prg_bank_offset];
    return ((u8*)this->PRG_ROM.ptr)[(addr - 0xC000) + this->last_bank_offset];
}

void NM_UXROM_CPU_write(struct NES* nes, u32 addr, u32 val)
{
    NTHIS;
    addr &= 0xFFFF;
    if (addr < 0x2000) {
        this->CPU_RAM[addr & 0x7FF] = (u8)val;
        return;
    }
    if (addr < 0x3FFF)
        return NES_bus_PPU_write_regs(nes, addr, val);
    if (addr < 0x4020)
        return NES_bus_CPU_write_reg(nes, addr, val);

    if (addr < 0x8000) return;
    if (this->is_unrom)
        val &= 7;
    if (this->is_uorom)
        val &= 15;
    u32 old_offset = this->prg_bank_offset;
    this->prg_bank_offset = 16384 * (val % this->num_PRG_banks);
    if (old_offset != this->prg_bank_offset) {
        debugger_interface_dirty_mem(this->bus->dbgr, NESMEM_CPUBUS, 0x8000, 0xBFFF);
    }
}

u32 NM_UXROM_PPU_read_effect(struct NES* nes, u32 addr)
{
    NTHIS;
    if (addr < 0x2000) return this->CHR_RAM[addr];
    return this->CIRAM[this->ppu_mirror(addr)];
}

u32 NM_UXROM_PPU_read_noeffect(struct NES* nes, u32 addr)
{
    return NM_UXROM_PPU_read_effect(nes, addr);
}

void NM_UXROM_PPU_write(struct NES* nes, u32 addr, u32 val)
{
    NTHIS;
    if (addr < 0x2000) this->CHR_RAM[addr] = (u8)val;
    else this->CIRAM[this->ppu_mirror(addr)] = (u8)val;
}

void NM_UXROM_reset(struct NES* nes)
{
    NTHIS;
    this->prg_bank_offset = 0;
}

void NM_UXROM_set_mirroring(struct NES_mapper_UXROM* this)
{
    switch(this->ppu_mirror_mode) {
        case PPUM_Vertical:
            this->ppu_mirror = &NES_mirror_ppu_vertical;
            return;
        case PPUM_Horizontal:
            this->ppu_mirror = &NES_mirror_ppu_horizontal;
            return;
        case PPUM_FourWay:
            this->ppu_mirror = &NES_mirror_ppu_four;
            return;
        case PPUM_ScreenAOnly:
            this->ppu_mirror = &NES_mirror_ppu_Aonly;
            return;
        case PPUM_ScreenBOnly:
            this->ppu_mirror = &NES_mirror_ppu_Bonly;
            return;
    }
}

void NM_UXROM_set_cart(struct NES* nes, struct NES_cart* cart)
{
    NTHIS;
    buf_copy(&this->PRG_ROM, &cart->PRG_ROM);

    this->PRG_ROM_size = cart->header.prg_rom_size;
    this->num_PRG_banks = cart->header.prg_rom_size / 16384;

    this->ppu_mirror_mode = cart->header.mirroring;
    NM_UXROM_set_mirroring(this);

    this->prg_bank_offset = 0;
    this->last_bank_offset = (this->num_PRG_banks - 1) * 16384;
}

void NM_UXROM_a12_watch(struct NES* nes, u32 addr) // MMC3 only
{}

void NM_UXROM_cycle(struct NES* nes) // VRC only
{}

void NES_mapper_UXROM_init(struct NES_mapper* mapper, struct NES* nes)
{
    mapper->ptr = (void *)malloc(sizeof(struct NES_mapper_UXROM));
    mapper->which = NESM_UXROM;

    mapper->CPU_read = &NM_UXROM_CPU_read;
    mapper->CPU_write = &NM_UXROM_CPU_write;
    mapper->PPU_read_effect = &NM_UXROM_PPU_read_effect;
    mapper->PPU_read_noeffect = &NM_UXROM_PPU_read_noeffect;
    mapper->PPU_write = &NM_UXROM_PPU_write;
    mapper->a12_watch = &NM_UXROM_a12_watch;
    mapper->reset = &NM_UXROM_reset;
    mapper->set_cart = &NM_UXROM_set_cart;
    mapper->cycle = &NM_UXROM_cycle;
    MTHIS;

    this->bus = nes;
    a12_watcher_init(&this->a12_watcher, &nes->clock);
    buf_init(&this->PRG_ROM);

    this->is_unrom = this->is_uorom = this->PRG_ROM_size = this->last_bank_offset = 0;

    this->ppu_mirror = &NES_mirror_ppu_horizontal;
    this->ppu_mirror_mode = 0;
}

void NES_mapper_UXROM_delete(struct NES_mapper* mapper)
{
    MTHIS;
    buf_delete(&this->PRG_ROM);
}
