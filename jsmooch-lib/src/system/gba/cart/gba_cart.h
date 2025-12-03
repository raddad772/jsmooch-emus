//
// Created by . on 12/4/24.
//

#pragma once
namespace GBA {
    struct core;
}

namespace GBA::cart {
#include "helpers/buf.h"
#include "helpers/int.h"

enum flash_kinds {
    FK_atmel,
    FK_macronix,
    FK_panasonic,
    FK_SST,
    FK_sanyo128k,
    FK_macronix128k,
    FK_other
};


enum flash_states {
    FS_idle,
    FS_get_id,
    FS_erase_4k,
    FS_write_byte,
    FS_await_bank
};

struct EEPROM {
    void serial_clear();
    void serial_add(u32 v);
    u32 addr_bus_size{};
    bool size_was_detected{true};
    u32 size_in_bytes{8192};
    u32 addr_mask{};

    u32 mode{1}; // STATE_GET_CMD

    u64 ready_at{0};

    struct {
        u64 data{};
        u32 sz{};
    } serial{};

    struct {
        u32 cur_bit_addr{};
    } cmd{};

};

struct FLASH {
    u32 cmd_loc{};
    flash_kinds kind{};
    flash_states state{};
    u32 flash_cmd[2]{};
    u32 bank_offset{};
    u32 last_cmd{};

    struct {
        u32 r55{};
        u32 r2a{};
    } regs{};
};

struct core {
    explicit core(GBA::core *parent) : gba(parent) {}
    GBA::core *gba;
    buf ROM{};

    bool prefetch_stop() const;
    u32 read(u32 addr, u32 sz, u32 access, bool has_effect, u32 ws);
    void write(u32 addr, u32 sz, u32 access, u32 val);
    bool load_ROM_from_RAM(char* fil, u64 fil_sz, physical_io_device *pio, u32 *SRAM_enable);

private:
    u32 read_wait0(u32 addr, u32 sz, u32 access, bool has_effect);
    u32 read_wait1(u32 addr, u32 sz, u32 access, bool has_effect);
    u32 read_wait2(u32 addr, u32 sz, u32 access, bool has_effect);
    u32 read_sram(u32 addr, u32 sz, u32 access, bool has_effect);
    void write_sram(u32 addr, u32 sz, u32 access, u32 val);
    void write_RTC(u32 addr, u32 sz, u32 access, u32 val);
    void detect_RTC(buf *ROM);
    void init_eeprom();
    void write_eeprom(u32 addr, u32 sz, u32 access, u32 val);
    u32 read_eeprom(u32 addr, u32 sz, u32 access, bool has_effect);
    void erase_flash();
    void write_flash_cmd(u32 addr, u32 cmd);
    u32 read_flash(u32 addr, u32 sz, u32 access, bool has_effect);
    void write_flash(u32 addr, u32 sz, u32 access, u32 val);
    void finish_flash_cmd(u32 addr, u32 sz, u32 val);

public:
    struct {
        u32 mask{}, size{}, present{}, persists{};
        bool is_sram{};
        bool is_flash{};
        bool is_eeprom{};

        FLASH flash{};
        EEPROM eeprom{};
        persistent_store *store{};
    } RAM{};

    struct {
        u32 present{};
        u64 timestamp_start{};
    } RTC{};

    struct {
        i64 cycles_banked{};
        u32 next_addr{};
        u32 duty_cycle{};
        u64 last_access{};
        bool enable{true};
        bool was_disabled{};
    } prefetch{};
};

}