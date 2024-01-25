#ifndef _JSMOOCH_MAPPER_H
#define _JSMOOCH_MAPPER_H

#include "helpers/int.h"
#include "../gb_enums.h"
#include "system/gb/cart.h"

struct GB_mapper {
    void *ptr;

    enum GB_mappers which;

    void (*reset)(struct GB_mapper*);
    void (*set_cart)(struct GB_mapper*, struct GB_cart*);
    //void (*set_BIOS)(struct GB_mapper*, u8*, u32);

    u32 (*CPU_read)(struct GB_mapper*, u32, u32, u32);
    void (*CPU_write)(struct GB_mapper*, u32, u32);
};

struct GB_mapper* new_GB_mapper(struct GB_clock* clock, struct GB_bus* bus, enum GB_mappers which);
void delete_GB_mapper(struct GB_mapper* whom);

#endif
