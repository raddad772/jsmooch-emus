#ifndef _JSMOOCH_MAPPER_H
#define _JSMOOCH_MAPPER_H

#include "helpers/int.h"
#include "../gb_enums.h"
#include "system/gb/cart.h"

struct serialized_state;
struct GB_mapper {
    void *ptr;

    enum GB_mappers which;

    void (*reset)(GB_mapper*);
    void (*set_cart)(GB_mapper*, GB_cart*);

    void (*serialize)(GB_mapper*, serialized_state *state);
    void (*deserialize)(GB_mapper*, serialized_state *state);
    //void (*set_BIOS)(GB_mapper*, u8*, u32);

    u32 (*CPU_read)(GB_mapper*, u32, u32, u32);
    void (*CPU_write)(GB_mapper*, u32, u32);
};

struct GB_mapper* new_GB_mapper(GB_clock* clock, GB_bus* bus, enum GB_mappers which);
void delete_GB_mapper(GB_mapper* whom);

#endif
