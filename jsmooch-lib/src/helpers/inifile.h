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
    char name[500];
    std::vector<kv_pair> kvs;
};

struct kv_pair {
    char key[MAX_KEY_LEN]{};
    kv_kind kind{};

    char str_value[MAX_VAL_LEN]{};
    i64 int_value{};
    float float_value{};
};

struct inifile {
    char path[500]{};
    std::vector<kv_section> sections;
};

void inifile_init(struct inifile*);
void inifile_delete(struct inifile*);

u32 inifile_has_key(struct inifile*, const char* section, const char* key);
struct kv_pair* inifile_get_or_make_key(struct inifile *, const char* section, const char* key);

int inifile_load(struct inifile*, const char* path);
void inifile_save(struct inifile*);

char *construct_path_with_home(char *w, size_t w_sz, const char *who);

