//
// Created by RadDad772 on 4/14/24.
//

#ifndef JSMOOCH_EMUS_M68000_H
#define JSMOOCH_EMUS_M68000_H

#include "helpers/int.h"

struct M68k_regs {
    /*
     * When a data register is used as either a source or a destination operand,
     * only the appropriate low-order portion is changed; the remaining high-order
     * portion is neither used nor changed.
     */
    u32 D[8];
    u32 A[8];
    u32 PC;
    union SR {
        struct {
            u16 C: 1;
            u16 V: 1;
            u16 Z: 1;
            u16 N: 1;
            u16 X: 1;

            u16 _: 3;

            u16 I: 3;
            u16 _2: 2;
            u16 S: 1;
            u16 _3: 1;
            u16 T: 1;
        };
        u16 u;
    };

    u32 SSP; // System stack pointer

    // Prefetch registers
    u32 IRC; // holds last word prefetched from external memory
    u32 IR; // instruction currently decoding
    u32 IRD; // instruction currently executing

};

struct M68k {
    struct M68k_regs regs;
};


void M68k_init(struct M68k* this);
void M68k_delete(struct M68k* this);
void M68k_reset(struct M68k* this);

#endif //JSMOOCH_EMUS_M68000_H
