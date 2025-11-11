#pragma once

#include "helpers/int.h"
#include <vector>

#define MFS_MAX 20

struct buf {
    explicit buf(int n);
    buf() = default;
    ~buf();
    void *ptr{};
    u64 size{};
    u32 dirty{}; // used by external programs

    void allocate(u64 size);
    void del();
    void copy(const buf* src);
};

struct read_file_buf {
    buf buf{};
    char path[255]{};
    char name[255]{};
    u64 pos{};

    int read(const char *fname, const char *fpath);
};

struct multi_file_set {
    std::vector<read_file_buf> files{};

    u32 num_files;
    void add(const char *fname, const char *fpath);
    void clear();
};
