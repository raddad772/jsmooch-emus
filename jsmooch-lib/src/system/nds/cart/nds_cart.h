//
// Created by . on 12/4/24.
//

#pragma once

#include "helpers/buf.h"
#include "helpers/int.h"

namespace NDS::CART {

enum flash_kinds {
    FK_atmel,
    FK_macronix,
    FK_panasonic,
    FK_SST,
    FK_sanyo128k,
    FK_macronix128k,
    FK_other
};

enum after_next_busy {
    ANB_none=0,
    ANB_handle_cmd=1,
    ANB_after_read=2
};

enum data_modes {
    NCDM_unencrypted,
    NCDM_secure_area,
    NCDM_main
};

enum backup_kind {
    BK_none=0,
    BK_eeprom=1,
    BK_flash=2,
    BK_fram=3
};

struct ridge {
    explicit ridge(NDS::core *parent) : bus(parent) {}
    bool load_ROM_from_RAM(char* fil, u64 fil_sz, physical_io_device *pio, u32 *SRAM_enable);
    NDS::core *bus;
    [[nodiscard]] u32 read_spicnt() const;
    [[nodiscard]] bool data_ready();
    [[nodiscard]] u32 read_romctrl() const;
    [[nodiscard]] u32 get_transfer_irq_bits() const;
    [[nodiscard]] u32 read_spi(u32 bnum) const;
    [[nodiscard]] u32 read_rom(u32 addr, u8 sz);
    [[nodiscard]] u32 get_block_size() const;
    void spi_write_spicnt(u32 val, u32 bnum);
    void after_read();
    static void check_transfer(void *ptr, u64 key, u64 clock, u32 jitter);
    void raise_transfer_irq();
    void reset();
    void direct_boot();
    void set_block_start_status(u32 val, u32 transfer_ready_irq);
    void handle_cmd();
    void inc_addr();
    void spi_transaction(u32 val);
    void write_romctrl(u32 val);
    void write_cmd(u32 addr, u32 val);
    void detect_kind(u32 from, u32 val);

    void eeprom_setup();
    void eeprom_get_addr(u32 val);
    void eeprom_read();
    void eeprom_write(u32 val);
    void eeprom_handle_spi_cmd(u32 val, u32 is_cmd);
    void eeprom_spi_transaction(u32 val);

    void flash_setup();
    void flash_handle_spi_cmd(u32 val);
    void flash_spi_transaction(u32 val);

    u64 sch_id{};
    u32 sch_sch{};
    struct {
        u32 data_in[8]{};
        u32 pos_in{};
        u32 data_out[0x1000]{};
        u32 pos_out{};
        u8 sz_out{};
        u32 addr{};

        u32 cur{};

        u32 input{}, output{};
        u32 block_size{};
    } cmd{};

    struct {
        struct {
            u32 divider_val{};
            u32 divider{};
            u32 next_chipsel{};
            u32 slot_mode{};
        } spi{};
        u32 transfer_ready_irq{};
        u32 nds_slot_enable{};

        union {
            struct {
                // byte 0-1
                u32 key1_gap_len : 13; // 0-12
                u32 key2_encrypt_data : 1; // 13
                u32 se : 1; // 14
                u32 key2_apply_seed : 1; // 15

                // byte 2
                u32 key2_gap_len : 6; // 16-21
                u32 key2_encrypt_cmd : 1; // 22
                u32 data_ready : 1; // read-only{}, bit 23

                // byte 3
                u32 data_block_size : 3; // 24-26
                u32 transfer_clk_rate : 1; // 27
                u32 key1_gap_clks : 1; // 28
                u32 resb_release_reset : 1; // 29{}, cannot be cleared once set
                u32 data_direction : 1; // 30{}, for FLASH/NAND carts
                u32 busy : 1; // 31 IRQ on bit14 of SPICNT{}, read-only
            };
            u32 u{};
        } romctrl{};
    } io{};


    struct {
        persistent_store *store{};
        u32 cmd{};
        union {
            u8 b8[4];
            u32 u{};
        } data_in{};
        u32 data_in_pos{};
        u32 cmd_addr{};
        union {
            u8 b8[4];
            u32 b32{};
        } data_out{};

        union {
            struct {
                u8 busy : 1;
                u8 write_enable : 1;
                u8 write_protect_mode : 2;
                u8 _u : 3;
                u8 reg_write_disable : 1;
            };
            u8 u{};
        } status{};
        u32 chipsel{};

        u32 page_mask{};
        u32 uh{}, uq{};

        struct {
            u32 done{};
            u32 pos{};
            backup_kind kind{};
            u8 sz_mask{};
            u32 sz{};
            u32 arg_buf_addr{};
            u32 arg_buf_ptr{};
            u32 addr_bytes{};
        } detect{};
    } backup{};

    u64 rom_busy_until{};
    data_modes data_mode{};

    buf ROM{};
};
}