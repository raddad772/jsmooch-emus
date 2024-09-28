//
// Created by . on 9/17/24.
//

#include <stdlib.h>
#include <stdio.h>

#include "sunsoft_57.h"
#include "helpers/debugger/debugger.h"
#include "../nes.h"

#define MTHIS struct NES_mapper_sunsoft_57* this = (struct NES_mapper_sunsoft_57*)mapper->ptr
#define NTHIS struct NES_mapper_sunsoft_57* this = (struct NES_mapper_sunsoft_57*)nes->bus.ptr


static void set_mirroring(struct NES_mapper_sunsoft_57 *this)
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

static void set_CHR_ROM_1k(struct NES_mapper_sunsoft_57* this, u32 b, u32 bank_num)
{
    this->CHR_map[b].offset = (bank_num % this->num_CHR_banks) * 0x0400;
}


static void set_PRG_ROM(struct NES_mapper_sunsoft_57* this, u32 addr, u32 bank_num) {
    u32 b = addr >> 13;
    this->PRG_map[b].addr = addr;
    u32 old_offset = this->PRG_map[b].offset;
    this->PRG_map[b].offset = (bank_num % this->num_PRG_banks) * 0x2000;
    if (old_offset != this->PRG_map[b].offset) {
        debugger_interface_dirty_mem(this->bus->dbg.interface, NESMEM_CPUBUS, addr, addr + 0x1FFF);
    }
}

static void remap(struct NES_mapper_sunsoft_57* this, u32 boot)
{
    if (boot) {
        // Initialize maps to sane, if not CORRECT, values
        for (u32 i = 0; i < 8; i++) {
            this->PRG_map[i].data = (u8*)this->PRG_ROM.ptr;
            this->PRG_map[i].addr = 0x2000 * i;
            this->PRG_map[i].ROM = true;
            this->PRG_map[i].RAM = false;

            this->CHR_map[i].data = (u8*)this->CHR_ROM.ptr;
            this->CHR_map[i].addr = 0x400 * i;
            this->CHR_map[i].ROM = true;
            this->CHR_map[i].RAM = false;
        }

        // fixed to last bank
        this->io.cpu.banks[0] = 0;
        this->io.cpu.banks[1] = 1;
        this->io.cpu.banks[2] = 2;
        this->io.cpu.banks[3] = this->num_PRG_banks - 1;
    }

    set_PRG_ROM(this, 0x6000, this->io.cpu.banks[0]);
    set_PRG_ROM(this, 0x8000, this->io.cpu.banks[1]);
    set_PRG_ROM(this, 0xA000, this->io.cpu.banks[2]);
    set_PRG_ROM(this, 0xE000, this->io.cpu.banks[3]);

    set_CHR_ROM_1k(this, 0, this->io.ppu.banks[0]);
    set_CHR_ROM_1k(this, 1, this->io.ppu.banks[1]);
    set_CHR_ROM_1k(this, 2, this->io.ppu.banks[2]);
    set_CHR_ROM_1k(this, 3, this->io.ppu.banks[3]);
    set_CHR_ROM_1k(this, 4, this->io.ppu.banks[4]);
    set_CHR_ROM_1k(this, 5, this->io.ppu.banks[5]);
    set_CHR_ROM_1k(this, 6, this->io.ppu.banks[6]);
    set_CHR_ROM_1k(this, 7, this->io.ppu.banks[7]);
}


static u32 CPU_read(struct NES* nes, u32 addr, u32 val, u32 has_effect)
{
    NTHIS;
    if (addr < 0x2000)
        return this->CPU_RAM[addr & 0x7FF];
    if (addr < 0x4000)
        return NES_bus_PPU_read_regs(nes, addr, val, has_effect);
    if (addr < 0x4020)
        return NES_bus_CPU_read_reg(nes, addr, val, has_effect);

    if (addr < 0x6000) return val;
    if (this->io.wram_enabled && this->io.wram_mapped && (addr < 0x8000))
        return ((u8*)this->PRG_RAM.ptr)[(addr - 0x6000) % this->prg_ram_size];

    return MMC3b_map_read(&this->PRG_map[addr >> 13], addr, val);
}

static void reset(struct NES* nes)
{
    NTHIS;
    remap(this, 1);
}

static void write_reg(struct NES* nes, u32 val)
{
    NTHIS;
    u32 reg = this->io.reg;
    if (reg < 8) {
        // CHR bank
        set_CHR_ROM_1k(this, reg, val);
        return;
    }
    switch(reg) {
        case 8:
            this->io.cpu.banks[0] = val & this->bank_mask;
            this->io.wram_mapped = (val >> 6) & 1;
            this->io.wram_enabled = (val >> 7) & 1;
            remap(this, 0);
            break;
        case 9:
        case 10:
        case 11:
            this->io.cpu.banks[reg - 8] = val & this->bank_mask;
            remap(this, 0);
            break;
        case 12:
            // 0 = vertical
            // 1 = horizontal
            // 2 = 1screen a
            // 3 = 1screen b
            switch(val & 3) {
                case 0:
                    this->ppu_mirror_mode = PPUM_Vertical;
                    break;
                case 1:
                    this->ppu_mirror_mode = PPUM_Horizontal;
                    break;
                case 2:
                    this->ppu_mirror_mode = PPUM_ScreenAOnly;
                    break;
                case 3:
                    this->ppu_mirror_mode = PPUM_ScreenBOnly;
                    break;
            }
            set_mirroring(this);
            break;
        case 13: // IRQ control
            this->irq.enabled = val & 1;
            this->irq.counter_enabled = (val >> 7) & 1;
            this->irq.output = 0;
            r2A03_notify_IRQ(&nes->cpu, 0, 1);
            break;
        case 14: // IRQ counter low
            this->irq.counter = (this->irq.counter & 0xFF00) | val;
            break;
        case 15:
            this->irq.counter = (this->irq.counter & 0xFF) | (val << 8);
            break;

    }
}

static void write_audio_reg(struct NES_mapper_sunsoft_57* this, u32 val)
{
    printf("\nwrite A%d: %02x", this->io.audio.reg, val);
}

static void CPU_write(struct NES* nes, u32 addr, u32 val) {
    NTHIS;
    // Conventional CPU map
    if (addr < 0x2000) { // 0x0000-0x1FFF 4 mirrors of 2KB banks
        this->CPU_RAM[addr & 0x7FF] = (u8) val;
        return;
    }
    if (addr < 0x4000) // 0x2000-0x3FFF mirrored PPU registers
        return NES_bus_PPU_write_regs(nes, addr, val);
    if (addr < 0x4020)
        return NES_bus_CPU_write_reg(nes, addr, val);
    if (addr < 0x6000) return;
    if (this->io.wram_enabled && this->io.wram_mapped && (addr < 0x8000)) {
        ((u8*)this->PRG_RAM.ptr)[(addr - 0x6000) % this->prg_ram_size] = val;
        return;
    }
    // The rest is ROM
    switch(addr & 0xE000) {
        case 0x8000:
            this->io.reg = val & 15;
            break;
        case 0xA000:
            write_reg(nes, val);
            break;
        case 0xC000:
            this->io.audio.reg = val & 15;
            this->io.audio.write_enabled = (val >> 4) == 0;
            break;
        case 0xE000:
            write_audio_reg(this, val);
            break;

    }
}

static void set_cart(struct NES* nes, struct NES_cart* cart)
{
    NTHIS;
    buf_copy(&this->CHR_ROM, &cart->CHR_ROM);
    buf_copy(&this->PRG_ROM, &cart->PRG_ROM);

    this->prg_ram_size = cart->header.prg_ram_size;
    buf_allocate(&this->PRG_RAM, this->prg_ram_size);

    this->num_PRG_banks = this->PRG_ROM.size / 8192;
    this->num_CHR_banks = this->CHR_ROM.size / 1024;

    this->ppu_mirror_mode = cart->header.mirroring;
    set_mirroring(this);
    remap(this, 1);
}

static void a12_watch(struct NES* nes, u32 addr) // MMC3 only
{}

static void cpu_cycle(struct NES* nes)
{
    NTHIS;
    if (this->irq.enabled && this->irq.counter_enabled) {
        this->irq.counter = (this->irq.counter - 1) & 0xFFFF;
        if (this->irq.counter == 0xFFFF) {
            this->irq.output = 1;
            r2A03_notify_IRQ(&nes->cpu, 1, 1);
        }
    }
}

static u32 PPU_read_effect(struct NES* nes, u32 addr)
{
    NTHIS;
    if (addr < 0x2000)
        return MMC3b_map_read(&this->CHR_map[addr >> 10], addr, 0);
    return this->CIRAM[this->ppu_mirror(addr)];
}

static u32 PPU_read_noeffect(struct NES* nes, u32 addr)
{
    return PPU_read_effect(nes, addr);
}

static void PPU_write(struct NES* nes, u32 addr, u32 val)
{
    if (addr < 0x2000) return;
    NTHIS;
    this->CIRAM[this->ppu_mirror(addr)] = val;
}


void NES_mapper_sunsoft_57_init(struct NES_mapper* mapper, struct NES* nes, enum NES_mapper_sunsoft_57_variants variant)
{
    mapper->ptr = (void *)malloc(sizeof(struct NES_mapper_sunsoft_57));
    mapper->CPU_read = &CPU_read;
    mapper->CPU_write = &CPU_write;
    mapper->PPU_read_effect = &PPU_read_effect;
    mapper->PPU_read_noeffect = &PPU_read_noeffect;
    mapper->PPU_write = &PPU_write;
    mapper->a12_watch = &a12_watch;
    mapper->reset = &reset;
    mapper->set_cart = &set_cart;
    MTHIS;

    this->variant = variant;
    this->bus = nes;
    switch(variant) {
        case NES_mapper_sunsoft_5a:
            this->has_sound = 0;
            this->bank_mask = 0x1F;
            break;
        case NES_mapper_sunsoft_5b:
            this->has_sound = 1;
            this->bank_mask = 0x1F;
            break;
        case NES_mapper_sunsoft_fme_7:
            this->has_sound = 0;
            this->bank_mask = 0x3F;
            break;
    }

    mapper->cycle = &cpu_cycle;

    a12_watcher_init(&this->a12_watcher, &nes->clock);
    buf_init(&this->PRG_ROM);
    buf_init(&this->CHR_ROM);
    buf_init(&this->PRG_RAM);

    for (u32 i = 0; i < 8; i++) {
        MMC3b_map_init(&this->PRG_map[i]);
        MMC3b_map_init(&this->CHR_map[i]);
        this->io.ppu.banks[i] = 0;
    }

    this->ppu_mirror = &NES_mirror_ppu_horizontal;
    this->ppu_mirror_mode = 0;
    this->io.wram_enabled = this->io.wram_mapped = 0;
    this->irq.enabled = 0;
}


void NES_mapper_sunsoft_57_delete(struct NES_mapper* mapper)
{
    MTHIS;
    buf_delete(&this->PRG_ROM);
    buf_delete(&this->CHR_ROM);
    buf_delete(&this->PRG_RAM);
}

