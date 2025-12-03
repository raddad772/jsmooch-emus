//
// Created by . on 8/9/24.
//

#include "bitbuffer.h"
#include <cstdio>


bitbuf::bitbuf(size_t num_bits_to_preallocate, bool lsb_first_in)
{
    buf.reserve(num_bits_to_preallocate * 2);
    write.partial = 0;
    write.next_bit = lsb_first_in ? 0 : 7;
    lsb_first = lsb_first_in;
    num_bits = 0;
}

void bitbuf::clear()
{
    write.partial = 0;
    write.next_bit = lsb_first ? 0 : 7;
    buf.clear();
    num_bits = 0;
}

u32 bitbuf::read_bit(u32 pos)
{
    if (pos >= num_bits) return 2;
    return buf[pos];
}

void bitbuf::write_bits(i32 num, u32 val)
{
    for (i32 i = num-1; i >=0; i--) {
        buf.emplace_back((val >> i) & 1);
        num_bits++;
    }
}

int bitbuf::write_final()
{
    return 0;
}