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


struct NDS_cart {
    struct {
        u32 data[8];
        u32 pos;

    } cmd;

    struct buf ROM;
};

u32 NDS_cart_load_ROM_from_RAM(struct NDS_cart*, char* fil, u64 fil_sz, struct physical_io_device *pio, u32 *SRAM_enable);
#endif //JSMOOCH_EMUS_NDS_CART_H
