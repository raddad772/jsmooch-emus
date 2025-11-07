//
// Created by . on 7/13/25.
//

#ifndef JSMOOCH_EMUS_TG16B2_H
#define JSMOOCH_EMUS_TG16B2_H

#include "helpers_c/int.h"
#include "helpers_c/physical_io.h"

struct TG16_2button {
    struct physical_io_device *pio;
    u32 sel, clr;
};

void TG16_2button_init(struct TG16_2button*);
void TG16_2button_delete(struct TG16_2button*);
void TG16_2button_setup_pio(struct physical_io_device *d, u32 num, const char*name, u32 connected);
u8 TG16_2button_read_data(struct TG16_2button *);
void TG16_2button_write_data(struct TG16_2button *, u8 val);

#endif //JSMOOCH_EMUS_TG16B2_H
