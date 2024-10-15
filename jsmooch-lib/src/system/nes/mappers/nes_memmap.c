//
// Created by . on 10/3/24.
//

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "helpers/debugger/debugger.h"

#include "helpers/simplebuf.h"
#include "nes_memmap.h"

void NES_memmap_map(struct NES_memmap *mmap, u32 shift, u32 range_start, u32 range_end, struct simplebuf8* buf, u32 offset, u32 is_readonly, struct debugger_interface *iface, u32 bus_num)
{
    u32 range_size = 1 << shift;
    for (u32 addr = range_start; addr < range_end; addr += range_size) {
        struct NES_memmap *m = mmap + (addr >> shift);
        if (iface && ((m->offset != offset) || (m->buf != buf))) {
            // Mark as dirty!
            debugger_interface_dirty_mem(iface, bus_num, range_start, range_end);
        }
        m->offset = offset;
        m->buf = buf;
        m->empty = buf == NULL;
        m->read_only = is_readonly;
        m->addr = addr;
        m->mask = range_size - 1;

        offset = (offset + range_size) % buf->sz;
    }
}

void NES_memmap_init_empty(struct NES_memmap *map, u32 addr_start, u32 addr_end, u32 shift)
{
    for (u32 i = addr_start; i < addr_end; i += (1 << shift)) {
        struct NES_memmap *m = &map[i >> shift];
        m->empty = 1;
        m->addr = i;
        m->offset = 0;
        m->read_only = 1;
        m->buf = NULL;
    }
}

u32 NES_mmap_read(struct NES_memmap *this, u32 addr, u32 old_val)
{
    if (this->empty) {
        printf("\nREAD EMPTY ADDR %04x", addr);
        return old_val;
    }
    assert(this->buf);
    assert(this->buf->ptr);
    //if (do_print) printf("\nADDR:%04x  mask:%04x   offset:%x  sz:%lld", addr, this->mask, this->offset, this->buf->sz);
    return this->buf->ptr[((addr & this->mask) + this->offset) % this->buf->sz];
}

void NES_mmap_write(struct NES_memmap *this, u32 addr, u32 val)
{
    if (this->read_only || this->empty) return;
    assert(this->buf);
    assert(this->buf->ptr);
    this->buf->ptr[((addr & this->mask) + this->offset) % this->buf->sz] = val;
}
