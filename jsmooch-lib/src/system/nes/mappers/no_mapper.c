//
// Created by Dave on 2/6/2024.
//

#include "stdlib.h"
#include "stdio.h"

#include "no_mapper.h"

#include "../nes.h"
#include "../nes_ppu.h"
#include "../nes_cpu.h"

#define MTHIS struct NES_mapper_none* this = (struct NES_mapper_none*)mapper->ptr
#define NTHIS struct NES_mapper_none* this = (struct NES_mapper_none*)nes->bus.ptr

u32 NM_none_CPU_read(struct NES* nes, u32 addr, u32 val, u32 has_effect)
{
    NTHIS;
    addr &= 0xFFFF;
    if (addr < 0x2000)
        return this->CPU_RAM[addr & 0x7FF];
    if (addr < 0x3FFF)
        return NES_bus_PPU_read_regs(nes, addr, val, has_effect);
    if (addr < 0x4020)
        return NES_bus_CPU_read_reg(nes, addr, val, has_effect);
    if (addr >= 0x8000)
        return ((u8 *)this->PRG_ROM.ptr)[(addr - 0x8000) % this->PRG_ROM.size];
    return val;
}

void NM_none_CPU_write(struct NES* nes, u32 addr, u32 val)
{
    NTHIS;
    addr &= 0xFFFF;
    if (addr < 0x2000) {
        this->CPU_RAM[addr & 0x7FF] = (u8)val;
        return;
    }
    if (addr < 0x3FFF) {
        NES_bus_PPU_write_regs(nes, addr, val);
        return;
    }
    if (addr < 0x4020) {
        NES_bus_CPU_write_reg(nes, addr, val);
        return;
    }
}

u32 NM_none_PPU_read_effect(struct NES* nes, u32 addr)
{
    NTHIS;
    if (addr < 0x2000)
        return ((u8 *)this->CHR_ROM.ptr)[addr];
    return this->CIRAM[this->ppu_mirror(addr)];
    
}

u32 NM_none_PPU_read_noeffect(struct NES* nes, u32 addr)
{
    return NM_none_PPU_read_effect(nes, addr);
}

void NM_none_PPU_write(struct NES* nes, u32 addr, u32 val)
{
    if (addr < 0x2000) return;
    NTHIS;
    this->CIRAM[this->ppu_mirror(addr)] = (u8)val;
}

void NM_none_reset(struct NES* nes)
{
    // Nothing to do on reset
}

void NM_set_mirroring(struct NES_mapper_none* this)
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

void NM_none_set_cart(struct NES* nes, struct NES_cart* cart)
{
    NTHIS;
    buf_copy(&this->CHR_ROM, &cart->CHR_ROM);
    buf_copy(&this->PRG_ROM, &cart->PRG_ROM);

    this->ppu_mirror_mode = cart->header.mirroring;
    NM_set_mirroring(this);
}

void NM_none_a12_watch(struct NES* nes, u32 addr) // MMC3 only
{}

void NM_none_cycle(struct NES* nes) // VRC only
{}

void NES_mapper_none_init(struct NES_mapper* mapper, struct NES* nes)
{
    mapper->ptr = (void *)malloc(sizeof(struct NES_mapper_none));
    mapper->which = NESM_NONE;

    mapper->CPU_read = &NM_none_CPU_read;
    mapper->CPU_write = &NM_none_CPU_write;
    mapper->PPU_read_effect = &NM_none_PPU_read_effect;
    mapper->PPU_read_noeffect = &NM_none_PPU_read_noeffect;
    mapper->PPU_write = &NM_none_PPU_write;
    mapper->a12_watch = &NM_none_a12_watch;
    mapper->reset = &NM_none_reset;
    mapper->set_cart = &NM_none_set_cart;
    mapper->cycle = &NM_none_cycle;
    MTHIS;

    a12_watcher_init(&this->a12_watcher, &nes->clock);
    buf_init(&this->PRG_ROM);
    buf_init(&this->CHR_ROM);

    this->ppu_mirror = &NES_mirror_ppu_horizontal;
    this->ppu_mirror_mode = 0;
}

void NES_mapper_none_delete(struct NES_mapper* mapper)
{
    MTHIS;
    buf_delete(&this->PRG_ROM);
    buf_delete(&this->CHR_ROM);
}
