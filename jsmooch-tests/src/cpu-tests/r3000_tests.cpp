//
// Created by . on 3/11/26.
//
#include "component/cpu/r3000/r3000.h"
#include "helpers/better_irq_multiplexer.h"
#include "helpers/scheduler.h"

struct test_cpu_state {
    u32 R[32]{};
    u32 hi{}, lo{}, EPC{}, PC{};
    struct {
        struct {
            i32 target;
            u32 val;
        } load;

        struct {
            u32 slot{}; // Is a delay slot?
            u32 take{}; // Are we taking it?
            u32 target{}; // What is the target?
        } branch;
    } delay;
};

enum : u64 { NOACTION = 0, READ = 1, WRITE = 2, FETCH = 4};

struct test_cycle {
    i64 val{}; // 1
    u32 actions{}; // 2
    i64 addr{}; // 3
    u32 sz{}; // 4

    bool visited{};
    u32 my_actions{};
};

struct test {
    test_cpu_state initial{}, final{};
    u32 num_cycles{};
    test_cycle cycles[50];
};

struct gstate_t {
    u64 waitstates{};
    u64 clock{};
    scheduler_t scheduler{&clock};
    IRQ_multiplexer_b irq_multiplexer{0};
    R3000::core cpu {&clock, &waitstates, &scheduler, &irq_multiplexer};
    test test{};
    bool failed{};
} gstate{};

u32 do_read(void *ptr, u32 addr, u8 sz) {
    for (u32 i = 0; i < gstate.test.num_cycles; i++) {
        auto &c = gstate.test.cycles[i];
        if (c.addr == addr) {
            c.visited = true;
            c.my_actions |= READ;
            if (c.sz != sz) {
                gstate.failed = true;
                printf("\nFAIL WRONG READ SIZE");
            }
            return c.val;
        }
    }
    gstate.failed = true;
    printf("\nFAIL WRONG READ ADDR %08x", addr);
    return 0;
}

void do_write(void *ptr, u32 addr, u8 sz, u32 val) {
    for (u32 i = 0; i < gstate.test.num_cycles; i++) {
        auto &c = gstate.test.cycles[i];
        if (c.addr == addr) {
            c.visited = true;
            c.my_actions |= WRITE;
            if (c.sz != sz) {
                gstate.failed = true;
                printf("\nFAIL WRONG WRITE SIZE");
            }
            if (c.val != val) {
                gstate.failed = true;
                printf("\nFAIL WRONG WRITE VALUE");
            }
            return;
        }
    }
    printf("\nFAIL BAD WRITE ADDRESS %08x", addr);
}

u32 do_fetch_ins(void *ptr, u32 addr) {
    for (u32 i = 0; i < gstate.test.num_cycles; i++) {
        auto &c = gstate.test.cycles[i];
        if (c.addr == addr) {
            c.visited = true;
            c.my_actions |= FETCH;
            return c.val;
        }
    }
    gstate.failed = true;
    printf("\nFAIL WRONG FETCH ADDR %08x", addr);
    return 0;
}

void test_r3000() {
    gstate.cpu.read = &do_read;
    gstate.cpu.write = &do_write;
    gstate.cpu.fetch_ins = &do_fetch_ins;
}