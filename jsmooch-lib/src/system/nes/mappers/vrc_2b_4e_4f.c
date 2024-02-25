//
// Created by Dave on 2/7/2024.
//

//
// Created by Dave on 2/6/2024.
//

#include "stdlib.h"
#include "stdio.h"

#include "vrc_2b_4e_4f.h"

#include "../nes.h"
#include "../nes_ppu.h"
#include "../nes_cpu.h"

#define MTHIS struct NES_mapper_VRC2B_4E_4F* this = (struct NES_mapper_VRC2B_4E_4F*)mapper->ptr
#define NTHIS struct NES_mapper_VRC2B_4E_4F* this = (struct NES_mapper_VRC2B_4E_4F*)nes->bus.ptr

u32 mess_up_addr(u32 addr) {
    // Thanks Mesen! NESdev is not correct!
    u32 A0 = addr & 0x01;
    u32 A1 = (addr >> 1) & 0x01;

    //VRC4e
    A0 |= (addr >> 2) & 0x01;
    A1 |= (addr >> 3) & 0x01;
    return (addr & 0xFF00) | (A1 << 1) | A0;
}


void NM_VRC2B_4E_4F_set_mirroring(struct NES_mapper_VRC2B_4E_4F* this)
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
u32 NM_VRC2B_4E_4F_CPU_read(struct NES* nes, u32 addr, u32 val, u32 has_effect)
{
    NTHIS;
    if (addr < 0x2000)
        return this->CPU_RAM[addr & 0x7FF];
    if (addr < 0x4000)
        return NES_bus_PPU_read_regs(nes, addr, val, has_effect);
    if (addr < 0x4020)
        return NES_bus_CPU_read_reg(nes, addr, val, has_effect);
    // VRC mapping
    if (addr < 0x6000) return val;
    if (addr < 0x8000) {
        if (this->io.wram_enabled) return ((u8*)this->PRG_RAM.ptr)[addr - 0x6000];
        if (!this->is_vrc4) return (val & 0xFE) | this->io.latch60;
    }
    return MMC3b_map_read(&this->PRG_map[addr >> 13], addr, val);
}

void NM_VRC2B_4E_4F_set_CHR_ROM_1k(struct NES_mapper_VRC2B_4E_4F* this, u32 b, u32 bank_num)
{
    this->CHR_map[b].offset = (bank_num % this->num_CHR_banks) * 0x0400;
}

void NM_VRC2B_4E_4F_set_PRG_ROM(struct NES_mapper_VRC2B_4E_4F* this, u32 addr, u32 bank_num) {
u32 b = addr >> 13;
this->PRG_map[b].addr = addr;
this->PRG_map[b].offset = (bank_num % this->num_PRG_banks) * 0x2000;
}


void NM_VRC2B_4E_4F_remap(struct NES_mapper_VRC2B_4E_4F* this, u32 boot)
{
    if (boot) {
        // VRC2 maps last c0 and e0 to last 2 banks
        this->io.vrc4.banks_swapped = 0;
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

        this->PRG_map[0x6000 >> 13].ROM = false;
        this->PRG_map[0x6000 >> 13].RAM = true;
        this->PRG_map[0x6000 >> 13].offset = 0;
        this->PRG_map[0x6000 >> 13].data = (u8*)this->PRG_RAM.ptr;
        NM_VRC2B_4E_4F_set_PRG_ROM(this, 0xE000, this->num_PRG_banks - 1);
    }

    if (this->is_vrc4 && this->io.vrc4.banks_swapped) {
        NM_VRC2B_4E_4F_set_PRG_ROM(this, 0x8000, this->num_PRG_banks - 2);
        NM_VRC2B_4E_4F_set_PRG_ROM(this, 0xA000, this->io.cpu.banka0);
        NM_VRC2B_4E_4F_set_PRG_ROM(this, 0xC000, this->io.cpu.bank80);
    } else {
        // VRC2
        NM_VRC2B_4E_4F_set_PRG_ROM(this, 0x8000, this->io.cpu.bank80);
        NM_VRC2B_4E_4F_set_PRG_ROM(this, 0xA000, this->io.cpu.banka0);
        NM_VRC2B_4E_4F_set_PRG_ROM(this, 0xC000, this->num_PRG_banks - 2);
    }
    NM_VRC2B_4E_4F_set_CHR_ROM_1k(this, 0, this->io.ppu.banks[0]);
    NM_VRC2B_4E_4F_set_CHR_ROM_1k(this, 1, this->io.ppu.banks[1]);
    NM_VRC2B_4E_4F_set_CHR_ROM_1k(this, 2, this->io.ppu.banks[2]);
    NM_VRC2B_4E_4F_set_CHR_ROM_1k(this, 3, this->io.ppu.banks[3]);
    NM_VRC2B_4E_4F_set_CHR_ROM_1k(this, 4, this->io.ppu.banks[4]);
    NM_VRC2B_4E_4F_set_CHR_ROM_1k(this, 5, this->io.ppu.banks[5]);
    NM_VRC2B_4E_4F_set_CHR_ROM_1k(this, 6, this->io.ppu.banks[6]);
    NM_VRC2B_4E_4F_set_CHR_ROM_1k(this, 7, this->io.ppu.banks[7]);
}

void NM_VRC2B_4E_4F_set_ppu_lo(struct NES_mapper_VRC2B_4E_4F* this, u32 bank, u32 val)
{
    u32 b = this->io.ppu.banks[bank];
    if (this->is_vrc2a) b <<= 1;

    b = (b & 0x1F0) | (val & 0x0F);

    if (this->is_vrc2a) b >>= 1;
    this->io.ppu.banks[bank] = b;
    NM_VRC2B_4E_4F_remap(this, 0);
    
}

void NM_VRC2B_4E_4F_set_ppu_hi(struct NES_mapper_VRC2B_4E_4F* this, u32 bank, u32 val)
{
    u32 b = this->io.ppu.banks[bank];
    if (this->is_vrc2a) b <<= 1;
    if (this->is_vrc4) val = (val & 0x1F) << 4;
    else val = (val & 0x0F) << 4;
    b = (b & 0x0F) | val;
    if (this->is_vrc2a) b >>= 1;
    this->io.ppu.banks[bank] = b;
    NM_VRC2B_4E_4F_remap(this, 0);
}
void NM_VRC2B_4E_4F_CPU_write(struct NES* nes, u32 addr, u32 val)
{
    NTHIS;
    // Conventional CPU map
    if (addr < 0x2000) { // 0x0000-0x1FFF 4 mirrors of 2KB banks
        this->CPU_RAM[addr & 0x7FF] = (u8)val;
        return;
    }
    if (addr < 0x4000) // 0x2000-0x3FFF mirrored PPU registers
        return NES_bus_PPU_write_regs(nes, addr, val);
    if (addr < 0x4020)
        return NES_bus_CPU_write_reg(nes, addr, val);
    if (addr < 0x6000) return;
    if (addr < 0x8000) {
        if (this->io.wram_enabled) {
            ((u8*)this->PRG_RAM.ptr)[addr - 0x6000] = (u8)val;
        }
        else if (!this->is_vrc4) {
            this->io.latch60 = val & 1;
        }
        return;
    }

    addr = mess_up_addr(addr) & 0xF00F;
    switch(addr) {
        case 0x8000:
        case 0x8001:
        case 0x8002:
        case 0x8003:
        case 0x8004:
        case 0x8005:
        case 0x8006:
            this->io.cpu.bank80 = val & 0x1F;
            NM_VRC2B_4E_4F_remap(this, 0);
            return;
        case 0x9000:
        case 0x9001:
        case 0x9002:
        case 0x9003:
        case 0x9004:
        case 0x9005:
        case 0x9006:
            if (this->is_vrc4 && (addr == 0x9002)) {
                // wram control
                this->io.wram_enabled = (val & 1);
                // swap mode
                this->io.vrc4.banks_swapped = (val & 2) >> 1;
            }
            if (this->is_vrc4 && (addr == 0x9003)) {
                return;
            }

            if (this->is_vrc4) val &= 3;
            else val &= 1;
            // 0 vertical 1 horizontal 2 a 3 b

            switch(val) {
                case 0: this->ppu_mirror_mode = PPUM_Vertical; break;
                case 1: this->ppu_mirror_mode = PPUM_Horizontal; break;
                case 2: this->ppu_mirror_mode = PPUM_ScreenAOnly; break;
                case 3: this->ppu_mirror_mode = PPUM_ScreenBOnly; break;
            }
            NM_VRC2B_4E_4F_set_mirroring(this);
            return;
        case 0xA000:
        case 0xA001:
        case 0xA002:
        case 0xA003:
        case 0xA004:
        case 0xA005:
        case 0xA006:
            this->io.cpu.banka0 = val & 0x1F;
            NM_VRC2B_4E_4F_remap(this, 0);
            return;
        case 0xF000: // IRQ latch low 4
            if (!this->is_vrc4) return;
            this->irq.reload = (this->irq.reload & 0xF0) | (val & 0x0F);
            return;
        case 0xF001: // IRQ latch hi 4
            if (!this->is_vrc4) return;
            this->irq.reload = (this->irq.reload & 0x0F) | ((val & 0x0F) << 4);
            return;
        case 0xF002: // IRQ control
            if (!this->is_vrc4) return;
            this->irq.prescaler = 341;
            if (val & 2) this->irq.counter = this->irq.reload;
            this->irq.cycle_mode = (val & 4) >> 2;
            this->irq.enable = (val & 2) >> 1;
            this->irq.enable_after_ack = (val & 1);
            return;
        case 0xF003: // IRQ ack
            if (!this->is_vrc4) return;
            this->irq.enable = this->irq.enable_after_ack;
            r2A03_notify_IRQ(&nes->cpu, 0, 1);
            return;
    }
    // Thanks Messen! NESdev wiki was wrong here...
    if ((addr >= 0xB000) && (addr <= 0xE006)) {
        u32 rn = ((((addr >> 12) & 0x07) - 3) << 1) + ((addr >> 1) & 0x01);
        u32 lowBits = (addr & 0x01) == 0;
        if (lowBits) {
            //The other reg contains the low 4 bits
            NM_VRC2B_4E_4F_set_ppu_lo(this, rn, val);
        } else {
            //One reg contains the high 5 bits
            NM_VRC2B_4E_4F_set_ppu_hi(this, rn, val);
        }
    }
}

u32 NM_VRC2B_4E_4F_PPU_read_effect(struct NES* nes, u32 addr)
{
    NTHIS;
    if (addr < 0x2000)
        return MMC3b_map_read(&this->CHR_map[addr >> 10], addr, 0);
    return this->CIRAM[this->ppu_mirror(addr)];
}

u32 NM_VRC2B_4E_4F_PPU_read_noeffect(struct NES* nes, u32 addr)
{
    return NM_VRC2B_4E_4F_PPU_read_effect(nes, addr);
}

void NM_VRC2B_4E_4F_PPU_write(struct NES* nes, u32 addr, u32 val)
{
    if (addr < 0x2000) return;
    NTHIS;
    this->CIRAM[this->ppu_mirror(addr)] = val;
}

void NM_VRC2B_4E_4F_reset(struct NES* nes)
{
    // Nothing to do on reset
    NTHIS;
    NM_VRC2B_4E_4F_remap(this, 1);
}

void NM_VRC2B_4E_4F_set_cart(struct NES* nes, struct NES_cart* cart)
{
    NTHIS;
    buf_copy(&this->CHR_ROM, &cart->CHR_ROM);
    buf_copy(&this->PRG_ROM, &cart->PRG_ROM);

    this->prg_ram_size = cart->header.prg_ram_size;
    buf_allocate(&this->PRG_RAM, this->prg_ram_size);

    this->num_PRG_banks = this->PRG_ROM.size / 8192;
    this->num_CHR_banks = this->CHR_ROM.size / 1024;

    this->ppu_mirror_mode = cart->header.mirroring;
    NM_VRC2B_4E_4F_set_mirroring(this);
    NM_VRC2B_4E_4F_remap(this, 1);
}

void NM_VRC2B_4E_4F_a12_watch(struct NES* nes, u32 addr) // MMC3 only
{}

void NM_VRC2B_4E_4F_cycle_vrc2(struct NES* nes) // VRC only
{}

void NM_VRC2B_4E_4F_cycle_vrc4(struct NES* nes)
{
    NTHIS;
    if (this->irq.enable) {
        this->irq.prescaler -= 3;
        if (this->irq.cycle_mode || ((this->irq.prescaler <= 0) && !this->irq.cycle_mode)) {
            if (this->irq.counter == 0xFF) {
                this->irq.counter = this->irq.reload;
                r2A03_notify_IRQ(&nes->cpu, 1, 1);
            } else {
                this->irq.counter++;
            }
            this->irq.prescaler += 341;
        }
    }
}

void NES_mapper_VRC2B_4E_4F_init(struct NES_mapper* mapper, struct NES* nes)
{
    mapper->ptr = (void *)malloc(sizeof(struct NES_mapper_VRC2B_4E_4F));
    mapper->which = NESM_VRC4E_4F;

    mapper->CPU_read = &NM_VRC2B_4E_4F_CPU_read;
    mapper->CPU_write = &NM_VRC2B_4E_4F_CPU_write;
    mapper->PPU_read_effect = &NM_VRC2B_4E_4F_PPU_read_effect;
    mapper->PPU_read_noeffect = &NM_VRC2B_4E_4F_PPU_read_noeffect;
    mapper->PPU_write = &NM_VRC2B_4E_4F_PPU_write;
    mapper->a12_watch = &NM_VRC2B_4E_4F_a12_watch;
    mapper->reset = &NM_VRC2B_4E_4F_reset;
    mapper->set_cart = &NM_VRC2B_4E_4F_set_cart;
    MTHIS;
    this->is_vrc2a = false;
    this->is_vrc4 = true;

    if (this->is_vrc4) mapper->cycle = &NM_VRC2B_4E_4F_cycle_vrc4;
    else mapper->cycle = &NM_VRC2B_4E_4F_cycle_vrc2;

    a12_watcher_init(&this->a12_watcher, &nes->clock);
    buf_init(&this->PRG_ROM);
    buf_init(&this->CHR_ROM);
    buf_init(&this->PRG_RAM);

    for (u32 i = 0; i < 8; i++) {
        MMC3b_map_init(&this->PRG_map[i]);
        MMC3b_map_init(&this->CHR_map[i]);
        this->io.ppu.banks[i] = 0;
    }

    this->irq.cycle_mode = 0;
    this->irq.enable = 0;
    this->irq.enable_after_ack = 0;
    this->irq.reload = 0;
    this->irq.prescaler = 341;
    this->irq.counter = 0;

    this->ppu_mirror = &NES_mirror_ppu_horizontal;
    this->ppu_mirror_mode = 0;
    this->io.vrc4.banks_swapped = 0;
    this->io.wram_enabled = 0;
}

void NES_mapper_VRC2B_4E_4F_delete(struct NES_mapper* mapper)
{
    MTHIS;
    buf_delete(&this->PRG_ROM);
    buf_delete(&this->CHR_ROM);
    buf_delete(&this->PRG_RAM);
}
