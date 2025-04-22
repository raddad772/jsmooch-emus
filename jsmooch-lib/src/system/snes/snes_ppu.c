//
// Created by . on 4/21/25.
//

#include "snes_ppu.h"
#include "snes_bus.h"

void SNES_PPU_init(struct SNES *this)
{
    
}

void SNES_PPU_delete(struct SNES *this)
{
    
}

void SNES_PPU_reset(struct SNES *this)
{
    
}

void SNES_PPU_write(struct SNES *this, u32 addr, u32 val, struct SNES_memmap_block *bl)
{
    printf("\nWARN SNES PPU WRITE NOT IN");
}

u32 SNES_PPU_read(struct SNES *this, u32 addr, u32 old, u32 has_effect, struct SNES_memmap_block *bl)
{
    printf("\nWARN SNES PPU READ NOT IN");
    return 0;
}
