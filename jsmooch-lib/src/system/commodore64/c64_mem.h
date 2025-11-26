#pragma once

#include <cassert>
#include <cstdio>

#include "helpers/int.h"
#include "helpers/buf.h"

namespace C64 {

u8 bad_read_func(void *ptr, u16 addr, u8 old, bool has_effect);
void bad_write_func(void *ptr, u16 addr, u8 val);

typedef u8 (*read_func)(void *ptr, u16 addr, u8 old, bool has_effect);
typedef void (*write_func)(void *ptr, u16 addr, u8 val);

struct mem_map {
    bool empty{true};
    void *read_ptr{nullptr}, *write_ptr{nullptr};
    read_func read;
    write_func write;
};

struct core;

struct mem {
    u8 read_main_bus(u16 addr, u8 old, bool has_effect);
    void write_main_bus(u16 addr, u8 val);

    core *bus;

    void load_KERNAL(buf &b, size_t offset);
    void load_BASIC(buf &b, size_t offset);
    void load_CHARGEN(buf &b, size_t offset);

    explicit mem(core *parent);
    u8 KERNAL[0x2000]{};
    u8 BASIC[0x2000]{};
    u8 CHARGEN[0x1000]{};
    u8 RAM[0x10000]{};
    u8 COLOR[1000]{};

    mem_map map[0x100]{};

    u8 read_IO(u16 addr, u8 old, bool has_effect);
    void write_IO(u16 addr, u8 val);
    void reset();

    struct {
        struct {
            u8 dir{};
            u8 u{};
        } cpu{};
    } io{};

    struct {
        bool present{};
        buf data{};
        u32 mask{};
    } cart{};

private:
    void first_map();
    void remap();
    void map_reads(u32 addr_start, u32 addr_end, void *ptr, read_func func);
    void map_reads_baseptr(u32 addr_start, u32 addr_end, void *ptr, read_func func);
    void map_writes(u32 addr_start, u32 addr_end, void *ptr, write_func func);
    void map_writes_baseptr(u32 addr_start, u32 addr_end, void *ptr, write_func func);
    void map_8000();
    void map_d000();
    void map_rom_a000();
    void map_rom_e000();
    u8 read_color_ram(u16 addr, u8 old, bool has_effect);
    void write_color_ram(u16 addr, u8 val);
};
}