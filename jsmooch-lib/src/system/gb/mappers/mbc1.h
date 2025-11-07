#ifndef JSMOOCH_EMUS_MBC1_H
#define JSMOOCH_EMUS_MBC1_H

#include "helpers_c/int.h"
#include "mapper.h"
#include "../gb_clock.h"
#include "../gb_bus.h"

struct GB_mapper_MBC1 {
    struct GB_bus *bus;
    struct GB_clock *clock;

    u8 *ROM;
    u32 RAM_mask;
    u32 has_RAM;
    struct GB_cart* cart;

    //
    u32 ROM_bank_lo_offset;// = 0;
    u32 ROM_bank_hi_offset;// = 16384;

    u32 num_ROM_banks;
    u32 num_RAM_banks;
    struct {
        u32 banking_mode;
        u32 BANK1;
        u32 BANK2;
        u32 ext_RAM_enable;
    } regs;
    u32 cartRAM_offset;
};

void GB_mapper_MBC1_new(struct GB_mapper *parent, struct GB_clock *clock, struct GB_bus *bus);
void GB_mapper_MBC1_delete(struct GB_mapper *parent);
void GBMBC1_reset(struct GB_mapper* parent);
void GBMBC1_set_cart(struct GB_mapper* parent, struct GB_cart* cart);

u32 GBMBC1_CPU_read(struct GB_mapper* parent, u32 addr, u32 val, u32 has_effect);
void GBMBC1_CPU_write(struct GB_mapper* parent, u32 addr, u32 val);

#endif //JSMOOCH_EMUS_MBC1_H
