//
// Created by . on 12/4/24.
//

#ifndef JSMOOCH_EMUS_NDS_CART_H
#define JSMOOCH_EMUS_NDS_CART_H

#include "helpers/buf.h"
#include "helpers/int.h"

enum NDS_flash_kinds {
    NDSFK_atmel,
    NDSFK_macronix,
    NDSFK_panasonic,
    NDSFK_SST,
    NDSFK_sanyo128k,
    NDSFK_macronix128k,
    NDSFK_other
};


enum NDS_flash_states {
    NDSFS_idle,
    NDSFS_get_id,
    NDSFS_erase_4k,
    NDSFS_write_byte,
    NDSFS_await_bank
};

enum NDS_cart_data_modes {
    NCDM_unencrypted,
    NCDM_secure_area,
    NCDM_main
};

enum NDS_after_next_busy {
    NDANB_none,
    NDANB_handle_cmd,
    NDANB_after_read
};


enum NDS_BACKUP_KIND {
    NDSBK_none=0,
    NDSBK_eeprom=1,
    NDSBK_flash=2,
    NDSBK_fram
};

struct NDS_cart {
    u64 sch_id;
    u32 sch_sch;
    struct {
        u32 data_in[8];
        u32 pos_in;
        u32 data_out[0x1000];
        u32 pos_out;
        u32 sz_out;
        u32 addr;

        u32 cur;

        u32 input, output;
        u32 block_size;
    } cmd;

    struct {
        struct {
            u32 divider_val;
            u32 divider;
            u32 next_chipsel;
            u32 slot_mode;
        } spi;
        u32 transfer_ready_irq;
        u32 nds_slot_enable;

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
                u32 data_ready : 1; // read-only, bit 23

                // byte 3
                u32 data_block_size : 3; // 24-26
                u32 transfer_clk_rate : 1; // 27
                u32 key1_gap_clks : 1; // 28
                u32 resb_release_reset : 1; // 29, cannot be cleared once set
                u32 data_direction : 1; // 30, for FLASH/NAND carts
                u32 busy : 1; // 31 IRQ on bit14 of SPICNT, read-only
            };
            u32 u;
        } romctrl;
    } io;

    struct {
        struct persistent_store *store;
        u32 cmd;

        union {
            u8 b8[4];
            u32 u;
        } data_in;
        u32 data_in_pos;
        u32 cmd_addr;

        union {
            u8 b8[4];
            u32 b32;
        } data_out;

        union {
            struct {
                u8 busy : 1;
                u8 write_enable : 1;
            };
            u8 u;
        } status;
        u32 chipsel;
        struct {
            u32 done;
            u32 pos;
            enum NDS_BACKUP_KIND kind;
            u32 sz_mask;
            u32 sz;
            u32 arg_buf_addr;
            u32 arg_buf_ptr;
            u32 addr_bytes;
        } detect;

    } RAM;

    u64 spi_busy_until;
    u64 rom_busy_until;
    enum NDS_cart_data_modes data_mode;

    struct buf ROM;
};

void NDS_cart_init(struct NDS *);
void NDS_cart_reset(struct NDS *);
void NDS_cart_delete(struct NDS *);
void NDS_cart_direct_boot(struct NDS *);
u32 NDS_cart_load_ROM_from_RAM(struct NDS_cart*, char* fil, u64 fil_sz, struct physical_io_device *pio, u32 *SRAM_enable);
void NDS_cart_spi_write_spicnt(struct NDS *, u32 val, u32 bnum);
void NDS_cart_spi_transaction(struct NDS *, u32 val);
void NDS_cart_write_romctrl(struct NDS *, u32 val);
void NDS_cart_write_cmd(struct NDS *, u32 addr, u32 val);
u32 NDS_cart_read_romctrl(struct NDS *);
u32 NDS_cart_read_rom(struct NDS *, u32 addr, u32 sz);
void NDS_cart_check_transfer(void *ptr, u64 key, u64 clock, u32 jitter);
u32 NDS_cart_read_spicnt(struct NDS *);
u32 NDS_cart_read_spi(struct NDS *, u32 bnum);
void NDS_cart_detect_kind(struct NDS *, u32 from, u32 val);

#endif //JSMOOCH_EMUS_NDS_CART_H
