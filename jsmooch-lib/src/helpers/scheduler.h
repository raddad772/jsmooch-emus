//
// Created by RadDad772 on 2/28/24.
//

#ifndef JSMOOCH_EMUS_SCHEDULER_H
#define JSMOOCH_EMUS_SCHEDULER_H

#include "int.h"
#include "helpers/cvec.h"

// void* ptr, current timecode, jitter
typedef void (*scheduler_callback)(void *bound_ptr, u64 user_key, u64 current_clock, u32 jitter);

struct scheduled_bound_function { scheduler_callback func; void *ptr; u32 saveinfo; };

struct scheduler_event {
    i64 timecode;    // Timecode can be for instance, cycle in frame

    u64 key; // user-data
    struct scheduled_bound_function bound_func;

    u64 id; // ID that can be used to track & delete
    struct scheduler_event* next;

    u32 *still_sched;
};

/* Scheduler currently, starts at 0 at start of frame.
 * Does each frame seperately.
 * Can't reschedule easily
 *
 * We want to:
 * Make continuous scheduler
 * Completely ignore the past, use a circular buffer
 * Schedule in the future with insertion
 */
#define SCHEDULER_DELETE_NUM 1000
struct scheduler_t {
    u64 max_block_size;

    struct scheduler_event* first_event;

    struct {
        struct scheduler_event *items[SCHEDULER_DELETE_NUM];
        u32 num;
    } to_delete;

    struct scheduled_bound_function schedule_more;
    struct scheduled_bound_function run;

    i64 cycles_left_to_run;
    u64 *clock, *waitstates;

    u64 id_counter;
};

//void scheduler_allocate(struct scheduler_t*, u32 howmany);

enum scheduler_actions {
    SA_run_cycles,
    SA_keyed_function,
    SA_schedule_more,
    SA_exit_loop
};

struct scheduler_action_return {
    enum scheduler_actions action;
    u64 arg;
};

void scheduler_init(struct scheduler_t*, u64 *clock, u64 *waitstates);
void scheduler_delete(struct scheduler_t*);
void scheduler_clear(struct scheduler_t*);

u64 scheduler_only_add_abs(struct scheduler_t *, i64 timecode, u64 key, void *ptr, scheduler_callback callback, u32 *still_sched);
struct scheduler_event *scheduler_add_abs(struct scheduler_t*, i64 timecode, u64 key, u32 do_instant);
void scheduler_delete_if_exist(struct scheduler_t *this, u64 id);
u64 scheduler_add_next(struct scheduler_t *this, u64 key, void *ptr, scheduler_callback callback, u32 *still_sched);

void scheduler_run_for_cycles(struct scheduler_t *this, u64 howmany);
u64 scheduler_bind_or_run(struct scheduler_event *e, void *ptr, scheduler_callback func, i64 timecode, u64 key, u32 *still_sched);

// Combine add with bind
u64 scheduler_add_or_run_abs(struct scheduler_t *this, i64 timecode, u64 key, void *ptr, scheduler_callback callback, u32 *still_sched);

struct scheduled_bound_function* scheduler_bind_function(scheduler_callback func, void *ptr);

#endif //JSMOOCH_EMUS_SCHEDULER_H
