#pragma once

#include "helpers/int.h"
#include "../gb_enums.h"
#include "system/gb/cart.h"

namespace GB {
struct cart;

struct mapper {
    void *ptr;

    mappers which;

    void (*reset)(mapper*);
    void (*set_cart)(mapper*, cart*);

    void (*serialize)(mapper*, serialized_state &state);
    void (*deserialize)(mapper*, serialized_state &state);
    //void (*set_BIOS)(mapper*, u8*, u32);

    u32 (*CPU_read)(mapper*, u32, u32, u32);
    void (*CPU_write)(mapper*, u32, u32);
};

mapper* new_mapper(clock* clock, core* bus, mappers which);
void delete_mapper(mapper* whom);

}
