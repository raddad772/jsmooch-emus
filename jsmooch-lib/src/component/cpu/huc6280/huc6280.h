//
// Created by . on 6/12/25.
//

#pragma once

#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/debugger/debuggerdefs.h"
#include "helpers/scheduler.h"
#include "component/audio/huc6280_psg/huc6280_psg.h"

#include "huc6280_opcodes.h"

//#define HUC6280_TESTING
namespace HUC6280 {

union regs_P {

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
    u8 u{};
};

union regs_IRQ {
    struct {
        u8 IRQ2 : 1; // HuCard{}, Break
        u8 IRQ1 : 1; // VDC
        u8 TIQ : 1; // Timer IRQ
    };

    u8 u{};
};

struct regs {
    regs_P P{};
    u32 A{}, X{}, Y{};
    u32 S{}, PC{};

    u32 MPR[8]{};
    u32 MPL{};

    u32 TR[8]{}, TA{};
    u32 TCU{};

    u32 do_IRQ{};

    u32 IR{};

    u32 timer_startstop{};

    regs_IRQ IRQR{}, IRQD{}, IRQR_polled{};


    u32 clock_div{};
};

struct pins {
    u32 D{}, Addr{}, RD{}, WR{}, BM{}, IRQ1{}, IRQ2{}, TIQ{}; // M is MRQ. BM is BlockMove.

    void *debugger_interface{};
    u32 debugger_mem_bus{};
};

struct core {
    explicit core(scheduler_t *scheduler_in, u64 clocks_per_second) : scheduler(scheduler_in) {}
    regs regs{};
    pins pins{};
    PSG::core psg{};
    scheduler_t *scheduler{};
    ins_func current_instruction{};
    u32 PCO{};

    void *read_ptr{}, *write_ptr{}, *read_io_ptr{}, *write_io_ptr{};
    u32 (*read_func)(void *ptr, u32 addr, u32 old, u32 has_effect){};
    u32 (*read_io_func)(void *ptr){};
    void (*write_func)(void *ptr, u32 addr, u32 val){};
    void (*write_io_func)(void *ptr, u32 val){};

    u32 ins_decodes{};

    struct {
        u8 counter{}, reload{};
        u64 sch_id{};
        u32 still_sch{};

        u64 sch_interval{};
    } timer{};

    struct {
        u8 buffer{};
    } io{};

    struct {
        jsm_debug_read_trace strct{};
        jsm_string str{100}, str2{100};
        u32 ins_PC{};
        i32 source_id{};

        struct {
            struct dbglog_view *view{};
            u32 id{};

            u32 id_read{}, id_write{};
        } dbglog{};
        u32 ok{};
        u64 my_cycles{};
        u64 *cycles{};

    } trace{};

    u32 extra_cycles{};

    DBG_START
        DBG_EVENT_VIEW_START
        IRQ1{}, IRQ2{}, TIQ{}
        DBG_EVENT_VIEW_END

        DBG_TRACE_VIEW

        DBG_LOG_VIEW
    DBG_END

};

}