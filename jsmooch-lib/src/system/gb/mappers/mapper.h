#pragma once

#include "helpers/int.h"
#include "helpers/serialize/serialize.h"
#include "../gb_enums.h"
#include "system/gb/cart.h"

namespace GB {
struct cart;
struct core;
struct clock;

struct MAPPER {
    void *ptr;

    mappers which;

    void (*reset)(MAPPER*);
    void (*set_cart)(MAPPER*, cart*);

    void (*serialize)(MAPPER*, serialized_state &state);
    void (*deserialize)(MAPPER*, serialized_state &state);
    //void (*set_BIOS)(MAPPER*, u8*, u32);

    u32 (*CPU_read)(MAPPER*, u32, u32, u32);
    void (*CPU_write)(MAPPER*, u32, u32);
};

MAPPER* new_GB_mapper(clock* clock, core* bus, mappers which);
void delete_GB_mapper(MAPPER* whom);

}
