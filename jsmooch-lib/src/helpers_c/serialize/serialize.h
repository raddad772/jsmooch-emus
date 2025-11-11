//
// Created by . on 11/10/24.
//

#ifndef JSMOOCH_EMUS_SERIALIZE_H
#define JSMOOCH_EMUS_SERIALIZE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#include "helpers/int.h"
#include "helpers/enums.h"
#include "helpers/cvec.h"
#include "helpers/img.h"
#include "helpers/jsm_string.h"

struct serialized_opt_data {
    char buf[512];
    u32 len;
};

struct ser_iter {
    u8 *buf;
    u64 offset;
};

struct serialized_state_section {
    u32 offset;

    char friendly_name[50]; // PPU, CPU, etc.
    u32 kind; // each system will have its own section kinds
    u32 version;
    struct serialized_opt_data opt;
    u64 sz;
};

struct serialized_state {
    enum jsm::systems kind;
    u32 version;
    struct serialized_opt_data opt;

    struct jsm_string fname, fpath;

    struct cvec sections;
    struct cvec buf;

    u32 has_screenshot;
    struct jsimg screenshot;

    struct ser_iter iter;

    struct serialized_state_section *cur_section;
};


void serialized_state_init(serialized_state *);
void serialized_state_delete(serialized_state *);
void serialized_state_new_section(serialized_state *, const char *friendly_name, int kind, int version);
void Sadd(serialized_state *, const void *ptr, u64 howmuch);
void Sload(serialized_state *, void *ptr, u64 howmuch);
void serialized_state_write_to_file(serialized_state *, FILE *f);
int serialized_state_read_from_file(serialized_state *, FILE *f, size_t file_size);
#define u64S(x) Sadd(state, &(x), 8)
#define i64S(x) u64S(x)
#define u32S(x) Sadd(state, &(x), 4)
#define i32S(x) u32S(x)
#define u16S(x) Sadd(state, &(x), 2)
#define i16S(x) u16S(x)
#define u8S(x) Sadd(state, &(x), 1)
#define i8S(x) u8S(x)

#ifdef __cplusplus
}
#endif

#endif //JSMOOCH_EMUS_SERIALIZE_H
