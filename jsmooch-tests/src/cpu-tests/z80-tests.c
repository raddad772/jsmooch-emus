//
// Created by RadDad772 on 2/28/24.
//

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#if !defined(_MSC_VER)
#include <unistd.h>
#include <pwd.h>
#endif
#include "z80-tests.h"
#include "helpers/int.h"
#include "rfb.h"
#include "component/cpu/z80/z80.h"
//#include "component/cpu/z80/z80_opcodes.h"

#include "../json.h"
#include "helpers/user.h"

struct test_cpu_regs {
    u32 r;
    u32 af_; u32 bc_; u32 de_; u32 hl_;


    u32 pc; u32 sp; u32 ix; u32 iy;
    u32 a; u32 b; u32 c; u32 d;
    u32 e; u32 f; u32 h; u32 l;

    u32 p; u32 i;

    u32 wz; u32 ei; u32 q; u32 iff1; u32 iff2; u32 im;
};

struct ram_entry {
    u32 addr;
    u32 val;
};


struct test_state {
    struct test_cpu_regs regs;
    u32 num_ram_entry;
    struct ram_entry ram[50];
};

struct port_entry {
    u32 addr;
    u32 data;
    u32 rw; // 0 = r, 1 = w
};

struct test_cycle {
    i32 addr;
    i32 data;
    u32 r;
    u32 w;
    u32 m; // memory request
    u32 i; // IO request
};

static u32 test_RAM[65536];

struct jsontest {
    char name[50];
    struct test_state initial;
    struct test_state final;
    struct test_cycle cycles[40];
    u32 num_cycles;
    u32 num_ports;

    struct ram_entry opcodes[14];
    struct port_entry ports[10];
};

struct z80_test_result
{
    u32 passed;
    struct test_cycle cycles[100];
    char msg[5000];
    u32 addr_io_mismatches;
    u32 length_mismatches;
    struct jsontest *failed_test_struct;
};


static u32 skip_tests(u32 class, u32 ins) {
    switch(class) {
        case 0:
        case 0xDD:
        case 0xFD: {
            switch(ins) {
                case 0x76:
                    return 1;
            }
        }
    }

    switch(class) {
        case 0:
            switch(ins) {
                case 0xCB:
                case 0xDD:
                case 0xED:
                case 0xFD:
                    return 1;
            }
    }
    return 0;
}

static u32 is_call(u32 class, u32 ins) {
    switch(class) {
        case 0x00:
            switch(ins) {
                case 0xCD:
                case 0xE9:
                    return 1;
            }
            return 0;
        case 0xCB:
            return 0;
        case 0xDD:
            switch(ins) {
                case 0xCD:
                case 0xE9:
                    return 1;
            }
            return 0;
        case 0xFD:
            switch(ins) {
                case 0xCD:
                case 0xE9:
                    return 1;
            }
            return 0;
        case 0xED:
            if (ins == 0xB1)
                return 1;
            return 0;
    }
    return 0;
}

static void construct_path(char *out, u32 iclass, u32 ins)
{
    char test_path[500];
    const char *homeDir = get_user_dir();

    char *tp = out;
    tp += sprintf(tp, "%s", homeDir);
    tp += sprintf(tp, "/dev/z80/v1");

    tp += sprintf(tp, "%s/", test_path);
    if (iclass != 0)
        tp += sprintf(tp, "cb ");
    tp += sprintf(tp, "%02x.json", ins);
}
//"ports":[[40816,2,"w"]]


/*
{
  "name": "00 0000",
  "initial": {
    "pc": 19935,
    "sp": 59438,
    "a": 110,
    "b": 185,
    "c": 144,
    "d": 208,
    "e": 190,
    "f": 250,
    "h": 131,
    "l": 147,
    "i": 166,
    "r": 16,
    "ei": 1,
    "wz": 62861,
    "ix": 35859,
    "iy": 45708,
    "af_": 30257,
    "bc_": 17419,
    "de_": 13842,
    "hl_": 28289,
    "im": 0,
    "p": 1,
    "q": 0,
    "iff1": 1,
    "iff2": 1,
    "ram": [
      [
        19935,
        0
      ]
    ]
  },
  "final": {
    "a": 110,
    "b": 185,
    "c": 144,
    "d": 208,
    "e": 190,
    "f": 250,
    "h": 131,
    "l": 147,
    "i": 166,
    "r": 17,
    "af_": 30257,
    "bc_": 17419,
    "de_": 13842,
    "hl_": 28289,
    "ix": 35859,
    "iy": 45708,
    "pc": 19936,
    "sp": 59438,
    "wz": 62861,
    "iff1": 1,
    "iff2": 1,
    "im": 0,
    "ei": 0,
    "p": 0,
    "q": 0,
    "ram": [
      [
        19935,
        0
      ]
    ]
  },
  "cycles": [
    [
      19935,
      null,
      "----"
    ],
    [
      19935,
      null,
      "r-m-"
    ],
    [
      42512,
      0,
      "----"
    ],
    [
      42512,
      null,
      "----"
    ]
  ]
}
 */

static void parse_state(struct json_object_s *object, test_state *state)
{
    struct json_object_element_s *el = object->start;
    state->num_ram_entry = 0;
    for (u32 i = 0; i < object->length; i++) {
        u32 p = 0;
        struct json_string_s *str = (struct json_string_s *)el->value->payload;
        u32 *dest = 0;
        if (strcmp(el->name->string, "a") == 0) {
            dest = &state->regs.a;
        }
        else if (strcmp(el->name->string, "b") == 0) {
            dest = &state->regs.b;
        }
        else if (strcmp(el->name->string, "c") == 0) {
            dest = &state->regs.c;
        }
        else if (strcmp(el->name->string, "d") == 0) {
            dest = &state->regs.d;
        }
        else if (strcmp(el->name->string, "e") == 0) {
            dest = &state->regs.e;
        }
        else if (strcmp(el->name->string, "f") == 0) {
            dest = &state->regs.f;
        }
        else if (strcmp(el->name->string, "h") == 0) {
            dest = &state->regs.h;
        }
        else if (strcmp(el->name->string, "l") == 0) {
            dest = &state->regs.l;
        }
        else if (strcmp(el->name->string, "pc") == 0) {
            dest = &state->regs.pc;
        }
        else if (strcmp(el->name->string, "sp") == 0) {
            dest = &state->regs.sp;
        }
        else if (strcmp(el->name->string, "i") == 0) {
            dest = &state->regs.i;
        }
        else if (strcmp(el->name->string, "r") == 0) {
            dest = &state->regs.r;
        }
        else if (strcmp(el->name->string, "wz") == 0) {
            dest = &state->regs.wz;
        }
        else if (strcmp(el->name->string, "ix") == 0) {
            dest = &state->regs.ix;
            //p = 1;
        }
        else if (strcmp(el->name->string, "iy") == 0) {
            dest = &state->regs.iy;
        }
        else if (strcmp(el->name->string, "af_") == 0) {
            dest = &state->regs.af_;
        }
        else if (strcmp(el->name->string, "bc_") == 0) {
            dest = &state->regs.bc_;
        }
        else if (strcmp(el->name->string, "de_") == 0) {
            dest = &state->regs.de_;
        }
        else if (strcmp(el->name->string, "hl_") == 0) {
            dest = &state->regs.hl_;
        }
        else if (strcmp(el->name->string, "im") == 0) {
            dest = &state->regs.im;
        }
        else if (strcmp(el->name->string, "p") == 0) {
            dest = &state->regs.p;
        }
        else if (strcmp(el->name->string, "q") == 0) {
            dest = &state->regs.q;
        }
        else if (strcmp(el->name->string, "iff1") == 0) {
            dest = &state->regs.iff1;
        }
        else if (strcmp(el->name->string, "iff2") == 0) {
            dest = &state->regs.iff2;
        }
        if (strcmp(el->name->string, "ram") == 0) {
            struct json_array_s *arr1 = (struct json_array_s *)el->value->payload;
            state->num_ram_entry = (u32)arr1->length;
            struct json_array_element_s *arr_el = arr1->start;
            for (u32 arr1_i = 0; arr1_i < arr1->length; arr1_i++) {
                assert(arr_el->value->type == json_type_array);
                struct json_array_s *arr2 = (struct json_array_s*) arr_el->value->payload;
                assert(arr2->length == 2);
                struct json_array_element_s *arr2_el = arr2->start;
                // Address
                struct json_number_s* nr = (struct json_number_s*)arr2_el->value->payload;
                state->ram[arr1_i].addr = atoi(nr->number);
                // Data
                arr2_el = arr2_el->next;
                nr = (struct json_number_s*)arr2_el->value->payload;
                state->ram[arr1_i].val = atoi(nr->number);

                arr_el = arr_el->next;
            }
        }
        else {
            // Grab number
            assert(el->value->type == json_type_number);
            struct json_number_s *num = (struct json_number_s *) el->value->payload;
            char *yo;
            u32 a = strtol(num->number, &yo, 10);
            assert(yo != num->number);
            if (dest == NULL) {
                //printf("\nUHOH! %s \n", el->name->string);
            }
            else {
                *dest = a;
                if (p) { printf("\nIX:%d %s", a, num->number); }
            }
        }
        el = el->next;
    }
}


static void parse_and_fill_out(struct jsontest tests[1000], read_file_buf *infile)
{
    struct json_value_s *root = json_parse(infile->buf.ptr, infile->buf.size);
    assert(root->type == json_type_array);

    struct json_array_s* arr = (struct json_array_s*)root->payload;
    struct json_array_element_s r;
    assert(arr->length == 1000);
    struct json_array_element_s* cur_node = arr->start;
    for (u32 i = 0; i < 1000; i++) {
        struct jsontest *test = tests+i;
        test->num_cycles = 0;
        // TODO: add opcodes here

        sprintf(test->name, "Unknown");
        assert(cur_node->value->type == json_type_object);
        struct json_object_s *tst = (struct json_object_s *)cur_node->value->payload;
        struct json_object_element_s *s = (struct json_object_element_s *)tst->start;
        for (u32 j = 0; j < tst->length; j++) {
            if (strcmp(s->name->string, "name") == 0) {
                assert(s->value->type == json_type_string);
                struct json_string_s *str = (struct json_string_s *)s->value->payload;
                sprintf(test->name,"%s", str->string);
            }
            else if (strcmp(s->name->string, "initial") == 0) {
                assert(s->value->type == json_type_object);
                struct json_object_s* state = (struct json_object_s *)s->value->payload;
                parse_state(state, &test->initial);
            }
            else if (strcmp(s->name->string, "final") == 0) {
                assert(s->value->type == json_type_object);
                struct json_object_s* state = (struct json_object_s*)s->value->payload;
                parse_state(state, &test->final);
            }
            else if (strcmp(s->name->string, "cycles") == 0) {
                assert(s->value->type == json_type_array);
                struct json_array_s* arr1 = (struct json_array_s*)s->value->payload;
                test->num_cycles = (u32)arr1->length;
                struct json_array_element_s* arr1_el = arr1->start;
                for (u32 h = 0; h < arr1->length; h++) {
                    assert(arr1_el != NULL);
                    assert(arr1->length < 20);
                    struct json_array_s* arr2 = (struct json_array_s*)arr1_el->value->payload;
                    struct json_array_element_s* arr2_el = (struct json_array_element_s*)arr2->start;

                    // number, number, string
                    struct json_number_s *num = (struct json_number_s *)arr2_el->value->payload;

                    test->cycles[h].addr = atoi(num->number);

                    arr2_el = arr2_el->next;
                    num = (struct json_number_s *)arr2_el->value->payload;

                    if (num == NULL) {
                        test->cycles[h].data = -1;
                    }
                    else
                        test->cycles[h].data = atoi(num->number);

                    arr2_el = arr2_el->next;

                    struct json_string_s* st = (struct json_string_s *)arr2_el->value->payload;
                    // rwmi
                    test->cycles[h].r = st->string[0] == 'r' ? 1 : 0;
                    test->cycles[h].w = st->string[1] == 'w' ? 1 : 0;
                    test->cycles[h].m = st->string[2] == 'm' ? 1 : 0;
                    test->cycles[h].i = st->string[3] == 'i' ? 1 : 0;

                    arr1_el = arr1_el->next;
                }
            }
            else if (strcmp(s->name->string, "ports") == 0) {
                // "ports":[[40816,2,"w"]]
                assert(s->value->type == json_type_array);
                struct json_array_s* arr1 = (struct json_array_s*)s->value->payload;
                test->num_ports = (u32)arr1->length;
                struct json_array_element_s* arr1_el = arr1->start;
                for (u32 h = 0; h < arr1->length; h++) {
                    assert(arr1_el != NULL);
                    assert(arr1->length < 20);
                    struct json_array_s* arr2 = (struct json_array_s*)arr1_el->value->payload;
                    struct json_array_element_s* arr2_el = (struct json_array_element_s*)arr2->start;

                    // number, number, string
                    struct json_number_s *num = (struct json_number_s *)arr2_el->value->payload;

                    test->ports[h].addr = atoi(num->number);

                    arr2_el = arr2_el->next;
                    num = (struct json_number_s *)arr2_el->value->payload;

                    test->ports[h].data = atoi(num->number);
                    arr2_el = arr2_el->next;

                    struct json_string_s* st = (struct json_string_s *)arr2_el->value->payload;
                    // rwmi
                    test->ports[h].rw = st->string[0] == 'w' ? 1 : 0;

                    arr1_el = arr1_el->next;
                }
            }
            else {
                printf("\nno done! %s", s->name->string);
            }

            s = s->next;
        }

        cur_node = cur_node->next;
    }
    free(root);
}

static void pprint_regs(struct Z80_regs *cpu_regs, test_cpu_regs *test_regs, u32 last_pc, u32 only_print_diff)
{
    printf("\nREG CPU       TEST");
    printf("\n------------------");
    if ((only_print_diff && (last_pc != test_regs->pc)) || (!only_print_diff))
        printf("\nPC  %04x      %04x", last_pc, test_regs->pc);
    if ((only_print_diff && (cpu_regs->SP != test_regs->sp)) || (!only_print_diff))
        printf("\nSP  %04x      %04x", cpu_regs->SP, test_regs->sp);
    if ((only_print_diff && (cpu_regs->A != test_regs->a)) || (!only_print_diff))
        printf("\nA   %02x        %02x", cpu_regs->A, test_regs->a);
    if ((only_print_diff && (cpu_regs->B != test_regs->b)) || (!only_print_diff))
        printf("\nB   %02x        %02x", cpu_regs->B, test_regs->b);
    if ((only_print_diff && (cpu_regs->C != test_regs->c)) || (!only_print_diff))
        printf("\nC   %02x        %02x", cpu_regs->C, test_regs->c);
    if ((only_print_diff && (cpu_regs->D != test_regs->d)) || (!only_print_diff))
        printf("\nD   %02x        %02x", cpu_regs->D, test_regs->d);
    if ((only_print_diff && (cpu_regs->E != test_regs->e)) || (!only_print_diff))
        printf("\nE   %02x        %02x", cpu_regs->E, test_regs->e);
    if ((only_print_diff && (cpu_regs->H != test_regs->h)) || (!only_print_diff))
        printf("\nH   %02x        %02x", cpu_regs->H, test_regs->h);
    if ((only_print_diff && (cpu_regs->L != test_regs->l)) || (!only_print_diff))
        printf("\nL   %02x        %02x", cpu_regs->L, test_regs->l);
    if ((only_print_diff && (Z80_regs_F_getbyte(&cpu_regs->F) != test_regs->f)) || (!only_print_diff))
        printf("\nF   %02x        %02x", Z80_regs_F_getbyte(&cpu_regs->F), test_regs->f);
    if ((only_print_diff && (cpu_regs->I != test_regs->i)) || (!only_print_diff))
        printf("\nI   %02x        %02x", cpu_regs->I, test_regs->i);
    if ((only_print_diff && (cpu_regs->R != test_regs->r)) || (!only_print_diff))
        printf("\nR   %02x        %02x", cpu_regs->R, test_regs->r);
    if ((only_print_diff && (cpu_regs->IX != test_regs->ix)) || (!only_print_diff))
        printf("\nIX  %02x        %02x", cpu_regs->IX, test_regs->ix);
    if ((only_print_diff && (cpu_regs->IY != test_regs->iy)) || (!only_print_diff))
        printf("\nIY  %02x        %02x", cpu_regs->IY, test_regs->iy);
    if ((only_print_diff && (cpu_regs->AF_ != test_regs->af_)) || (!only_print_diff))
        printf("\nAF_ %02x        %02x", cpu_regs->AF_, test_regs->af_);
    if ((only_print_diff && (cpu_regs->BC_ != test_regs->bc_)) || (!only_print_diff))
        printf("\nBC_ %02x        %02x", cpu_regs->BC_, test_regs->bc_);
    if ((only_print_diff && (cpu_regs->DE_ != test_regs->de_)) || (!only_print_diff))
        printf("\nDE_ %02x        %02x", cpu_regs->DE_, test_regs->de_);
    if ((only_print_diff && (cpu_regs->HL_ != test_regs->hl_)) || (!only_print_diff))
        printf("\nHL_ %02x        %02x", cpu_regs->HL_, test_regs->hl_);
    if ((only_print_diff && (cpu_regs->WZ != test_regs->wz)) || (!only_print_diff))
        printf("\nWZ  %02x        %02x", cpu_regs->WZ, test_regs->wz);
    if ((only_print_diff && (cpu_regs->IFF1 != test_regs->iff1)) || (!only_print_diff))
        printf("\nIF1 %02x        %02x", cpu_regs->IFF1, test_regs->iff1);
    if ((only_print_diff && (cpu_regs->IFF2 != test_regs->iff2)) || (!only_print_diff))
        printf("\nIF2 %02x        %02x", cpu_regs->IFF2, test_regs->iff2);
    if ((only_print_diff && (cpu_regs->IM != test_regs->im)) || (!only_print_diff))
        printf("\nIM  %02x        %02x", cpu_regs->IM, test_regs->im);
}

static void pprint_test(struct jsontest *test, test_cycle *cpucycles) {
    printf("\nCycles");
    for (u32 i = 0; i < test->num_cycles; i++) {
        printf("\n\nTEST cycle:%d  addr:%04x  data:%02x  rwmi:%d%d%d%d", i, test->cycles[i].addr, test->cycles[i].data, test->cycles[i].r, test->cycles[i].w, test->cycles[i].m, test->cycles[i].i);
        printf("\nCPU  cycle:%d  addr:%04x  data:%02x  rwmi:%d%d%d%d", i, cpucycles[i].addr, cpucycles[i].data, cpucycles[i].r, cpucycles[i].w, cpucycles[i].m, cpucycles[i].i);
    }
}

//#define PP(a, b) passed &= cpu->regs.a == final->regs.b; printf("\n%s %04x %04x %d", #a, cpu->regs.a, final->regs.b, passed)
#define PP(a, b) passed &= cpu->regs.a == final->regs.b

static u32 testregs(struct Z80* cpu, test_state* final, u32 last_pc, u32 is_call, u32 tn)
{
    u32 passed = 1;
    u32 rpc = last_pc == final->regs.pc;
    if ((!rpc) && is_call && (((cpu->regs.PC - 1) & 0xFFFF) == final->regs.pc))
        rpc = 1;
    //printf("\nPC %04x %04x %d", last)
    passed &= rpc;
    PP(SP, sp);
    PP(A, a);
    PP(B, b);
    PP(C, c);
    PP(D, d);
    PP(E, e);
    passed &= Z80_regs_F_getbyte(&cpu->regs.F) == final->regs.f;
    //printf("\nF %02x %02x", Z80_regs_F_getbyte(&cpu->regs.F), final->regs.f);
    PP(H, h);
    PP(L, l);
    PP(I, i);
    PP(R, r);
    PP(IX, ix);
    PP(IY, iy);
    PP(AF_, af_);
    PP(BC_, bc_);
    PP(DE_, de_);
    PP(HL_, hl_);
    PP(WZ, wz);
    PP(IFF1, iff1);
    PP(IFF2, iff2);
    PP(IM, im);
    //passed &= cpu->regs.Q == final->regs.q;
    if (passed == 0) {
        printf("\nFAILED ON REGS %d", tn);
        pprint_regs(&cpu->regs, &final->regs, last_pc, true);
    }

    return passed;

}

static void test_z80_automated(struct z80_test_result *out, Z80* cpu, jsontest tests[1000], u32 is_call)
{
    out->passed = 0;
    sprintf(out->msg, "");
    char *msgptr = out->msg;
    out->addr_io_mismatches = 0;
    out->length_mismatches = 0;
    out->failed_test_struct = NULL;

    u32 last_pc;
    u32 ins;
    *cpu->trace.cycles = 1;
    for (u32 i = 0; i < 1000; i++) {
        out->failed_test_struct = &tests[i];
        struct jsontest *test = &tests[i];
        struct test_state *initial = &tests[i].initial;
        struct test_state *final = &tests[i].final;
        /*
             "pc": 19935,
    "sp": 59438,
    "a": 110,
    "b": 185,
    "c": 144,
    "d": 208,
    "e": 190,
    "f": 250,
    "h": 131,
    "l": 147,
    "i": 166,
    "r": 16,
    "ei": 1,
    "wz": 62861,
    "ix": 35859,
    "iy": 45708,
    "af_": 30257,
    "bc_": 17419,
    "de_": 13842,
    "hl_": 28289,
    "im": 0,
    "p": 1,
    "q": 0,
    "iff1": 1,
    "iff2": 1,
*
         */
        cpu->regs.PC = initial->regs.pc;
        cpu->regs.SP = initial->regs.sp;
        cpu->regs.A = initial->regs.a;
        cpu->regs.B = initial->regs.b;
        cpu->regs.C = initial->regs.c;
        cpu->regs.D = initial->regs.d;
        cpu->regs.E = initial->regs.e;
        Z80_regs_F_setbyte(&cpu->regs.F, initial->regs.f);
        cpu->regs.H = initial->regs.h;
        cpu->regs.L = initial->regs.l;
        cpu->regs.I = initial->regs.i;
        cpu->regs.R = initial->regs.r;
        //printf("\nI R:%02x %02x", initial->regs.i, initial->regs.r);
        cpu->regs.EI = initial->regs.ei;
        cpu->regs.WZ = initial->regs.wz;
        cpu->regs.IX = initial->regs.ix;
        cpu->regs.IY = initial->regs.iy;
        cpu->regs.AF_ = initial->regs.af_;
        cpu->regs.BC_ = initial->regs.bc_;
        cpu->regs.DE_ = initial->regs.de_;
        cpu->regs.HL_ = initial->regs.hl_;
        cpu->regs.IM = initial->regs.im;
        cpu->regs.P = initial->regs.p;
        cpu->regs.Q = initial->regs.q;
        cpu->regs.IFF1 = initial->regs.iff1;
        cpu->regs.IFF2 = initial->regs.iff2;

        for (u32 j = 0; j < initial->num_ram_entry; j++) {
            test_RAM[initial->ram[j].addr] = (u8)initial->ram[j].val;
        }

        cpu->regs.prefix = 0;
        cpu->regs.rprefix = Z80P_HL;
        cpu->regs.IR = Z80_S_DECODE;
        cpu->pins.Addr = cpu->regs.PC;
        cpu->pins.D = -1;
        cpu->pins.MRQ = 1;
        cpu->pins.RD = 1;
        cpu->pins.IO = 0;
        cpu->pins.WR = 0;
        cpu->regs.TCU = -1;
        //cpu->regs.PC = (cpu->regs.PC + 1) & 0xFFFF;

        u32 addr;
        u32 passed = 1;
        for (u32 cyclei = 0; cyclei < test->num_cycles; cyclei++) {
            struct test_cycle *cycle = &test->cycles[cyclei];

            struct test_cycle *out_cycle = &out->cycles[cyclei];

            addr = cpu->pins.Addr;
            if (cpu->pins.RD && cpu->pins.MRQ) {
                if (test_RAM[cpu->pins.Addr] != 0xFFFFFFFF)
                    cpu->pins.D = test_RAM[cpu->pins.Addr];
            }
            if (cpu->pins.RD && cpu->pins.IO  && (cpu->pins.M1 == 0)) {
                for (u32 p = 0; p < test->num_ports; p++) {
                    if (test->ports[p].addr == cpu->pins.Addr) {
                        cpu->pins.D = test->ports[p].data;
                        break;
                    }
                }
            }

            Z80_cycle(cpu);
            addr = cpu->pins.Addr;

            if (cycle->addr != addr) {
                printf("\nMISMATCH IN PIN ADDR ME:%04x  TEST:%04x  STEP:%d", addr, cycle->addr, cyclei);
                passed = 0;
            }
            if ((cycle->data != cpu->pins.D) && (cycle->data != 0xFFFFFFFF)) {
                printf("\nMISMATCH IN DATA PINS!");
                passed = 0;
            }
            if (cpu->pins.RD != cycle->r) {
                printf("\nMISMATCH IN R");
                passed = 0;
            }
            if (cpu->pins.WR != cycle->w) {
                printf("\nMISMATCH IN W. MINE:%d THEIRS:%d cycle:%d", cpu->pins.WR, cycle->w, cyclei);
                passed = 0;
            }
            if (cpu->pins.MRQ != cycle->m) {
                printf("\nMISMATCH IN M");
                passed = 0;
            }
            if (cpu->pins.IO != cycle->i) {
                printf("\nMISMATCH IN M");
                passed = 0;
            }
            out_cycle->data = (i32)cpu->pins.D;
            out_cycle->addr = (i32)cpu->pins.Addr;
            out_cycle->r = cpu->pins.RD;
            out_cycle->w = cpu->pins.WR;
            out_cycle->m = cpu->pins.MRQ;
            out_cycle->i = cpu->pins.IO;

            last_pc = cpu->regs.PC;
            /*if (cyclei == (test->num_cycles-1)) {
                if (cpu->regs.IR != Z80_S_DECODE) {
                    printf("\nLENGTH MISMATCH! IR:%d cycle:%d, %d", cpu->regs.IR, cyclei, test->num_cycles);
                    out->length_mismatches++;
                }
            }*/

            if (cpu->pins.WR && cpu->pins.MRQ) {
                test_RAM[cpu->pins.Addr] = (u8)cpu->pins.D;
            }
        }
        if (!passed) {
            printf("\nFAILED TEST! %d", i);
            Z80_cycle(cpu);
            out->passed = 0;
            return;
        }

        //SM83_cycle(cpu);
        Z80_cycle(cpu);
        passed &= testregs(cpu, final, last_pc, is_call, i);
        for (u32 x = 0; x < final->num_ram_entry; x++) {
            if (test_RAM[final->ram[x].addr] != final->ram[x].val) {
                passed = 0;
                printf("\nRAM failed! addr:%04x mine:%02x test:%02x", final->ram[x].addr, test_RAM[final->ram[x].addr], final->ram[x].val);
            }
        }

        if (passed == 0) {
            printf("\nFAILED AT END!");
            Z80_cycle(cpu);
            out->passed = 0;
            return;
        }
    }
    out->passed = 1;
    out->failed_test_struct = NULL;
}

u32 test_z80_ins(struct Z80* cpu, u32 iclass, u32 ins, u32 is_call)
{
    char path[200];
    construct_path(path, iclass, ins);

    struct read_file_buf infile;
    rfb_init(&infile);
    struct jsontest tests[1000];
    if (!rfb_read(NULL, path, &infile)) {
        printf("\n\nCouldn't open file! %s", path);
        return 0;
    }
    parse_and_fill_out(tests, &infile);

    struct z80_test_result result;
    test_z80_automated(&result, cpu, tests, is_call);
    if (!result.passed) {
        printf("\nFAILED INSTRUCTION: %02x %02x", iclass ? 0xCB : 0, ins);
        pprint_test(result.failed_test_struct, result.cycles);
    }
    rfb_delete(&infile);

    return result.passed;
}

static u32 read_trace_z80(void *ptr, u32 addr)
{
    return test_RAM[addr];
}

void test_z80()
{
    dbg_init();
    struct Z80 cpu;
    u64 trace_cycles = 0;
    struct jsm_debug_read_trace rd;
    rd.ptr = NULL;
    rd.read_trace = &read_trace_z80;
    Z80_init(&cpu, 0);
    Z80_setup_tracing(&cpu, &rd, &trace_cycles);
    u32 test_classes[7] = {0, 0xCB, 0xED, 0xDD, 0xFD, 0xDDCB, 0xFDCB};
    u32 start_test = 0;
    u32 start_class = 0;
    u32 total_fail = 0;

    for (u32 mclass = start_class; mclass < 7; mclass++) {
        u32 iclass = test_classes[mclass];
        printf("\nTesting instruction class %04x", iclass);
        for (u32 i = start_test; i < 256; i++) {
            if (skip_tests(iclass, i)) {
                printf("\nTest for %02x skipped.", i);
                continue;
            }
            u32 icall = is_call(iclass, i);
            u32 result = test_z80_ins(&cpu, iclass, i, icall);
            if (!result) {
                total_fail = 1;
                printf("\nFAIL!");
                break;
            }
            if (total_fail) break;
            printf(" %02x", i);
        }
        if (total_fail) break;
    }

}