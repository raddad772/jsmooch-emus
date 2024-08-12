//
// Created by Dave on 2/7/2024.
//

#include "stdlib.h"
#include "stdio.h"

#include "mmc1.h"

#include "../nes.h"
#include "helpers/debugger/debugger.h"

#define MTHIS struct NES_mapper_MMC1* this = (struct NES_mapper_MMC1*)mapper->ptr
#define NTHIS struct NES_mapper_MMC1* this = (struct NES_mapper_MMC1*)nes->bus.ptr

void NM_MMC1_set_mirroring(struct NES_mapper_MMC1* this)
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

u32 NM_MMC1_CPU_read(struct NES* nes, u32 addr, u32 val, u32 has_effect)
{
    NTHIS;
    if (addr < 0x2000)
        return this->CPU_RAM[addr & 0x7FF];
    if (addr < 0x4000)
        return NES_bus_PPU_read_regs(nes, addr, val, has_effect);
    if (addr < 0x4020)
        return NES_bus_CPU_read_reg(nes, addr, val, has_effect);
    if (addr < 0x6000) return val;
    if (addr < 0x8000) {
        if (this->has_prg_ram) return ((u8*)this->PRG_RAM.ptr)[addr - 0x6000];
        return val;
    }
    return MMC3b_map_read(&this->PRG_map[(addr - 0x8000) >> 14], addr, val);
}

void NM_MMC1_set_PRG_ROM(struct NES_mapper_MMC1* this, u32 addr, u32 bank_num)
{
    bank_num %= this->num_PRG_banks;
    u32 b = (addr - 0x8000) >> 14;
    u32 old_offset = this->PRG_map[b].offset;
    this->PRG_map[b].addr = addr;
    this->PRG_map[b].offset = (bank_num % this->num_PRG_banks) * 0x4000;
    if (this->PRG_map[b].offset != old_offset) {
        debugger_interface_dirty_mem(this->bus->dbg.interface, NESMEM_CPUBUS, addr, addr + 0x3FFF);
    }
}

void NM_MMC1_set_CHR_ROM(struct NES_mapper_MMC1* this, u32 addr, u32 bank_num)
{
    bank_num %= this->num_CHR_banks;
    u32 b = (addr >> 12);
    this->CHR_map[b].addr = addr;
    this->CHR_map[b].offset = (bank_num % this->num_CHR_banks) * 0x1000;
}

void NM_MMC1_remap(struct NES_mapper_MMC1* this, u32 boot, u32 may_contain_PRG)
{
    if (boot) {
        for (u32 i = 0; i < 2; i++) {
            this->PRG_map[i].data = (u8*)this->PRG_ROM.ptr;
            this->PRG_map[i].ROM = true;
            this->PRG_map[i].RAM = false;
            this->PRG_map[i].addr = (0x8000) + (i * 0x4000);

            this->CHR_map[i].data = (u8*)this->CHR_ROM.ptr;
            this->CHR_map[i].addr = 0x1000 * i;
            this->CHR_map[i].ROM = true;
            this->CHR_map[i].RAM = false;
        }
    }

    switch(this->io.prg_bank_mode) {
        case 0:
        case 1: // 32k at 0x8000
            NM_MMC1_set_PRG_ROM(this, 0x8000, this->io.bank & 0xFE);
            NM_MMC1_set_PRG_ROM(this, 0xC000, this->io.bank | 1);
            break;
        case 2:
            NM_MMC1_set_PRG_ROM(this, 0x8000, 0);
            NM_MMC1_set_PRG_ROM(this, 0xC000, this->io.bank);
            break;
        case 3:
            NM_MMC1_set_PRG_ROM(this, 0x8000, this->io.bank);
            NM_MMC1_set_PRG_ROM(this, 0xC000, this->num_PRG_banks - 1);
            break;
    }

    if (!this->has_chr_ram) {
        switch (this->io.chr_bank_mode) {
            case 0: // 8kb switch at a time
                NM_MMC1_set_CHR_ROM(this, 0x0000, this->io.ppu_bank00 & 0xFE);
                NM_MMC1_set_CHR_ROM(this, 0x1000, this->io.ppu_bank00 | 1);
                break;
            case 1: // 4kb switch at a time
                NM_MMC1_set_CHR_ROM(this, 0x0000, this->io.ppu_bank00);
                NM_MMC1_set_CHR_ROM(this, 0x1000, this->io.ppu_bank10);
                break;
        }
    }
}

void NM_MMC1_CPU_write(struct NES* nes, u32 addr, u32 val)
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
    if (addr < 0x6000) return;
    if (addr < 0x8000) {
        if (this->has_prg_ram) ((u8*)this->PRG_RAM.ptr)[addr - 0x6000] = (u8)val;
        return;
    }

    // MMC1 stuff

    // 8000-FFFF register
    // Clear with bit 7 set
    if (val & 0x80) {
        // Writes ctrl | 0x0C
        this->io.shift_pos = 4;
        this->io.shift_value = this->io.ctrl | 0x0C;
        this->last_cycle_write = nes->clock.cpu_master_clock;
        addr = 0x8000;
    } else {
        if (nes->clock.cpu_master_clock == (this->last_cycle_write + nes->clock.timing.cpu_divisor)) {
            // writes on consecutive cycles fail
            this->last_cycle_write = nes->clock.cpu_master_clock;
            return;
        } else {
            this->io.shift_value = (this->io.shift_value >> 1) | ((val & 1) << 4);
        }
    }

    this->last_cycle_write = nes->clock.cpu_master_clock;

    this->io.shift_pos++;
    if (this->io.shift_pos == 5) {
        addr &= 0xE000;
        val = this->io.shift_value;
        switch (addr) {
            case 0x8000: // control register
                this->io.ctrl = this->io.shift_value;
                switch(val & 3) {
                    case 0: this->ppu_mirror_mode = PPUM_ScreenAOnly; break;
                    case 1: this->ppu_mirror_mode = PPUM_ScreenBOnly; break;
                    case 2: this->ppu_mirror_mode = PPUM_Vertical; break;
                    case 3: this->ppu_mirror_mode = PPUM_Horizontal; break;
                }
                NM_MMC1_set_mirroring(this);
                this->io.prg_bank_mode = (val >> 2) & 3;
                this->io.chr_bank_mode = (val >> 4) & 1;
                NM_MMC1_remap(this, 0, 1);
                break;
            case 0xA000: // CHR bank 0x0000
                this->io.ppu_bank00 = this->io.shift_value;
                NM_MMC1_remap(this, 0, 0);
                break;
            case 0xC000: // CHR bank 1
                this->io.ppu_bank10 = this->io.shift_value;
                NM_MMC1_remap(this, 0, 0);
                break;
            case 0xE000: // PRG bank
                this->io.bank = this->io.shift_value & 0x0F;
                if (this->io.shift_value & 0x10)
                    printf("WARNING50!");
                NM_MMC1_remap(this, 0, 1);
                break;
        }
        this->io.shift_value = 0;
        this->io.shift_pos = 0;
    }
}

u32 NM_MMC1_PPU_read_effect(struct NES* nes, u32 addr)
{
    NTHIS;
    addr &= 0x3FFF;
    if (addr < 0x2000) {
        if (this->has_chr_ram) return ((u8*)this->CHR_RAM.ptr)[addr];
        return MMC3b_map_read(&this->CHR_map[addr >> 12], addr, 0);
    }
    return this->CIRAM[this->ppu_mirror(addr)];
}

u32 NM_MMC1_PPU_read_noeffect(struct NES* nes, u32 addr)
{
    return NM_MMC1_PPU_read_effect(nes, addr);
}

void NM_MMC1_PPU_write(struct NES* nes, u32 addr, u32 val)
{
    NTHIS;
    addr &= 0x3FFF;
    if (addr < 0x2000) {
        if (this->has_chr_ram) ((u8*)this->CHR_RAM.ptr)[addr] = (u8)val;
        return;
    }
    this->CIRAM[this->ppu_mirror(addr)] = (u8)val;
}

void NM_MMC1_reset(struct NES* nes)
{
    NTHIS;
    this->io.prg_bank_mode = 3;
    NM_MMC1_remap(this, 1, 1);
}

void NM_MMC1_set_cart(struct NES* nes, struct NES_cart* cart)
{
    NTHIS;
    buf_copy(&this->CHR_ROM, &cart->CHR_ROM);
    buf_copy(&this->PRG_ROM, &cart->PRG_ROM);

    this->prg_ram_size = cart->header.prg_ram_size;
    this->chr_ram_size = cart->header.chr_ram_size;
    this->has_chr_ram = this->chr_ram_size > 0;
    this->has_prg_ram = this->prg_ram_size > 0;

    buf_allocate(&this->PRG_RAM, this->prg_ram_size);
    buf_allocate(&this->CHR_RAM, this->chr_ram_size);

    this->ppu_mirror_mode = cart->header.mirroring;
    NM_MMC1_set_mirroring(this);

    this->num_PRG_banks = this->PRG_ROM.size / 16384;
    this->num_CHR_banks = this->CHR_ROM.size / 4096;
    NM_MMC1_reset(nes);
}

void NM_MMC1_a12_watch(struct NES* nes, u32 addr) // MMC3 only
{}

void NM_MMC1_cycle(struct NES* nes) // VRC only
{}

void NES_mapper_MMC1_init(struct NES_mapper* mapper, struct NES* nes)
{
    mapper->ptr = (void *)malloc(sizeof(struct NES_mapper_MMC1));
    mapper->which = NESM_MMC1;

    mapper->CPU_read = &NM_MMC1_CPU_read;
    mapper->CPU_write = &NM_MMC1_CPU_write;
    mapper->PPU_read_effect = &NM_MMC1_PPU_read_effect;
    mapper->PPU_read_noeffect = &NM_MMC1_PPU_read_noeffect;
    mapper->PPU_write = &NM_MMC1_PPU_write;
    mapper->a12_watch = &NM_MMC1_a12_watch;
    mapper->reset = &NM_MMC1_reset;
    mapper->set_cart = &NM_MMC1_set_cart;
    mapper->cycle = &NM_MMC1_cycle;
    MTHIS;

    this->bus = nes;
    a12_watcher_init(&this->a12_watcher, &nes->clock);
    buf_init(&this->PRG_ROM);
    buf_init(&this->CHR_ROM);
    buf_init(&this->PRG_RAM);
    buf_init(&this->CHR_RAM);

    MMC3b_map_init(&this->PRG_map[0]);
    MMC3b_map_init(&this->PRG_map[1]);
    MMC3b_map_init(&this->CHR_map[0]);
    MMC3b_map_init(&this->CHR_map[1]);

    this->io.shift_pos = 0;
    this->io.shift_value = 0;
    this->io.ppu_bank00 = this->io.ppu_bank10 = 0;
    this->io.bank = this->io.ctrl = 0;
    this->io.prg_bank_mode = this->io.chr_bank_mode = 0;

    this->ppu_mirror = &NES_mirror_ppu_horizontal;
    this->ppu_mirror_mode = 0;
}

void NES_mapper_MMC1_delete(struct NES_mapper* mapper)
{
    MTHIS;
    buf_delete(&this->PRG_ROM);
    buf_delete(&this->CHR_ROM);
    buf_delete(&this->PRG_RAM);
    buf_delete(&this->CHR_RAM);
}
