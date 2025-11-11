//
// Created by . on 8/9/24.
//
// Basic bit-by-bit read/write functions

#ifndef JSMOOCH_EMUS_BITBUFFER_H
#define JSMOOCH_EMUS_BITBUFFER_H

#ifdef __cplusplus
extern "C" {
#endif


#include "int.h"
#include "cvec.h"

struct bitbuf {
    struct cvec buf;
    u32 lsb_first;
    u32 num_bits;

    struct {
        u8 partial;
        u8 next_bit;
    } write;

};

void bitbuf_init(bitbuf *, u32 num_bits_preallocate, u32 lsb_first);
void bitbuf_delete(bitbuf *);
void bitbuf_clear(bitbuf *);

u32 bitbuf_read_bits(bitbuf*, u32 pos, u32 num);
void bitbuf_write_bits(bitbuf*, u32 num, u32 val);
int bitbuf_write_final(bitbuf *this);

#ifdef __cplusplus
}
#endif


#endif //JSMOOCH_EMUS_BITBUFFER_H
