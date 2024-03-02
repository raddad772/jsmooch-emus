//
// Created by Dave on 2/6/2024.
//

#ifndef JSMOOCH_EMUS_BUF_H
#define JSMOOCH_EMUS_BUF_H

#include "helpers/int.h"

struct buf {
    void *ptr;
    u64 size;
};

void buf_init(struct buf* this);
void buf_allocate(struct buf* this, u64 size);
void buf_delete(struct buf* this);
void buf_copy(struct buf* dst, struct buf* src);

struct read_file_buf {
    struct buf buf;
    char path[255];
    char name[255];
};

void rfb_init(struct read_file_buf* this);
void rfb_delete(struct read_file_buf *rfb);
int rfb_read(const char *fname, const char *fpath, struct read_file_buf *rfb);

#define MFS_MAX 20
struct multi_file_set {
    u32 num_files;
    struct read_file_buf files[MFS_MAX];
};

void mfs_init(struct multi_file_set* this);
void mfs_delete(struct multi_file_set* this);
void mfs_add(const char *fname, const char *fpath, struct multi_file_set *this);

#endif //JSMOOCH_EMUS_BUF_H
