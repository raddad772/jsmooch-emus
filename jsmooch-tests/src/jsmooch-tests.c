//
// Created by Dave on 1/25/2024.
//

#include <stdio.h>
#include <string.h>

#include "z80_drag_race.h"
#include "jsmooch-tests.h"
#include "cpu-tests/sm83-tests.h"
#include "cpu-tests/z80-tests.h"
#include "cpu-test-generators/sh4_test_generator.h"
#include "cpu-tests/sh4-tests.h"
#include "cpu-tests/m6502_tests.h"
#include "cpu-tests/spc700_tests.h"
#include "cpu-tests/m68000-tests.h"
#include "helper-tests/dasm-range-tests.h"
#include "cpu-tests/wdc65816_tests.h"
#include "helper-tests/scheduler-tests.h"
#include "cpu-tests/arm7tdmi_tests.h"
#include "helpers/debug.h"
#include "helpers/bitbuffer.h"

void test_bitbuf()
{
    struct bitbuf bb;
    bitbuf_init(&bb, 500, 1); // We're only going to test LSB-first

    bitbuf_write_bits(&bb, 10, 4);
    return;
    // Test writing to this thing
    char test_str[50] = "hello,world";
    u32 *p = (u32 *)test_str;
    bitbuf_write_bits(&bb, 32, *p);
    p++;
    bitbuf_write_bits(&bb, 32, *p);
    p++;
    bitbuf_write_bits(&bb, 32, *p);
    p++;
    printf("\nShould be 'hello,world': '%s'", (char *)bb.buf.data);

    bitbuf_clear(&bb);
    memset(&bb.buf.data, 0, bb.buf.len);

    char test_str2[50] = "0123456789ABCDE";
    p = (u32 *)test_str2;
    u32 shifts3[4][3] = { { 10, 3, 19}, {1, 1, 30}, {8, 10, 14}, {1, 30, 1}};
    for (u32 i = 0; i < 4; i++) {
        u32 a = *p;
        p++;
        u32 snum = 0;
        for (u32 j = 0; j < 3; j++) {
            bitbuf_write_bits(&bb, shifts3[i][j], a >> snum);
            snum += shifts3[i][j];
        }
    }

    printf("\nString should be '0123456789ABCDE': '%s'", (char *)bb.buf.data);


    char out_str[50] = {};
    // Now read back from that buffer strange numbers of bits
    //u32 strange_nums[10] = { 1, 10, 30, 5, 24, 7, 8, 2 };
    u32 strange_nums[10] = { 8, 8, 8, 8, 8, 8, 8, 8, 8, 8 };
    u32 bits_left = 128;
    struct bitbuf out;
    bitbuf_init(&out, bits_left, 1);
    u32 pos = 0;
    u32 bitpos = 0;
    while (bits_left > 0) {
        u32 todo = strange_nums[pos];
        pos = (pos + 1) % 10;
        if (todo > bits_left) todo = bits_left;
        u32 a = bitbuf_read_bits(&bb, bitpos, todo);
        bitbuf_write_bits(&out, todo, a);
        bitpos += todo;
        bits_left -= todo;
    }

    printf("\nShould be: '0123456789ABCDE', is: '%s'", (char *)out.buf.data);

    bitbuf_delete(&bb);
    bitbuf_delete(&out);
}

int main()
{
    dbg_init();
    dbg.trace_on = 0;
    test_spc700();
    //test_wdc65816();
    //z80_drag_race();
    //test_scheduler();
    //test_sm83();
    //test_z80();
    //generate_sh4();
    //generate_sh4_tests(); // NOTE: reicast tests superceded these
    //test_sh4();
    //test_m68000();
    //test_bitbuf();
    //test_dasm_ranges();
    //test_nesm6502();
    //test_arm7tdmi();
}