//
// Created by RadDad772 on 2/28/24.
//

#include "stdio.h"
#include "assert.h"
#include "stdlib.h"
#include "scheduler.h"

void schedule_event(struct scheduler_t* this, struct scheduled_bound_function* func, i64 timecode)
{
}

void scheduler_init(struct scheduler_t* this)
{
    this->first_event = 0;
    this->timecode = 0;
    this->jitter = 0;
    this->num_events = 0;
}

void scheduler_delete(struct scheduler_t* this)
{
    scheduler_clear(this);
}

void scheduler_clear(struct scheduler_t* this)
{
    this->num_events = 0;
    this->current_key = 0;
    struct scheduler_event* cur = this->first_event;
    while(cur != 0) {
        struct scheduler_event* d = cur;

        cur = cur->next;

        if (d->bound_func)
            free(d->bound_func);
        free(d);
    }
    this->first_event = 0;
}

struct scheduler_event* alloc_event(i64 timecode, enum scheduler_event_kind event_kind, u64 key, struct scheduled_bound_function *bound_func, struct scheduler_event* next)
{
    struct scheduler_event* se = malloc(sizeof(struct scheduler_event));
    se->timecode = timecode;
    se->kind = event_kind;

    se->key = key;
    se->bound_func = bound_func;

    se->next = next;
    return se;
}

struct scheduled_bound_function* scheduler_bind_function(scheduler_callback func, void *ptr)
{
    struct scheduled_bound_function *f = malloc(sizeof(struct scheduled_bound_function));
    f->func = func;
    f->ptr = ptr;
    return f;
}

void scheduler_add(struct scheduler_t* this, i64 timecode, enum scheduler_event_kind event_kind, u64 key, struct scheduled_bound_function* bound_func) {
    u32 instant = false;
    if ((timecode <= this->timecode) || (timecode == 0)) {
        instant = true;
    }

    if ((instant) && (event_kind == SE_bound_function)) {
        assert(bound_func);
        bound_func->func(bound_func->ptr, key, this->timecode, this->jitter);
        return;
    }
    else if (instant) {
        struct scheduler_event* fe = this->first_event;
        this->first_event = alloc_event(timecode, event_kind, key, bound_func, fe);
        return;
    }

    // Insert into linked list at correct place
    // No events currently...
    if (this->first_event == 0) {
        this->first_event = alloc_event(timecode, event_kind, key, bound_func, 0);
        return;
    }
    else if (this->first_event->next == 0) {
        if (timecode <= this->first_event->timecode) {// Insert before first
            this->first_event = alloc_event(timecode, event_kind, key, bound_func, this->first_event);
        }
        else {
            this->first_event->next = alloc_event(timecode, event_kind, key, bound_func, 0);
        }
        return;
    }

    // First see if we insert to the first...
    if (this->first_event->timecode > timecode) {
        this->first_event = alloc_event(timecode, event_kind, key, bound_func, this->first_event);
        return;
    }

    // If we're at this point, the list has two or more events, and does not need an insertion at the start.
    // Find one to insert AFTER
    struct scheduler_event *insert_after = this->first_event;
    while(insert_after->next != 0) {
        if (insert_after->next->timecode > timecode) break;
        insert_after = insert_after->next;
    }

    insert_after->next = alloc_event(timecode, event_kind, key, bound_func, insert_after->next);
}

i64 scheduler_til_next_event(struct scheduler_t* this, i64 timecode) {
    if (!this->first_event) return this->jitter;
    i64 r = this->first_event->timecode - timecode;
    if (r > this->jitter) r = this->jitter;
    return r;
}

void scheduler_ran_cycles(struct scheduler_t* this, i64 howmany) {
    this->timecode += howmany;
}

u64 scheduler_next_event_if_any(struct scheduler_t* this)
{
    if (this->first_event == 0) return -1;
    struct scheduler_event *e;
    while (this->first_event != NULL) {
        // If we are ahead of current....
        if (this->timecode >= this->first_event->timecode) {
            e = this->first_event;
            if (this->first_event->kind == SE_bound_function) {
                e->bound_func->func(e->bound_func->ptr, e->key, this->timecode, this->jitter);
                free(e->bound_func);
                e->bound_func = NULL;
                this->first_event = this->first_event->next;
                free(e);
                continue;
            }
            else {
                u64 r = e->key;
                this->first_event = this->first_event->next;
                free(e);
                return r;
            }
        }
        if (this->timecode < this->first_event->timecode) return -1;
    }
    return -1;
}
