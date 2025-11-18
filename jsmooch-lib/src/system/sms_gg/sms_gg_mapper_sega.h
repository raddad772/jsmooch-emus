//
// Created by Dave on 2/9/2024.
//

#ifndef JSMOOCH_EMUS_SMS_GG_MAPPER_SEGA_H
#define JSMOOCH_EMUS_SMS_GG_MAPPER_SEGA_H

#include "helpers/int.h"
#include "helpers/simplebuf.h"
#include "helpers/sys_interface.h"

enum SMSGG_mem_kinds {
    SMSGGMK_empty,
    SMSGGMK_sys_ram,
    SMSGGMK_cart_rom,
    SMSGGMK_cart_ram,
    SMSGGMK_bios
};

struct SMSGG_mem_region {
    u32 read_only, empty;
    enum SMSGG_mem_kinds kind;
    u32 offset;

    struct simplebuf8 *buf; // Actual pointer to the memory in question
};

struct SMSGG_mapper_sega {
    struct simplebuf8 ROM;
    struct simplebuf8 BIOS;
    struct simplebuf8 RAM;
    struct simplebuf8 cart_RAM;

    enum jsm_systems variant;

    struct SMSGG_mem_region regions[256]; // upper 8 bits of an address
    u32 sega_mapper_enabled;
    u32 has_bios;
    u32 is_sms;

    struct {
        u32 frame_control_register[3];

        struct {
            u32 mapped;
            u32 bank;
        } cart_ram;

        u32 bank_shift;
        u32 bios_enabled;
        u32 cart_enabled;
    } io;
};

void SMSGG_mapper_sega_init(struct SMSGG_mapper_sega*, enum jsm_systems variant);
void SMSGG_mapper_sega_delete(struct SMSGG_mapper_sega*);
void SMSGG_mapper_sega_set_BIOS(struct SMSGG_mapper_sega*, u32 to);
void SMSGG_mapper_sega_reset(struct SMSGG_mapper_sega*);
struct SMSGG;
u8 SMSGG_bus_read(struct SMSGG* bus, u16 addr, u32 has_effect);
void SMSGG_bus_write(struct SMSGG* bus, u16 addr, u8 val);
void SMSGG_mapper_load_BIOS_from_RAM(struct SMSGG_mapper_sega*, struct buf *BIOS);
void SMSGG_mapper_load_ROM_from_RAM(struct SMSGG_mapper_sega*, struct buf* inbuf);
void SMSGG_mapper_refresh_mapping(struct SMSGG_mapper_sega *);

#endif //JSMOOCH_EMUS_SMS_GG_MAPPER_SEGA_H
