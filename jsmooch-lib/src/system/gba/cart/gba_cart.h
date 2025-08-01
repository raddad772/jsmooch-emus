//
// Created by . on 12/4/24.
//

#ifndef JSMOOCH_EMUS_GBA_CART_H
#define JSMOOCH_EMUS_GBA_CART_H

#include "helpers/buf.h"
#include "helpers/int.h"

enum GBA_flash_kinds {
    GBAFK_atmel,
    GBAFK_macronix,
    GBAFK_panasonic,
    GBAFK_SST,
    GBAFK_sanyo128k,
    GBAFK_macronix128k,
    GBAFK_other
};


enum GBA_flash_states {
    GBAFS_idle,
    GBAFS_get_id,
    GBAFS_erase_4k,
    GBAFS_write_byte,
    GBAFS_await_bank
};


struct GBA_cart {
    struct buf ROM;
    struct {
        u32 mask, size, present, persists;
        u32 is_sram;
        u32 is_flash;
        u32 is_eeprom;
        struct {
            u32 cmd_loc;
            enum GBA_flash_kinds kind;
            enum GBA_flash_states state;
            u32 flash_cmd[2];
            u32 bank_offset;
            u32 last_cmd;

            struct {
                u32 r55;
                u32 r2a;
            } regs;

        } flash;

        struct GBA_CART_EEPROM {
            u32 addr_bus_size;
            u32 size_was_detected;
            u32 size_in_bytes;
            u32 addr_mask;

            u32 mode;

            u64 ready_at;

            struct {
                u64 data;
                u32 sz;
            } serial;

            struct {
                u32 cur_bit_addr;
            } cmd;

        } eeprom;

        struct persistent_store *store;
    } RAM;

    struct {
        u32 present;
        u64 timestamp_start;
    } RTC;

    struct {
        i64 cycles_banked;
        u32 next_addr;
        u32 duty_cycle;
        u64 last_access;
        u32 enable;
        u32 was_disabled;
    } prefetch;
};

void GBA_cart_init(struct GBA_cart*);
void GBA_cart_delete(struct GBA_cart *);
void GBA_cart_write(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val);
u32 GBA_cart_read(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect, u32 ws);
u32 GBA_cart_load_ROM_from_RAM(struct GBA_cart*, char* fil, u64 fil_sz, struct physical_io_device *pio, u32 *SRAM_enable);
u64 GBA_clock_current(struct GBA *);

struct GBA;
u32 GBA_cart_read_wait0(struct GBA*, u32 addr, u32 sz, u32 access, u32 has_effect);
u32 GBA_cart_read_wait1(struct GBA*, u32 addr, u32 sz, u32 access, u32 has_effect);
u32 GBA_cart_read_wait2(struct GBA*, u32 addr, u32 sz, u32 access, u32 has_effect);
u32 GBA_cart_read_sram(struct GBA*, u32 addr, u32 sz, u32 access, u32 has_effect);
void GBA_cart_write_sram(struct GBA*, u32 addr, u32 sz, u32 access, u32 val);
#endif //JSMOOCH_EMUS_GBA_CART_H
