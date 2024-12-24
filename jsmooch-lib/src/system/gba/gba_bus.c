//
// Created by . on 12/4/24.
//

#include "gba_bus.h"
#include "helpers/multisize_memaccess.c"

static u32 busrd_invalid(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    printf("\nREAD UNKNOWN ADDR:%08x sz:%d", addr, sz);
    dbg.var++;
    //if (dbg.var > 6) dbg_break("too many bad reads", this->clock.master_cycle_count);
    return 0;
}

static void buswr_invalid(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val) {
    printf("\nWRITE UNKNOWN ADDR:%08x sz:%d DATA:%08x", addr, sz, val);
    dbg.var++;
    //if (dbg.var > 6) dbg_break("too many bad reads", this->clock.master_cycle_count);
}

static u32 busrd_bios(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    if (addr < 0x40000) return cR[sz](this->BIOS.data, addr);
    return busrd_invalid(this, addr, sz, access, has_effect);
}

static u32 busrd_WRAM_slow(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    if (addr < 0x02040000)
        return cR[sz](this->WRAM_slow, addr - 0x02000000);
    return busrd_invalid(this, addr, sz, access, has_effect);
}

static u32 busrd_WRAM_fast(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    if (addr < 0x03008000)
        return cR[sz](this->WRAM_fast, addr - 0x03000000);

    return busrd_invalid(this, addr, sz, access, has_effect);
}

static void buswr_WRAM_slow(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val) {
    if (addr < 0x02040000)
        return cW[sz](this->WRAM_slow, addr - 0x02000000, val);

    buswr_invalid(this, addr, sz, access, val);
}

static void buswr_WRAM_fast(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val) {
    if (addr < 0x03008000) {
        return cW[sz](this->WRAM_fast, addr - 0x03000000, val);
    }
    buswr_invalid(this, addr, sz, access, val);
}

static u32 busrd_IO(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    if (addr < 0x4000060) return GBA_PPU_mainbus_read_IO(this, addr, sz, access, has_effect);
    switch(addr) {
        case 0x04000130: // buttons!!!
            return GBA_get_controller_state(this->controller.pio);
        case 0x04000208:
            return this->io.IME;
    }
    return busrd_invalid(this, addr, sz, access, has_effect);
}

void GBA_eval_irqs(struct GBA *this)
{
    this->cpu.regs.IRQ_line = (!!(this->io.IE & this->io.IF & 0x3FFF)) & this->io.IME;
    /*

  Bit   Expl.
  0     LCD V-Blank                    (0=Disable)
  1     LCD H-Blank                    (etc.)
  2     LCD V-Counter Match            (etc.)
  3     Timer 0 Overflow               (etc.)
  4     Timer 1 Overflow               (etc.)
  5     Timer 2 Overflow               (etc.)
  6     Timer 3 Overflow               (etc.)
  7     Serial Communication           (etc.)
  8     DMA 0                          (etc.)
  9     DMA 1                          (etc.)
  10    DMA 2                          (etc.)
  11    DMA 3                          (etc.)
  12    Keypad                         (etc.)
  13    Game Pak (external IRQ source) (etc.)
  14-15 Not used */
}

static void buswr_IO(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val) {
    if (addr < 0x4000060) return GBA_PPU_mainbus_write_IO(this, addr, sz, access, val);

    u32 mask = sz == 4 ? 0xFFFFFFFF : (sz == 2 ? 0xFFFF : 0xFF);
    switch(addr) {
        case 0x04000200: // IE
            this->io.IE = (this->io.IE & ~mask) | (val & mask);
            GBA_eval_irqs(this);
            return;
        case 0x04000202: // IF
            val &= mask;
            val ^= 0xFFFFFFFF;
            this->io.IF &= val;
            GBA_eval_irqs(this);
            return;
        case 0x04000208:
            this->io.IME = val & 1;
            return;
    }
    buswr_invalid(this, addr, sz, access, val);
}

void GBA_bus_init(struct GBA *this)
{
    for (u32 i = 0; i < 16; i++) {
        this->mem.read[i] = &busrd_invalid;
        this->mem.write[i] = &buswr_invalid;
    }
    this->mem.read[0x0] = &busrd_bios;
    this->mem.read[0x2] = &busrd_WRAM_slow;
    this->mem.read[0x3] = &busrd_WRAM_fast;
    this->mem.read[0x4] = &busrd_IO;
    this->mem.read[0x5] = &GBA_PPU_mainbus_read_palette;
    this->mem.read[0x6] = &GBA_PPU_mainbus_read_VRAM;
    this->mem.read[0x7] = &GBA_PPU_mainbus_read_OAM;
    this->mem.read[0x8] = &GBA_cart_read_wait0;
    this->mem.read[0x9] = &GBA_cart_read_wait0;
    this->mem.read[0xA] = &GBA_cart_read_wait1;
    this->mem.read[0xB] = &GBA_cart_read_wait1;
    this->mem.read[0xC] = &GBA_cart_read_wait2;
    this->mem.read[0xD] = &GBA_cart_read_wait2;
    this->mem.read[0xE] = &GBA_cart_read_sram;

    this->mem.write[0x2] = &buswr_WRAM_slow;
    this->mem.write[0x3] = &buswr_WRAM_fast;
    this->mem.write[0x4] = &buswr_IO;
    this->mem.write[0x5] = &GBA_PPU_mainbus_write_palette;
    this->mem.write[0x6] = &GBA_PPU_mainbus_write_VRAM;
    this->mem.write[0x7] = &GBA_PPU_mainbus_write_OAM;
    this->mem.write[0xE] = &GBA_cart_write_sram;
}

static u32 GBA_mainbus_IO_read(struct GBA *this, u32 addr, u32 sz, u32 has_effect)
{
    printf("\nUnknown IO read addr:%08x sz:%d", addr, sz);
    return 0;
}

static void GBA_mainbus_IO_write(struct GBA *this, u32 addr, u32 sz, u32 val)
{
    printf("\nUnknown IO write addr:%08x sz:%d val:%08x", addr, sz, val);
}


u32 GBA_mainbus_read(void *ptr, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    if (sz == 4) addr &= ~3;
    if (sz == 2) addr &= ~1;
    struct GBA *this = (struct GBA *)ptr;
    this->cpu.cycles_executed++;
    if (addr < 0x10000000) return this->mem.read[(addr >> 24) & 15](this, addr, sz, access, has_effect);

    return busrd_invalid(this, addr, sz, access, has_effect);
}

u32 GBA_mainbus_fetchins(void *ptr, u32 addr, u32 sz, u32 access)
{
    struct GBA *this = (struct GBA*)ptr;
    this->cpu.cycles_executed++;
    return GBA_mainbus_read(ptr, addr, sz, access, 1);
}

void GBA_mainbus_write(void *ptr, u32 addr, u32 sz, u32 access, u32 val)
{
    if (sz == 4) addr &= ~3;
    if (sz == 2) addr &= ~1;
    struct GBA *this = (struct GBA *)ptr;
    this->cpu.cycles_executed++;

    if (addr < 0x10000000) {
        //printf("\nWRITE addr:%08x sz:%d val:%08x", addr, sz, val);
        return this->mem.write[(addr >> 24) & 15](this, addr, sz, access, val);
    }

    buswr_invalid(this, addr, sz, access, val);
}
