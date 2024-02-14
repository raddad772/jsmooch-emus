//
// Created by Dave on 2/11/2024.
//

#ifndef JSMOOCH_EMUS_DREAMCAST_H
#define JSMOOCH_EMUS_DREAMCAST_H

#include "helpers/buf.h"
#include "helpers/sys_interface.h"

#include "component/cpu/sh4/sh4_interpreter_opcodes.h"
#include "component/cpu/sh4/sh4_interpreter.h"

void DC_new(struct jsm_system* system, struct JSM_IOmap *iomap);
void DC_delete(struct jsm_system* system);

struct DC {
    struct SH4 sh4;

    u8 RAM[16 * 1024 * 1024];
    u8 VRAM[8 * 1024 * 1024];

    struct buf BIOS;
    struct buf ROM;
};


#endif //JSMOOCH_EMUS_DREAMCAST_H
