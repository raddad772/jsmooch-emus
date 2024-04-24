#ifndef _GB_CPU_H
#define _GB_CPU_H

#include "helpers/int.h"
#include "gb_enums.h"
#include "component/cpu/sm83/sm83.h"
#include "helpers/physical_io.h"

struct GB_timer {
    u32 TIMA;//: u32 = 0
    u32 TMA;// : u32 = 0
    u32 TAC;// : u32 = 0
    u32 last_bit;// : u32 = 0
    u32 TIMA_reload_cycle;// : bool = false
    u32 TMA_reload_cycle; // bool = false
    u32 SYSCLK;// : u32 = 0
    u32 cycles_til_TIMA_IRQ;// : u32 = 0

    struct GB_CPU *raise_IRQ_cpu;
    void (*raise_IRQ)(struct GB_CPU *this);

    struct SM83_regs* cpu_regs;
};

void GB_timer_write_IO(struct GB_timer* this, u32 addr, u32 val);
u32 GB_timer_read_IO(struct GB_timer* this, u32 addr);

struct GB_CPU {
    enum GB_variants variant;
    struct GB_clock* clock;
    struct GB_bus* bus;
    struct SM83 cpu;

    u8 FFregs[256];
    struct GB_timer timer;
    u32 tracing;

    struct {
        struct {
            u32 action_select;
            u32 direction_select;
        } JOYP;
        u32 speed_switch_prepare;
        i32 speed_switch_cnt;
    } io;

    struct {
        u32 cycles_til;
        u32 new_high;
        u32 running;
        u32 index;
        u32 high;
        u32 last_write;
    } dma;

    struct {
        u32 source;
        u32 dest;
        u32 length;
        u32 mode;
        u32 source_index;
        u32 dest_index;
        u32 waiting;
        u32 completed;
        u32 enabled;
        u32 active;
        u32 notify_hblank;
        u32 til_next_byte;
    } hdma;

    struct {
        u32 a;
        u32 b;
        u32 start;
        u32 select;
        u32 up;
        u32 down;
        u32 left;
        u32 right;
    } input_buffer;

    struct physical_io_device *io_device;
};

void GB_CPU_init(struct GB_CPU* this, enum GB_variants variant, struct GB_clock* clock, struct GB_bus* bus);
void GB_CPU_cycle(struct GB_CPU* this);
void GB_CPU_reset(struct GB_CPU* this);
void GB_CPU_quick_boot(struct GB_CPU* this);
u32 GB_CPU_bus_read_IO(struct GB_bus *bus, u32 addr, u32 val);
void GB_CPU_bus_write_IO(struct GB_bus* bus, u32 addr, u32 val);

void GB_CPU_enable_tracing(struct GB_CPU *this);
void GB_CPU_disable_tracing(struct GB_CPU *this);
struct GB_inputs;

#endif