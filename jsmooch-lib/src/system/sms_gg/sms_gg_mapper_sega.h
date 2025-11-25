//
// Created by Dave on 2/9/2024.
//

#pragma once

#include "helpers/int.h"
#include "helpers/simplebuf.h"
#include "helpers/sys_interface.h"

namespace SMSGG {
enum mem_kinds {
    MK_empty,
    MK_sys_ram,
    MK_cart_rom,
    MK_cart_ram,
    MK_bios
};

struct mem_region {
    u32 read_only, empty;
    mem_kinds kind;
    u32 offset;

    simplebuf8 *buf; // Actual pointer to the memory in question
};

struct mapper_sega {
private:
    void map(u16 sms_start_addr, u16 sms_end_addr, mem_kinds kind, u32 offset);
    void refresh_nomapper();
    void refresh_sega_mapper();
public:
    void refresh_mapping();
    void write_registers(u16 addr, u8 val);
    explicit mapper_sega(jsm::systems in_variant);
    void load_BIOS_from_RAM(buf &BIOS);
    void load_ROM_from_RAM(buf& inbuf);
    simplebuf8 ROM{};
    simplebuf8 BIOS{};
    simplebuf8 RAM{};
    simplebuf8 cart_RAM{};
    void reset();
    void set_BIOS(u32 to);

    jsm::systems variant;

    mem_region regions[256]{}; // upper 8 bits of an address
    u32 sega_mapper_enabled{};
    u32 has_bios{};
    bool is_sms{};

    struct {
        u32 frame_control_register[3]{};

        struct {
            u32 mapped{};
            u32 bank{};
        } cart_ram{};

        u32 bank_shift{};
        bool bios_enabled{false};
        bool cart_enabled{true};
    } io{};
};

}