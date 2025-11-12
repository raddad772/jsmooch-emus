//
// Created by Dave on 2/6/2024.
//

#include <cassert>
#include <cstdio>
#include <cstring>

#include "buf.h"
#include "file_exists.h"

void buf::allocate(size_t insize)
{
    if (ptr != nullptr) {
        free(ptr);
        ptr = nullptr;
    }
    dirty = 0;
    if (insize == 0) {
        ptr = nullptr;
        size = 0;
        return;
    }
    ptr = malloc(insize);
    size = insize;
}

void buf::del() {
    if (ptr)
        free(ptr);
    ptr = nullptr;
    size = 0;
}

void buf::copy(const buf* src) {
    if (src->ptr == nullptr) {
        del();
        return;
    }
    allocate(src->size);
    if (src->size > 0)
        memcpy(ptr, src->ptr, src->size);
}

int read_file_buf::read(const char *fname, const char *fpath)
{
    char OUTPATH[500];
    if (fname == nullptr) {
        snprintf(OUTPATH, 500, "%s", fpath);
        strncpy(path, fpath, 255);
        snprintf(name, 255, "");
    }
    else {
        snprintf(OUTPATH, 500, "%s/%s", fpath, fname);
        strncpy(name, fname, 255);
        strncpy(path, fpath, 255);
    }
    if (!file_exists(OUTPATH)) {
        printf("\nFILE %s NOT FOUND", OUTPATH);
        return 0;
    }

    FILE *fil = fopen(OUTPATH, "rb");
    fseek(fil, 0L, SEEK_END);
    buf.allocate(ftell(fil));

    fseek(fil, 0L, SEEK_SET);
    fread(buf.ptr, sizeof(char), buf.size, fil);

    fclose(fil);
    return 1;
}

void multi_file_set::clear() {
    files.clear();
}


void multi_file_set::add(const char *fname, const char *fpath) {
    auto &r = files.emplace_back();
    if (!r.read(fname, fpath)) {
        printf("\nERROR GETTING FILE %s", fname);
    }
}
