//
// Created by . on 6/18/25.
//

#ifndef JSMOOCH_EMUS_TG16_BUS_H
#define JSMOOCH_EMUS_TG16_BUS_H

#include "helpers/scheduler.h"

#include "component/cpu/huc6280/huc6280.h"
#include "component/gpu/huc6270/huc6270.h"
#include "component/gpu/huc6260/huc6260.h"
#include "tg16_clock.h"

struct TG16 {
    struct HUC6280 cpu;
    struct HUC6270 vdc;
    struct HUC6260 vce;

    struct TG16_clock clock;

    struct scheduler_t scheduler;

    struct {
        struct cvec* IOs;
        u32 described_inputs;
    } jsm;

};

u32 TG16_bus_read(struct TG16 *, u32 addr, u32 old, u32 has_effect);

u32 TG16_huc_read_mem(void *ptr, u32 addr, u32 old, u32 has_effect);
void TG16_huc_write_mem(void *ptr, u32 addr, u32 val);
u32 TG16_huc_read_io(void *ptr);
void TG16_huc_write_io(void *ptr, u32 val);

#endif //JSMOOCH_EMUS_TG16_BUS_H
