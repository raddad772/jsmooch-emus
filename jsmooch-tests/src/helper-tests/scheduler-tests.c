//
// Created by . on 4/25/24.
//

#include "stdio.h"

#include "scheduler-tests.h"
#include "helpers/int.h"
#include "helpers/scheduler.h"

void print_event(struct scheduler_event* event)
{
    printf("\n\nEVENT timecode:%lld %d kind:%s", event->timecode, event->kind, event->kind == SE_keyed_event ? "KEYED" : "BOUND_FUNC");
    if (event->kind == SE_keyed_event) printf(": %llu", event->key);
}

void print_schedule(struct scheduler_t* scheduler)
{
    struct scheduler_event *evt = scheduler->first_event;
    printf("\n---Scheduled list start. Cycles til start: %lld", scheduler_til_next_event(scheduler, 0));
    while (evt) {
        print_event(evt);
        evt = evt->next;
        printf("\nEVT! %016llx", (u64)evt);
    }
    printf("\n---Scheduled list end\n");
}

u32 print_hello(void *r, u64 k, u64 cycles, u32 jitter) {
    printf("\nhello...");
    return 0;
}

void test_scheduler()
{
    struct scheduler_t scheduler;
    scheduler_init(&scheduler);
    //print_schedule(&scheduler);

    scheduler_add(&scheduler, 200, SE_keyed_event, 2, 0);
    print_schedule(&scheduler);

    scheduler_add(&scheduler, -1, SE_keyed_event, 0, 0);
    print_schedule(&scheduler);

    scheduler_add(&scheduler, -1, SE_keyed_event, 50, 0);
    scheduler_add(&scheduler, 500, SE_keyed_event, 4, 0);
    scheduler_add(&scheduler, 150, SE_keyed_event, 1, 0);
    scheduler_add(&scheduler, 300, SE_keyed_event, 3, 0);
    print_schedule(&scheduler);

    struct scheduled_bound_function *f = scheduler_bind_function(&print_hello, NULL);

    scheduler_add(&scheduler, -10, SE_bound_function, 0, f);

    scheduler_delete(&scheduler);
}