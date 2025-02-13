//
// Created by . on 2/13/25.
//

#ifndef JSMOOCH_EMUS_PS1_DEVICE_H
#define JSMOOCH_EMUS_PS1_DEVICE_H

#include "helpers/int.h"

enum DsrStateKind {
    PS1DSK_idle,
    PS1DSK_pending,
    PS1DSK_active
};

struct dsr_state {
    enum DsrStateKind kind;
    i32 delay, duration;

};

struct u8_dsr_state {
    u8 r1;
    struct dsr_state r2;
};

enum PS1_pad_target {
    PS1PT_PadMemCard1,
    PS1PT_PadMemCard2
};

enum PS1_peripheral_kinds {
    PS1PK_disconnected,
    PS1PK_gamepad,
    PS1PK_memorycard
};

struct PS1_gamepad {
    u16 state;
    u8 memory[16];
};

struct PS1_peripheral {
    u8 seq;
    u32 active;
    enum PS1_peripheral_kinds kind;

    union {
        struct PS1_gamepad gamepad;
        //struct PS1_memcard memcard;
    } device;
};

enum PS1TransferStateKind {
    PS1TSK_idle,
    PS1TSK_tx_start,
    PS1TSK_rx_available
};

struct PS1_transfer_state {
    enum PS1TransferStateKind kind;
    i32 delay;
    i32 rx_available_delay;
    u8 value;
};

struct PS1_pad_memcard {
    u16 baud_div;
    u8 mode;
    u32 tx_en;
    i16 tx_pending;
    u32 select;
    enum PS1_pad_target target;
    u8 unknown;
    u32 rx_en, dsr_it, interrupt;
    u8 response;
    u32 rx_not_empty;

    struct PS1_peripheral pad1;
    struct dsr_state pad1_dsr;

    struct PS1_peripheral pad2;
    struct dsr_state pad2_dsr;

    struct PS1_peripheral memcard1;
    struct dsr_state memcard1_dsr;

    struct PS1_peripheral memcard2;
    struct dsr_state memcard2_dsr;

    struct PS1_transfer_state transfer_state;

};

void PS1_peripheral_init(struct PS1_peripheral *, enum PS1_peripheral_kinds kind);
void PS1_peripheral_delete(struct PS1_peripheral *);

struct PS1;
void PS1_run_controllers(struct PS1 *, u32 numcycles);

#endif //JSMOOCH_EMUS_PS1_DEVICE_H
