//
// Created by Dave on 2/9/2024.
//


#include "stdio.h"
#include "string.h"

#include "sms_gg_mapper_sega.h"
#include "sms_gg.h"

static void map(struct SMSGG_mapper_sega *this, u16 sms_start_addr, u16 sms_end_addr, enum SMSGG_mem_kinds kind, u32 offset)
{
    u32 start_page = sms_start_addr >> 8;
    u32 end_page = sms_end_addr >> 8;
    for (u32 page = start_page; page <= end_page; page++) {
        assert(page < 256);
        struct SMSGG_mem_region *region = &this->regions[page];
        region->kind = kind;
        region->empty = 0;

        switch(kind) {
            case SMSGGMK_empty:
                region->empty = 1;
                region->buf = NULL;
                region->read_only = 1;
                region->offset = 0;
                break;
            case SMSGGMK_cart_rom:
                region->empty = 0;
                region->buf = &this->ROM;
                region->read_only = 1;
                region->offset = offset;
                break;
            case SMSGGMK_bios:
                region->empty = 0;
                region->buf = &this->BIOS;
                region->read_only = 1;
                region->offset = offset;
                break;
            case SMSGGMK_cart_ram:
                region->empty = 0;
                region->buf = &this->cart_RAM;
                region->read_only = 0;
                region->offset = offset;
                break;
            case SMSGGMK_sys_ram:
                region->empty = 0;
                region->buf = &this->RAM;
                region->read_only = 0;
                region->offset = offset;
                break;
            default:
                assert(1==0);
        }
        offset += 0x100;
    }
}

static void refresh_nomapper(struct SMSGG_mapper_sega *this)
{
    map(this, 0, 0x3FFF, SMSGGMK_cart_rom, 0);
    map(this, 0x4000, 0x7FFF, SMSGGMK_cart_rom, 0x4000);
    map(this, 0x8000, 0xBFFF, SMSGGMK_cart_rom, 0x8000);
}

static void refresh_sega_mapper(struct SMSGG_mapper_sega *this)
{
    // slot0 0-3FF is fixed on ROM
    map(this, 0x0000, 0x03FF,  SMSGGMK_cart_rom, 0);

    // slot0 400-3FFF is bankable
    map(this, 0x0400, 0x3FFF, SMSGGMK_cart_rom, 0x400 + (this->io.frame_control_register[0] << 14));

    // slot1
    map(this, 0x4000, 0x7FFF, SMSGGMK_cart_rom, this->io.frame_control_register[1] << 14);

    // slot2, ROM or RAM
    if (this->io.cart_ram.mapped) {
        map(this, 0x8000, 0xBFFF, SMSGGMK_cart_ram, this->io.cart_ram.bank << 14);
    }
    else {
        map(this, 0x8000, 0xBFFF, SMSGGMK_cart_rom, (this->io.frame_control_register[2] + this->io.bank_shift) << 14);
    }


    // C000-FFFF stays mapped to RAM
}

void SMSGG_mapper_refresh_mapping(struct SMSGG_mapper_sega *this)
{
    if (!this->sega_mapper_enabled) // Just generic SMS mapping
        refresh_nomapper(this);
    else
        refresh_sega_mapper(this);
}

void SMSGG_mapper_sega_init(struct SMSGG_mapper_sega* this, enum jsm_systems variant)
{
    memset(this, 0, sizeof(struct SMSGG_mapper_sega));
    simplebuf8_init(&this->RAM);
    simplebuf8_init(&this->ROM);
    simplebuf8_init(&this->BIOS);
    simplebuf8_init(&this->cart_RAM);

    u32 is_sms = (variant == SYS_SMS1) || (variant == SYS_SMS2);
    u32 is_gg = (variant == SYS_SG1000);
    switch(variant) {
        case SYS_SMS1:
        case SYS_SMS2:
        case SYS_GG:
            simplebuf8_allocate(&this->RAM, 8 * 1024);
            simplebuf8_allocate(&this->cart_RAM, 32 * 1024);
            simplebuf8_clear(&this->RAM);
            simplebuf8_clear(&this->cart_RAM);
            this->sega_mapper_enabled = 1;
            break;
        case SYS_SG1000:
            simplebuf8_allocate(&this->RAM, 2 * 1024);
            simplebuf8_clear(&this->RAM);
            this->sega_mapper_enabled = 0;
            break;
        default:
            assert(1==2);
    }
    map(this, 0xC000, 0xFFFF, SMSGGMK_sys_ram, 0);

    this->io.cart_enabled = !this->io.bios_enabled;
    this->io.frame_control_register[0] = 0;
    this->io.frame_control_register[1] = 1;
    this->io.frame_control_register[2] = 2;
}

void SMSGG_mapper_sega_delete(struct SMSGG_mapper_sega* this)
{
    simplebuf8_delete(&this->ROM);
    simplebuf8_delete(&this->BIOS);
    simplebuf8_delete(&this->RAM);
    simplebuf8_delete(&this->cart_RAM);
}

void SMSGG_mapper_sega_reset(struct SMSGG_mapper_sega* this)
{
    this->io.bios_enabled = this->has_bios;
    SMSGG_mapper_refresh_mapping(this);
}

void SMSGG_mapper_sega_set_BIOS(struct SMSGG_mapper_sega* this, u32 to)
{
    // BIOS currently not supported
    return;
    if ((this->has_bios) && (to != this->io.bios_enabled)) {
        this->io.bios_enabled = to;
        SMSGG_mapper_refresh_mapping(this);
    }
}

static void write_registers(struct SMSGG_mapper_sega* this, u16 addr, u8 val)
{
    if (this->sega_mapper_enabled) {
        u32 mapping_changed = 0;
        switch (addr) {
            case 0xFFFC:
                switch(val & 3) {
                    case 0:
                        this->io.bank_shift = 0;
                        break;
                    case 1:
                        this->io.bank_shift = 0x18;
                        break;
                    case 2:
                        this->io.bank_shift = 0x10;
                        break;
                    case 3:
                        this->io.bank_shift = 0x08;
                        break;
                }
                if (this->io.bank_shift > 0) printf("\nWARNING BANKSHIFT %02x", this->io.bank_shift);
                this->io.cart_ram.bank = (val >> 2) & 1;
                this->io.cart_ram.mapped = (val >> 3) & 1;
                if ((val >> 4) & 1) {
                    printf("\nWARNING ATTEMPT TO MAP CART RAM TO SYSTEM RAM");
                }
                break;
            case 0xFFFD:
            case 0xFFFE:
            case 0xFFFF:
                ;u32 w = addr - 0xFFFD;
                mapping_changed = (val != this->io.frame_control_register[w]);
                this->io.frame_control_register[w] = val;
                if (mapping_changed) SMSGG_mapper_refresh_mapping(this);
                break;
        }
    }

    //return 1;
}


u8 SMSGG_bus_read(struct SMSGG* bus, u16 addr, u32 has_effect)
{
    struct SMSGG_mem_region *region = &bus->mapper.regions[addr >> 8];
    if (region->empty) return 0xFF;
    if (!region->buf->ptr) {
        assert(1==2);
        return 0xFF;
    }
    u32 baddr = ((addr & 0xFF) + region->offset) & region->buf->mask;
    return region->buf->ptr[baddr];
}

void SMSGG_bus_write(struct SMSGG* bus, u16 addr, u8 val)
{
    write_registers(&bus->mapper, addr, val);

    struct SMSGG_mem_region *region = &bus->mapper.regions[addr >> 8];
    if (region->read_only) return;
    if (region->empty) return;
    if (!region->buf->ptr) {
        assert(1==2);
    }
    u32 baddr = ((addr & 0xFF) + region->offset) & region->buf->mask;
    assert(baddr < region->buf->sz);
    region->buf->ptr[baddr] = val;
}

void SMSGG_mapper_load_BIOS_from_RAM(struct SMSGG_mapper_sega* this, struct buf *BIOS)
{
    simplebuf8_allocate(&this->BIOS, BIOS->size);
    memcpy(this->BIOS.ptr, BIOS->ptr, BIOS->size);
    this->has_bios = 1;
}

void SMSGG_mapper_load_ROM_from_RAM(struct SMSGG_mapper_sega* this, struct buf* inbuf)
{
    u64 sz = inbuf->size;
    u64 offset = 0;
    if ((inbuf->size % 16384) != 0) {
        printf("\nWARNING HEADER DETECTED SIZE: %lld", inbuf->size % 16384);
        offset = 512;
        sz -= 512;
    }
    assert(sz % 16384 == 0);

    simplebuf8_allocate(&this->ROM, sz);
    memcpy(this->ROM.ptr, inbuf->ptr+offset, sz);
}