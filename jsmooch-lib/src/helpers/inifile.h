//
// Created by . on 8/3/24.
//

#pragma once

#include "int.h"
#include "cvec.h"

enum kv_kind {
    kvk_empty,
    kvk_string,
    kvk_int,
    kvk_float
};

#define MAX_KEY_LEN 500
#define MAX_VAL_LEN 500

struct kv_pair;
struct kv_section {
    char name[500]{};
    std::vector<kv_pair> kvs{};
    kv_pair *find_key(const char *fname);
    kv_pair &new_pair(const char *fname);
    void parse_pair(char *key, char *val);
};

struct kv_pair {
    char key[MAX_KEY_LEN]{};
    kv_kind kind{};

    char str_value[MAX_VAL_LEN]{};
    i64 int_value{};
    float float_value{};
};

struct inifile {
    kv_section *find_section(const char* name);
    kv_section &new_section(const char *name);
    void clear();
    u32 has_key(const char* section, const char* key);
    kv_section *get_or_make_section(const char* section);
    kv_pair* get_or_make_key(const char* section, const char* key);
    int  load(const char* path);

    char path[500]{};
    std::vector<kv_section> sections;
};

char *construct_path_with_home(char *w, size_t w_sz, const char *who);

