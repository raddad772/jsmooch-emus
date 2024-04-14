//
// Created by Dave on 1/25/2024.
//

#include "jsmooch-tests.h"
#include "cpu-tests/sm83-tests.h"
#include "cpu-tests/z80-tests.h"
#include "cpu-test-generators/sh4.h"
#include "stdio.h"

int main()
{
    //test_sm83();
    //test_z80();
    generate_sh4();
}