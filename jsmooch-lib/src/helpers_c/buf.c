//
// Created by Dave on 2/6/2024.
//

#include <cassert>
#include <cstdio>
#include "file_exists.h"
#include <cstring>
#include <cstdlib>

#include "buf.h"

void buf_init(buf* this)
{
    this->ptr = NULL;
    this->size = 0;
}

void buf_allocate(buf* this, u64 size)
{
    if (this->ptr != NULL) {
        free(this->ptr);
        this->ptr = NULL;
    }
    this->dirty = 0;
    if (size == 0) {
        this->ptr = NULL;
        this->size = 0;
        return;
    }
    this->ptr = malloc(size);
    this->size = size;
}

void buf_delete(buf* this)
{
    if (this->ptr != NULL)
        free(this->ptr);
    this->ptr = NULL;
    this->size = 0;
}

void buf_copy(buf* dst, buf* src) {
    if (src->ptr == NULL) {
        buf_delete(dst);
        return;
    }
    buf_allocate(dst, src->size);
    if (src->size > 0)
        memcpy(dst->ptr, src->ptr, src->size);
}

void rfb_init(read_file_buf* this){
    buf_init(&this->buf);
    this->path[0] = 0;
    this->name[0] = 0;
    this->pos = 0;
}

int rfb_read(const char *fname, const char *fpath, read_file_buf *rfb)
{
    char OUTPATH[500];
    if (fname == NULL) {
        snprintf(OUTPATH, 500, "%s", fpath);
        strncpy(rfb->path, fpath, 255);
        snprintf(rfb->name, 255, "");
    }
    else {
        snprintf(OUTPATH, 500, "%s/%s", fpath, fname);
        strncpy(rfb->name, fname, 255);
        strncpy(rfb->path, fpath, 255);
    }
    if (!file_exists(OUTPATH)) {
        printf("\nFILE %s NOT FOUND", OUTPATH);
        return 0;
    }

    FILE *fil = fopen(OUTPATH, "rb");
    fseek(fil, 0L, SEEK_END);
    buf_allocate(&rfb->buf, ftell(fil));

    fseek(fil, 0L, SEEK_SET);
    fread(rfb->buf.ptr, sizeof(char), rfb->buf.size, fil);

    fclose(fil);
    return 1;
}

void rfb_delete(read_file_buf *rfb)
{
    buf_delete(&rfb->buf);
}

void mfs_init(multi_file_set* this)
{
    this->num_files = 0;
    for (u32 i = 0; i < MFS_MAX; i++) {
        rfb_init(&this->files[i]);
    }
}

void mfs_delete(multi_file_set* this)
{
    for (u32 i = 0; i < this->num_files; i++) {
        rfb_delete(&this->files[i]);
    }
    this->num_files = 0;
}

void mfs_add(const char *fname, const char *fpath, multi_file_set *this)
{
    assert(this->num_files < (MFS_MAX - 1));
    if (!rfb_read(fname, fpath, &this->files[this->num_files])) {
        printf("\nERROR GETTING FILE %s", fname);
    };
    this->num_files++;
}
