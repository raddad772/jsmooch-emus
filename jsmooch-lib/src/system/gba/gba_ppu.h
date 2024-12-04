//
// Created by . on 12/4/24.
//

#ifndef JSMOOCH_EMUS_GBA_PPU_H
#define JSMOOCH_EMUS_GBA_PPU_H

#include "helpers/int.h"
#include "helpers/physical_io.h"


struct GBA_PPU {
    u16 *cur_output;
    u32 cur_pixel;
    struct cvec_ptr display_ptr;
    struct JSM_DISPLAY *display;

};

struct GBA;
void GBA_PPU_init(struct GBA*);
void GBA_PPU_delete(struct GBA*);
void GBA_PPU_reset(struct GBA*);

void GBA_PPU_start_scanline(struct GBA*); // Called on scanline start
void GBA_PPU_hblank(struct GBA*); // Called at hblank time
void GBA_PPU_finish_scanline(struct GBA*); // Called on scanline end, to render and do housekeeping

#endif //JSMOOCH_EMUS_GBA_PPU_H
