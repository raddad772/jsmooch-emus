//
// Created by Dave on 2/9/2024.
//

#ifndef JSMOOCH_EMUS_SMS_GG_MAPPER_SEGA_H
#define JSMOOCH_EMUS_SMS_GG_MAPPER_SEGA_H

#include "helpers/int.h"
#include "helpers/buf.h"
#include "helpers/sys_interface.h"

struct SMSGG_mapper_sega {
    struct buf ROM;
    struct buf BIOS;
    u8 RAM[16384];

    enum jsm_systems variant;

    u32 enable_RAM;
    u32 enable_bios;
    u32 enable_cart;

    struct SMSGG_cart {
        u32 ram_80_enabled;
        u32 ram_C0_enabled;

        u32 num_ROM_banks;
        u32 rom_shift;

        u32 rom_bank_upper;

        u32 rom_00_bank;
        u32 rom_40_bank;
        u32 rom_80_bank;
    } cart;

    struct SMSGG_bios {
        u32 rom_00_bank;
        u32 rom_40_bank;
        u32 rom_80_bank;
        u32 num_banks;
    } bios;
};

void SMSGG_mapper_sega_init(struct SMSGG_mapper_sega*, enum jsm_systems variant);
void SMSGG_mapper_sega_delete(struct SMSGG_mapper_sega*);
void SMSGG_mapper_sega_set_BIOS(struct SMSGG_mapper_sega*, u32 to);
void SMSGG_mapper_sega_reset(struct SMSGG_mapper_sega*);
struct SMSGG;
u32 SMSGG_bus_read(struct SMSGG* bus, u32 addr, u32 val, u32 has_effect);
void SMSGG_bus_write(struct SMSGG* bus, u32 addr, u32 val);
void SMSGG_mapper_load_BIOS_from_RAM(struct SMSGG_mapper_sega*, struct buf *BIOS);
void SMSGG_mapper_load_ROM_from_RAM(struct SMSGG_mapper_sega*, struct buf* inbuf);

#endif //JSMOOCH_EMUS_SMS_GG_MAPPER_SEGA_H
