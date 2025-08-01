//
// Created by . on 4/25/24.
//

#include <string.h>
#include "stdio.h"

#include "scheduler-tests.h"
#include "helpers/int.h"
#include "helpers/scheduler.h"

#define MAXT 10000

struct stester {
    u64 clock;
    u32 sch_sch[MAXT];
    u32 sch_id[MAXT];
    u32 sch_done[MAXT];
    u32 num;
};

static struct scheduler_t scheduler;
static struct stester st;

static void test_reset()
{
    memset(&st, 0, sizeof(st));

}

static void tstsetkey(void *ptr, u64 key, u64 clock, u32 jitter);

static void add_set(i64 timecode)
{
    u32 num = st.num++;
    st.sch_id[num] = scheduler_add_or_run_abs(&scheduler, timecode, num, NULL, &tstsetkey, &st.sch_sch[num]);
}

static void add_set_immediate()
{
    printf("\nadd_set_immediate()");
    u32 num = st.num++;
    st.sch_id[num] = scheduler_add_next(&scheduler, num, NULL, &tstsetkey, &st.sch_sch[num]);
}


static void tstsetkey(void *ptr, u64 key, u64 clock, u32 jitter)
{
    assert(key < MAXT);
    printf("\ntstsetkey(%lld)", key);
    //assert(st.sch_done[key] == 0);
    st.sch_done[key]++;
    if (key < 100) {
        add_set_immediate();
    }
}

static void tst_schedulemore_none(void *ptr, u64 key, u64 clock, u32 jitter)
{
    printf("\nSCHEDULE MORE!?!?");
    add_set(clock+50000000000);
}

static void tst_run(void *ptr, u64 key, u64 clock, u32 jitter)
{
    printf("\ntst_run(%lld), clock=%lld jitter=%d", key, clock, jitter);
    st.clock += key;
}

static void testgrp1()
{
    test_reset();

    scheduler.run.func = &tst_run;

    // schedule 100, 10 cycles apart
    for (u32 i = 0; i < 100; i++) {
        add_set(i * 10);
    }
    scheduler_run_for_cycles(&scheduler, 100 * 10);
    printf("\n\nEVAL!");
    for (u32 i = 0; i < 200; i++) {
        if (st.sch_done[i] != 1) {
            printf("\nNOT DONE:%d", i);
        }
        if (st.sch_sch[i]) {
            printf("\nNOT CLEARED:%d", i);
        }
    }
    printf("\nDONE!");
}


void test_scheduler()
{
    printf("\nTEST SCHEDULER!");
    scheduler_init(&scheduler, &st.clock, NULL);
    scheduler.max_block_size = 50;

    printf("\nTest group1!");
    testgrp1();


}