//
// Created by . on 6/1/24.
//

#pragma once
#include "helpers/int.h"
#include "helpers/buf.h"

namespace genesis {

enum cart_kinds {
    sc_invalid,
    sc_genesis,
    sc_32x,
    sc_everdrive,
    sc_ssf,
    sc_megawifi,
    sc_pico,
    sc_tera68k,
    sc_tera286,
};

enum cart_devices_supported {
    scd_cnt_3button,
    scd_cnt_6button,
    scd_cnt_master_system,
    scd_analog_joystick,
    scd_multitap,
    scd_lightgun,
    scd_activator,
    scd_mouse,
    scd_trackball,
    scd_tablet,
    scd_paddle,
    scd_keyboard_or_keypad,
    scd_rs_232,
    scd_printer,
    scd_segacd,
    scd_floppy,
    scd_download,
    scd_none,
};

enum cart_extra_RAM_kind {
    sce_none,
    sce_16bit,
    sce_8bit_even,
    sce_8bit_odd,
};

struct cart {
    cart();
    buf ROM{};

private:
    void write_RAM8(u32 addr, u32 val);
    void write_RAM16(u32 addr, u32 val);
    u8 read_RAM8(u32 addr);
    u16 read_RAM16(u32 addr);
    char *eval_cart_RAM(char *tptr);

public:
    u32 RAM_mask{};
    u32 RAM_present{};
    u32 RAM_always_on{};
    u32 RAM_size{};
    u32 RAM_persists{};

    struct {
        char copyright[17]{};
        char title_domestic[49]{};
        char title_overseas[49]{};
        char serial_number[15]{};

        u16 rom_checksum{};

        u32 num_devices_supported{};
        cart_devices_supported devices_supported[16]{};

        u32 rom_addr_start{};
        u32 rom_addr_end{};

        u32 ram_addr_start{};
        u32 ram_addr_end{};

        u32 extra_ram_addr_start{};
        u32 extra_ram_addr_end{};
        u32 extra_ram_is_eeprom{};
        cart_extra_RAM_kind extra_ram_kind{};

        u32 region_japan{};
        u32 region_europe{};
        u32 region_usa{};
    } header{};

    u32 bank_offset[8]{};

    cart_kinds kind{};

    persistent_store *SRAM{};

    u16 read(u32 addr, u32 mask, bool has_effect, bool SRAM_enable);
    void write(u32 addr, u32 mask, u32 val, bool SRAM_enable);
    bool load_ROM_from_RAM(char *fil, size_t fil_sz, physical_io_device *pio, bool *SRAM_enable);
};


}
