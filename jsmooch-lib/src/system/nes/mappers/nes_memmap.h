//
// Created by . on 10/3/24.
//

#ifndef JSMOOCH_EMUS_NES_MEMMAP_H
#define JSMOOCH_EMUS_NES_MEMMAP_H

#include "helpers/int.h"
#include "helpers/sram.h"

struct NES_memmap {
    u32 addr;                       // Addr at which this chunk starts
    u32 offset;                     // Offset from u8* data
    u32 read_only, empty;
    u32 mask;
    u32 bank;
    u32 is_SRAM;
    struct persistent_store *SRAM;
    struct simplebuf8 *buf;         // Pointer to data

    u32 read(u32 read_addr, u32 old_val);
    void write(u32 write_addr, u32 val);

};

struct NES_mapper;
void NES_memmap_map(NES_memmap *mmap, u32 shift, u32 range_start, u32 range_end, struct simplebuf8* buf, u32 offset, u32 is_readonly, struct debugger_interface *iface, u32 bus_num, struct persistent_store *SRAM);
void NES_memmap_init_empty(NES_memmap *map, u32 addr_start, u32 addr_end, u32 shift);



#endif //JSMOOCH_EMUS_NES_MEMMAP_H
