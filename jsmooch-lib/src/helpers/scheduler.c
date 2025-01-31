//
// Created by RadDad772 on 2/28/24.
//

#include <string.h>
#include "stdio.h"
#include "assert.h"
#include "stdlib.h"
#include "scheduler.h"
#include "helpers/debug.h"

static void del_event(struct scheduler_t *this, struct scheduler_event *e)
{
    this->to_delete.items[this->to_delete.num++] = e;
    if (this->to_delete.num == SCHEDULER_DELETE_NUM) {
        for (u32 i = 0; i < SCHEDULER_DELETE_NUM; i++) {
            free(this->to_delete.items[i]);
            this->to_delete.items[i] = NULL;
        }
        this->to_delete.num = 0;
    }
}

void scheduler_init(struct scheduler_t* this, u64 *clock)
{
    memset(this, 0, sizeof(*this));
    this->clock = clock;
    this->id_counter = 100;
}

void scheduler_delete(struct scheduler_t* this)
{
    scheduler_clear(this);
}

void scheduler_clear(struct scheduler_t* this)
{
    struct scheduler_event* cur = this->first_event;
    while(cur != NULL) {
        struct scheduler_event* d = cur;

        cur = cur->next;

        del_event(this, d);
    }
    this->first_event = NULL;
}

struct scheduler_event* alloc_event(struct scheduler_t *this, i64 timecode, u64 key, struct scheduler_event* next, u64 id) {
    struct scheduler_event *se;

    // Reuse a struct if we can. FOR SPEED!
    if (this->to_delete.num) {
        this->to_delete.num--;
        se = this->to_delete.items[this->to_delete.num];
        this->to_delete.items[this->to_delete.num] = NULL;
    }
    else se = malloc(sizeof(struct scheduler_event));

    se->timecode = timecode;
    se->id = id;
    se->key = key;
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

void scheduler_delete_if_exist(struct scheduler_t *this, u64 id)
{
    // If empty...
    if (this->first_event == NULL) return;

    // If first/one...
    struct scheduler_event *e;
    if (this->first_event->id == id) {
        e = this->first_event;
        this->first_event = e->next;

        del_event(this, e);
        return;
    }
    e = this->first_event;
    struct scheduler_event *last = NULL;

    while (e != NULL) {
        if (e->id == id) {
            // Delete current.
            if (last) last->next = e->next;
            del_event(this, e);
            return;
        }

        last = e;
        e = e->next;
    }
}

u64 scheduler_bind_or_run(struct scheduler_event *e, void *ptr, scheduler_callback func, i64 timecode, u64 key)
{
    if (!e) {
        func(ptr, key, timecode, 0);
        return 0;
    }
    else {
        e->bound_func.ptr = ptr;
        e->bound_func.func = func;
        return e->id;
    }
}

u64 scheduler_add_or_run_abs(struct scheduler_t *this, i64 timecode, u64 key, void *ptr, scheduler_callback callback)
{
    struct scheduler_event *e = scheduler_add_abs(this, timecode, key);
    return scheduler_bind_or_run(e, ptr, callback, timecode, key);
}


struct scheduler_event *scheduler_add_abs(struct scheduler_t* this, i64 timecode, u64 key) {
    u32 instant = 0;
    assert(timecode>=0);
    //assert(timecode<50000);
    u64 id = this->id_counter++;
    if ((timecode <= *this->clock) || (timecode == 0)) {
        return NULL;
    }

    // Insert into linked list at correct place
    // No events currently...
    struct scheduler_event *re = NULL;
    if (this->first_event == NULL) {
        this->first_event = alloc_event(this, timecode, key, NULL, id);
        return this->first_event;
    }
    else if (this->first_event->next == NULL) { // If there's only one item...
        if (timecode <= this->first_event->timecode) {// Insert before first
            re = this->first_event = alloc_event(this, timecode, key, this->first_event, id);
        }
        else { // Insert after first
            re = this->first_event->next = alloc_event(this, timecode, key, NULL, id);
        }
        return re;
    }

    // First see if we insert to the first...
    if (this->first_event->timecode > timecode) {
        this->first_event = alloc_event(this, timecode, key, this->first_event, id);
        return this->first_event;
    }

    // If we're at this point, the list has two or more events, and does not need an insertion at the start.
    // Find one to insert AFTER
    struct scheduler_event *insert_after = this->first_event;
    while(insert_after->next != NULL) {
        if (insert_after->next->timecode > timecode) break;
        insert_after = insert_after->next;
    }

    struct scheduler_event *after_after = NULL;
    if (insert_after->next) after_after = insert_after->next->next;
    re = insert_after->next = alloc_event(this, timecode, key, after_after, id);
    return re;
}

void scheduler_run_for_cycles(struct scheduler_t *this, u64 howmany)
{
    this->cycles_left_to_run += (i64)howmany;
    //printf("\nRun %lld. Cycles left to run: %lld", howmany, this->cycles_left_to_run);
    while((this->cycles_left_to_run > 0) && (!dbg.do_break)) {
        struct scheduler_event *e = this->first_event;
        u64 loop_start_clock = *this->clock;

        // If there's no next event...
        if (!e) { // Schedule more!
            //printf("\nSchedule more...");
            this->schedule_more.func(this->schedule_more.ptr, 0, loop_start_clock, 0);
            continue;
        }

        // If we're not yet to the next event...
        if (*this->clock < e->timecode) {
            // Run up to that many cycles...
            u64 num_cycles_to_run = e->timecode - *this->clock;
            if (num_cycles_to_run > this->max_block_size) num_cycles_to_run = this->max_block_size;
            this->run.func(this->run.ptr, num_cycles_to_run, *this->clock, 0);
            i64 cycles_run = *this->clock - loop_start_clock;
            this->cycles_left_to_run -= (i64)cycles_run;
            //printf("\nTried:%lld ran:%lld left:%lld", num_cycles_to_run, cycles_run, this->cycles_left_to_run);
            continue;
        }

        i64 jitter = *this->clock - e->timecode;
        if (jitter < 0) jitter = 0 - jitter;
        this->first_event = e->next;
        //printf("\nRun event id:%lld", e->id);
        e->bound_func.func(e->bound_func.ptr, e->key, *this->clock, (u32)jitter);
        e->next = NULL;

        // Add event to discard list
        del_event(this, e);
    }
}

/*
 * TODO: make it so
 */