//
// Created by . on 6/18/25.
//

#ifndef JSMOOCH_EMUS_TG16_BUS_H
#define JSMOOCH_EMUS_TG16_BUS_H

#include "helpers/scheduler.h"
#include "helpers/debugger/debugger.h"
#include "helpers/debugger/debuggerdefs.h"

#include "component/cpu/huc6280/huc6280.h"
#include "component/gpu/huc6270/huc6270.h"
#include "component/gpu/huc6260/huc6260.h"
#include "tg16_clock.h"
#include "tg16_cart.h"
#include "tg16_controllerport.h"
#include "component/controller/tg16b2/tg16b2.h"

struct TG16 {
    struct HUC6280 cpu;
    struct HUC6270 vdc0, vdc1; // Video Display Controller
    struct HUC6260 vce; // Video Color Encoder
    struct TG16_cart cart;
    struct TG16_controllerport controller_port;
    struct TG16_2button controller;

    u8 RAM[8192];

    struct TG16_clock clock;

    struct scheduler_t scheduler;

    struct {
        struct cvec* IOs;
        u32 described_inputs;
    } jsm;
    DBG_START
        DBG_EVENT_VIEW
        DBG_CPU_REG_START1
            *A, *X, *Y, *PC, *S, *P, *MPR[8]
        DBG_CPU_REG_END1

        DBG_IMAGE_VIEWS_START
            MDBG_IMAGE_VIEW(palettes)
            MDBG_IMAGE_VIEW(tiles)
        DBG_IMAGE_VIEWS_END
        DBG_LOG_VIEW
    DBG_END
};

u32 TG16_bus_read(struct TG16 *, u32 addr, u32 old, u32 has_effect);

u32 TG16_huc_read_mem(void *ptr, u32 addr, u32 old, u32 has_effect);
void TG16_huc_write_mem(void *ptr, u32 addr, u32 val);
u32 TG16_huc_read_io(void *ptr);
void TG16_huc_write_io(void *ptr, u32 val);

#endif //JSMOOCH_EMUS_TG16_BUS_H
