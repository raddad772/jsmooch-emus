#pragma once

#include "helpers/int.h"
#include <vector>

#define MFS_MAX 20


struct buf {
    explicit buf(size_t s) { allocate(s); };
    buf() = default;
    ~buf() { del(); }

    // Don't copy that floppy!
    buf (const buf&) = delete;
    buf& operator=(const buf&) = delete;

    // ...but do move it...
    buf(buf&& other) noexcept
    {
        ptr = other.ptr;
        size = other.size;
        other.ptr = nullptr;
        other.size = 0;
    }

    buf& operator=(buf&& other) noexcept {
        if (this != &other) {
            del();
            ptr = other.ptr;
            size = other.size;
            other.ptr = nullptr;
            other.size = 0;
        }
        return *this;
    }

    void *ptr{};
    size_t size{};
    u32 dirty{}; // used by external programs

    void allocate(size_t size);
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

    void add(const char *fname, const char *fpath);
    void clear();
};
