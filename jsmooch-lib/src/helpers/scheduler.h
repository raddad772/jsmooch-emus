//
// Created by RadDad772 on 2/28/24.
//

#ifndef JSMOOCH_EMUS_SCHEDULER_H
#define JSMOOCH_EMUS_SCHEDULER_H

#include "int.h"

typedef u32 (*scheduler_callback)(void*,u32,u32);

struct scheduled_bound_function {
    void *ptr;
    scheduler_callback func;
};


struct scheduler_event {
    i64 timecode;    // Timecode can be for instance, cycle in frame
    u32 event;  // Per-implementation
};

struct scheduler_event_array {
    struct scheduler_event *events;
    u32 used_len;
    u32 allocated_len;
};

struct scheduler_t {
    i64 timecode;
    u32 event_index;
    struct scheduler_event_array array;
};

//void scheduler_allocate(struct scheduler_t* this, u32 howmany);

void scheduler_init(struct scheduler_t* this);
void scheduler_delete(struct scheduler_t* this);
void scheduler_clear(struct scheduler_t* this);
void scheduler_add(struct scheduler_t* this, u64 timecode, u32 event);
void scheduler_ran(struct scheduler_t* this, u64 howmany);
i64 scheduler_til_next_event(struct scheduler_t* this); // Returns time til next event
struct scheduler_event* scheduler_next_event(struct scheduler_t* this); // Returns next event
void scheduler_advance_event(struct scheduler_t* this);
u32 scheduler_at_end(struct scheduler_t* this);

void schedule_event(struct scheduler_t* this, struct scheduled_bound_function* func, i64 timecode);

#endif //JSMOOCH_EMUS_SCHEDULER_H
