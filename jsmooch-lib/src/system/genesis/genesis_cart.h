//
// Created by . on 6/1/24.
//

#ifndef JSMOOCH_EMUS_GENESIS_CART_H
#define JSMOOCH_EMUS_GENESIS_CART_H

#include "helpers/int.h"
#include "helpers/buf.h"

enum genesis_cart_kinds {
    sega_cart_invalid,
    sega_cart_genesis,
    sega_cart_32x,
    sega_cart_everdrive,
    sega_cart_ssf,
    sega_cart_megawifi,
    sega_cart_pico,
    sega_cart_tera68k,
    sega_cart_tera286,
};

enum sega_cart_devices_supported {
    sega_cart_device_cnt_3button,
    sega_cart_device_cnt_6button,
    sega_cart_device_cnt_master_system,
    sega_cart_device_analog_joystick,
    sega_cart_device_multitap,
    sega_cart_device_lightgun,
    sega_cart_device_activator,
    sega_cart_device_mouse,
    sega_cart_device_trackball,
    sega_cart_device_tablet,
    sega_cart_device_paddle,
    sega_cart_device_keyboard_or_keypad,
    sega_cart_device_rs_232,
    sega_cart_device_printer,
    sega_cart_device_segacd,
    sega_cart_device_floppy,
    sega_cart_device_download,
    sega_cart_device_none,
};


enum sega_cart_extra_RAM_kind {
    sega_cart_exram_none,
    sega_cart_exram_16bit,
    sega_cart_exram_8bit_even,
    sega_cart_exram_8bit_odd,
};

struct genesis_cart {
    struct buf ROM;

    u32 RAM_mask;
    u32 RAM_present;
    u32 RAM_always_on;
    u32 RAM_size;
    u32 RAM_persists;

    struct {
        char copyright[17];
        char title_domestic[49];
        char title_overseas[49];
        char serial_number[15];

        u16 rom_checksum;

        u32 num_devices_supported;
        enum sega_cart_devices_supported devices_supported[16];

        u32 rom_addr_start;
        u32 rom_addr_end;

        u32 ram_addr_start;
        u32 ram_addr_end;

        u32 extra_ram_addr_start;
        u32 extra_ram_addr_end;
        u32 extra_ram_is_eeprom;
        enum sega_cart_extra_RAM_kind extra_ram_kind;

        u32 region_japan;
        u32 region_europe;
        u32 region_usa;
    } header;

    u32 bank_offset[8];

    enum genesis_cart_kinds kind;

    struct persistent_store *SRAM;
};


void genesis_cart_init(genesis_cart*);
void genesis_cart_delete(genesis_cart *);
u16 genesis_cart_read(genesis_cart *, u32 addr, u32 mask, u32 has_effect, u32 SRAM_enable);
void genesis_cart_write(genesis_cart *, u32 addr, u32 mask, u32 val, u32 SRAM_enable);

u32 genesis_cart_load_ROM_from_RAM(genesis_cart* this, char* fil, u64 fil_sz, physical_io_device *pio, u32 *SRAM_enable);

#endif //JSMOOCH_EMUS_GENESIS_CART_H
