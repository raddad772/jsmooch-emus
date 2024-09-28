//
// Created by Dave on 2/7/2024.
//

#include "stdlib.h"
#include "stdio.h"

#include "dxrom.h"

#include "../nes.h"
#include "../nes_ppu.h"
#include "../nes_cpu.h"
#include "helpers/debugger/debugger.h"

#define MTHIS struct NES_mapper_DXROM* this = (struct NES_mapper_DXROM*)mapper->ptr
#define NTHIS struct NES_mapper_DXROM* this = (struct NES_mapper_DXROM*)nes->bus.ptr

u32 NM_DXROM_CPU_read(struct NES* nes, u32 addr, u32 val, u32 has_effect)
{
    NTHIS;
    addr &= 0xFFFF;
    if (addr < 0x2000)
        return this->CPU_RAM[addr & 0x7FF];
    if (addr < 0x3FFF)
        return NES_bus_PPU_read_regs(nes, addr, val, has_effect);
    if (addr < 0x4020)
        return NES_bus_CPU_read_reg(nes, addr, val, has_effect);
    if (addr < 0x8000)
        return val;
    return ((u8*)this->PRG_ROM.ptr)[this->regs.prg_banks[(addr - 0x8000) >> 13] + (addr & 0x1FFF)];
}

void NM_DXROM_set_CHR_ROM_2k(struct NES_mapper_DXROM* this, u32 addr, u32 bank_num)
{
    u32 bn = addr >> 10;
    bank_num = (bank_num % this->num_CHR_banks);
    this->regs.chr_banks[bn] = bank_num * 1024;
    this->regs.chr_banks[bn+1] = (bank_num+1) * 1024;
}

void NM_DXROM_set_CHR_ROM_1k(struct NES_mapper_DXROM* this, u32 addr, u32 bank_num)
{
    u32 bn = addr >> 10;
    bank_num = (bank_num % this->num_CHR_banks);
    this->regs.chr_banks[bn] = (bank_num % this->num_CHR_banks) * 1024;
}

void NM_DXROM_set_PRG_ROM_8k(struct NES_mapper_DXROM* this, u32 addr, u32 bank_num)
{
    u32 bnk = (addr == 0x8000) ? 0 : 1;
    u32 old_val = this->regs.prg_banks[bnk];
    this->regs.prg_banks[bnk] = (bank_num % this->num_PRG_banks) * 8192;
    if (old_val != this->regs.prg_banks[bnk]) {
        debugger_interface_dirty_mem(this->bus->dbg.interface, NESMEM_CPUBUS, addr, addr + 0x1FFF);
    }
}

void NM_DXROM_CPU_write(struct NES* nes, u32 addr, u32 val)
{
    NTHIS;
    if (addr < 0x2000) {
        this->CPU_RAM[addr & 0x7FF] = (u8)val;
        return;
    }
    if (addr < 0x4000)
        return NES_bus_PPU_write_regs(nes, addr, val);
    if (addr < 0x4020)
        return NES_bus_CPU_write_reg(nes, addr, val);

    if ((addr < 0x8000) || (addr > 0xA000)) return;
    switch(addr & 1) {
        case 0: // 8000-FFFF even
            this->regs.select = val & 7;
            return;
        case 1:
            switch(this->regs.select) {
                case 0:
                    return NM_DXROM_set_CHR_ROM_2k(this, 0x0000, val & 0x3E);
                case 1:
                    return NM_DXROM_set_CHR_ROM_2k(this, 0x0800, val & 0x3E);
                case 2:
                    return NM_DXROM_set_CHR_ROM_1k(this, 0x1000, val & 0x3F);
                case 3:
                    return NM_DXROM_set_CHR_ROM_1k(this, 0x1400, val & 0x3F);
                case 4:
                    return NM_DXROM_set_CHR_ROM_1k(this, 0x1800, val & 0x3F);
                case 5:
                    return NM_DXROM_set_CHR_ROM_1k(this, 0x1C00, val & 0x3F);
                case 6:
                    return NM_DXROM_set_PRG_ROM_8k(this, 0x8000, val & 0x0F);
                case 7:
                    return NM_DXROM_set_PRG_ROM_8k(this, 0xA000, val & 0x0F);
            }
            return;
    }
}

u32 NM_DXROM_PPU_read_effect(struct NES* nes, u32 addr)
{
    NTHIS;
    if (addr < 0x2000)
        return ((u8*)this->CHR_ROM.ptr)[this->regs.chr_banks[addr >> 10] + (addr & 0x3FF)];

    return this->CIRAM[this->ppu_mirror(addr)];
}

u32 NM_DXROM_PPU_read_noeffect(struct NES* nes, u32 addr)
{
    return NM_DXROM_PPU_read_effect(nes, addr);
}

void NM_DXROM_PPU_write(struct NES* nes, u32 addr, u32 val)
{
    if (addr < 0x2000) return;
    NTHIS;
    this->CIRAM[this->ppu_mirror(addr)] = (u8) val;
}

void NM_DXROM_remap(struct NES_mapper_DXROM* this, u32 boot)
{
    if (boot) {
        NM_DXROM_set_PRG_ROM_8k(this, 0x8000, 0);
        NM_DXROM_set_PRG_ROM_8k(this, 0xA000, 1);
        // 0xC000-0xFFFF fixed to last two banks
        this->regs.prg_banks[2] = (this->num_PRG_banks - 2) * 8192;
        this->regs.prg_banks[3] = (this->num_PRG_banks - 1) * 8192;

        NM_DXROM_set_CHR_ROM_2k(this, 0x0000, 0);
        NM_DXROM_set_CHR_ROM_2k(this, 0x0800, 2);
        NM_DXROM_set_CHR_ROM_1k(this, 0x1000, 4);
        NM_DXROM_set_CHR_ROM_1k(this, 0x1400, 5);
        NM_DXROM_set_CHR_ROM_1k(this, 0x1800, 6);
        NM_DXROM_set_CHR_ROM_1k(this, 0x1C00, 7);
    }
}

void NM_DXROM_reset(struct NES* nes)
{
    NTHIS;
    NM_DXROM_remap(this, 1);
}

void NM_DXROM_set_mirroring(struct NES_mapper_DXROM* this)
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

void NM_DXROM_set_cart(struct NES* nes, struct NES_cart* cart)
{
    NTHIS;
    buf_copy(&this->CHR_ROM, &cart->CHR_ROM);
    buf_copy(&this->PRG_ROM, &cart->PRG_ROM);

    this->ppu_mirror_mode = cart->header.mirroring;
    NM_DXROM_set_mirroring(this);

    this->num_PRG_banks = this->PRG_ROM.size / 8192;
    this->num_CHR_banks = this->CHR_ROM.size / 1024;
    NM_DXROM_remap(this, 1);
}

void NM_DXROM_a12_watch(struct NES* nes, u32 addr) // MMC3 only
{}

void NM_DXROM_cycle(struct NES* nes) // VRC only
{}

void NES_mapper_DXROM_init(struct NES_mapper* mapper, struct NES* nes)
{
    mapper->ptr = (void *)malloc(sizeof(struct NES_mapper_DXROM));
    mapper->which = NESM_DXROM;

    mapper->CPU_read = &NM_DXROM_CPU_read;
    mapper->CPU_write = &NM_DXROM_CPU_write;
    mapper->PPU_read_effect = &NM_DXROM_PPU_read_effect;
    mapper->PPU_read_noeffect = &NM_DXROM_PPU_read_noeffect;
    mapper->PPU_write = &NM_DXROM_PPU_write;
    mapper->a12_watch = &NM_DXROM_a12_watch;
    mapper->reset = &NM_DXROM_reset;
    mapper->set_cart = &NM_DXROM_set_cart;
    mapper->cycle = &NM_DXROM_cycle;
    MTHIS;

    this->bus = nes;
    a12_watcher_init(&this->a12_watcher, &nes->clock);
    buf_init(&this->PRG_ROM);
    buf_init(&this->CHR_ROM);

    this->ppu_mirror = &NES_mirror_ppu_horizontal;
    this->ppu_mirror_mode = 0;
}

void NES_mapper_DXROM_delete(struct NES_mapper* mapper)
{
    MTHIS;
    buf_delete(&this->PRG_ROM);
    buf_delete(&this->CHR_ROM);
}
