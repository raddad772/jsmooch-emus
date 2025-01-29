//
// Created by RadDad772 on 2/28/24.
//

#ifndef JSMOOCH_EMUS_SCHEDULER_H
#define JSMOOCH_EMUS_SCHEDULER_H

#include "int.h"
#include "helpers/cvec.h"

enum scheduler_event_kind {
    SE_bound_function,
    SE_keyed_event
};

// void* ptr, current timecode, jitter
typedef u32 (*scheduler_callback)(void *bound_ptr, u64 user_key, u64 current_clock, u32 jitter);

struct scheduled_bound_function { scheduler_callback func; void *ptr; u32 saveinfo; };


struct scheduler_event {
    u64 timecode;    // Timecode can be for instance, cycle in frame
    enum scheduler_event_kind kind;

    u64 key; // user-data
    struct scheduled_bound_function* bound_func;

    u64 id; // ID that can be used to track & delete
    struct scheduler_event* next;
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

struct scheduler_t {
    i64 timecode;
    i64 jitter;

    i32 current_key;

    u32 num_events;
    struct scheduler_event* first_event;


    u32 exit_current; // Programs should exit any inner loops if they are checking this...
    u64 id_counter;

};

//void scheduler_allocate(struct scheduler_t*, u32 howmany);

void scheduler_init(struct scheduler_t*);
void scheduler_delete(struct scheduler_t*);
void scheduler_clear(struct scheduler_t*);
u64 scheduler_add(struct scheduler_t* this, i64 timecode, enum scheduler_event_kind event_kind, u64 key, struct scheduled_bound_function* bound_func);
void scheduler_delete_if_exist(struct scheduler_t *this, u64 id);
i64 scheduler_til_next_event(struct scheduler_t*, i64 timecode); // Returns time til next event
u64 scheduler_next_event_if_any(struct scheduler_t*);
void scheduler_ran_cycles(struct scheduler_t*, i64 howmany);

struct scheduled_bound_function* scheduler_bind_function(scheduler_callback func, void *ptr);

#endif //JSMOOCH_EMUS_SCHEDULER_H
