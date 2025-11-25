//
// Created by . on 4/16/25.
//

#pragma once

#include "helpers/int.h"

#include "helpers/debugger/debuggerdefs.h"
#include "helpers/debugger/debugger.h"
#include "helpers/debug.h"

namespace SPC700 {
   
constexpr bool TEST{false};

union regs_P {
    struct {
        u32 C : 1;
        u32 Z : 1;
        u32 I : 1;
        u32 H : 1;
        u32 B : 1;
        u32 P : 1;
        u32 V : 1;
        u32 N : 1;
        u32 DO : 16;
    };
    u32 v;
    u8 getbyte();
    void setbyte(u8 val);
};

inline u8 regs_P::getbyte() {
    return v & 0xFF;
}

inline void regs_P::setbyte(u8 val)
{
    v = val;
    DO = P << 8;
}

struct regs {
    regs_P P{};

    u32 IR{};
    i32 TR{}, TA{}, TA2{};

    i32 opc_cycles{};

    u8 A{}, X{}, Y{};
    u16 SP{}, PC{};
};

struct core {
    explicit core(u64 *clock_ptr) : clock(clock_ptr) { }
    regs regs{};
    void reset();
    void cycle(i64 how_many);
    void setup_tracing(jsm_debug_read_trace *strct);
    u8 readIO(u32 addr);

private:
    void advance_timers(i32 cycles);
    void trace_format();

    void writeIO(u32 addr, u32 val);
    void log_write(u32 addr, u32 val);
    void log_read(u32 addr, u32 val);

public:
    u8 read8D(u32 addr);
    void write8D(u32 addr, u32 val);
    void write8(u32 addr, u32 val);
    u8 read8(u32 addr);
    void *read_ptr{}, *write_ptr{};
    u8 (*read_dsp)(void *, u16 addr){};
    void (*write_dsp)(void *, u16 addr, u8 val){};

    u64 *clock{};
    u64 int_clock{};

    i32 cycles{};
    u32 WAI{}, STP{};

    u8 dsp_regs[0x80]{};

    struct {
        u32 ROM_readable{};
        u32 T0_enable{}, T1_enable{}, T2_enable{};
        u16 DSPADDR{}, DSPDATA{};

        u8 CPUI[4]{};
        u8 CPUO[4]{};
    } io{};

    struct {
        i32 divider{}, counter{};
        struct {
            u32 target{}, out{};
        };
    } timers[3]{};

    struct {
        jsm_debug_read_trace strct{};
        jsm_string str{1000}, str2{200};
        u32 ins_PC{};
        i32 source_id{};

        struct {
            dbglog_view *view{};
            u32 id{};

            u32 id_read{}, id_write{};
        } dbglog{};
        u32 ok{};

    } trace{};

    u8 RAM[0x10000]{};

    DBG_START
        DBG_EVENT_VIEW
        DBG_TRACE_VIEW
    DBG_END

};

typedef void (*ins_func)(core &);
}