//
// Created by . on 1/21/25.
//

#ifndef JSMOOCH_EMUS_NDS_IPC_H
#define JSMOOCH_EMUS_NDS_IPC_H

#include "helpers/int.h"

struct NDS_CPU_FIFO {
    u32 data[16];
    u32 head, tail;
    u32 last;
    u32 len;

    u32 enable;
};

struct NDS;
u32 NDS_IPC_fifo_is_empty(struct NDS_CPU_FIFO *);
u32 NDS_IPC_fifo_is_full(struct NDS_CPU_FIFO *);
u32 NDS_IPC_fifo_is_not_empty(struct NDS_CPU_FIFO *);
void NDS_IPC_empty_fifo(struct NDS *this, struct NDS_CPU_FIFO *f);
u32 NDS_IPC_fifo_push(struct NDS_CPU_FIFO *, u32 val);
u32 NDS_IPC_fifo_peek_last(struct NDS_CPU_FIFO *);
u32 NDS_IPC_fifo_pop(struct NDS_CPU_FIFO *);

#endif //JSMOOCH_EMUS_NDS_IPC_H
