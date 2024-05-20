//
// Created by RadDad772 on 4/14/24.
//

#ifndef JSMOOCH_EMUS_M68000_H
#define JSMOOCH_EMUS_M68000_H

#include "helpers/int.h"
#include "m68000_opcodes.h"

struct M68k_regs {
    /*
     * When a data register is used as either a source or a destination operand,
     * only the appropriate low-order portion is changed; the remaining high-order
     * portion is neither used nor changed.
     */
    u32 D[8];
    u32 A[8];
    u32 PC;
    union {
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
    } SR;

    u32 SSP; // System stack pointer

    // Prefetch registers
    u32 IRC; // holds last word prefetched from external memory
    u32 IR; // instruction currently decoding
    u32 IRD; // instruction currently executing

};

struct M68k_pins {
    u32 FC; // Function codes
    u32 Addr;
    u32 D[2];
    u32 DTACK;
    u32 AS; // Address Strobe
    u32 RW; // Read-write
    u32 IPL; // Interrupt request level
    u32 UDS;
    u32 LDS;
};

enum M68k_states {
    M68kS_read8,
    M68kS_read16,
    M68kS_write8,
    M68kS_write16,
    M68kS_decode, // decode
    M68kS_exec, // execute
};

struct M68k_ins_t;

struct M68k {
    struct M68k_regs regs;
    struct M68k_pins pins;


    enum M68k_states state;

    struct {
        u32 TCU; // Subcycle of like rmw8 etc.
        u32 addr;
        u32 data;
        u32 done;
        void (*func)(struct M68k*);
    } bus_cycle;

    struct {
        u32 TCU;
        u32 done;
        u32 addr;
        u32 data;
        u32 data32;
        void (*func)(struct M68k*, struct M68k_ins_t*);
        struct M68k_ins_t *ins;
    } instruction;
    u32 wait_steps;



};

void M68k_cycle(struct M68k* this);
void M68k_init(struct M68k* this);
void M68k_delete(struct M68k* this);
void M68k_reset(struct M68k* this);

#endif //JSMOOCH_EMUS_M68000_H
