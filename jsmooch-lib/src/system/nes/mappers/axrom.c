//
// Created by Dave on 2/7/2024.
//

#include "stdlib.h"
#include "stdio.h"

#include "axrom.h"
#include "helpers/debugger/debugger.h"

#include "../nes.h"
#include "../nes_ppu.h"
#include "../nes_cpu.h"

#define MTHIS struct NES_mapper_AXROM* this = (struct NES_mapper_AXROM*)mapper->ptr
#define NTHIS struct NES_mapper_AXROM* this = (struct NES_mapper_AXROM*)nes->bus.ptr

void NM_AXROM_set_mirroring(struct NES_mapper_AXROM* this)
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

u32 NM_AXROM_CPU_read(struct NES* nes, u32 addr, u32 val, u32 has_effect)
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
    return ((u8*)this->PRG_ROM.ptr)[(addr - 0x8000) + this->prg_bank_offset];
}

void NM_AXROM_CPU_write(struct NES* nes, u32 addr, u32 val) {
    NTHIS;
    addr &= 0xFFFF;
    if (addr < 0x2000) {
        this->CPU_RAM[addr & 0x7FF] = (u8) val;
        return;
    }
    if (addr < 0x3FFF)
        return NES_bus_PPU_write_regs(nes, addr, val);
    if (addr < 0x4020)
        return NES_bus_CPU_write_reg(nes, addr, val);

    if (addr < 0x8000) return;


    u32 old_bank_offset = this->prg_bank_offset;
    this->prg_bank_offset = ((val & 15) % this->num_PRG_banks) * 32768;
    if (old_bank_offset != this->prg_bank_offset) {
        debugger_interface_dirty_mem(this->bus->dbg.interface, NESMEM_CPUBUS, 0x8000, 0xFFFF);
    }

    this->ppu_mirror_mode = (val & 0x10) ? PPUM_ScreenBOnly : PPUM_ScreenAOnly;
    NM_AXROM_set_mirroring(this);
}

u32 NM_AXROM_PPU_read_effect(struct NES* nes, u32 addr)
{
    NTHIS;
    if (addr < 0x2000) return this->CHR_RAM[addr];
    return this->CIRAM[this->ppu_mirror(addr)];
}

u32 NM_AXROM_PPU_read_noeffect(struct NES* nes, u32 addr)
{
    return NM_AXROM_PPU_read_effect(nes, addr);
}

void NM_AXROM_PPU_write(struct NES* nes, u32 addr, u32 val)
{
    NTHIS;
    if (addr < 0x2000) this->CHR_RAM[addr] = (u8)val;
    else this->CIRAM[this->ppu_mirror(addr)] = (u8)val;
}

void NM_AXROM_reset(struct NES* nes)
{
    // Nothing to do on reset
    NTHIS;
    this->prg_bank_offset = (this->num_PRG_banks - 1) * 32768;
}

void NM_AXROM_set_cart(struct NES* nes, struct NES_cart* cart)
{
    NTHIS;
    buf_copy(&this->PRG_ROM, &cart->PRG_ROM);
    this->num_PRG_banks = cart->header.prg_rom_size / 32768;

    this->ppu_mirror_mode = cart->header.mirroring;
    NM_AXROM_set_mirroring(this);

    this->prg_bank_offset = 0;
}

void NM_AXROM_a12_watch(struct NES* nes, u32 addr) // MMC3 only
{}

void NM_AXROM_cycle(struct NES* nes) // VRC only
{}

void NES_mapper_AXROM_init(struct NES_mapper* mapper, struct NES* nes)
{
    mapper->ptr = (void *)malloc(sizeof(struct NES_mapper_AXROM));
    mapper->which = NESM_AXROM;

    mapper->CPU_read = &NM_AXROM_CPU_read;
    mapper->CPU_write = &NM_AXROM_CPU_write;
    mapper->PPU_read_effect = &NM_AXROM_PPU_read_effect;
    mapper->PPU_read_noeffect = &NM_AXROM_PPU_read_noeffect;
    mapper->PPU_write = &NM_AXROM_PPU_write;
    mapper->a12_watch = &NM_AXROM_a12_watch;
    mapper->reset = &NM_AXROM_reset;
    mapper->set_cart = &NM_AXROM_set_cart;
    mapper->cycle = &NM_AXROM_cycle;
    MTHIS;

    this->bus = nes;
    a12_watcher_init(&this->a12_watcher, &nes->clock);
    buf_init(&this->PRG_ROM);

    this->ppu_mirror = &NES_mirror_ppu_horizontal;
    this->ppu_mirror_mode = 0;
}

void NES_mapper_AXROM_delete(struct NES_mapper* mapper)
{
    MTHIS;
    buf_delete(&this->PRG_ROM);
}
