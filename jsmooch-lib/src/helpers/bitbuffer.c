//
// Created by . on 8/9/24.
//

#include "bitbuffer.h"


void bitbuf_init(struct bitbuf *this, u32 num_bits_preallocate, u32 lsb_first)
{
    cvec_init(&this->buf, 1, num_bits_preallocate * 8);
    this->write.partial = 0;
    this->write.next_bit = lsb_first ? 0 : 7;
    this->lsb_first = lsb_first;
}

void bitbuf_delete(struct bitbuf *this)
{
    cvec_delete(&this->buf);
}


u32 bitbuf_read_bits(struct bitbuf *this, u32 pos, u32 num)
{
    u32 bitmask;
    if (this->lsb_first)
        bitmask = 1 << (pos & 7);
    else
        bitmask = 1 << (7 - (pos & 7));
    u32 bytenum = pos >> 4;

    assert(bytenum < this->buf.len);
    assert(num<33);
    assert(num>0);

    u8 b = *(((u8 *)this->buf.data) + bytenum);
    u32 out = 0;

    for (u32 i = 0; i < num; i++) {
        out <<= 1;
        out |= ((b & bitmask) != 0);
        if (this->lsb_first) {
            if (bitmask == 0x80) {
                bytenum++;
                bitmask = 1;
                b = *(((u8 *)this->buf.data) + bytenum);
            }
            else
                bitmask <<= 1;
        }
        else {
            if (bitmask == 1) {
                bytenum++;
                bitmask = 0x80;
                b = *(((u8 *)this->buf.data) + bytenum);
            }
            else
                bitmask >>= 1;
        }
    }
    return out;
}

void bitbuf_write_bits(struct bitbuf *this, u32 num, u32 val)
{
    // output num bits from val to buffer
    for (u32 i = 0; i < num; i++) {
        u32 bit = val & 1;
        num >>= 1;
        this->write.partial |= (bit << this->write.next_bit);
        u32 finished_byte = 0;
        if (this->lsb_first) {
            if (this->write.next_bit == 7) {
                this->write.next_bit = 0;
                finished_byte = 1;
            }
            else
                this->write.next_bit++;
        }
        else {
            if (this->write.next_bit == 0) {
                this->write.next_bit = 7;
                finished_byte = 1;
            }
            else
                this->write.next_bit--;
        }

        if (finished_byte) {
            u8 *y = cvec_push_back(&this->buf);
            *y = this->write.partial;
            this->write.partial = 0;
        }
    }
}

int bitbuf_write_final(struct bitbuf *this)
{
    if ((this->lsb_first) && (this->write.next_bit == 0)) return 0;
    if ((!this->lsb_first) && (this->write.next_bit == 7)) return 0;

    u8 *y = cvec_push_back(&this->buf);
    *y = this->write.partial;
    this->write.partial = 0;

    return 1;
}