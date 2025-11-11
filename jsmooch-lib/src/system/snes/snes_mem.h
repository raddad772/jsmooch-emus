//
// Created by . on 4/21/25.
//

#ifndef JSMOOCH_EMUS_SNES_MEM_H
#define JSMOOCH_EMUS_SNES_MEM_H

#include "helpers/buf.h"

#include "helpers/int.h"

struct SNES_memmap_block {
    enum SMB_kind {
        SMB_open,
        SMB_CPU,
        SMB_APU,
        SMB_PPU,
        SMB_WRAM,
        SMB_ROM,
        SMB_SRAM
    } kind;
    //u8 speed;
    u32 offset, mask;
};

struct SNES;
typedef void (*SNES_memmap_write)(SNES *, u32 addr, u32 val, SNES_memmap_block *bl);
typedef u32 (*SNES_memmap_read)(SNES *, u32 addr, u32 old, u32 has_effect, SNES_memmap_block *bl);

struct SNES_mem {
    struct SNES_memmap_block blockmap[0x1000];
    SNES_memmap_read read[0x1000];
    SNES_memmap_write write[0x1000];

    u8 WRAM[0x20000];

    u32 ROMSizebit;
    u32 SRAMSizebit;
    u32 ROMSize;
    u32 SRAMSize;
    struct buf ROM;
};

struct SNES;
void SNES_mem_init(SNES *);
u32 SNES_wdc65816_read(SNES *, u32 addr, u32 old, u32 has_effect);
void SNES_wdc65816_write(SNES *, u32 addr, u32 val);
u32 SNES_spc700_read(SNES *, u32 addr, u32 old, u32 has_effect);
void SNES_spc700_write(SNES *, u32 addr, u32 val);
void SNES_mem_cart_inserted(SNES *);

#endif //JSMOOCH_EMUS_SNES_MEM_H
