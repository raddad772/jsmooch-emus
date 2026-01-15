#pragma once

#include "helpers/int.h"
#include "helpers/physical_io.h"
#include "helpers/debugger/debuggerdefs.h"

#include "component/cpu/sm83/sm83.h"
#include "gb_bus.h"

#include "gb_enums.h"
#include "component/cpu/m6502/m6502.h"

namespace GB {

struct CPU;

struct timer {
    explicit timer(void (*raise_IRQ_func)(CPU *), CPU *cpu_in);
    void SYSCLK_change(u32 new_value);
    void inc();
    void detect_edge(u32 before, u32 after);


    u32 TIMA{};//: u32 = 0
    u32 TMA{};// : u32 = 0
    u32 TAC{};// : u32 = 0
    u32 last_bit{};// : u32 = 0
    u32 TIMA_reload_cycle{};// : bool = false
    u32 SYSCLK{};// : u32 = 0
    u32 cycles_til_TIMA_IRQ{};// : u32 = 0

    DBG_EVENT_VIEW_ONLY;

    CPU *raise_IRQ_ptr{};
    void (*raise_IRQ)(CPU *){};

    void write_IO(u32 addr, u32 val);
    u32 read_IO(u32 addr) const;
};

struct core;

struct CPU {
    explicit CPU(core* parent, GB::clock *clock_in, variants variant_in);
    u32 read_IO(u32 addr, u32 val);
    void write_IO(u32 addr, u32 val);

    void enable_tracing();
    void disable_tracing();

    void cycle();
    void reset();
    void quick_boot();
    void dma_eval();
    u32 hdma_run();
    void ghdma_run();
    u32 hdma_eval();
    void switch_speed() const;
    void update_inputs();
    u32 get_input();

    variants variant{};
    clock* clock{};
    core* bus{};
    SM83::core cpu{};

    u8 FFregs[256]{};
    timer timer;
    bool tracing{};

    struct {
        struct {
            u32 action_select{};
            u32 direction_select{};
        } JOYP{};
        u32 speed_switch_prepare{};
        i32 speed_switch_cnt{-1};
    } io{};

    struct {
        u32 cycles_til{};
        u32 new_high{};
        u32 running{};
        u32 index{};
        u32 high{};
        u32 last_write{};
    } dma{};

    struct {
        u32 source{};
        u32 dest{};
        u32 length{};
        u32 mode{};
        u32 source_index{};
        u32 dest_index{};
        u32 waiting{};
        u32 completed{};
        u32 enabled{};
        u32 active{};
        u32 notify_hblank{};
        u32 til_next_byte{};
    } hdma{};

    struct {
        u32 a{};
        u32 b{};
        u32 start{};
        u32 select{};
        u32 up{};
        u32 down{};
        u32 left{};
        u32 right{};
    } input_buffer{};

    DBG_EVENT_VIEW_ONLY;
    cvec_ptr<physical_io_device> device_ptr{};
};

}
