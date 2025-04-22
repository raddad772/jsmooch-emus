//
// Created by . on 4/21/25.
//

#ifndef JSMOOCH_EMUS_SNES_PPU_H
#define JSMOOCH_EMUS_SNES_PPU_H

#include <helpers/int.h>
#include "helpers/cvec.h"

struct SNES_PPU {
    u16 *cur_output;
    u32 cur_pixel;
    struct cvec_ptr display_ptr;
    struct JSM_DISPLAY *display;

    u8 VRAM[0x10000];
    u16 CGRAM[256];
    u8 OAM[544];

    struct {

    } io;
};

struct SNES;
struct SNES_memmap_block;
void SNES_PPU_init(struct SNES *);
void SNES_PPU_delete(struct SNES *);
void SNES_PPU_reset(struct SNES *);
void SNES_PPU_write(struct SNES *, u32 addr, u32 val, struct SNES_memmap_block *bl);
u32 SNES_PPU_read(struct SNES *, u32 addr, u32 old, u32 has_effect, struct SNES_memmap_block *bl);

#endif //JSMOOCH_EMUS_SNES_PPU_H
