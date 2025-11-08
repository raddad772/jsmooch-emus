//
// Created by RadDad772 on 2/28/24.
//

#include <string.h>
#include "stdio.h"
#include "assert.h"
#include "stdlib.h"
#include "scheduler.h"
#include "helpers/debug.h"


static inline i64 current_time(struct scheduler_t *this) {
    return (i64)(*this->clock) + (i64)(*this->waitstates);
}

static void del_event(struct scheduler_t *this, struct scheduler_event *e)
{
    this->to_delete.items[this->to_delete.num++] = e;
    //printf("\nDELETE EVENT ID %lld", e->id);
    //if (e->id == 110) {

    //}
    e->next = NULL;
    if (this->to_delete.num == SCHEDULER_DELETE_NUM) {
        printf("\nFILLED UP!");
        for (u32 i = 0; i < SCHEDULER_DELETE_NUM; i++) {
            if (this->to_delete.items[i]) {
                free(this->to_delete.items[i]);
                this->to_delete.items[i] = NULL;
            }
        }
        this->to_delete.num = 0;
    }
}

void scheduler_init(struct scheduler_t* this, u64 *clock, u64 *waitstates)
{
    memset(this, 0, sizeof(*this));
    this->clock = clock;
    this->waitstates = waitstates;
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
    se->tag = 0;
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
    if (id == 0) return;

    // If first/one...
    struct scheduler_event *e;
    if (this->first_event->id == id) {
        e = this->first_event;
        this->first_event = e->next;

        if (e->still_sched) *e->still_sched = 0;
        //printf("\n\n\nSCHED:Deleting first event!");
        del_event(this, e);
        return;
    }
    e = this->first_event;
    struct scheduler_event *last = NULL;

    while (e != NULL) {
        if (e->id == id) {
            // Delete current.
            if (last) last->next = e->next;
            if (e->still_sched) *e->still_sched = 0;
            del_event(this, e);
            return;
        }

        //printf("\n\n\nSCHED:Deleting event!");
        last = e;
        e = e->next;
    }
}

u64 scheduler_bind_or_run(struct scheduler_event *e, void *ptr, scheduler_callback func, i64 timecode, u64 key, u32 *still_sched)
{
    if (!e) {
        if (still_sched) *still_sched = 0;
        func(ptr, key, timecode, 0);
        return 0;
    }

    if (still_sched) *still_sched = 1;
    e->bound_func.ptr = ptr;
    e->bound_func.func = func;
    e->still_sched = still_sched;
    return e->id;
}

static void pprint_list(char *s, struct scheduler_t *this)
{
    return;
    //printf("\n\nScheduled tasks %s:", s);
    struct scheduler_event *e = this->first_event;
    u32 i = 0;
    while(e) {
        //printf("\n%d. %lld", i++, e->id);
        e = e->next;
    }
    //printf("\n");
}

u64 scheduler_add_or_run_abs(struct scheduler_t *this, i64 timecode, u64 key, void *ptr, scheduler_callback callback, u32 *still_sched)
{
    struct scheduler_event *e = scheduler_add_abs(this, timecode, key, 1);
    return scheduler_bind_or_run(e, ptr, callback, timecode, key, still_sched);
}

u64 scheduler_only_add_abs_w_tag(struct scheduler_t *this, i64 timecode, u64 key, void *ptr, scheduler_callback callback, u32 *still_sched, u32 tag)
{
    struct scheduler_event *e = scheduler_add_abs(this, timecode, key, 0);
    u64 id = scheduler_bind_or_run(e, ptr, callback, timecode, key, still_sched);
    e->tag = tag;
    //printf("\n%lld %lld", id, key);
    return id;
}

u64 scheduler_only_add_abs(struct scheduler_t *this, i64 timecode, u64 key, void *ptr, scheduler_callback callback, u32 *still_sched)
{
    struct scheduler_event *e = scheduler_add_abs(this, timecode, key, 0);
    u64 id = scheduler_bind_or_run(e, ptr, callback, timecode, key, still_sched);
    //printf("\n%lld %lld", id, key);
    return id;
}

u64 scheduler_add_next(struct scheduler_t *this, u64 key, void *ptr, scheduler_callback callback, u32 *still_sched)
{
    u64 id = this->id_counter++;
    //printf("\nscheduler_add_next(id:%lld)", id-100);
    this->first_event = alloc_event(this, 0, key, this->first_event, id);
    if (still_sched) *still_sched = 1;
    this->first_event->bound_func.ptr = ptr;
    this->first_event->bound_func.func = callback;
    this->first_event->still_sched = still_sched;
    return id;
}

struct scheduler_event *scheduler_add_abs(struct scheduler_t* this, i64 timecode, u64 key, u32 do_instant) {
    u32 instant = 0;
    //assert(timecode>=-2);
    //assert(timecode<50000);
    u64 id = this->id_counter++;
    //printf("\nAdd item timecode:%lld id:%lld", timecode, id);
    i64 curtime = current_time(this);
    if ((timecode <= curtime) || (timecode == 0)) {
        if (do_instant) {
            return NULL;
        }
        else {
            this->first_event = alloc_event(this, timecode, key, this->first_event, id);
            return this->first_event;
        }
    }

    // Insert into linked list at correct place
    // No events currently...
    struct scheduler_event *re = NULL;
    if (this->first_event == NULL) {
        //printf("\nMAKE FIRST %lld!", id);
        this->first_event = alloc_event(this, timecode, key, NULL, id);
        return this->first_event;
    }
    else if (this->first_event->next == NULL) { // If there's only one item...
        if (timecode <= this->first_event->timecode) {// Insert before first
            //printf("\n1 there. insert before!");
            re = this->first_event = alloc_event(this, timecode, key, this->first_event, id);
        }
        else { // Insert after first
            //printf("\n1 there. insert after!");
            re = this->first_event->next = alloc_event(this, timecode, key, NULL, id);
        }
        return re;
    }

    // First see if we insert to the first...
    if (this->first_event->timecode > timecode) {
        //printf("\ninsert first of many!");
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
    if (insert_after->next) after_after = insert_after->next;
    re = insert_after->next = alloc_event(this, timecode, key, after_after, id);
    //printf("\nInserted later after %lld", insert_after->id);
    return re;
}


void scheduler_run_for_cycles(struct scheduler_t *this, u64 howmany)
{
    this->cycles_left_to_run += (i64)howmany;
    //printf("\nRun %lld. Cycles left to run: %lld", howmany, this->cycles_left_to_run);
    assert(this->first_event);

    while(!dbg.do_break) {
        // First, check if there's no events.
        // Then, discharge any waiting events.
        // Then, if we hace any, run some cycles.
        struct scheduler_event *e = this->first_event;
        this->loop_start_clock = current_time(this);

        this->in_event = 1;
        while(this->loop_start_clock >= e->timecode) {
            i64 jitter = this->loop_start_clock - e->timecode;
            this->first_event = e->next;
            i64 d = this->loop_start_clock;
            //printf("\nRun event id:%lld next:%lld event timecode:%lld our timecode:%lld %lld", e->id, e->next ? e->next->id : 0, e->timecode, this->loop_start_clock, current_time(this));
            if (e->still_sched) *e->still_sched = 0; // Set it now, so it can be reset if needed during function execution
            e->next = NULL;
            e->bound_func.func(e->bound_func.ptr, e->key, current_time(this), (u32) jitter);

            // Add event to discard list
            del_event(this, e);

            //char ww[50];
            //snprintf(ww, sizeof(ww), "after run %lld", idn);
            //pprint_list(ww, this);
            e = this->first_event;
            if (e == NULL) {
                this->in_event = 0;
                return;
            }

            this->loop_start_clock = current_time(this);
            d = this->loop_start_clock - d;
            if (d != 0) {
                this->cycles_left_to_run -= d;
                if (this->cycles_left_to_run <= 0) break;
            }
        }

        // Run up to that many cycles...
        if (this->cycles_left_to_run <= 0) break;

        // Now...Run cycles!
        u64 num_cycles_to_run = e->timecode - this->loop_start_clock;
        if (num_cycles_to_run > this->max_block_size) num_cycles_to_run = this->max_block_size;
        if (num_cycles_to_run > this->cycles_left_to_run) num_cycles_to_run = this->cycles_left_to_run;
        this->in_event = 0;
        this->run.func(this->run.ptr, num_cycles_to_run, *this->clock, 0);
        i64 cycles_run = current_time(this) - this->loop_start_clock;
        this->cycles_left_to_run -= (i64)cycles_run;
    }
    if (dbg.do_break) {
        this->cycles_left_to_run = 0;
    }
    this->in_event = 0;
}


void scheduler_run_for_cycles_tg16(struct scheduler_t *this, u64 howmany)
{
    //printf("\nRun %lld. Cycles left to run: %lld", howmany, this->cycles_left_to_run);
    assert(this->first_event);
    u64 exit_at = current_time(this) + howmany;
    this->in_event = 1;

    while(!dbg.do_break && *this->clock < exit_at) {
        // First, check if there's no events.
        // Then, discharge any waiting events.
        // Then, if we hace any, run some cycles.
        struct scheduler_event *e = this->first_event;

        this->first_event = e->next;
        //printf("\nRun event id:%lld next:%lld event timecode:%lld our timecode:%lld %lld", e->id, e->next ? e->next->id : 0, e->timecode, this->loop_start_clock, current_time(this));
        if (e->still_sched) *e->still_sched = 0; // Set it now, so it can be reset if needed during function execution
        e->next = NULL;
        *this->clock = e->timecode;
        e->bound_func.func(e->bound_func.ptr, e->key, current_time(this), 0);

        // Add event to discard list
        del_event(this, e);

        e = this->first_event;
        if (e == NULL) {
            this->in_event = 0;
            return;
        }
    }
    if (dbg.do_break) {
        this->cycles_left_to_run = 0;
    }
    this->in_event = 0;
}

void scheduler_run_til_tag_tg16(struct scheduler_t *this, u32 tag)
{
    assert(this->first_event);
    this->in_event = 1;

    while(!dbg.do_break) {
        // First, check if there's no events.
        // Then, discharge any waiting events.
        // Then, if we hace any, run some cycles.
        struct scheduler_event *e = this->first_event;
        assert(e);

        this->first_event = e->next;
        //printf("\nRun event id:%lld next:%lld event timecode:%lld our timecode:%lld %lld tag:%d", e->id, e->next ? e->next->id : 0, e->timecode, this->loop_start_clock, current_time(this), e->tag);
        if (e->still_sched) *e->still_sched = 0; // Set it now, so it can be reset if needed during function execution
        e->next = NULL;
        *this->clock = e->timecode;
        e->bound_func.func(e->bound_func.ptr, e->key, e->timecode, 0);
        u32 etag = e->tag;

        // Add event to discard list
        del_event(this, e);

        if (etag == tag) {
            this->in_event = 0;
            return;
        }

    }
    if (dbg.do_break)
        this->cycles_left_to_run = 0;
    this->in_event = 0;
}


void scheduler_run_til_tag(struct scheduler_t *this, u32 tag)
{
    assert(this->first_event);

    while(!dbg.do_break) {
        // First, check if there's no events.
        // Then, discharge any waiting events.
        // Then, if we hace any, run some cycles.
        struct scheduler_event *e = this->first_event;
        this->loop_start_clock = current_time(this);

        this->in_event = 1;
        while(this->loop_start_clock >= e->timecode) {
            i64 jitter = this->loop_start_clock - e->timecode;
            this->first_event = e->next;
            //printf("\nRun event id:%lld next:%lld event timecode:%lld our timecode:%lld %lld tag:%d", e->id, e->next ? e->next->id : 0, e->timecode, this->loop_start_clock, current_time(this), e->tag);
            if (e->still_sched) *e->still_sched = 0; // Set it now, so it can be reset if needed during function execution
            e->next = NULL;
            e->bound_func.func(e->bound_func.ptr, e->key, current_time(this), (u32) jitter);
            u32 etag = e->tag;

            // Add event to discard list
            del_event(this, e);

            //char ww[50];
            //snprintf(ww, sizeof(ww), "after run %lld", idn);
            //pprint_list(ww, this);
            if (etag == tag) {
                this->in_event = 0;
                return;
            }

            e = this->first_event;

            if (e == NULL) {
                this->in_event = 0;
                return;
            }

            this->loop_start_clock = current_time(this);
        }

        if (dbg.do_break) {
            this->cycles_left_to_run = 0;
            break;
        }

        // Now...Run cycles!
        u64 num_cycles_to_run = e->timecode - this->loop_start_clock;
        if (num_cycles_to_run > this->max_block_size) num_cycles_to_run = this->max_block_size;
        this->in_event = 0;
        this->run.func(this->run.ptr, num_cycles_to_run, *this->clock, 0);
        //i64 cycles_run = current_time(this) - this->loop_start_clock;
        //printf("\nCYCLES RUN:%lld", cycles_run);
        //this->cycles_left_to_run -= (i64)cycles_run;
    }
    if (dbg.do_break) {
        this->cycles_left_to_run = 0;
    }
    this->in_event = 0;
}

void scheduler_from_event_adjust_master_clock(struct scheduler_t *this, i64 howmany)
{
    // If called from an event, this will accurately adjust the clock.
    // If called from inside a cycle block, this may cause jitter; adjust there as well!
    //assert(this->in_event);
    *(this->clock) += howmany;
}
