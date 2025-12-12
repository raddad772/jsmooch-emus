//
// Created by . on 1/21/25.
//

#include "nds_ipc.h"

namespace NDS {

bool IPC_FIFO::is_empty() const
{
    return len == 0;
}

bool IPC_FIFO::is_not_empty() const {
    return len != 0;
}

void IPC_FIFO::empty()
{
    head = 0;
    tail = 0;
    len = 0;
    last = 0;
    data[0] = data[1] = data[2] = data[3] = 0;
}

bool IPC_FIFO::is_full() const
{
    return len == 16;
}

bool IPC_FIFO::push(u32 val)
{
    if (is_full()) return true;
    data[tail] = val;
    tail = (tail + 1) &  15;
    len++;

    return false;
}

u32 IPC_FIFO::peek_last() const
{
    return last;
}

u32 IPC_FIFO::pop()
{
    if (len == 0) return last;
    last = data[head];
    len--;
    head = (head + 1) & 15;
    return last;
}
}