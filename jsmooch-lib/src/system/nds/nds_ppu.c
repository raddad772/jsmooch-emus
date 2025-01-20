//
// Created by . on 1/18/25.
//
#include <string.h>

#include "helpers/multisize_memaccess.c"

#include "nds_bus.h"

void NDS_PPU_init(struct NDS *this)
{
    memset(&this->ppu, 0, sizeof(this->ppu));

    for (u32 en=0; en<2; en++) {
        struct NDSENG2D *p = &this->ppu.eng2d[en];
        p->mosaic.bg.hsize = p->mosaic.bg.vsize = 1;
        p->mosaic.obj.hsize = p->mosaic.obj.vsize = 1;
        p->bg[2].pa = 1 << 8; p->bg[2].pd = 1 << 8;
        p->bg[3].pa = 1 << 8; p->bg[3].pd = 1 << 8;
    }
}

void NDS_PPU_delete(struct NDS *this)
{
    
}

void NDS_PPU_reset(struct NDS *this)
{
    for (u32 en=0; en<2; en++) {
        struct NDSENG2D *p = &this->ppu.eng2d[en];
        p->mosaic.bg.hsize = p->mosaic.bg.vsize = 1;
        p->mosaic.obj.hsize = p->mosaic.obj.vsize = 1;
        p->bg[2].pa = 1 << 8; p->bg[2].pd = 1 << 8;
        p->bg[3].pa = 1 << 8; p->bg[3].pd = 1 << 8;
    }
}


void NDS_PPU_start_scanline(struct NDS *this) // Called on scanline start
{

}

void NDS_PPU_hblank(struct NDS *this) // Called at hblank time
{

}

void NDS_PPU_finish_scanline(struct NDS *this) // Called on scanline end, to render and do housekeeping
{

}

u32 NDS_PPU_read_2d_bg_palette(struct NDS *this, u32 eng_num, u32 addr, u32 sz)
{
    return cR[sz](this->ppu.eng2d[eng_num].mem.bg_palette, addr);
}

u32 NDS_PPU_read_2d_obj_palette(struct NDS *this, u32 eng_num, u32 addr, u32 sz)
{
    return cR[sz](this->ppu.eng2d[eng_num].mem.obj_palette, addr);
}

void NDS_PPU_write_2d_bg_palette(struct NDS *this, u32 eng_num, u32 addr, u32 sz, u32 val)
{
    cW[sz](this->ppu.eng2d[eng_num].mem.bg_palette, addr, val);
}

void NDS_PPU_write_2d_obj_palette(struct NDS *this, u32 eng_num, u32 addr, u32 sz, u32 val)
{
    cW[sz](this->ppu.eng2d[eng_num].mem.obj_palette, addr, val);
}
