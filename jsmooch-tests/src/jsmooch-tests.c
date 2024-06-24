//
// Created by Dave on 1/25/2024.
//

#include "jsmooch-tests.h"
#include "cpu-tests/sm83-tests.h"
#include "cpu-tests/z80-tests.h"
#include "cpu-test-generators/sh4_test_generator.h"
#include "cpu-tests/sh4-tests.h"
#include "cpu-tests/m68000-tests.h"
#include "helper-tests/scheduler-tests.h"
#include "helpers/debug.h"
#include "stdio.h"

int main()
{
    dbg_init();
    dbg.trace_on = 0;
    //test_scheduler();
    //test_sm83();
    //test_z80();
    //generate_sh4();
    //generate_sh4_tests(); // NOTE: reicast tests superceded these
    //test_sh4();
    test_m68000();
}