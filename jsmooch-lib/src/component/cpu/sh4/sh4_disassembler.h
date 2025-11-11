//
// Created by Dave on 2/13/2024.
//

#ifndef JSMOOCH_EMUS_SH4_DISASSEMBLER_H
#define JSMOOCH_EMUS_SH4_DISASSEMBLER_H

#include "helpers/int.h"
#include "helpers/buf.h"

struct dasm_support {
    u32 (*read8)(u32);
    u32 (*read16)(u32);
    u32 (*read32)(u32);

    struct buf strout;
};

void dasm_support_init(dasm_support*);
void dasm_support_delete(dasm_support*);

#endif //JSMOOCH_EMUS_SH4_DISASSEMBLER_H
