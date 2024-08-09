//
// Created by Dave on 2/6/2024.
//

#include "mmc3b.h"

//
// Created by Dave on 2/6/2024.
//

#include "stdlib.h"
#include "stdio.h"

#include "no_mapper.h"

#include "../nes.h"
#include "../nes_ppu.h"
#include "../nes_cpu.h"

#include "helpers/debugger/debugger.h"

#define MTHIS struct NES_mapper_MMC3b* this = (struct NES_mapper_MMC3b*)mapper->ptr
#define NTHIS struct NES_mapper_MMC3b* this = (struct NES_mapper_MMC3b*)nes->bus.ptr

void NM_MMC3b_remap(struct NES_mapper_MMC3b* this, u32 boot);
void NM_MMC3b_set_mirroring(struct NES_mapper_MMC3b* this);
void NM_MMC3b_a12_watch(struct NES* nes, u32 addr);


void MMC3b_map_init(struct MMC3b_map* this)
{
    this->addr = this->offset = 0;
    this->ROM = this->RAM = 0;
    this->data = NULL;
}

void MMC3b_map_set(struct MMC3b_map* this, u32 addr, u32 offset, u32 ROM, u32 RAM)
{
    this->addr = addr;
    this->offset = offset;
    this->ROM = ROM;
    this->RAM = RAM;
}

void MMC3b_map_write(struct MMC3b_map* this, u32 addr, u32 val)
{
    if (this->ROM) return;
    this->data[(addr - this->addr) + this->offset] = (u8)val;
}

u32 MMC3b_map_read(struct MMC3b_map* this, u32 addr, u32 val)
{
    return (u32)(this->data[(addr - this->addr) + this->offset]);
}

u32 NM_MMC3b_CPU_read(struct NES* nes, u32 addr, u32 val, u32 has_effect)
{
    NTHIS;
    if (addr < 0x2000)
        return this->CPU_RAM[addr & 0x7FF];
    if (addr < 0x4000)
        return NES_bus_PPU_read_regs(nes, addr, val, has_effect);
    if (addr < 0x4020)
        return NES_bus_CPU_read_reg(nes, addr, val, has_effect);
    if (addr >= 0x6000)
        return MMC3b_map_read(&this->PRG_map[(addr - 0x6000) >> 13], addr, val);
    return val;
}

void NM_MMC3b_CPU_write(struct NES* nes, u32 addr, u32 val)
{
    NTHIS;
    if (addr < 0x2000) {
        this->CPU_RAM[addr & 0x7FF] = (u8)val;
        return;
    }
    // 0x2000-0x3FFF mirrored PPU registers
    if (addr <= 0x3FFF) {
        NES_bus_PPU_write_regs(nes, addr, val);
        return;
    }
    // 0x4000-0x401F CPU registers
    if (addr < 0x4020) {
        NES_bus_CPU_write_reg(nes, addr, val);
        return;
    }

    // MMC3 map
    // 0x4020-0x5FFF nothing
    if (addr < 0x6000) return;

    if (addr < 0x8000) {
        MMC3b_map_write(&this->PRG_map[(addr - 0x6000) >> 13], addr, val);
        return;
    }

    switch(addr & 0xE001) {
        case 0x8000: // Bank select
            this->regs.bank_select = (val & 7);
            this->status.PRG_mode = (val & 0x40) >> 6;
            this->status.CHR_mode = (val & 0x80) >> 7;
            break;
        case 0x8001: // Bank data
            this->regs.bank[this->regs.bank_select] = val;
            NM_MMC3b_remap(this, 0);
            break;
        case 0xA000:
            this->ppu_mirror_mode = val & 1 ? PPUM_Horizontal : PPUM_Vertical;
            NM_MMC3b_set_mirroring(this);
            break;
        case 0xA001:
            break;
        case 0xC000:
            this->regs.rC000 = val;
            break;
        case 0xC001:
            this->irq_counter = 0;
            this->irq_reload = true;
            break;
        case 0xE000:
            this->irq_enable = 0;
            r2A03_notify_IRQ(&nes->cpu, 0, 1);
            break;
        case 0xE001:
            this->irq_enable = 1;
            break;
    }
    
}

u32 NM_MMC3b_PPU_read_effect(struct NES* nes, u32 addr) {
    NTHIS;
    NM_MMC3b_a12_watch(nes, addr);
    if (addr < 0x2000) {
        if (this->has_chr_ram)
            return ((u8 *) this->CHR_RAM.ptr)[addr];
        return MMC3b_map_read(&this->CHR_map[addr >> 10], addr, 0);
    }
    return this->CIRAM[this->ppu_mirror(addr)];
}

u32 NM_MMC3b_PPU_read_noeffect(struct NES* nes, u32 addr)
{
    NTHIS;
    if (addr < 0x2000) {
        if (this->has_chr_ram)
            return ((u8 *)this->CHR_RAM.ptr)[addr];
        return MMC3b_map_read(&this->CHR_map[addr >> 10], addr, 0);
    }
    return this->CIRAM[this->ppu_mirror(addr)];
}

void NM_MMC3b_PPU_write(struct NES* nes, u32 addr, u32 val)
{
    NTHIS;
    NM_MMC3b_a12_watch(nes, addr);
    if (addr < 0x2000) {
        if (this->has_chr_ram)
            ((u8*)this->CHR_RAM.ptr)[addr] = val;
        return;
    }// can't write ROM
    this->CIRAM[this->ppu_mirror(addr)] = val;
}


void NM_MMC3b_reset(struct NES* nes)
{
    NTHIS;
    NM_MMC3b_remap(this, 1);
}

void NM_MMC3b_set_mirroring(struct NES_mapper_MMC3b* this)
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

void MMC3b_set_PRG_ROM(struct NES_mapper_MMC3b* this, u32 addr, u32 bank_num) {
    u32 b = (addr - 0x6000) >> 13;
    this->PRG_map[b].addr = addr;
    this->PRG_map[b].offset = (bank_num % this->num_PRG_banks) * 0x2000;
}

void MMC3b_set_CHR_ROM_1k(struct NES_mapper_MMC3b* this, u32 b, u32 bank_num)
{
    this->CHR_map[b].offset = (bank_num % this->num_CHR_banks) * 0x0400;
}

void NM_MMC3b_remap(struct NES_mapper_MMC3b* this, u32 boot) {
    if (boot) {
        for (u32 i = 1; i < 5; i++) {
            this->PRG_map[i].data = (u8 *) this->PRG_ROM.ptr;
            this->PRG_map[i].ROM = true;
            this->PRG_map[i].RAM = false;
        }

        for (u32 i = 0; i < 8; i++) {
            this->CHR_map[i].data = (u8 *) this->CHR_ROM.ptr;
            this->CHR_map[i].addr = 0x400 * i;
            this->CHR_map[i].ROM = true;
            this->CHR_map[i].RAM = false;
        }

        MMC3b_map_set(&this->PRG_map[0], 0x6000, 0,false, true);
        this->PRG_map[0].data = (u8 *)this->PRG_RAM.ptr;
        this->PRG_map[0].RAM = true;
        this->PRG_map[0].ROM = false;
        MMC3b_set_PRG_ROM(this, 0xE000, this->num_PRG_banks-1);
    }

    if (this->status.PRG_mode == 0) {
        // 0 = 8000-9FFF swappable,
        //     C000-DFFF fixed to second-last bank
        // 1 = 8000-9FFF fixed to second-last bank
        //     C000-DFFF swappable
        MMC3b_set_PRG_ROM(this, 0x8000, this->regs.bank[6]);
        MMC3b_set_PRG_ROM(this, 0xC000, this->num_PRG_banks-2);
    }
    else {
        MMC3b_set_PRG_ROM(this, 0x8000, this->num_PRG_banks-2);
        MMC3b_set_PRG_ROM(this, 0xC000, this->regs.bank[6]);
    }

    MMC3b_set_PRG_ROM(this, 0xA000, this->regs.bank[7]);

    if (this->status.CHR_mode == 0) {
        MMC3b_set_CHR_ROM_1k(this, 0, this->regs.bank[0] & 0xFE);
        MMC3b_set_CHR_ROM_1k(this, 1, this->regs.bank[0] | 0x01);
        MMC3b_set_CHR_ROM_1k(this, 2, this->regs.bank[1] & 0xFE);
        MMC3b_set_CHR_ROM_1k(this, 3, this->regs.bank[1] | 0x01);
        MMC3b_set_CHR_ROM_1k(this, 4, this->regs.bank[2]);
        MMC3b_set_CHR_ROM_1k(this, 5, this->regs.bank[3]);
        MMC3b_set_CHR_ROM_1k(this, 6, this->regs.bank[4]);
        MMC3b_set_CHR_ROM_1k(this, 7, this->regs.bank[5]);
    }
    else {
        // 1KB CHR banks at 0, 2KB CHR banks at 1000
        MMC3b_set_CHR_ROM_1k(this, 0, this->regs.bank[2]);
        MMC3b_set_CHR_ROM_1k(this, 1, this->regs.bank[3]);
        MMC3b_set_CHR_ROM_1k(this, 2, this->regs.bank[4]);
        MMC3b_set_CHR_ROM_1k(this, 3, this->regs.bank[5]);
        MMC3b_set_CHR_ROM_1k(this, 4, this->regs.bank[0] & 0xFE);
        MMC3b_set_CHR_ROM_1k(this, 5, this->regs.bank[0] | 0x01);
        MMC3b_set_CHR_ROM_1k(this, 6, this->regs.bank[1] & 0xFE);
        MMC3b_set_CHR_ROM_1k(this, 7, this->regs.bank[1] | 0x01);
    }
    debugger_interface_dirty_mem(this->bus->dbgr, NESMEM_CPUBUS, 0x8000, 0xFFFF);
}

void NM_MMC3b_set_cart(struct NES* nes, struct NES_cart* cart)
{
    NTHIS;
    buf_copy(&this->CHR_ROM, &cart->CHR_ROM);
    buf_copy(&this->PRG_ROM, &cart->PRG_ROM);

    this->prg_ram_size = cart->header.prg_ram_size;
    buf_allocate(&this->PRG_RAM, this->prg_ram_size);

    buf_allocate(&this->CHR_RAM, cart->header.chr_ram_size);
    this->has_chr_ram = cart->header.chr_ram_size > 0;

    this->ppu_mirror_mode = cart->header.mirroring;
    this->num_PRG_banks = this->PRG_ROM.size / 8192;
    this->num_CHR_banks = this->CHR_ROM.size / 1024;

    NM_MMC3b_remap(this, 1);

    NM_MMC3b_set_mirroring(this);
}

void NM_MMC3b_a12_watch(struct NES* nes, u32 addr) // MMC3 only
{
    NTHIS;

    if (a12_watcher_update(&this->a12_watcher, addr) == A12_RISE) {
        if ((this->irq_counter == 0) || (this->irq_reload)) {
            this->irq_counter = (i32)this->regs.rC000;
        } else {
            this->irq_counter--;
            if (this->irq_counter < 0) this->irq_counter = 0;
        }

        if ((this->irq_counter == 0) && this->irq_enable) {
            r2A03_notify_IRQ(&nes->cpu, 1, 1);
        }
        this->irq_reload = false;
    }
}

void NM_MMC3b_cycle(struct NES* nes) // VRC only
{}

void NES_mapper_MMC3b_init(struct NES_mapper* mapper, struct NES* nes)
{
    mapper->ptr = (void *)malloc(sizeof(struct NES_mapper_MMC3b));
    mapper->which = NESM_MMC3b;

    mapper->CPU_read = &NM_MMC3b_CPU_read;
    mapper->CPU_write = &NM_MMC3b_CPU_write;
    mapper->PPU_read_effect = &NM_MMC3b_PPU_read_effect;
    mapper->PPU_read_noeffect = &NM_MMC3b_PPU_read_noeffect;
    mapper->PPU_write = &NM_MMC3b_PPU_write;
    mapper->a12_watch = &NM_MMC3b_a12_watch;
    mapper->reset = &NM_MMC3b_reset;
    mapper->set_cart = &NM_MMC3b_set_cart;
    mapper->cycle = &NM_MMC3b_cycle;
    MTHIS;

    this->bus = nes;
    a12_watcher_init(&this->a12_watcher, &nes->clock);
    buf_init(&this->PRG_ROM);
    buf_init(&this->CHR_ROM);
    buf_init(&this->CHR_RAM);
    buf_init(&this->PRG_RAM);

    this->prg_ram_size = 0;
    this->irq_enable = this->irq_reload = 0;
    this->irq_counter = 0;

    this->regs.rC000 = this->regs.bank_select = 0;
    for (u32 i = 0; i < 8; i++) {
        this->regs.bank[i] = 0;
        if (i < 5) MMC3b_map_init(&this->PRG_map[i]);
        MMC3b_map_init(&this->CHR_map[i]);
    }

    this->status.ROM_bank_mode = 0;
    this->status.CHR_mode = 0;
    this->status.PRG_mode = 0;

    this->num_PRG_banks = this->num_CHR_banks = 0;

    this->ppu_mirror = &NES_mirror_ppu_horizontal;
    this->ppu_mirror_mode = PPUM_Horizontal;
}

void NES_mapper_MMC3b_delete(struct NES_mapper* mapper)
{
    MTHIS;
    buf_delete(&this->PRG_ROM);
    buf_delete(&this->CHR_ROM);
    buf_delete(&this->PRG_RAM);
    buf_delete(&this->CHR_RAM);
}
