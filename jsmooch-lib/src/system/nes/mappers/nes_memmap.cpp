//
// Created by . on 10/3/24.
//

#include <cassert>
#include <cstdlib>

#include "helpers/debugger/debugger.h"

#include "helpers/simplebuf.h"
#include "nes_memmap.h"

void NES_memmap_map(NES_memmap *mmap, u32 shift, u32 range_start, u32 range_end, simplebuf8* buf, u32 offset, u32 is_readonly, debugger_interface *iface, u32 bus_num, persistent_store *SRAM)
{
    u32 range_size = 1 << shift;
    u32 is_SRAM = 0;
    if (buf && SRAM) {
        is_SRAM = buf->ptr == SRAM->data;
    }
    for (u32 addr = range_start; addr < range_end; addr += range_size) {
        NES_memmap *m = mmap + (addr >> shift);
        if (iface && ((m->offset != offset) || (m->buf != buf))) {
            // Mark as dirty!
            debugger_interface_dirty_mem(iface, bus_num, range_start, range_end);
        }
        m->offset = offset;
        m->buf = buf;
        m->empty = buf == nullptr;
        m->read_only = is_readonly;
        m->addr = addr;
        m->is_SRAM = is_SRAM;
        m->SRAM = SRAM;
        m->mask = range_size - 1;

        offset = (offset + range_size) % buf->sz;
    }
}

void NES_memmap_init_empty(NES_memmap *map, u32 addr_start, u32 addr_end, u32 shift)
{
    for (u32 i = addr_start; i < addr_end; i += (1 << shift)) {
        NES_memmap *m = &map[i >> shift];
        m->empty = 1;
        m->addr = i;
        m->offset = 0;
        m->read_only = 1;
        m->is_SRAM = 0;
        m->buf = nullptr;
    }
}

u32 NES_memmap::read(u32 read_addr, u32 old_val)
{
    if (this->empty) {
        printf("\nREAD EMPTY ADDR %04x", addr);
        return old_val;
    }
    assert(this->buf);
    assert(this->buf->ptr);
    //if (do_print) printf("\nADDR:%04x  mask:%04x   offset:%x  sz:%lld", addr, mask, offset, buf->sz);
    return buf->ptr[((read_addr & mask) + offset) % buf->sz];
}

void NES_memmap::write(u32 write_addr, u32 val)
{
    if (read_only || empty) return;
    assert(buf);
    assert(buf->ptr);
    buf->ptr[((write_addr & mask) + offset) % buf->sz] = val;
    if (is_SRAM && SRAM) SRAM->dirty = 1;
}
