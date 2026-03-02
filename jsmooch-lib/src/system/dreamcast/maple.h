//
// Created by RadDad772 on 3/13/24.
//

#pragma once

#include "component/gpu/cdp1861/cdp1861.h"
#include "helpers/int.h"
namespace DC {
    struct core;
}

namespace DC::MAPLE {

enum devices {
    DK_NONE,
    DK_CONTROLLER
};


struct PORT {
    explicit PORT(DC::core *parent);
    u32 read(u32* more);
    void write(u32 data);
    DC::core *bus;
    PORT *port{};
    u32 num{};


    devices device_kind{};
    void *device_ptr{};

    u32 (*read_device)(void*, u32*){};
    void (*write_device)(void *,u32){};
};

struct core{
#include "generated/maple_decls.h"
    explicit core(DC::core *parent);
    DC::core *bus;
    u32 vblank_repeat_trigger{};

    PORT ports[4];
    void dma_init();
    void write(u32 addr, u64 val, u8 sz, bool* success);
    u64 read(u32 addr, u8 sz, bool* success);
};



}