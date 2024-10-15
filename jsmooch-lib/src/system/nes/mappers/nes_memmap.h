//
// Created by . on 10/3/24.
//

#ifndef JSMOOCH_EMUS_NES_MEMMAP_H
#define JSMOOCH_EMUS_NES_MEMMAP_H

#include "helpers/int.h"

struct NES_memmap {
    u32 addr;                       // Addr at which this chunk starts
    u32 offset;                     // Offset from u8* data
    u32 read_only, empty;
    u32 mask;
    u32 bank;
    struct simplebuf8 *buf;         // Pointer to data
};

struct NES_bus;
void NES_memmap_map(struct NES_memmap *mmap, u32 shift, u32 range_start, u32 range_end, struct simplebuf8* buf, u32 offset, u32 is_readonly);
void NES_memmap_init_empty(struct NES_memmap *map, u32 addr_start, u32 addr_end, u32 shift);


u32 NES_mmap_read(struct NES_memmap *this, u32 addr, u32 old_val);
void NES_mmap_write(struct NES_memmap *this, u32 addr, u32 val);


#endif //JSMOOCH_EMUS_NES_MEMMAP_H
