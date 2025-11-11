//
// Created by . on 6/12/25.
//

#ifndef JSMOOCH_EMUS_HUC6280_H
#define JSMOOCH_EMUS_HUC6280_H

#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/debugger/debuggerdefs.h"
#include "helpers/scheduler.h"
#include "component/audio/huc6280_psg/huc6280_psg.h"

#include "huc6280_opcodes.h"

//#define HUC6280_TESTING

union HUC6280_P {

    struct {
        // N V T B D I Z C
        u8 C : 1;
        u8 Z : 1;
        u8 I : 1;
        u8 D : 1;
        u8 B : 1;
        u8 T : 1;
        u8 V : 1;
        u8 N : 1;
    };
    u8 u;
};

union HUC6280_IRQ_reg {
    struct {
        u8 IRQ2 : 1; // HuCard, Break
        u8 IRQ1 : 1; // VDC
        u8 TIQ : 1; // Timer IRQ
    };

    u8 u;
};

struct HUC6280_regs {
    union HUC6280_P P;
    u32 A, X, Y;
    u32 S, PC;

    u32 MPR[8];
    u32 MPL;

    u32 TR[8], TA;
    u32 TCU;

    u32 do_IRQ;

    u32 IR;

    u32 timer_startstop;

    union HUC6280_IRQ_reg IRQR, IRQD, IRQR_polled;


    u32 clock_div;
};

struct HUC6280_pins {
    u32 D, Addr, RD, WR, BM, IRQ1, IRQ2, TIQ; // M is MRQ. BM is BlockMove.

    void *debugger_interface;
    u32 debugger_mem_bus;
};

struct HUC6280 {
    struct HUC6280_regs regs;
    struct HUC6280_pins pins;
    struct HUC6280_PSG psg;
    struct scheduler_t *scheduler;
    HUC6280_ins_func current_instruction;
    u32 PCO;

    void *read_ptr, *write_ptr, *read_io_ptr, *write_io_ptr;
    u32 (*read_func)(void *ptr, u32 addr, u32 old, u32 has_effect);
    u32 (*read_io_func)(void *ptr);
    void (*write_func)(void *ptr, u32 addr, u32 val);
    void (*write_io_func)(void *ptr, u32 val);

    u32 ins_decodes;

    struct {
        u8 counter, reload;
        u64 sch_id;
        u32 still_sch;

        u64 sch_interval;
    } timer;

    struct {
        u8 buffer;
    } io;

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
        u64 my_cycles;
        u64 *cycles;

    } trace;

    u32 extra_cycles;

    DBG_START
        DBG_EVENT_VIEW_START
        IRQ1, IRQ2, TIQ
        DBG_EVENT_VIEW_END

        DBG_TRACE_VIEW

        DBG_LOG_VIEW
    DBG_END

};

void HUC6280_init(HUC6280 *, scheduler_t *scheduler, u64 clocks_per_second);
void HUC6280_delete(HUC6280 *);
void HUC6280_reset(HUC6280 *);
void HUC6280_setup_tracing(HUC6280 *, jsm_debug_read_trace *strct, u64 *trace_cycle_ptr, i32 source_id);
void HUC6280_poll_IRQs(HUC6280_regs *regs, HUC6280_pins *pins);
void HUC6280_tick_timer(HUC6280 *);

void HUC6280_schedule_first(HUC6280 *, u64 clock);

void HUC6280_cycle(HUC6280 *); // Only really affects "CPU-ish" stuff

// Internal cycle, handles catching IO or scheduling
void HUC6280_internal_cycle(void *ptr, u64 key, u64 clock, u32 jitter);

#endif //JSMOOCH_EMUS_HUC6280_H
