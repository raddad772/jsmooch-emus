//
// Created by . on 6/12/25.
//

#ifndef JSMOOCH_EMUS_HUC6280_H
#define JSMOOCH_EMUS_HUC6280_H

#include "helpers/int.h"
#include "helpers/debug.h"

union HUC6280_P {
    struct {

    };
    u8 u;
};

struct HUC6280_regs {
    union HUC6280_P P;
    u32 A, X, Y;
    u32 SP, PC;
};

struct HUC6280_pins {
    u32 D, Addr, RW, M;
};

struct HUC6280 {
    struct HUC6280_regs regs;
    struct HUC6280_pins pins;

    struct {
        struct jsm_debug_read_trace strct;
        struct jsm_string str, str2;
        u32 ins_PC;
        i32 source_id;

        struct {
            struct dbglog_view *view;
            u32 id;

            u32 id_read, id_write;
        } dbglog;
        u32 ok;

    } trace;
};

void HUC6280_init(struct HUC6280 *);
void HUC6280_delete(struct HUC6280 *);
void HUC6280_reset(struct HUC6280 *);
void HUC6280_setup_tracing(struct HUC6280* this, struct jsm_debug_read_trace *strct);

#endif //JSMOOCH_EMUS_HUC6280_H
