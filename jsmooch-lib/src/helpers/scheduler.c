//
// Created by RadDad772 on 2/28/24.
//

#include "stdio.h"
#include "assert.h"
#include "stdlib.h"
#include "scheduler.h"

void schedule_event(struct scheduler_t* this, struct scheduled_bound_function* func, i64 timecode)
{
    //printf("\nIM SUPPOSED TO SCHEDULE A FUNCTION FOR %lld CYCLES FROM NOW! (%llu)", timecode, this->timecode);
    //func->func(func->ptr, this->timecode, 5);
    //TODO: MAKE THIS WORK!
}

static void scheduler_array_init(struct scheduler_event_array* this)
{
    this->allocated_len = 20;
    this->used_len = 0;
    this->events = malloc(20 * sizeof(struct scheduler_event));
}

static void scheduler_array_delete(struct scheduler_event_array* this)
{
    if (this->events != NULL) {
        free(this->events);
    }
    this->allocated_len = 0;
    this->used_len = 0;
    this->events = NULL;
}

void scheduler_init(struct scheduler_t* this)
{
    scheduler_array_init(&this->array);
    this->timecode = 0;
    this->event_index = 0;
}

void scheduler_clear(struct scheduler_t* this)
{
    this->timecode = 0;
    this->event_index = 0;
    this->array.used_len = 0;
    //scheduler_add(this, 0, 0);
}

void scheduler_add(struct scheduler_t* this, u64 timecode, u32 event) {
    struct scheduler_event* evt = &this->array.events[this->array.used_len];
    this->array.used_len++;
    evt->timecode = timecode;
    evt->event = event;
}

void scheduler_delete(struct scheduler_t* this)
{
    scheduler_array_delete(&this->array);
}

i64 scheduler_til_next_event(struct scheduler_t* this) {
    return this->array.events[this->event_index].timecode - this->timecode;
}

void scheduler_ran(struct scheduler_t* this, u64 howmany)
{
    this->timecode += howmany;
}

u32 scheduler_at_end(struct scheduler_t* this) {
    return this->event_index >= this->array.used_len;
}

struct scheduler_event* scheduler_next_event(struct scheduler_t* this) {
    if (this->event_index >= this->array.used_len) return NULL;
    return &this->array.events[this->event_index];
}

void scheduler_advance_event(struct scheduler_t* this)
{
    this->event_index++;
}

void scheduler_allocate(struct scheduler_t* this, u32 howmany)
{
    assert(1!=0);
}

/*
 do {
    do_cycles = cycles_til_next_event = event[current_event].cycles - current_cycles;
    if (cycles_til_next_event < cycles_left_to_emulate) do_cycles = cycles_left_to_emulate;
    current_cycles += emulate(cycles_til_next_event);
    if (interrupted && needs_reschedule) {reschedule; continue;}
    if (cycles_left_to_emulate == 0) break;
    current_event++;
    fire_event(event[current_event]);
}
 */
