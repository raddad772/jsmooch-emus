//
// Created by . on 7/25/24.
//

#include <cstdio>
#include <cstring>

#include "helpers/debug.h"
#include "via6522.h"

void via6522_init(via6522* this, u64* trace_cycles)
{
    memset(this, 0, sizeof(via6522));
    this->trace.cycles = trace_cycles;
    if (trace_cycles == NULL) this->trace.cycles = &this->trace.dummy_cycles;
}

void via6522_delete(via6522* this)
{

}

void via6522_write(via6522* this, u8 addr, u8 val)
{
    printf("\nUnhandled VIA6522 write addr:%02x val:%08x cyc:%lld", addr, val, *this->trace.cycles);
}

u8 via6522_read(via6522* this, u8 addr, u8 old)
{
    printf("\nUnhandled VIA6522 write addr:%02x cyc:%lld", addr, *this->trace.cycles);
    return old;
}
