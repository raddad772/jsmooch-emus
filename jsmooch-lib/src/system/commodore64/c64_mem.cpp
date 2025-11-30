//
// Created by . on 11/26/25.
//
#include <cassert>
#include "c64_mem.h"
#include "c64_bus.h"

namespace C64 {

mem::mem(core *parent) : bus(parent)
{
    for (u32 i = 0; i < 256; i++) {
        map[i].empty = true;
        map[i].read = &bad_read_func;
        map[i].write = &bad_write_func;
        map[i].read_ptr = nullptr;
        map[i].write_ptr = nullptr;
    }
    first_map();
}

u8 mem::read_IO(u16 addr, u8 old, bool has_effect) {
    switch (addr & 0xDF00) {
        case 0xD000:
        case 0xD100:
        case 0xD200:
        case 0xD300:
            return bus->vic2.read_IO(addr & 0x3FF, old, has_effect);
        case 0xD400:
        case 0xD500:
        case 0xD600:
        case 0xD700:
            return bus->sid.read_IO(addr, old, has_effect);
        case 0xD800:
        case 0xD900:
        case 0xDA00:
        case 0xDB00:
            return bus->vic2.read_color_ram(addr, old, has_effect);
        case 0xDC00:
            return bus->cia1.read(addr, old, has_effect);
        case 0xDD00:
            return bus->cia2.read(addr, old, has_effect);
        case 0xDE00:
            return bus->read_io1(addr, old, has_effect);
        case 0xDF00:
            return bus->read_io2(addr, old, has_effect);
        default:
            printf("\nSOMEHOW READ UNMAPPED IO %04x", addr);
            return 0;
    }
}

void mem::write_IO(u16 addr, u8 val) {
    switch (addr & 0xDF00) {
        case 0xD000:
        case 0xD100:
        case 0xD200:
        case 0xD300:
            return bus->vic2.write_IO(addr & 0x3FF, val);
        case 0xD400:
        case 0xD500:
        case 0xD600:
        case 0xD700:
            return bus->sid.write_IO(addr, val);
        case 0xD800:
        case 0xD900:
        case 0xDA00:
        case 0xDB00:
            return bus->vic2.write_color_ram(addr, val);
        case 0xDC00: {
            bus->cia1.write(addr, val);
            u8 v = bus->cia1.read_PRA();
            bus->cia1.pins.PRB_in = bus->keyboard.read_cols(v);
            return; }
        case 0xDD00: {
            bus->cia2.write(addr, val);
            return; }
        case 0xDE00:
            return bus->write_io1(addr, val);
        case 0xDF00:
            return bus->write_io2(addr, val);
        default:
            printf("\nSOMEHOW WROTE UNMAPPED IO %04x", addr);
            return;
    }
}

u8 bad_read_func(void *ptr, u16 addr, u8 old, bool has_effect) {
    printf("\nWARN unmapped read to addr %04x", addr);
    return old;
}

void bad_write_func(void *ptr, u16 addr, u8 val) {
    printf("\nWARN unmapped write to addr %04x: %02x", addr, val);
}

void mem::load_KERNAL(buf &b, size_t offset) {
    assert((b.size - offset) >= 0x2000);
    memcpy(KERNAL, static_cast<u8 *>(b.ptr)+offset, 0x2000);
}

void mem::load_BASIC(buf &b, size_t offset) {
    assert((b.size - offset) >= 0x2000);
    memcpy(BASIC, static_cast<u8 *>(b.ptr)+offset, 0x2000);
}

void mem::load_CHARGEN(buf &b, size_t offset) {
    assert((b.size - offset) >= 0x1000);
    memcpy(CHARGEN, static_cast<u8 *>(b.ptr)+offset, 0x1000);
}

u8 mem::read_main_bus(u16 addr, u8 old, bool has_effect) {
    return map[addr >> 8].read(map[addr >> 8].read_ptr, addr, old, has_effect);
}

void mem::write_main_bus(u16 addr, u8 val) {
    map[addr >> 8].write(map[addr >> 8].write_ptr, addr, val);
}

u8 mem::read_cpu_IO(u16 addr, u8 old, bool has_effect) {
    switch(addr) {
        case 0x0000:
            return io.cpu.dir;
        case 0x0001:
            return io.cpu.u;
    }
    assert(1==2);
    return 0xFF;
}

void mem::write_cpu_IO(u16 addr, u8 val) {
    switch(addr) {
        case 0x0000:
            io.cpu.dir = val & 0x3F;
            return;
        case 0x0001:
            io.cpu.u &= ~io.cpu.dir;
            io.cpu.u |=val & io.cpu.dir & 0x3F;
            return;
    }
    assert(1==2);
}

static u8 read_zp(void *ptr, u16 addr, u8 old, bool has_effect) {
    auto *mem = static_cast<C64::mem *>(ptr);
    if (addr > 1) return mem->RAM[addr & 0xFF];
    return mem->read_cpu_IO(addr, old, has_effect);;
}

static void write_zp(void *ptr, u16 addr, u8 val) {
    auto *mem = static_cast<C64::mem *>(ptr);
    mem->RAM[addr & 0xFF] = val;
    if (addr < 2) mem->write_cpu_IO(addr, val);
}

static u8 read_ram(void *ptr, u16 addr, u8 old, bool has_effect) {
    return static_cast<u8 *>(ptr)[addr & 0xFF];
}

static void write_ram(void *ptr, u16 addr, u8 val) {
    static_cast<u8 *>(ptr)[addr & 0xFF] = val;
}

void mem::map_reads(u32 addr_start, u32 addr_end, void *ptr, read_func func) {
    for (u32 addr = addr_start; addr < addr_end; addr += 0x0100) {
        //printf("\nMAP READ %02x", addr);
        auto &blk = map[addr >> 8];
        blk.read_ptr = static_cast<u8 *>(ptr) + addr;
        blk.read = func;
        blk.empty = false;
    }
}

void mem::map_reads_baseptr(u32 addr_start, u32 addr_end, void *ptr, read_func func) {
    for (u32 addr = addr_start; addr < addr_end; addr += 0x0100) {
        //printf("\nMAP READ BASEPTR %02x", addr);
        auto &blk = map[addr >> 8];
        blk.read_ptr = ptr;
        blk.read = func;
        blk.empty = false;
    }
}


void mem::map_writes(u32 addr_start, u32 addr_end, void *ptr, write_func func) {
    for (u32 addr = addr_start; addr < addr_end; addr += 0x0100) {
        //printf("\nMAP WRITE %02x", addr);
        auto &blk = map[addr >> 8];
        blk.write_ptr = static_cast<u8 *>(ptr) + addr;
        blk.write = func;
        blk.empty = false;
    }
}

void mem::map_writes_baseptr(u32 addr_start, u32 addr_end, void *ptr, write_func func) {
    for (u32 addr = addr_start; addr < addr_end; addr += 0x0100) {
        //printf("\nMAP WRITE BASEPTR %02x", addr);
        auto &blk = map[addr >> 8];
        blk.write_ptr = ptr;
        blk.write = func;
        blk.empty = false;
    }
}


void mem::reset() {
    io.cpu.dir = 0b101111;
    io.cpu.u = 0b110111;
    remap();
}

void mem::first_map() {
    // Here are the things that never change!
    for (auto &blk : map) {
        blk.read_ptr = blk.write_ptr = this;
    }

    // IO/RAM split happens at 0000-0100
    map_writes(0x0000, 0x0100, this, write_zp);
    map_reads(0x0000, 0x01000, this, read_zp);;

    // so RAM is always writeable at 0100-CFFF, E000-FFFF
    map_writes(0x0100, 0xD000, RAM, &write_ram);
    map_writes(0xE000, 0xFFFF, RAM, &write_ram);

    // RAM is always readable at 0100-7FFF, C000-CFFF
    map_reads(0x0100, 0x7FFF, RAM, &read_ram);
    map_writes(0xC000, 0xCFFF, RAM, &write_ram);
    map_reads(0xC000, 0xCFFF, RAM, &read_ram);
}


static u8 read_cart_lo(void *ptr, u16 addr, u8 old, bool has_effect) {
    auto *mem = static_cast<C64::mem *>(ptr);
    if (mem->cart.present) return static_cast<u8 *>(mem->cart.data.ptr)[addr & mem->cart.mask];
    printf("\nWARN read lo from non-allocated cart %04x", addr);
    return 0;
}

static u8 read_cart_hi(void *ptr, u16 addr, u8 old, bool has_effect) {
    auto *mem = static_cast<C64::mem *>(ptr);
    if (mem->cart.present) return static_cast<u8 *>(mem->cart.data.ptr)[(addr | 0x1000) & mem->cart.mask];
    printf("\nWARN read lo from non-allocated cart %04x", addr);
    return 0;
}

void mem::map_8000() {
    // if cart is there, map it.
    // else,
    if (cart.present)
        map_reads_baseptr(0x8000, 0x9FFF, this, &read_cart_lo);
    else
        map_reads(0x8000, 0x9FFF, RAM, &read_ram);
}

static void write_io(void *ptr, u16 addr, u8 val) {
    auto *mem = static_cast<C64::mem *>(ptr);
    mem->write_IO(addr, val);;
}

static u8 read_io(void *ptr, u16 addr, u8 old, bool has_effect) {
    auto *mem = static_cast<C64::mem *>(ptr);
    return mem->read_IO(addr, old, has_effect);
}

static u8 read_chargen(void *ptr, u16 addr, u8 old, bool has_effect) {
    auto *mem = static_cast<C64::mem *>(ptr);
    return mem->CHARGEN[addr & 0xFFF];
}

static u8 read_basic(void *ptr, u16 addr, u8 old, bool has_effect) {
    auto *mem = static_cast<C64::mem *>(ptr);
    return mem->BASIC[addr & 0x1FFF];
}

static u8 read_kernal(void *ptr, u16 addr, u8 old, bool has_effect) {
    auto *mem = static_cast<C64::mem *>(ptr);
    return mem->KERNAL[addr & 0x1FFF];
}


void mem::map_d000() {
    // bits 0-1 00 = read/write RAM
    if ((io.cpu.u & 3) == 0) {
        map_reads(0xD000, 0xDFFF, RAM, &read_ram);
        map_writes(0xD000, 0xDFFF, RAM, &write_ram);
    }
    else {
        if (io.cpu.u & 4) { // R/W IO
            map_reads_baseptr(0xD000, 0xDFFF, this, &read_io);
            map_writes_baseptr(0xD000, 0xDFFF, this, &write_io);
        }
        else { // read character rom, write ram
            map_reads_baseptr(0xD000, 0xDFFF, this, &read_chargen);
            map_writes(0xD000, 0xDFFF, RAM, &write_ram);
        }
    }

}

void mem::map_rom_a000() {
    // cart ROM HI or BASIC ROM, or...
    map_reads_baseptr(0xA000, 0xBFFF, this, cart.present ? &read_cart_hi : &read_basic);
}

void mem::map_rom_e000() {
    map_reads_baseptr(0xE000, 0xFFFF, this, cart.present ? &read_cart_hi : &read_kernal);
}

void mem::remap() {
// We must account for...
    // R to 8000-9FFF (cart rom lo, RAM)
    // R to A000-BFFF (cart rom hi, RAM, BASIC ROM)
    // RW to D000-DFFF (RAM, CHAR ROM, IO)
    // R to E000-EFFF (cart ROM hi, RAM, kernal ROM)
    u32 bits = io.cpu.u & 0b111;
    map_8000();
    map_d000();
    switch (bits & 0b11) {
        case 0b00:
            map_reads(0xA000, 0xBFFF, RAM, &read_ram);
            map_reads(0xE000, 0xFFFF, RAM, &read_ram);
            break;
        case 0b01:
            map_reads(0xA000, 0xBFFF, RAM, &read_ram);
            map_reads(0xE000, 0xFFFF, RAM, &read_ram);
            break;
        case 0b10:
            map_reads(0xA000, 0xBFFF, RAM, &read_ram);
            map_rom_e000();
            break;
        case 0b11:
            map_rom_a000();
            map_rom_e000();
            break;
    }
}

}