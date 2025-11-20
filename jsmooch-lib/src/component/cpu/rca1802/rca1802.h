//
// Created by . on 11/18/25.
//

#pragma once

#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/debugger/debuggerdefs.h"
#include "helpers/debugger/debugger.h"
#include "helpers/serialize/serialize.h"

/*
 *In order for IO/DMA to work properly, you need to do
 *
 * RCA1802_cycle()
 * check_and_service_reads()
 * devices here
 * check_and_service_writes()
*/

namespace RCA1802 {
    constexpr bool TEST_MODE = true;

struct pins {
    enum SC {
        S0_fetch=0,
        S1_execute=1,
        S2_dma=2,
        S3_interrupt=3
    }SC{}; //
    enum clear_wait {
        LOAD = 0,
        RESET = 1,
        PAUSE = 2,
        RUN = 3
    } clear_wait{};

    u8 EF{}; // EF lines

    u8 INTERRUPT{};
    u8 DMA_IN{};
    u8 DMA_OUT{}; // priority: DMA in, DMA out, interrupt

    u8 MRD{}, MWR{}; // Memory read and write

    u8 Q{};
    u16 Addr{}; // Memory Address lines
    u8 D{}; // Data lines
    u8 N{}; // IO device line selector
};

struct regs {
    union {
        struct {
            u8 lo, hi;
        };
        u16 u{0};
    } R[16];

    u8 N{}; // holds low 4 bits of instruction
    u8 I{}; // holds high 4 bits of instruction
    union {
        struct {
            u8 lo : 4;
            u8 hi : 4;
        };
        u8 u{};
    } T{};// high nibble = X, low = P after interrupt
    u8 IE{};
    u8 P{}; // program counter designator
    u8 X{}; // operand pointer designator

    u8 D{};

    u8 IR{}; // holds current instruction opcode
    u8 B{};
    u8 DF{}; // ALU carry
};

struct core;
typedef void (*ins_func)(core*);

struct core {
    pins pins{};
    regs regs{};

    void cycle();
    void reset();

    int num_fetches;

    ins_func ins{};
    void prepare_fetch();
    i32 execs_left{};
    bool perform_interrupts();

    DBG_EVENT_VIEW_ONLY_START
    IRQ, DMA
    DBG_EVENT_VIEW_ONLY_END
    struct {
        u32 ok{};
        u64 *cycles{};
        u64 my_cycles{};
        jsm_debug_read_trace strct{};
    } trace{};

    void setup_tracing(jsm_debug_read_trace *strct, u64 *trace_cycle_pointer);


private:
    void fetch();
    void execute();
    void dma_in();
    void dma_out();
    void interrupt();
    void do_out();
    void do_in();
    void dma_end();
    void interrupt_end();

    void prepare_execute();
    void prepare_execute_70();
    void prepare_execute_F0();

    void do_load(u8 addr_ptr);
    void do_store(u8 addr_ptr, u8 val);
    void do_immediate();
};

}