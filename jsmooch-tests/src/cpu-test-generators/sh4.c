//
// Created by RadDad772 on 4/9/24.
//

#include "component/cpu/sh4/sh4_interpreter.h"
#include "component/cpu/sh4/sh4_interpreter_opcodes.h"
#include "helpers/cvec.h"
#include "helpers/int.h"

struct mem_pair {
    u32 addr;
    u32 val;
    u32 rw;
};

struct SH4_tester{
    struct SH4 cpu;
    struct cvec mem_pairs;
} ;

void test_sh4()
{
    struct SH4_tester tester;
    SH4_init(&tester.cpu, NULL);
    cvec_init(&tester.mem_pairs, sizeof(struct mem_pair), 4096);

    /* Gotta fill these out
    void *mptr;
    u32 (*fetch_ins)(void*,u32);
    u64 (*read)(void*,u32,u32);
    void (*write)(void*,u32,u64,u32);
    */

}