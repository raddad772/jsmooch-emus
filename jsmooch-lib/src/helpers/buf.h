#pragma once

#include "helpers/int.h"
#include <vector>

struct buf {
    buf();
    ~buf();
    void *ptr;
    u64 size;
    u32 dirty; // used by external programs

    void allocate(u64 size);
    void del();
    void copy(buf* src);
};

struct read_file_buf {
    buf buf{};
    char path[255]{};
    char name[255]{};
    u64 pos{};

    read_file_buf();
    ~read_file_buf();
    int read(const char *fname, const char *fpath);
};

struct multi_file_set {
    std::vector<read_file_buf> files{};

    multi_file_set();
    ~multi_file_set();
    void add(const char *fname, const char *fpath);
};
