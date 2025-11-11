//
// Created by . on 8/9/24.
//

#include "bitbuffer.h"
#include <stdio.h>

void bitbuf_init(bitbuf *this, u32 num_bits_preallocate, u32 lsb_first)
{
    cvec_init(&this->buf, 1, num_bits_preallocate * 8);
    this->write.partial = 0;
    this->write.next_bit = lsb_first ? 0 : 7;
    this->lsb_first = lsb_first;
    this->num_bits = 0;
}

void bitbuf_delete(bitbuf *this)
{
    cvec_delete(&this->buf);
}

void bitbuf_clear(bitbuf *this)
{
    this->write.partial = 0;
    this->write.next_bit = this->lsb_first ? 0 : 7;
    cvec_clear(&this->buf);
    this->num_bits = 0;
}

u32 bitbuf_read_bit(bitbuf *this, u32 pos)
{
    if (pos >= this->num_bits) return 2;
    return *(((u8 *)this->buf.data) + pos);
}

void bitbuf_write_bits(bitbuf *this, u32 num, u32 val)
{
    for (i32 i = num-1; i >=0; i--) {
        u8 *by = cvec_push_back(&this->buf);
        *by = (val >> i) & 1;
        this->num_bits++;
    }
}

int bitbuf_write_final(bitbuf *this)
{
    return 0;
}