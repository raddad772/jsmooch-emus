//
// Created by . on 1/21/25.
//

#include "nds_ipc.h"

u32 NDS_IPC_fifo_is_empty(NDS_CPU_FIFO *this)
{
    return this->len == 0;
}

u32 NDS_IPC_fifo_is_not_empty(NDS_CPU_FIFO *this)
{
    return this->len != 0;
}

void NDS_IPC_empty_fifo(NDS *this, NDS_CPU_FIFO *f)
{
    f->head = 0;
    f->tail = 0;
    f->len = 0;
    f->last = 0;
    f->data[0] = f->data[1] = f->data[2] = f->data[3] = 0;
}

u32 NDS_IPC_fifo_is_full(NDS_CPU_FIFO *this)
{
    return this->len == 16;
}

u32 NDS_IPC_fifo_push(NDS_CPU_FIFO *this, u32 val)
{
    if (NDS_IPC_fifo_is_full(this)) return 1;
    this->data[this->tail] = val;
    this->tail = (this->tail + 1) &  15;
    this->len++;

    return 0;
}

u32 NDS_IPC_fifo_peek_last(NDS_CPU_FIFO *this)
{
    return this->last;
}

u32 NDS_IPC_fifo_pop(NDS_CPU_FIFO *this)
{
    if (this->len == 0) return this->last;
    this->last = this->data[this->head];
    this->len--;
    this->head = (this->head + 1) & 15;
    return this->last;
}
