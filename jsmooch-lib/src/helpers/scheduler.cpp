//
// Created by RadDad772 on 2/28/24.
//

#include <cstring>
#include <cassert>
#include "scheduler.h"
#include "helpers/debug.h"


inline i64 scheduler_t::current_time() const {
    return static_cast<i64>(*clock);
}

void scheduler_t::del_event(scheduler_event *e)
{
    to_delete.items[to_delete.num++] = e;
    //printf("\nDELETE EVENT ID %lld", e->id);
    //if (e->id == 110) {

    //}
    e->next = nullptr;
    if (to_delete.num == SCHEDULER_DELETE_NUM) {
        printf("\nFILLED UP!");
        for (auto & item : to_delete.items) {
            if (item) {
                free(item);
                item = nullptr;
            }
        }
        to_delete.num = 0;
    }
}

scheduler_t::scheduler_t(u64 *in_clock) : clock(in_clock)
{
}

scheduler_t::~scheduler_t()
{
    clear();
}

void scheduler_t::clear()
{
    scheduler_event* cur = first_event;
    while(cur != nullptr) {
        scheduler_event* d = cur;

        cur = cur->next;

        del_event(d);
    }
    first_event = nullptr;
}

scheduler_event* scheduler_t::alloc_event(i64 timecode, u64 key, scheduler_event* next, u64 id) {
    scheduler_event *se;

    // Reuse a if we can. FOR SPEED!
    if (to_delete.num) {
        to_delete.num--;
        se = to_delete.items[to_delete.num];
        to_delete.items[to_delete.num] = nullptr;
    }
    else se = static_cast<scheduler_event *>(malloc(sizeof(scheduler_event)));

    se->timecode = timecode;
    se->id = id;
    se->key = key;
    se->next = next;
    se->tag = 0;
    return se;
}

scheduled_bound_function* scheduler_t::bind_function(scheduler_callback func, void *ptr)
{
    auto *f = static_cast<scheduled_bound_function *>(malloc(sizeof(scheduled_bound_function)));
    f->func = func;
    f->ptr = ptr;
    return f;
}

void scheduler_t::delete_if_exist(u64 id)
{
    // If empty...
    if (first_event == nullptr) return;
    if (id == 0) return;

    // If first/one...
    scheduler_event *e;
    if (first_event->id == id) {
        e = first_event;
        first_event = e->next;

        if (e->still_sched) *e->still_sched = 0;
        //printf("\n\n\nSCHED:Deleting first event!");
        del_event(e);
        return;
    }
    e = first_event;
    scheduler_event *last = nullptr;

    while (e != nullptr) {
        if (e->id == id) {
            // Delete current.
            if (last) last->next = e->next;
            if (e->still_sched) *e->still_sched = 0;
            del_event(e);
            return;
        }

        //printf("\n\n\nSCHED:Deleting event!");
        last = e;
        e = e->next;
    }
}

u64 scheduler_t::bind_or_run(scheduler_event *e, void *ptr, scheduler_callback func, i64 timecode, u64 key, u32 *still_sched)
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

void scheduler_t::pprint_list(char *s)
{
    return;
    //printf("\n\nScheduled tasks %s:", s);
    scheduler_event *e = first_event;
    u32 i = 0;
    while(e) {
        //printf("\n%d. %lld", i++, e->id);
        e = e->next;
    }
    //printf("\n");
}

u64 scheduler_t::add_or_run_abs(i64 timecode, u64 key, void *ptr, scheduler_callback callback, u32 *still_sched)
{
    scheduler_event *e = add_abs(timecode, key, 1);
    return bind_or_run(e, ptr, callback, timecode, key, still_sched);
}

u64 scheduler_t::only_add_abs_w_tag(i64 timecode, u64 key, void *ptr, scheduler_callback callback, u32 *still_sched, u32 tag)
{
    scheduler_event *e = add_abs(timecode, key, 0);
    u64 id = bind_or_run(e, ptr, callback, timecode, key, still_sched);
    e->tag = tag;
    //printf("\n%lld %lld", id, key);
    return id;
}

u64 scheduler_t::only_add_abs(i64 timecode, u64 key, void *ptr, scheduler_callback callback, u32 *still_sched)
{
    scheduler_event *e = add_abs(timecode, key, 0);
    u64 id = bind_or_run(e, ptr, callback, timecode, key, still_sched);
    //printf("\n%lld %lld", id, key);
    return id;
}

u64 scheduler_t::add_next(u64 key, void *ptr, scheduler_callback callback, u32 *still_sched)
{
    u64 id = id_counter++;
    //printf("\nscheduler_add_next(id:%lld)", id-100);
    first_event = alloc_event(0, key, first_event, id);
    if (still_sched) *still_sched = 1;
    first_event->bound_func.ptr = ptr;
    first_event->bound_func.func = callback;
    first_event->still_sched = still_sched;
    return id;
}

scheduler_event *scheduler_t::add_abs(i64 timecode, u64 key, u32 do_instant) {
    u32 instant = 0;
    //assert(timecode>=-2);
    //assert(timecode<50000);
    u64 id = id_counter++;
    //printf("\nAdd item timecode:%lld id:%lld", timecode, id);
    i64 curtime = current_time();
    if ((timecode <= curtime) || (timecode == 0)) {
        if (do_instant) {
            return nullptr;
        }
        else {
            first_event = alloc_event(timecode, key, first_event, id);
            return first_event;
        }
    }

    // Insert into linked list at correct place
    // No events currently...
    scheduler_event *re = nullptr;
    if (first_event == nullptr) {
        //printf("\nMAKE FIRST %lld!", id);
        first_event = alloc_event(timecode, key, nullptr, id);
        return first_event;
    }
    else if (first_event->next == nullptr) { // If there's only one item...
        if (timecode <= first_event->timecode) {// Insert before first
            //printf("\n1 there. insert before!");
            re = first_event = alloc_event(timecode, key, first_event, id);
        }
        else { // Insert after first
            //printf("\n1 there. insert after!");
            re = first_event->next = alloc_event(timecode, key, nullptr, id);
        }
        return re;
    }

    // First see if we insert to the first...
    if (first_event->timecode > timecode) {
        //printf("\ninsert first of many!");
        first_event = alloc_event(timecode, key, first_event, id);
        return first_event;
    }

    // If we're at this point, the list has two or more events, and does not need an insertion at the start.
    // Find one to insert AFTER
    scheduler_event *insert_after = first_event;
    while(insert_after->next != nullptr) {
        if (insert_after->next->timecode > timecode) break;
        insert_after = insert_after->next;
    }

    scheduler_event *after_after = nullptr;
    if (insert_after->next) after_after = insert_after->next;
    re = insert_after->next = alloc_event(timecode, key, after_after, id);
    //printf("\nInserted later after %lld", insert_after->id);
    return re;
}


void scheduler_t::run_for_cycles(u64 howmany)
{
    cycles_left_to_run += static_cast<i64>(howmany);
    //printf("\nRun %lld. Cycles left to run: %lld", howmany, cycles_left_to_run);
    assert(first_event);

    while(!dbg.do_break) {
        // First, check if there's no events.
        // Then, discharge any waiting events.
        // Then, if we hace any, run some cycles.
        scheduler_event *e = first_event;
        loop_start_clock = current_time();

        in_event = 1;
        while(loop_start_clock >= e->timecode) {
            i64 jitter = loop_start_clock - e->timecode;
            first_event = e->next;
            i64 d = loop_start_clock;
            //printf("\nRun event id:%lld next:%lld event timecode:%lld our timecode:%lld %lld", e->id, e->next ? e->next->id : 0, e->timecode, loop_start_clock, current_time());
            if (e->still_sched) *e->still_sched = 0; // Set it now, so it can be reset if needed during function execution
            e->next = nullptr;
            e->bound_func.func(e->bound_func.ptr, e->key, current_time(), static_cast<u32>(jitter));

            // Add event to discard list
            del_event(e);

            //char ww[50];
            //snprintf(ww, sizeof(ww), "after run %lld", idn);
            //pprint_list(ww, this);
            e = first_event;
            if (e == nullptr) {
                in_event = 0;
                return;
            }

            loop_start_clock = current_time();
            d = loop_start_clock - d;
            if (d != 0) {
                cycles_left_to_run -= d;
                if (cycles_left_to_run <= 0) break;
            }
        }

        // Run up to that many cycles...
        if (cycles_left_to_run <= 0) break;

        // Now...Run cycles!
        u64 num_cycles_to_run = e->timecode - loop_start_clock;
        if (num_cycles_to_run > max_block_size) num_cycles_to_run = max_block_size;
        if (num_cycles_to_run > cycles_left_to_run) num_cycles_to_run = cycles_left_to_run;
        in_event = 0;
        run.func(run.ptr, num_cycles_to_run, *clock, 0);
        i64 cycles_run = current_time() - loop_start_clock;
        cycles_left_to_run -= (i64)cycles_run;
    }
    if (dbg.do_break) {
        cycles_left_to_run = 0;
    }
    in_event = 0;
}


void scheduler_t::run_for_cycles_tg16(u64 howmany)
{
    //printf("\nRun %lld. Cycles left to run: %lld", howmany, cycles_left_to_run);
    assert(first_event);
    u64 exit_at = current_time() + howmany;
    in_event = 1;

    while(!dbg.do_break && *clock < exit_at) {
        // First, check if there's no events.
        // Then, discharge any waiting events.
        // Then, if we hace any, run some cycles.
        scheduler_event *e = first_event;

        first_event = e->next;
        //printf("\nRun event id:%lld next:%lld event timecode:%lld our timecode:%lld %lld", e->id, e->next ? e->next->id : 0, e->timecode, loop_start_clock, current_time());
        if (e->still_sched) *e->still_sched = 0; // Set it now, so it can be reset if needed during function execution
        e->next = nullptr;
        *clock = e->timecode;
        e->bound_func.func(e->bound_func.ptr, e->key, current_time(), 0);

        // Add event to discard list
        del_event(e);

        e = first_event;
        if (e == nullptr) {
            in_event = 0;
            return;
        }
    }
    if (dbg.do_break) {
        cycles_left_to_run = 0;
    }
    in_event = 0;
}

void scheduler_t::run_til_tag_tg16(u32 tag)
{
    assert(first_event);
    in_event = 1;

    while(!dbg.do_break) {
        // First, check if there's no events.
        // Then, discharge any waiting events.
        // Then, if we hace any, run some cycles.
        scheduler_event *e = first_event;
        assert(e);

        first_event = e->next;
        //printf("\nRun event id:%lld next:%lld event timecode:%lld our timecode:%lld %lld tag:%d", e->id, e->next ? e->next->id : 0, e->timecode, loop_start_clock, current_time(), e->tag);
        if (e->still_sched) *e->still_sched = 0; // Set it now, so it can be reset if needed during function execution
        e->next = nullptr;
        *clock = e->timecode;
        e->bound_func.func(e->bound_func.ptr, e->key, e->timecode, 0);
        u32 etag = e->tag;

        // Add event to discard list
        del_event(e);

        if (etag == tag) {
            in_event = 0;
            return;
        }

    }
    if (dbg.do_break)
        cycles_left_to_run = 0;
    in_event = 0;
}


void scheduler_t::run_til_tag(u32 tag)
{
    assert(first_event);

    while(!dbg.do_break) {
        // First, check if there's no events.
        // Then, discharge any waiting events.
        // Then, if we hace any, run some cycles.
        scheduler_event *e = first_event;
        loop_start_clock = current_time();

        in_event = 1;
        while(loop_start_clock >= e->timecode) {
            i64 jitter = loop_start_clock - e->timecode;
            first_event = e->next;
            //printf("\nRun event id:%lld next:%lld event timecode:%lld our timecode:%lld %lld tag:%d", e->id, e->next ? e->next->id : 0, e->timecode, loop_start_clock, current_time(), e->tag);
            if (e->still_sched) *e->still_sched = 0; // Set it now, so it can be reset if needed during function execution
            e->next = nullptr;
            e->bound_func.func(e->bound_func.ptr, e->key, current_time(), static_cast<u32>(jitter));
            u32 etag = e->tag;

            // Add event to discard list
            del_event(e);

            //char ww[50];
            //snprintf(ww, sizeof(ww), "after run %lld", idn);
            //pprint_list(ww, this);
            if (etag == tag) {
                in_event = 0;
                return;
            }

            e = first_event;

            if (e == nullptr) {
                in_event = 0;
                return;
            }

            loop_start_clock = current_time();
        }

        if (dbg.do_break) {
            cycles_left_to_run = 0;
            break;
        }

        // Now...Run cycles!
        u64 num_cycles_to_run = e->timecode - loop_start_clock;
        if (num_cycles_to_run > max_block_size) num_cycles_to_run = max_block_size;
        in_event = 0;
        run.func(run.ptr, num_cycles_to_run, *clock, 0);
        //i64 cycles_run = current_time() - loop_start_clock;
        //printf("\nCYCLES RUN:%lld", cycles_run);
        //cycles_left_to_run -= (i64)cycles_run;
    }
    if (dbg.do_break) {
        cycles_left_to_run = 0;
    }
    in_event = 0;
}

void scheduler_t::from_event_adjust_master_clock(i64 howmany)
{
    // If called from an event, this will accurately adjust the clock.
    // If called from inside a cycle block, this may cause jitter; adjust there as well!
    //assert(in_event);
    *(clock) += howmany;
}
