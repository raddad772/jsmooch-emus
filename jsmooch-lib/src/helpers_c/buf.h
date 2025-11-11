//
// Created by Dave on 2/6/2024.
//

#ifndef JSMOOCH_EMUS_BUF_H
#define JSMOOCH_EMUS_BUF_H

#include "helpers/int.h"

struct buf {
    void *ptr;
    u64 size;
    u32 dirty; // used by external programs
};

void buf_init(buf*);
void buf_allocate(buf*, u64 size);
void buf_delete(buf*);
void buf_copy(buf* dst, buf* src);

struct read_file_buf {
    struct buf buf;
    char path[255];
    char name[255];
    u64 pos;
};

void rfb_init(read_file_buf*);
void rfb_delete(read_file_buf *rfb);
int rfb_read(const char *fname, const char *fpath, read_file_buf *rfb);

struct multi_file_set {
    u32 num_files;
    struct read_file_buf files[MFS_MAX];
};

void mfs_init(multi_file_set*);
void mfs_delete(multi_file_set*);
void mfs_add(const char *fname, const char *fpath, multi_file_set *);

#endif //JSMOOCH_EMUS_BUF_H
