//
// Created by RadDad772 on 3/13/24.
//

#ifndef JSMOOCH_EMUS_MAPLE_H
#define JSMOOCH_EMUS_MAPLE_H

#include "helpers/int.h"

enum MAPLE_devices {
    MAPLE_NONE,
    MAPLE_CONTROLLER
};

struct MAPLE_port;

struct MAPLE_port {
    struct MAPLE_port *port;
    enum MAPLE_devices device_kind;
    void *device_ptr;

    u32 (*read_device)(void *, u32*);
    void (*write_device)(void *,u32);
};

struct DC;

void MAPLE_port_init(MAPLE_port*);

u64 maple_read(DC*, u32 addr, u32 sz, u32* success);
void maple_write(DC*, u32 addr, u64 val, u32 sz, u32* success);
void maple_dma_init(DC*);


#endif //JSMOOCH_EMUS_MAPLE_H
