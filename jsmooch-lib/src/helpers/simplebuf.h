//
// Created by . on 8/29/24.
//

#ifndef JSMOOCH_EMUS_SIMPLEBUF_CPP_H
#define JSMOOCH_EMUS_SIMPLEBUF_CPP_H

#include "int.h"
struct buf;
struct simplebuf8 {
    simplebuf8() = default;
    ~simplebuf8();
    void allocate(u64 sz);
    void clear();
    void copy_from_buf(buf *src);
    u8 *ptr{};
    u64 sz{};
    u32 mask{};
};

struct buf;
void simplebuf8_init(struct simplebuf8*);
void simplebuf8_delete(struct simplebuf8*);
void simplebuf8_allocate(struct simplebuf8*, u64 sz);
void simplebuf8_clear(struct simplebuf8 *);
void simplebuf8_copy_from_buf(struct simplebuf8 *dest, struct buf *src);

#endif //JSMOOCH_EMUS_SIMPLEBUF_H
