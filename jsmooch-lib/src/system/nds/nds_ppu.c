//
// Created by . on 1/18/25.
//
#include <string.h>

#include "helpers/multisize_memaccess.c"

#include "nds_bus.h"
#include "nds_dma.h"
#include "nds_regs.h"
#include "nds_irq.h"
#include "nds_debugger.h"
#include "helpers/color.h"

#define NDS_WIN0 0
#define NDS_WIN1 1
#define NDS_WINOBJ 2
#define NDS_WINOUTSIDE 3

void NDS_PPU_init(struct NDS *this)
{
    memset(&this->ppu, 0, sizeof(this->ppu));

    for (u32 en=0; en<2; en++) {
        struct NDSENG2D *p = &this->ppu.eng2d[en];
        p->num = en;
        p->mosaic.bg.hsize = p->mosaic.bg.vsize = 1;
        p->mosaic.obj.hsize = p->mosaic.obj.vsize = 1;
        p->bg[2].pa = 1 << 8; p->bg[2].pd = 1 << 8;
        p->bg[3].pa = 1 << 8; p->bg[3].pd = 1 << 8;
    }
    this->ppu.eng2d[0].io.bg.do_3d = 0;
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

static u32 read_vram_bg(struct NDS *this, struct NDSENG2D *eng, u32 addr, u32 sz)
{
    assert((addr >> 14) < 32);
    u8 *ptr = eng->mem.bg_vram[(addr >> 14) & 31];
    if (!ptr) {
        static int doit = 0;
        if (!doit) {
            printf("\nVRAM BG READ FAIL!");
            doit = 1;
        }
        return 0;
    }
    return cR[sz](ptr, addr & 0x3FFF);
}

static u32 read_vram_obj(struct NDS *this, struct NDSENG2D *eng, u32 addr, u32 sz)
{
    assert((addr >> 14) < 16);
    u8 *ptr = eng->mem.obj_vram[(addr >> 14) & 15];
    if (!ptr) {
        static int doit = 0;
        if (!doit) {
            printf("\nVRAM OBJ READ FAIL!");
            doit = 1;
        }
        return 0;
    }
    return cR[sz](ptr, addr & 0x3FFF);
}

static u32 read_pram_bg(struct NDS *this, struct NDSENG2D *eng, u32 addr, u32 sz)
{
    return cR[sz](eng->mem.bg_palette, addr & 0x1FF);
}

static u32 read_ext_palette_obj(struct NDS *this, struct NDSENG2D *eng, u32 palette, u32 index)
{
    static int a = 1;
    if (a) {
        printf("\nWARN READ FROM UNIMPLEMENTED: OBJ EXT PALETTE");
        a = 0;
    }
    return 0x7F0F;
}

static u32 read_pram_obj2(struct NDS *this, struct NDSENG2D *eng, u32 palette, u32 index)
{
    if (eng->io.obj.extended_palettes)
        return read_ext_palette_obj(this, eng, palette, index) & 0x7FFF;
    u32 addr = ((palette << 5) | (index << 1)) & 0x1FF;
    u32 v = cR16(eng->mem.obj_palette, addr) & 0x7FFF;
    return v;
}

static u32 read_pram_obj(struct NDS *this, struct NDSENG2D *eng, u32 addr, u32 sz)
{
    return cR[sz](eng->mem.obj_palette, addr & 0x1FF);
}

static u32 read_oam(struct NDS *this, struct NDSENG2D *eng, u32 addr, u32 sz)
{
    return cR[sz](eng->mem.oam, addr & 0x3FF);
}

#define OUT_WIDTH 256

static void get_obj_tile_size(u32 sz, u32 shape, u32 *htiles, u32 *vtiles)
{
#define T(s1, s2, hn, vn) case (((s1) << 2) | (s2)): *htiles = hn; *vtiles = vn; return;
    switch((sz << 2) | shape) {
        T(0, 0, 1, 1)
        T(0, 1, 2, 1)
        T(0, 2, 1, 2)
        T(1, 0, 2, 2)
        T(1, 1, 4, 1)
        T(1, 2, 1, 4)
        T(2, 0, 4, 4)
        T(2, 1, 4, 2)
        T(2, 2, 2, 4)
        T(3, 0, 8, 8)
        T(3, 1, 8, 4)
        T(3, 2, 4, 8)
    }
    static int warned = 0;
    if (!warned) {
        warned = 1;
        printf("\nHEY! BITMAP OBJ DETECT!");
    }
    *htiles = 1;
    *vtiles = 1;
#undef T
}

static void calculate_windows_vflags(struct NDS *this, struct NDSENG2D *eng)
{
    for (u32 wn = 0; wn < 2; wn++) {
        struct NDS_PPU_window *w = &eng->window[wn];
        u32 y = this->clock.ppu.y & 0xFF;
        if (y == w->top) w->v_flag = 1;
        if (y == w->bottom) w->v_flag = 0;
    }
}

static u32 get_sprite_tile_addr(struct NDS *this, struct NDSENG2D *eng, u32 tile_num, u32 htiles, u32 block_y, u32 line_in_tile, u32 bpp8, u32 d1)
{
    if (d1) {
        tile_num *= eng->io.obj.tile.stride_1d;
    }
    else {
        tile_num += (block_y * eng->io.obj.tile.stride_1d);
        /*if (eng->io.bitmap_obj_2d_dim) {
            tile_num += block_y << 5;
        }
        else { // this for pokemon
            //tile_num += block_y * 32;
        }*/
    }

    //if (bpp8) tile_num &= 0x3FE;
    //tile_num &= 0x3FF;

    // Now get pointer to that tile
    u32 tile_base_addr = tile_num;
    u32 tile_line_addr = tile_base_addr;// + (line_in_tile * 4);
    return tile_line_addr;
}

static void output_sprite_8bpp(struct NDS *this, struct NDSENG2D *eng, u32 tile_addr, u32 mode, i32 screen_x, u32 priority, u32 hflip, u32 mosaic, struct NDS_PPU_window *w) {
    for (i32 tile_x = 0; tile_x < 8; tile_x++) {
        i32 sx;
        if (hflip) sx = (7 - tile_x) + screen_x;
        else sx = tile_x + screen_x;
        if ((sx >= 0) && (sx < 256)) {
            eng->obj.drawing_cycles -= 1;
            struct NDS_PX *opx = &eng->obj.line[sx];
            if ((mode > 1) || (!opx->has) || (priority < opx->priority)) {
                u8 c = read_vram_obj(this, eng, tile_addr+tile_x, 1);
                switch (mode) {
                    case 1:
                    case 0:
                        if (c != 0) {
                            opx->has = 1;
                            opx->color = read_pram_obj(this, eng, (0x100 + c) << 1, 2);
                            opx->translucent_sprite = mode;
                        }
                        opx->priority = priority;
                        opx->mosaic_sprite = mosaic;
                        break;
                    case 2: { // OBJ window
                        if (c != 0) w->sprites_inside[sx] = 1;
                        break;
                    }
                }
            }
        }
        if (eng->obj.drawing_cycles < 1) return;
    }
}

static int obj_size[4][4][2] = {
        { { 8 , 8  }, { 16, 16 }, { 32, 32 }, { 64, 64 } },
        { { 16, 8  }, { 32, 8  }, { 32, 16 }, { 64, 32 } },
        { { 8 , 16 }, { 8 , 32 }, { 16, 32 }, { 32, 64 } },
        { { 8 , 8  }, { 8 , 8  }, { 8 , 8  }, { 8 , 8  } }
};

static inline void draw_obj_on_line(struct NDS *this, struct NDSENG2D *eng, u32 oam_offset)
{
    i32 cycles_per_pixel;
    i32 pa, pb, pc, pd;

    u16 attr[3];
    attr[0] = read_oam(this, eng, oam_offset, 2);
    u32 affine = (attr[0] >> 8) & 1;
    u32 b9 = (attr[0] >> 9) & 1;
    if (!affine && b9) {
        eng->obj.drawing_cycles--;
        return;
    }
    cycles_per_pixel = affine ? 2 : 1;
    eng->obj.drawing_cycles -= affine ? 10 : 1;
    if (eng->obj.drawing_cycles < 0) return;

    attr[1] = read_oam(this, eng, oam_offset+2, 2);

    i32 x = attr[1] & 0x1FF;
    i32 y = attr[0] & 0xFF;
    u32 shape = (attr[0] >> 14) & 3;
    u32 sz = (attr[1] >> 14) & 3;
    u32 priority = (attr[2] >> 10) & 3;
    u32 mode = (attr[0] >> 10) & 3;
    u32 mosaic = (attr[0] >> 12) & 1;

    if (x & 0x100) x -= 512;
    if (y >= 192) y -= 256;

    i32 width = obj_size[shape][sz][0];
    i32 height = obj_size[shape][sz][1];
    i32 half_width = width >> 1;
    i32 half_height = height >> 1;
    i32 real_half_width = half_width;
    i32 real_half_height = half_height;

    x += half_width;
    y += half_height;
    if (affine && b9) {
        x += half_height;
        y += half_height;
        half_width <<= 1;
        half_height <<= 1;
    }
    i32 my_y = this->clock.ppu.y;
    if (my_y < (y - half_height) || my_y >= (y + half_height)){
        return;
    }

    if (affine) {
        u32 pgroup = ((attr[1] >> 9) & 31) << 5;
        pa = read_oam(this, eng, pgroup + 6, 2);
        pb = read_oam(this, eng, pgroup + 14, 2);
        pc = read_oam(this, eng, pgroup + 22, 2);
        pd = read_oam(this, eng, pgroup + 30, 2);
        pa = SIGNe16to32(pa);
        pb = SIGNe16to32(pb);
        pc = SIGNe16to32(pc);
        pd = SIGNe16to32(pd);
    }
    else {
        pa = 1 << 8;
        pb = 0;
        pc = 0;
        pd = 1 << 8;
    }

    attr[2] = read_oam(this, eng, oam_offset+4, 2);
    i32 sprite_y = my_y - y;
    u32 tile_num = attr[2] & 0x3FF;
    u32 palette = (attr[2] >> 12) + 16;
    u32 flip_h = ((attr[1] >> 12) & 1) & (affine ^ 1);
    u32 flip_v = ((attr[1] >> 13) & 1) & (affine ^ 1);
    u32 bpp8 = (attr[0] >> 13) & 1;

    if (mosaic) {
        sprite_y = this->ppu.mosaic.obj.y_current - y;
    }

    for (i32 lx = -half_width; lx <= half_width; lx++) {
        eng->obj.drawing_cycles -= cycles_per_pixel;
        if (eng->obj.drawing_cycles < 1) return;
        i32 line_x = lx + x;
        if ((line_x < 0) || (line_x > 255)) continue;

        i32 tx = ((pa * lx + pb * sprite_y) >> 8) + real_half_width;
        i32 ty = ((pc * lx + pd * sprite_y) >> 8) + real_half_height;

        if ((tx < 0) || (ty < 0) || (tx >= width) || (ty >= height)) continue;

        if (flip_h) tx = width - tx - 1;
        if (flip_v) ty = height - ty - 1;
        u32 tile_x = tx & 7;
        u32 tile_y = ty & 7;
        u32 block_x = tx >> 3;
        u32 block_y = ty >> 3;

        u32 c, is_transparent;
        u32 tile_addr;
        if (mode == 3) { // bitmap
            if (eng->io.obj.bitmap.map_1d) {
                u32 addr = (tile_num * (64 << eng->io.obj.bitmap.boundary_1d) + ty * width + tx) << 1;
                c = read_vram_obj(this, eng, addr, 2);
            }
            else {
                u32 mask = (16 << eng->io.obj.bitmap.dim_2d) - 1;
                u32 addr = ((tile_num & ~mask) * 64 + (tile_num & mask) * 8 + ty * (128 << eng->io.obj.bitmap.dim_2d) + tx) << 1;
                c = read_vram_obj(this, eng, addr, 2);
            }
            is_transparent = (c >> 15) & 1;
            c &= 0x7FFF;
        }
        else if (bpp8) {
            if (eng->io.obj.tile.map_1d)
                tile_addr = (tile_num << eng->io.obj.tile.boundary_1d) + block_y * (width >> 2);
            else
                tile_addr = (tile_num & 0x3FE) + block_y * 32;

            tile_addr = ((tile_addr + (block_x << 1)) << 5) + (tile_y << 3) + tile_x;
            c = read_vram_obj(this, eng, tile_addr, 1);
            is_transparent = c == 0;
            c = read_pram_obj2(this, eng, 0, c);
        }
        else { // 4bpp
            if (eng->io.obj.tile.map_1d)
                tile_addr = (tile_num << eng->io.obj.tile.boundary_1d) + block_y * (width >> 3);
            else
                tile_addr = tile_num + block_y * 32;

            tile_addr = ((tile_addr + block_x) << 5) + ((tile_y << 2) | (tile_x >> 1));
            c = read_vram_obj(this, eng, tile_addr, 1);
            if (tile_x & 1) c >>= 4;
            c &= 15;
            is_transparent = c == 0;
            c = read_pram_obj2(this, eng, palette, c);
        }

        struct NDS_PX *opx = &eng->obj.line[line_x];
        if ((mode == 2) || (!opx->has) || (priority < opx->priority)) {
            switch (mode) {
                case 3: // bitmap
                case 1: // translucent iirc?
                case 0: {
                    if (!is_transparent) {
                        opx->has = 1;
                        opx->color = c;
                        opx->translucent_sprite = mode;
                    }
                    opx->priority = priority;
                    opx->mosaic_sprite = mosaic;
                    break;
                }
                case 2: {
                    if (!is_transparent) eng->window[NDS_WINOBJ].sprites_inside[line_x] = 1;
                    break;
                }
            }
        }
    }
}

static void draw_obj_line(struct NDS *this, struct NDSENG2D *eng)
{
    eng->obj.drawing_cycles = eng->io.hblank_free ? 1530 : 2124;

    memset(eng->obj.line, 0, sizeof(struct NDS_PX) * 256);
    struct NDS_PPU_window *w = &eng->window[NDS_WINOBJ];
    memset(&w->sprites_inside, 0, sizeof(w->sprites_inside));

    if (!eng->obj.enable) return;
    // (GBA) Each OBJ takes:
    // n*1 cycles per pixel
    // 10 + n*2 per pixel if rotated/scaled
    for (u32 oam_offset = 0; oam_offset <= (127 * 8); oam_offset += 8) {
        if (eng->obj.drawing_cycles < 0) {
            printf("\nNO DRAW CYCLES LEFT!");
            return;
        }
        draw_obj_on_line(this, eng, oam_offset);
    }
}

// Thanks tonc!
static u32 se_index_fast(u32 tx, u32 ty, u32 bgcnt) {
    u32 n = tx + ty * 32;
    if (tx >= 32)
        n += 0x03E0;
    if (ty >= 32 && (bgcnt == 3))
        n += 0x0400;
    return n;
}

static void fetch_bg_slice(struct NDS *this, struct NDSENG2D *eng, struct NDS_PPU_bg *bg, u32 bgnum, u32 block_x, u32 vpos, struct NDS_PX px[8], u32 screen_x) {
    u32 block_y = vpos >> 3;
    //  engine A screen base: BGxCNT.bits*2K + DISPCNT.bits*64K
    //  engine B screen base: BGxCNT.bits*2K + 0
    //  engine A char base: BGxCNT.bits*16K + DISPCNT.bits*64K
    //  engine B char base: BGxCNT.bits*16K + 0


    u32 screenblock_addr = bg->screen_base_block + eng->io.bg.screen_base + (se_index_fast(block_x, block_y, bg->screen_size) << 1);

    u16 attr = read_vram_bg(this, eng, screenblock_addr, 2);
    //if (eng->num == 0) printf("\nATTR read from %06x", screenblock_addr);
    u32 tile_num = attr & 0x3FF;
    u32 hflip = (attr >> 10) & 1;
    u32 vflip = (attr >> 11) & 1;
    u32 palbank = bg->bpp8 ? 0 : ((attr >> 12) & 15) << 4;

    u32 line_in_tile = vpos & 7;
    if (vflip) line_in_tile = 7 - line_in_tile;

    u32 tile_bytes = bg->bpp8 ? 64 : 32;
    u32 line_size = bg->bpp8 ? 8 : 4;
    u32 tile_start_addr = bg->character_base_block + eng->io.bg.character_base + (tile_num * tile_bytes);
    u32 line_addr = tile_start_addr + (line_in_tile * line_size);
    //if (line_addr >= 0x10000) return; // hardware doesn't draw from up there
    u32 addr = line_addr;

    if (bg->bpp8) {
        u32 mx = hflip ? 7 : 0;
        for (u32 ex = 0; ex < 8; ex++) {
            u8 data = read_vram_bg(this, eng, mx + addr, 1);
            //if ((eng->num == 0) && (data != 0)) printf("\nGOT SONE8!");
            struct NDS_PX *p = &px[ex];
            if ((screen_x + mx) < 256) {
                if (data != 0) {
                    p->has = 1;
                    p->color = read_pram_bg(this, eng, data << 1, 2);
                    p->priority = bg->priority;
                } else
                    p->has = 0;
            }
            if (hflip) mx--;
            else mx++;
        }
    } else {
        u32 mx = 0;
        if (hflip) mx = 7;
        for (u32 ex = 0; ex < 4; ex++) {
            u8 data = read_vram_bg(this, eng, addr, 1);
            //if (eng->num == 0) printf("\nBG READ FROM %06x", addr);
            //if ((eng->num == 0) && (data != 0)) printf("\nGOT SONE4!");
            addr++;
            for (u32 i = 0; i < 2; i++) {
                u16 c = data & 15;
                data >>= 4;
                struct NDS_PX *p = &px[mx];
                if ((screen_x + mx) < 256) {
                    if (c != 0) {
                        p->has = 1;
                        p->color = read_pram_bg(this, eng, (c + palbank) << 1, 2);
                        p->priority = bg->priority;
                    } else
                        p->has = 0;
                }
                if (hflip) mx--;
                else mx++;
            }
        }
    }
}

static void draw_3d_line(struct NDS *this, struct NDSENG2D *eng, u32 bgnum)
{
    u32 line_num = this->clock.ppu.y;
    if (line_num > 191) return;
    // Copy line over
    struct NDS_PPU_bg *bg = &eng->bg[bgnum];
    memset(bg->line, 0, sizeof(bg->line));
    struct NDS_RE_LINEBUFFER *re_line = &this->re.out.linebuffer[line_num];
    for (u32 x = 0; x < 256; x++) {
        bg->line[x].color = re_line->rgb_top[x];
        bg->line[x].has = 1;
        bg->line[x].priority = bg->priority;
    }
}

static void draw_bg_line_extended(struct NDS *this, struct NDSENG2D *eng, u32 bgnum)
{
    struct NDS_PPU_bg *bg = &eng->bg[bgnum];
    memset(bg->line, 0, sizeof(bg->line));
    if (!bg->enable) { return; }

    u32 bpp;
    if (!bg->bpp8) bpp = 4;
    else {
        if ((bg->character_base_block >> 14) & 1) {
            bpp = 16;
        }
        else {
            bpp = 8;
        }
    }

    static int a = 1;
    if (a) {
        printf("\nWARN IMPLEMENT EXTENDED BG...");
        a = 0;
    }
}

static void draw_bg_line_normal(struct NDS *this, struct NDSENG2D *eng, u32 bgnum)
{
    struct NDS_PPU_bg *bg = &eng->bg[bgnum];
    memset(bg->line, 0, sizeof(bg->line));
    if (!bg->enable) {
        return;
    }
    // first do a fetch for fine scroll -1
    u32 hpos = bg->hscroll & bg->hpixels_mask;
    u32 vpos = (bg->vscroll + bg->mosaic_y) & bg->vpixels_mask;
    u32 fine_x = hpos & 7;
    u32 screen_x = 0;
    struct NDS_PX bgpx[8];
    //hpos = ((hpos >> 3) - 1) << 3;
    fetch_bg_slice(this, eng, bg, bgnum, hpos >> 3, vpos, bgpx, 0);
    //struct NDS_DBG_line *dbgl = &this->dbg_info.line[this->clock.ppu.y];
    //u8 *scroll_line = &this->dbg_info.bg_scrolls[bgnum].lines[((bg->vscroll + this->clock.ppu.y) & bg->vpixels_mask) * 128];
    u32 startx = fine_x;
    for (u32 i = startx; i < 8; i++) {
        bg->line[screen_x].color = bgpx[i].color;
        bg->line[screen_x].has = bgpx[i].has;
        bg->line[screen_x].priority = bgpx[i].priority;
        screen_x++;
        hpos = (hpos + 1) & bg->hpixels_mask;
        //scroll_line[hpos >> 3] |= 1 << (hpos & 7);
    }

    while (screen_x < 256) {
        fetch_bg_slice(this, eng, bg, bgnum, hpos >> 3, vpos, &bg->line[screen_x], screen_x);
/*        if (screen_x <= 248) {
            scroll_line[hpos >> 3] = 0xFF;
        }
        else {
            for (u32 rx = screen_x; rx < 256; rx++) {
                scroll_line[hpos >> 3] |= 1 << (rx - screen_x);
            }
        }*/
        screen_x += 8;
        hpos = (hpos + 8) & bg->hpixels_mask;
    }
}

static void affine_line_start(struct NDS *this, struct NDSENG2D *eng, struct NDS_PPU_bg *bg, i32 *fx, i32 *fy)
{
    if (!bg->enable) {
        if (bg->mosaic_y != bg->last_y_rendered) {
            bg->x_lerp += bg->pb * eng->mosaic.bg.vsize;
            bg->y_lerp += bg->pd * eng->mosaic.bg.vsize;
            bg->last_y_rendered = bg->mosaic_y;
        }
        return;
    }
    *fx = bg->x_lerp;
    *fy = bg->y_lerp;
}

static void affine_line_end(struct NDS *this, struct NDSENG2D *eng, struct NDS_PPU_bg *bg)
{
    if (bg->mosaic_y != bg->last_y_rendered) {
        bg->x_lerp += bg->pb * eng->mosaic.bg.vsize;
        bg->y_lerp += bg->pd * eng->mosaic.bg.vsize;
        bg->last_y_rendered = bg->mosaic_y;
    }
}

static void get_affine_bg_pixel(struct NDS *this, struct NDSENG2D *eng, u32 bgnum, struct NDS_PPU_bg *bg, i32 px, i32 py, struct NDS_PX *opx)
{
    // Now px and py represent a number inside 0...hpixels and 0...vpixels
    u32 block_x = px >> 3;
    u32 block_y = py >> 3;
    u32 line_in_tile = py & 7;

    u32 tile_width = bg->htiles;

    u32 screenblock_addr = bg->screen_base_block + eng->io.bg.screen_base;
    screenblock_addr += block_x + (block_y * tile_width);
    u32 tile_num = read_vram_bg(this, eng, screenblock_addr, 1);

    // so now, grab that tile...
    u32 tile_start_addr = bg->character_base_block + eng->io.bg.character_base + (tile_num * 64);
    u32 line_start_addr = tile_start_addr + (line_in_tile * 8);
    u32 addr = line_start_addr;
    u8 color = read_vram_bg(this, eng, line_start_addr + (px & 7), 1);
    if (color != 0) {
        opx->has = 1;
        opx->color = read_pram_bg(this, eng, color << 1, 2);
        opx->priority = bg->priority;
    }
    else {
        opx->has = 0;
    }
}

static void draw_bg_line_affine(struct NDS *this, struct NDSENG2D *eng, u32 bgnum)
{
    struct NDS_PPU_bg *bg = &eng->bg[bgnum];

    i32 fx, fy;
    affine_line_start(this, eng, bg, &fx, &fy);
    memset(bg->line, 0, sizeof(bg->line));
    if (!bg->enable) {
        return;
    }

    //struct NDS_DBG_tilemap_line_bg *dtl = &this->dbg_info.bg_scrolls[bgnum];

    for (i64 screen_x = 0; screen_x < 256; screen_x++) {
        i32 px = fx >> 8;
        i32 py = fy >> 8;
        fx += bg->pa;
        fy += bg->pc;
        if (bg->display_overflow) { // wraparound
            px &= bg->hpixels_mask;
            py &= bg->vpixels_mask;
        }
        else { // clip/transparent
            if (px < 0) continue;
            if (py < 0) continue;
            if (px >= bg->hpixels) continue;
            if (py >= bg->vpixels) continue;
        }
        get_affine_bg_pixel(this, eng, bgnum, bg, px, py, &bg->line[screen_x]);
        //dtl->lines[(py << 7) + (px >> 3)] |= 1 << (px & 7);
    }
    affine_line_end(this, eng, bg);
}

static void apply_mosaic(struct NDS *this, struct NDSENG2D *eng)
{
    // This function updates vertical mosaics, and applies horizontal.
    struct NDS_PPU_bg *bg;
    if (this->ppu.mosaic.bg.y_counter == 0) {
        this->ppu.mosaic.bg.y_current = this->clock.ppu.y;
    }
    for (u32 i = 0; i < 4; i++) {
        bg = &eng->bg[i];
        if (!bg->enable) continue;
        if (bg->mosaic_enable) bg->mosaic_y = this->ppu.mosaic.bg.y_current;
        else bg->mosaic_y = this->clock.ppu.y + 1;
    }
    this->ppu.mosaic.bg.y_counter = (this->ppu.mosaic.bg.y_counter + 1) % eng->mosaic.bg.vsize;

    if (this->ppu.mosaic.obj.y_counter == 0) {
        this->ppu.mosaic.obj.y_current = this->clock.ppu.y;
    }
    this->ppu.mosaic.obj.y_counter = (this->ppu.mosaic.obj.y_counter + 1) % eng->mosaic.obj.vsize;


    // Now do horizontal blend
    for (u32 i = 0; i < 4; i++) {
        bg = &eng->bg[i];
        if (!bg->enable || !bg->mosaic_enable) continue;
        u32 mosaic_counter = 0;
        struct NDS_PX *src;
        for (u32 x = 0; x < 256; x++) {
            if (mosaic_counter == 0) {
                src = &bg->line[x];
            }
            else {
                bg->line[x].has = src->has;
                bg->line[x].color = src->color;
                bg->line[x].priority = src->priority;
            }
            mosaic_counter = (mosaic_counter + 1) % eng->mosaic.bg.hsize;
        }
    }

    // Now do sprites, which is a bit different
    struct NDS_PX *src = &eng->obj.line[0];
    u32 mosaic_counter = 0;
    for (u32 x = 0; x < 256; x++) {
        struct NDS_PX *current = &eng->obj.line[x];
        if (!current->mosaic_sprite || !src->mosaic_sprite || (mosaic_counter == 0))  {
            src = current;
        }
        else {
            current->has = src->has;
            current->color = src->color;
            current->priority = src->priority;
            current->translucent_sprite = src->translucent_sprite;
            current->mosaic_sprite = src->mosaic_sprite;
        }
        mosaic_counter = (mosaic_counter + 1) % eng->mosaic.obj.hsize;
    }
}

#define NDSCTIVE_SFX 5

static inline struct NDS_PPU_window *get_active_window(struct NDS *this, struct NDSENG2D *eng, u32 x)
{
    struct NDS_PPU_window *active_window = NULL;
    if (eng->window[NDS_WIN0].enable || eng->window[NDS_WIN1].enable || eng->window[NDS_WINOBJ].enable) {
        active_window = &eng->window[NDS_WINOUTSIDE];
        if (eng->window[NDS_WINOBJ].enable && eng->window[NDS_WINOBJ].is_inside) active_window = &eng->window[NDS_WINOBJ];
        if (eng->window[NDS_WIN1].enable && eng->window[NDS_WIN1].is_inside) active_window = &eng->window[NDS_WIN1];
        if (eng->window[NDS_WIN0].enable && eng->window[NDS_WIN0].is_inside) active_window = &eng->window[NDS_WIN0];
    }
    return active_window;
}

static void find_targets_and_priorities(u32 bg_enables[6], struct NDS_PX *layers[6], u32 *layer_a_out, u32 *layer_b_out, i32 x, u32 *actives)
{
    u32 laout = 5;
    u32 lbout = 5;

    // Get highest priority 1st target
    for (i32 priority = 3; priority >= 0; priority--) {
        for (i32 layer_num = 4; layer_num >= 0; layer_num--) {
            if (actives[layer_num] && bg_enables[layer_num] && layers[layer_num]->has && (layers[layer_num]->priority == priority)) {
                lbout = laout;
                laout = layer_num;
            }
        }
    }
    *layer_a_out = laout;
    *layer_b_out = lbout;
}

static inline void calculate_windows_h(struct NDS *this, struct NDSENG2D *eng, u32 x)
{
    struct NDS_PPU_window *w0 = &eng->window[NDS_WIN0];
    struct NDS_PPU_window *w1 = &eng->window[NDS_WIN1];
    struct NDS_PPU_window *ws = &eng->window[NDS_WINOBJ];
    struct NDS_PPU_window *wo = &eng->window[NDS_WINOUTSIDE];

    for (u32 i = 0; i < 2; i++) {
        struct NDS_PPU_window *w = &eng->window[i];
        w->h_flag |= x == w->left;
        w->h_flag &= (x == w->right) ^ 1;
        w->is_inside = w->h_flag & w->v_flag;
    }

    ws->is_inside = ws->sprites_inside[x];
    //wo->is_inside = (w0->is_inside | w1->is_inside | ws->is_inside) ^ 1;
}

static void output_pixel(struct NDS *this, struct NDSENG2D *eng, u32 x, u32 obj_enable, u32 bg_enables[4]) {
    // Find which window applies to us.
    u32 default_active[6] = {1, 1, 1, 1, 1, 1}; // Default to active if no window.

    u32 *actives = default_active;

    // Calculate windows....we do this at each pixel...
    calculate_windows_h(this, eng, x);

    struct NDS_PPU_window *active_window = get_active_window(this, eng, x);
    if (active_window) actives = active_window->active;

    struct NDS_PX *sp_px = &eng->obj.line[x];
    struct NDS_PX empty_px = {.color=read_pram_bg(this, eng, 0, 2), .priority=4, .translucent_sprite=0, .has=1};
    sp_px->has &= obj_enable;

    struct NDS_PX *layers[6] = {
            sp_px,
            &eng->bg[0].line[x],
            &eng->bg[1].line[x],
            &eng->bg[2].line[x],
            &eng->bg[3].line[x],
            &empty_px
    };
    u32 obg_enables[6] = {
            obj_enable,
            bg_enables[0],
            bg_enables[1],
            bg_enables[2],
            bg_enables[3],
            1
    };
    u32 mode = eng->blend.mode;

    /* Find targets A and B */
    u32 target_a_layer, target_b_layer;
    find_targets_and_priorities(obg_enables, layers, &target_a_layer, &target_b_layer, x, actives);

    // Blending ONLY happens if the topmost pixel is a valid A target and the next-topmost is a valid B target
    // Brighten/darken only happen if topmost pixel is a valid A target

    struct NDS_PX *target_a = layers[target_a_layer];
    struct NDS_PX *target_b = layers[target_b_layer];

    u32 blend_b = target_b->color;

    u32 output_color = target_a->color;
    if (actives[NDSCTIVE_SFX] || (target_a->has && target_a->translucent_sprite &&
            eng->blend.targets_b[target_b_layer])) { // backdrop is contained in active for the highest-priority window OR above is a semi-transparent sprite & below is a valid target
        if (target_a->has && target_a->translucent_sprite &&
                eng->blend.targets_b[target_b_layer]) { // above is semi-transparent sprite and below is a valid target
            output_color = gba_alpha(output_color, blend_b, eng->blend.use_eva_a, eng->blend.use_eva_b);
        } else if (mode == 1 && eng->blend.targets_a[target_a_layer] &&
                   eng->blend.targets_b[target_b_layer]) { // mode == 1, both are valid
            output_color = gba_alpha(output_color, blend_b, eng->blend.use_eva_a, eng->blend.use_eva_b);
        } else if (mode == 2 && eng->blend.targets_a[target_a_layer]) { // mode = 2, A is valid
            output_color = gba_brighten(output_color, (i32) eng->blend.use_bldy);
        } else if (mode == 3 && eng->blend.targets_a[target_a_layer]) { // mode = 3, B is valid
            output_color = gba_darken(output_color, (i32) eng->blend.use_bldy);
        }
    }

    eng->line_px[x] = output_color;
}

static void draw_line0(struct NDS *this, struct NDSENG2D *eng, struct NDS_DBG_line *l)
{
    draw_obj_line(this, eng);
    if ((eng->num == 0) && (eng->io.bg.do_3d)) {
        draw_3d_line(this, eng, 0);
    }
    else {
        draw_bg_line_normal(this, eng, 0);
    }
    draw_bg_line_normal(this, eng, 1);
    draw_bg_line_normal(this, eng, 2);
    draw_bg_line_normal(this, eng, 3);
    apply_mosaic(this, eng);
    calculate_windows_vflags(this, eng);
    u32 bg_enables[4] = {eng->bg[0].enable, eng->bg[1].enable, eng->bg[2].enable, eng->bg[3].enable};
    for (u32 x = 0; x < 256; x++) {
        output_pixel(this, eng, x, eng->obj.enable, &bg_enables[0]);
    }
}

static void draw_line1(struct NDS *this, struct NDSENG2D *eng, struct NDS_DBG_line *l)
{
    draw_obj_line(this, eng);
    if ((eng->num == 0) && (eng->io.bg.do_3d)) {
        draw_3d_line(this, eng, 0);
    }
    else {
        draw_bg_line_normal(this, eng, 0);
    }
    draw_bg_line_normal(this, eng, 1);
    draw_bg_line_affine(this, eng, 2);
    apply_mosaic(this, eng);
    memset(eng->bg[3].line, 0, sizeof(eng->bg[3].line));

    calculate_windows_vflags(this, eng);
    u32 bg_enables[4] = {eng->bg[0].enable, eng->bg[1].enable, eng->bg[2].enable, 0};
    //memset(eng->line_px, 0x50, sizeof(eng->line_px));
    for (u32 x = 0; x < 256; x++) {
        output_pixel(this, eng, x, eng->obj.enable, &bg_enables[0]);
    }
}

static void draw_line3(struct NDS *this, struct NDSENG2D *eng, struct NDS_DBG_line *l)
{
    draw_obj_line(this, eng);
    if ((eng->num == 0) && (eng->io.bg.do_3d)) {
        draw_3d_line(this, eng, 0);
    }
    else {
        draw_bg_line_normal(this, eng, 0);
    }
    draw_bg_line_normal(this, eng, 1);
    draw_bg_line_affine(this, eng, 2);
    draw_bg_line_extended(this, eng, 3);
    apply_mosaic(this, eng);

    calculate_windows_vflags(this, eng);
    u32 bg_enables[4] = {eng->bg[0].enable, eng->bg[1].enable, eng->bg[2].enable, 0};
    //memset(eng->line_px, 0x50, sizeof(eng->line_px));
    for (u32 x = 0; x < 256; x++) {
        output_pixel(this, eng, x, eng->obj.enable, &bg_enables[0]);
    }
}

static void draw_line5(struct NDS *this, struct NDSENG2D *eng, struct NDS_DBG_line *l)
{
    draw_obj_line(this, eng);
    if ((eng->num == 0) && (eng->io.bg.do_3d)) {
        draw_3d_line(this, eng, 0);
    }
    else {
        draw_bg_line_normal(this, eng, 0);
    }
    draw_bg_line_normal(this, eng, 1);
    draw_bg_line_extended(this, eng, 2);
    draw_bg_line_extended(this, eng, 3);
    apply_mosaic(this, eng);

    calculate_windows_vflags(this, eng);
    u32 bg_enables[4] = {eng->bg[0].enable, eng->bg[1].enable, eng->bg[2].enable, 0};
    //memset(eng->line_px, 0x50, sizeof(eng->line_px));
    for (u32 x = 0; x < 256; x++) {
        output_pixel(this, eng, x, eng->obj.enable, &bg_enables[0]);
    }

}

static void draw_line(struct NDS *this, u32 eng_num)
{
    struct NDSENG2D *eng = &this->ppu.eng2d[eng_num];
    struct NDS_DBG_line *l = &this->dbg_info.eng[eng_num].line[this->clock.ppu.y];
    l->bg_mode = eng->io.bg_mode;
    // We have 2-4 display modes. They can be: WHITE, NORMAL, VRAM display, and "main memory display"
    // During this time, the 2d engine runs like normal!
    // So we will draw our lines...
    //if (eng->num == 1 && this->clock.ppu.y == 100) { printf("\nline:100 bg_mode:%d what:%d bg0:%d bg1:%d bg2:%d bg3:%d", eng->io.bg_mode, eng->io.bg_mode, eng->bg[0].enable, eng->bg[1].enable, eng->bg[2].enable, eng->bg[3].enable); }
    switch(eng->io.bg_mode) {
        case 0:
            draw_line0(this, eng, l);
            break;
        case 1:
            draw_line1(this, eng, l);
            break;
        case 3:
            draw_line3(this, eng, l);
            break;
        case 5:
            draw_line5(this, eng, l);
            break;
        default: {
            static int warned = 0;
            if (warned != eng->io.bg_mode) {
                printf("\nWARNING implement mode %d eng%d", eng->io.bg_mode, eng->num);
                warned = eng->io.bg_mode;
            }
        }
    }

    for (u32 ppun = 0; ppun < 2; ppun++) {
        for (u32 i = 0; i < 4; i++) {
            memcpy(this->dbg_info.eng[ppun].line[this->clock.ppu.y].bg[i].buf, this->ppu.eng2d[ppun].bg[i].line, sizeof(struct NDS_PX)*256);
        }
        memcpy(this->dbg_info.eng[ppun].line[this->clock.ppu.y].sprite_buf, this->ppu.eng2d[ppun].obj.line, sizeof(struct NDS_PX)*256);
    }
    // Then we will pixel output it to the screen...
    u16 *line_output = this->ppu.cur_output + (this->clock.ppu.y * OUT_WIDTH);
    if (eng_num ^ this->ppu.io.display_swap ^ 1) line_output += (192 * OUT_WIDTH);

    u32 display_mode = eng->io.display_mode;
    if (eng_num == 1) display_mode &= 1;
    switch(display_mode) {
        case 0: // WHITE!
            memset(line_output, 0xFF, 2*OUT_WIDTH);
            break;
        case 1: // NORMAL!
            memcpy(line_output, eng->line_px, 2*OUT_WIDTH);
            break;
        case 2: { // VRAM!
            u32 block_num = this->ppu.io.display_block << 17;
            u16 *line_input = (u16 *)(this->mem.vram.data + block_num);
            line_input += this->clock.ppu.y << 8; // * 256
            memcpy(line_output,  line_input, 2*OUT_WIDTH);
            break; }
        case 3: { //Main RAM
            u16 *line_input = (u16 *)this->mem.RAM;
            line_input += this->clock.ppu.y << 8; // * 256
            memcpy(line_output,  line_input, 2*OUT_WIDTH);
            break; }
    }
}

static void new_frame(struct NDS *this);

void NDS_PPU_hblank(void *ptr, u64 key, u64 clock, u32 jitter) // Called at hblank time
{
    struct NDS *this = (struct NDS *)ptr;
    this->clock.ppu.hblank_active = key;
    if (key == 0) { // end hblank...new line!
        this->clock.ppu.scanline_start = clock - jitter;

        this->clock.ppu.y++;
        //printf("\nhblank %lld: line %d  cyc:%lld", key, this->clock.ppu.y, this->clock.master_cycle_count7);

        if (this->clock.ppu.y >= 263) {
            new_frame(this);
        }

        if (this->dbg.events.view.vec) {
            debugger_report_line(this->dbg.interface, this->clock.ppu.y);
        }

        if ((this->ppu.io.vcount_at7 == this->clock.ppu.y) && this->ppu.io.vcount_irq_enable7) NDS_update_IF7(this, NDS_IRQ_VMATCH);
        if ((this->ppu.io.vcount_at9 == this->clock.ppu.y) && this->ppu.io.vcount_irq_enable9) NDS_update_IF9(this, NDS_IRQ_VMATCH);
    }
    else {
        // Start of hblank. Now draw line!
        if (this->clock.ppu.y < 192) {
            draw_line(this, 0);
            draw_line(this, 1);
        } else {
            calculate_windows_vflags(this, &this->ppu.eng2d[0]);
            calculate_windows_vflags(this, &this->ppu.eng2d[1]);
        }
        if (this->ppu.io.hblank_irq_enable7) NDS_update_IF7(this, NDS_IRQ_HBLANK);
        if (this->ppu.io.hblank_irq_enable9) NDS_update_IF9(this, NDS_IRQ_HBLANK);
        NDS_check_dma9_at_hblank(this);
    }
}

#define MASTER_CYCLES_PER_FRAME 570716
static void new_frame(struct NDS *this) {
    //printf("\n--new frame");
    //debugger_report_frame(this->dbg.interface);
    this->clock.ppu.y = 0;
    this->clock.frame_start_cycle = NDS_clock_current7(this);
    this->ppu.cur_output = ((u16 *) this->ppu.display->output[this->ppu.display->last_written ^ 1]);
    this->ppu.display->last_written ^= 1;
    this->clock.master_frame++;
    struct NDS_PPU_bg *bg;

    for (u32 pn = 0; pn < 2; pn++) {
        struct NDSENG2D *p = &this->ppu.eng2d[pn];
        for (u32 i = 0; i < 4; i++) {
            bg = &p->bg[i];
            bg->mosaic_y = 0;
            bg->last_y_rendered = 191;
        }
        this->ppu.mosaic.bg.y_counter = 0;
        this->ppu.mosaic.bg.y_current = 0;
        this->ppu.mosaic.obj.y_counter = 0;
        this->ppu.mosaic.obj.y_current = 0;

        /*for (u32 bgnum = 0; bgnum < 4; bgnum++) {
            for (u32 line = 0; line < 1024; line++) {
                memset(&this->dbg_info.bg_scrolls[bgnum].lines[0], 0, 1024 * 128);
            }
        }*/
    }

    NDS_trigger_dma9_if(this, NDS_DMA_START_OF_DISPLAY);
}


void NDS_PPU_vblank(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct NDS *this = (struct NDS *)ptr;
    //printf("\nvblank %lld: line %d cyc:%lld", key, this->clock.ppu.y, NDS_clock_current7(this));

    this->clock.ppu.vblank_active = key;
    if (key) {
        if (this->ppu.io.vblank_irq_enable7) NDS_update_IF7(this, NDS_IRQ_VBLANK);
        if (this->ppu.io.vblank_irq_enable9) NDS_update_IF9(this, NDS_IRQ_VBLANK);
        NDS_check_dma7_at_vblank(this);
        NDS_check_dma9_at_vblank(this);
        NDS_GE_vblank_up(this);
    }
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

static void calc_screen_sizeA(struct NDS *this, struct NDSENG2D *eng, u32 num) {
    struct NDS_PPU_bg *bg = &eng->bg[num];
    if ((num == 0) && bg->do3d) {
        bg->hpixels = 256;
        bg->vpixels = 192;
        return;
    }

    static const u32 sk[8][4] = {
            {1, 1, 1, 1}, // text  text  text  text
            {1, 1, 1, 2}, // text  text  text  affine
            {1, 1, 2, 2}, // text text affine affine
            {1, 1, 1, 3}, // text text text extend
            {1, 1, 2, 3}, // text text affine extend
            {1, 1, 3, 3}, // text text extend extend
            {1, 0, 4, 0},  // 3d empty big empty
            {0, 0, 0, 0},  // empty empty empty empty
    };
    u32 mk = sk[eng->io.bg_mode][num];
    if (mk == SK_extended) { // "extend" so look at other bits...
        switch(bg->bpp8) {
            case 0:
                mk = SK_rotscale_16bit; // rot/scale with 16bit bgmap entries
                break;
            case 1: {
                u32 b = (bg->character_base_block >> 14) & 1;
                if (!b) {
                    mk = SK_rotscale_8bit;
                }
                else {
                    mk = SK_rotscale_direct;
                }
                break; }
        }
    }
    if ((num == 0) && (mk == 1)) {
        if (bg->do3d) mk = SK_3donly;
    }

    bg->kind = mk;
    if (mk == 0) return;

/*
screen base however NOT used at all for Large screen bitmap mode
  bgcnt size  text     rotscal    bitmap   large bmp
  0           256x256  128x128    128x128  512x1024
  1           512x256  256x256    256x256  1024x512
  2           256x512  512x512    512x256  -
  3           512x512  1024x1024  512x512  - */
    if (mk == SK_text) {
        switch(bg->screen_size) {
            case 0: bg->htiles = 32; bg->vtiles = 32; break;
            case 1: bg->htiles = 64; bg->vtiles = 32; break;
            case 2: bg->htiles = 32; bg->vtiles = 64; break;
            case 3: bg->htiles = 64; bg->vtiles = 64; break;
        }
        bg->hpixels = bg->htiles * 8;
        bg->vpixels = bg->vtiles * 8;
    }
    else if (mk == SK_affine) {
        switch(bg->screen_size) {
            case 0: bg->htiles = 16; bg->vtiles = 16; break;
            case 1: bg->htiles = 32; bg->vtiles = 32; break;
            case 2: bg->htiles = 64; bg->vtiles = 64; break;
            case 3: bg->htiles = 128; bg->vtiles = 128; break;
        }
        bg->hpixels = bg->htiles * 8;
        bg->vpixels = bg->vtiles * 8;
    }
    else if ((mk == SK_rotscale_8bit) || (mk == SK_rotscale_16bit) || (mk == SK_rotscale_direct)) {
        switch(bg->screen_size) {
            case 0: bg->hpixels=128; bg->vpixels=128; break;
            case 1: bg->hpixels=256; bg->vpixels=256; break;
            case 2: bg->hpixels=512; bg->vpixels=256; break;
            case 3: bg->hpixels=512; bg->vpixels=512; break;
        }
    }

    bg->htiles_mask = bg->htiles - 1;
    bg->vtiles_mask = bg->vtiles - 1;
    bg->hpixels_mask = bg->hpixels - 1;
    bg->vpixels_mask = bg->vpixels - 1;
}

static void calc_screen_sizeB(struct NDS *this, struct NDSENG2D *eng, u32 num)
{
    struct NDS_PPU_bg *bg = &eng->bg[num];
    u32 mode = eng->io.bg_mode;
    static const u32 sk[8][4] = {
            { SK_text, SK_text, SK_text, SK_text },
            { SK_text, SK_text, SK_affine, SK_none },
            { SK_none, SK_none, SK_affine, SK_affine},
            { SK_none, SK_none, SK_gba_mode_3, SK_none },
            { SK_none, SK_none, SK_gba_mode_4, SK_none },
            { SK_none, SK_none, SK_gba_mode_5, SK_none },
            { SK_none, SK_none, SK_none, SK_none },
            { SK_none, SK_none, SK_none, SK_none },
    };
    bg->kind = sk[mode][num];
    if (mode >= 3) return;
    // mode0 they're all the same no scaling
    u32 scales = mode >= 1 ? (num > 1) :  0;
    // mode1 bg 0-1 no scaling, 2 scaling
    // mode2 is 2 and 3
#define T(scales, ssize, hrs, vrs) case (((scales) << 2) | (ssize)): bg->htiles = hrs; bg->vtiles = vrs; break
    switch((scales << 2) | bg->screen_size) {
        T(0, 0, 32, 32);
        T(1, 0, 16, 16);
        T(0, 1, 64, 32);
        T(1, 1, 32, 32);
        T(0, 2, 32, 64);
        T(1, 2, 64, 64);
        T(0, 3, 64, 64);
        T(1, 3, 128, 128);
    }
#undef T
    bg->htiles_mask = bg->htiles - 1;
    bg->vtiles_mask = bg->vtiles - 1;
    bg->hpixels = bg->htiles << 3;
    bg->vpixels = bg->vtiles << 3;
    bg->hpixels_mask = bg->hpixels - 1;
    bg->vpixels_mask = bg->vpixels - 1;
}

static void calc_screen_size(struct NDS *this, struct NDSENG2D *eng, u32 num)
{
    if (eng->num == 0) calc_screen_sizeA(this, eng, num);
    else calc_screen_sizeB(this, eng, num);
}

static void update_bg_x(struct NDS *this, struct NDSENG2D *eng, u32 bgnum, u32 which, u32 val)
{
    i32 v = eng->bg[bgnum].x & 0x0FFFFFFF;
    val &= 0xFF;
    switch(which) {
        case 0: v = (v & 0x0FFFFF00) | val; break;
        case 1: v = (v & 0x0FFF00FF) | (val << 8); break;
        case 2: v = (v & 0x0F00FFFF) | (val << 16); break;
        case 3: v = (v & 0x00FFFFFF) | (val << 24); break;
    }
    if ((v >> 27) & 1) v |= 0xF0000000;
    eng->bg[bgnum].x = v;
    eng->bg[bgnum].x_written = 1;
}

static void update_bg_y(struct NDS *this, struct NDSENG2D *eng, u32 bgnum, u32 which, u32 val)
{
    u32 v = eng->bg[bgnum].y & 0x0FFFFFFF;
    val &= 0xFF;
    switch(which) {
        case 0: v = (v & 0x0FFFFF00) | val; break;
        case 1: v = (v & 0x0FFF00FF) | (val << 8); break;
        case 2: v = (v & 0x0F00FFFF) | (val << 16); break;
        case 3: v = (v & 0x00FFFFFF) | (val << 24); break;
    }
    if ((v >> 27) & 1) v |= 0xF0000000;
    eng->bg[bgnum].y = v;
    eng->bg[bgnum].y_written = 1;
}

static const u32 boundary_to_stride[4] = {32, 64, 128, 256};
static const u32 boundary_to_stride_bitmap[2] = { 32, 64}; // ??

void NDS_PPU_write7_io8(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val) {
    u32 en = 0;
    if (addr >= 0x04001000) {
        addr -= 0x1000;
        en = 1;
    }
    struct NDSENG2D *eng = &this->ppu.eng2d[en];
    switch(addr) {
        case R_DISPSTAT+0:
            this->ppu.io.vblank_irq_enable7 = (val >> 3) & 1;
            this->ppu.io.hblank_irq_enable7 = (val >> 4) & 1;
            this->ppu.io.vcount_irq_enable7 = (val >> 5) & 1;
            this->ppu.io.vcount_at7 = (this->ppu.io.vcount_at7 & 0xFF) | ((val & 0x80) << 1);
            return;
        case R_DISPSTAT+1:
            this->ppu.io.vcount_at7 = (this->ppu.io.vcount_at7 & 0x100) | val;
            return;
    }
    printf("\nUNKNOWN PPU WR7 ADDR:%08x sz:%d VAL:%08x", addr, sz, val);
}

void NDS_PPU_write9_io8(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    u32 en = 0;
    if (addr >= 0x04001000) {
        addr -= 0x1000;
        en = 1;
    }
    struct NDSENG2D *eng = &this->ppu.eng2d[en];
    switch(addr) {
        case R9_DISP3DCNT+0:
            this->re.io.DISP3DCNT.u = (this->re.io.DISP3DCNT.u & 0xFF00) | (val & 0xFF);
            return;
        case R9_DISP3DCNT+1:
            // bits 5 and 6 are acks
            if (val & 0x10) { // ack rdlines underflow
                this->re.io.DISP3DCNT.rdlines_underflow = 0;
            }
            if (val & 0x20) {
                this->re.io.DISP3DCNT.poly_vtx_ram_overflow = 0;
            }
            this->re.io.DISP3DCNT.u = (this->re.io.DISP3DCNT.u & 0xFF) | ((val & 0b01001111) << 8);
            return;

        case R_VCOUNT+0: // read-only register...
        case R_VCOUNT+1:
            return;
        case R9_DISP3DCNT+2:
        case R9_DISP3DCNT+3: return;

        case R9_DISPCAPCNT+0:
        case R9_DISPCAPCNT+1:
        case R9_DISPCAPCNT+2:
        case R9_DISPCAPCNT+3: {
            static int doit = 0;
            if (!doit) {
                printf("\nwarning DISPCAPCNT not implement!");
                doit = 1;
            }
            return; }
        case R9_DISP_MMEM_FIFO+0:
        case R9_DISP_MMEM_FIFO+1:
        case R9_DISP_MMEM_FIFO+2:
        case R9_DISP_MMEM_FIFO+3: {
            static int doit = 0;
            if (!doit) {
                printf("\nwarning DISP_MMEM_FIFO not implement!");
                doit = 1;
            }
            return; }
        case R9_MASTER_BRIGHT_A+0:
        case R9_MASTER_BRIGHT_A+1:
        case R9_MASTER_BRIGHT_A+2:
        case R9_MASTER_BRIGHT_A+3: {
            static int doit = 0;
            if (!doit) {
                printf("\nwarning MASTER_BRIGHT_A not implement!");
                doit = 1;
            }
            return; }
        case R_DISPSTAT+0:
            this->ppu.io.vblank_irq_enable9 = (val >> 3) & 1;
            this->ppu.io.hblank_irq_enable9 = (val >> 4) & 1;
            this->ppu.io.vcount_irq_enable9 = (val >> 5) & 1;
            this->ppu.io.vcount_at9 = (this->ppu.io.vcount_at9 & 0xFF) | ((val & 0x80) << 1);
            return;
        case R_DISPSTAT+1:
            this->ppu.io.vcount_at9 = (this->ppu.io.vcount_at9 & 0x100) | val;
            return;
        case R9_DISPCNT+0:
            if ((val & 7) != eng->io.bg_mode) {
                printf("\neng%d NEW BG MODE: %d", en, val & 7);
                dbgloglog(NDS_CAT_PPU_BG_MODE, DBGLS_INFO, "eng%d new BG mode:%d", en, eng->io.bg_mode);
            }
            eng->io.bg_mode = val & 7;
            if (en == 0) {
                eng->io.bg.do_3d = (val >> 3) & 1;
                eng->bg[0].do3d = eng->io.bg.do_3d;
            }
            eng->io.obj.tile.map_1d = (val >> 4) & 1;
            eng->io.obj.bitmap.dim_2d = (val >> 5) & 1;
            eng->io.obj.bitmap.map_1d = (val >> 6) & 1;
            eng->io.force_blank = (val >> 7) & 1;
            calc_screen_size(this, eng, 0);
            calc_screen_size(this, eng, 1);
            calc_screen_size(this, eng, 2);
            calc_screen_size(this, eng, 3);
            return;
        case R9_DISPCNT+1:
            eng->bg[0].enable = (val >> 0) & 1;
            eng->bg[1].enable = (val >> 1) & 1;
            eng->bg[2].enable = (val >> 2) & 1;
            eng->bg[3].enable = (val >> 3) & 1;
            eng->obj.enable = (val >> 4) & 1;
            eng->window[0].enable = (val >> 5) & 1;
            eng->window[1].enable = (val >> 6) & 1;
            eng->window[NDS_WINOBJ].enable = (val >> 7) & 1;
            return;
        case R9_DISPCNT+2:
            eng->io.display_mode = val & 3;
            if ((en == 1) && (eng->io.display_mode > 1)) {
                printf("\nWARNING eng1 BAD DISPLAY MODE: %d", eng->io.display_mode);
            }
            eng->io.obj.tile.boundary_1d = (val >> 4) & 3;
            eng->io.obj.tile.stride_1d = boundary_to_stride[eng->io.obj.tile.boundary_1d];
            eng->io.hblank_free = (val >> 7) & 1;
            if (en == 0) {
                this->ppu.io.display_block = (val >> 2) & 3;
                eng->io.obj.bitmap.boundary_1d = (val >> 6) & 1;
                eng->io.obj.bitmap.stride_1d = boundary_to_stride_bitmap[eng->io.obj.bitmap.boundary_1d];
            }
            return;
        case R9_DISPCNT+3:
            if (en == 0) {
                eng->io.bg.character_base = (val & 7) << 16;
                eng->io.bg.screen_base = ((val >> 3) & 7) << 16;
            }
            eng->io.bg.extended_palettes = (val >> 6) & 1;
            eng->io.obj.extended_palettes = (val >> 7) & 1;
            return;
        case R9_BG0CNT+0:
        case R9_BG1CNT+0:
        case R9_BG2CNT+0:
        case R9_BG3CNT+0: { // BGCNT lo
            u32 bgnum = (addr & 0b0110) >> 1;
            struct NDS_PPU_bg *bg = &eng->bg[bgnum];
            bg->priority = val & 3;
            bg->extrabits = (val >> 4) & 3;
            bg->character_base_block = ((val >> 2) & 3) << 14;
            bg->mosaic_enable = (val >> 6) & 1;
            bg->bpp8 = (val >> 7) & 1;
            if (eng->num == 0) calc_screen_size(this, eng, bgnum);
            return; }
        case R9_BG0CNT+1:
        case R9_BG1CNT+1:
        case R9_BG2CNT+1:
        case R9_BG3CNT+1: { // BGCNT lo
            u32 bgnum = (addr & 0b0110) >> 1;
            struct NDS_PPU_bg *bg = &eng->bg[bgnum];
/*
  engine A screen base: BGxCNT.bits*2K + DISPCNT.bits*64K
  engine B screen base: BGxCNT.bits*2K + 0
  engine A char base: BGxCNT.bits*16K + DISPCNT.bits*64K
  engine B char base: BGxCNT.bits*16K + 0
 *
 */
            bg->screen_base_block = ((val >> 0) & 31) << 11;
            if (bgnum >= 2) bg->display_overflow = (val >> 5) & 1;
            bg->screen_size = (val >> 6) & 3;
            calc_screen_size(this, eng, bgnum);
            return; }

#define SET16L(thing, to) thing = (thing & 0xFF00) | to
#define SET16H(thing, to) thing = (thing & 0xFF) | (to << 8)

        case R9_BG0HOFS+0: SET16L(eng->bg[0].hscroll, val); return;
        case R9_BG0HOFS+1: SET16H(eng->bg[0].hscroll, val); return;
        case R9_BG0VOFS+0: SET16L(eng->bg[0].vscroll, val); return;
        case R9_BG0VOFS+1: SET16H(eng->bg[0].vscroll, val); return;
        case R9_BG1HOFS+0: SET16L(eng->bg[1].hscroll, val); return;
        case R9_BG1HOFS+1: SET16H(eng->bg[1].hscroll, val); return;
        case R9_BG1VOFS+0: SET16L(eng->bg[1].vscroll, val); return;
        case R9_BG1VOFS+1: SET16H(eng->bg[1].vscroll, val); return;
        case R9_BG2HOFS+0: SET16L(eng->bg[2].hscroll, val); return;
        case R9_BG2HOFS+1: SET16H(eng->bg[2].hscroll, val); return;
        case R9_BG2VOFS+0: SET16L(eng->bg[2].vscroll, val); return;
        case R9_BG2VOFS+1: SET16H(eng->bg[2].vscroll, val); return;
        case R9_BG3HOFS+0: SET16L(eng->bg[3].hscroll, val); return;
        case R9_BG3HOFS+1: SET16H(eng->bg[3].hscroll, val); return;
        case R9_BG3VOFS+0: SET16L(eng->bg[3].vscroll, val); return;
        case R9_BG3VOFS+1: SET16H(eng->bg[3].vscroll, val); return;

#define BG2 2
#define BG3 3
        case R9_BG2PA+0: eng->bg[2].pa = (eng->bg[2].pa & 0xFFFFFF00) | val; return;
        case R9_BG2PA+1: eng->bg[2].pa = (eng->bg[2].pa & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case R9_BG2PB+0: eng->bg[2].pb = (eng->bg[2].pb & 0xFFFFFF00) | val; return;
        case R9_BG2PB+1: eng->bg[2].pb = (eng->bg[2].pb & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case R9_BG2PC+0: eng->bg[2].pc = (eng->bg[2].pc & 0xFFFFFF00) | val; return;
        case R9_BG2PC+1: eng->bg[2].pc = (eng->bg[2].pc & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case R9_BG2PD+0: eng->bg[2].pd = (eng->bg[2].pd & 0xFFFFFF00) | val; return;
        case R9_BG2PD+1: eng->bg[2].pd = (eng->bg[2].pd & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case R9_BG2X+0: update_bg_x(this, eng, BG2, 0, val); return;
        case R9_BG2X+1: update_bg_x(this, eng, BG2, 1, val); return;
        case R9_BG2X+2: update_bg_x(this, eng, BG2, 2, val); return;
        case R9_BG2X+3: update_bg_x(this, eng, BG2, 3, val); return;
        case R9_BG2Y+0: update_bg_y(this, eng, BG2, 0, val); return;
        case R9_BG2Y+1: update_bg_y(this, eng, BG2, 1, val); return;
        case R9_BG2Y+2: update_bg_y(this, eng, BG2, 2, val); return;
        case R9_BG2Y+3: update_bg_y(this, eng, BG2, 3, val); return;

        case R9_BG3PA+0: eng->bg[3].pa = (eng->bg[3].pa & 0xFFFFFF00) | val; return;
        case R9_BG3PA+1: eng->bg[3].pa = (eng->bg[3].pa & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case R9_BG3PB+0: eng->bg[3].pb = (eng->bg[3].pb & 0xFFFFFF00) | val; return;
        case R9_BG3PB+1: eng->bg[3].pb = (eng->bg[3].pb & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case R9_BG3PC+0: eng->bg[3].pc = (eng->bg[3].pc & 0xFFFFFF00) | val; return;
        case R9_BG3PC+1: eng->bg[3].pc = (eng->bg[3].pc & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case R9_BG3PD+0: eng->bg[3].pd = (eng->bg[3].pd & 0xFFFFFF00) | val; return;
        case R9_BG3PD+1: eng->bg[3].pd = (eng->bg[3].pd & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case R9_BG3X+0: update_bg_x(this, eng, BG3, 0, val); return;
        case R9_BG3X+1: update_bg_x(this, eng, BG3, 1, val); return;
        case R9_BG3X+2: update_bg_x(this, eng, BG3, 2, val); return;
        case R9_BG3X+3: update_bg_x(this, eng, BG3, 3, val); return;
        case R9_BG3Y+0: update_bg_y(this, eng, BG3, 0, val); return;
        case R9_BG3Y+1: update_bg_y(this, eng, BG3, 1, val); return;
        case R9_BG3Y+2: update_bg_y(this, eng, BG3, 2, val); return;
        case R9_BG3Y+3: update_bg_y(this, eng, BG3, 3, val); return;

        case R9_WIN0H:   eng->window[0].right = val; return;
        case R9_WIN0H+1: eng->window[0].left = val; return;
        case R9_WIN1H:   eng->window[1].right = val; return;
        case R9_WIN1H+1: eng->window[1].left = val; return;
        case R9_WIN0V:   eng->window[0].bottom = val; return;
        case R9_WIN0V+1: eng->window[0].top = val; return;
        case R9_WIN1V:   eng->window[1].bottom = val; return;
        case R9_WIN1V+1: eng->window[1].top = val; return;

        case R9_WININ:
            eng->window[0].active[1] = (val >> 0) & 1;
            eng->window[0].active[2] = (val >> 1) & 1;
            eng->window[0].active[3] = (val >> 2) & 1;
            eng->window[0].active[4] = (val >> 3) & 1;
            eng->window[0].active[0] = (val >> 4) & 1;
            eng->window[0].active[5] = (val >> 5) & 1;
            return;
        case R9_WININ+1:
            eng->window[1].active[1] = (val >> 0) & 1;
            eng->window[1].active[2] = (val >> 1) & 1;
            eng->window[1].active[3] = (val >> 2) & 1;
            eng->window[1].active[4] = (val >> 3) & 1;
            eng->window[1].active[0] = (val >> 4) & 1;
            eng->window[1].active[5] = (val >> 5) & 1;
            return;
        case R9_WINOUT:
            eng->window[NDS_WINOUTSIDE].active[1] = (val >> 0) & 1;
            eng->window[NDS_WINOUTSIDE].active[2] = (val >> 1) & 1;
            eng->window[NDS_WINOUTSIDE].active[3] = (val >> 2) & 1;
            eng->window[NDS_WINOUTSIDE].active[4] = (val >> 3) & 1;
            eng->window[NDS_WINOUTSIDE].active[0] = (val >> 4) & 1;
            eng->window[NDS_WINOUTSIDE].active[5] = (val >> 5) & 1;
            return;
        case R9_WINOUT+1:
            eng->window[NDS_WINOBJ].active[1] = (val >> 0) & 1;
            eng->window[NDS_WINOBJ].active[2] = (val >> 1) & 1;
            eng->window[NDS_WINOBJ].active[3] = (val >> 2) & 1;
            eng->window[NDS_WINOBJ].active[4] = (val >> 3) & 1;
            eng->window[NDS_WINOBJ].active[0] = (val >> 4) & 1;
            eng->window[NDS_WINOBJ].active[5] = (val >> 5) & 1;
            return;
        case R9_MOSAIC+0:
            eng->mosaic.bg.hsize = (val & 15) + 1;
            eng->mosaic.bg.vsize = ((val >> 4) & 15) + 1;
            return;
        case R9_MOSAIC+1:
            eng->mosaic.obj.hsize = (val & 15) + 1;
            eng->mosaic.obj.vsize = ((val >> 4) & 15) + 1;
            return;
        case R9_BLDCNT:
            eng->blend.mode = (val >> 6) & 3;
            // sp, bg0, bg1, bg2, bg3, bd
            eng->blend.targets_a[0] = (val >> 4) & 1;
            eng->blend.targets_a[1] = (val >> 0) & 1;
            eng->blend.targets_a[2] = (val >> 1) & 1;
            eng->blend.targets_a[3] = (val >> 2) & 1;
            eng->blend.targets_a[4] = (val >> 3) & 1;
            eng->blend.targets_a[5] = (val >> 5) & 1;
            return;
        case R9_BLDCNT+1:
            eng->blend.targets_b[0] = (val >> 4) & 1;
            eng->blend.targets_b[1] = (val >> 0) & 1;
            eng->blend.targets_b[2] = (val >> 1) & 1;
            eng->blend.targets_b[3] = (val >> 2) & 1;
            eng->blend.targets_b[4] = (val >> 3) & 1;
            eng->blend.targets_b[5] = (val >> 5) & 1;
            return;

        case R9_BLDALPHA:
            eng->blend.eva_a = val & 31;
            eng->blend.use_eva_a = eng->blend.eva_a;
            if (eng->blend.use_eva_a > 16) eng->blend.use_eva_a = 16;
            return;
        case R9_BLDALPHA+1:
            eng->blend.eva_b = val & 31;
            eng->blend.use_eva_b = eng->blend.eva_b;
            if (eng->blend.use_eva_b > 16) eng->blend.use_eva_b = 16;
            return;
        case R9_BLDY+0:
            eng->blend.bldy = val & 31;
            eng->blend.use_bldy = eng->blend.bldy;
            if (eng->blend.use_bldy > 16) eng->blend.use_bldy = 16;
            return;
        case R9_BLDY+1:
            return;
        case 0x0400004e: // not used
        case 0x0400004f: // not used
        case 0x04000056: // not used
        case 0x04000057: // not used
        case 0x04000058: // not used
        case 0x04000059: // not used
        case 0x0400005A: // not used
        case 0x0400005B: // not used
        case 0x0400005C: // not used
        case 0x0400005D: // not used
        case 0x0400005E: // not used
        case 0x0400005F: // not used
            return;
    }
    printf("\nUNKNOWN PPU WR9 ADDR:%08x sz:%d VAL:%08x", addr, sz, val);
}

void NDS_PPU_write9_io(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    NDS_PPU_write9_io8(this, addr, sz, access, val & 0xFF);
    if (sz >= 2) NDS_PPU_write9_io8(this, addr+1, sz, access, (val >> 8) & 0xFF);
    if (sz == 4) {
        NDS_PPU_write9_io8(this, addr+2, sz, access, (val >> 16) & 0xFF);
        NDS_PPU_write9_io8(this, addr+3, sz, access, (val >> 24) & 0xFF);
    }
}

u32 NDS_PPU_read9_io8(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    u32 en = 0;
    if (addr >= 0x04001000) {
        addr -= 0x1000;
        en = 1;
    }
    struct NDSENG2D *eng = &this->ppu.eng2d[en];

    u32 v;

    switch(addr) {
        case R9_DISP3DCNT+0:
            return this->re.io.DISP3DCNT.u & 0xFF;
        case R9_DISP3DCNT+1:
            return (this->re.io.DISP3DCNT.u >> 8) & 0xFF;

        case R9_DISPCNT+0:
            v = eng->io.bg_mode;
            v |= eng->bg[0].do3d << 3;
            v |= eng->io.obj.tile.map_1d << 4;
            v |= eng->io.obj.bitmap.dim_2d << 5;
            v |= eng->io.obj.bitmap.map_1d << 6;
            v |= eng->io.force_blank << 7;
            return v;
        case R9_DISPCNT+1:
            v = eng->bg[0].enable;
            v |= eng->bg[1].enable << 1;
            v |= eng->bg[2].enable << 2;
            v |= eng->bg[3].enable << 3;
            v |= eng->obj.enable << 4;
            v |= eng->window[0].enable << 5;
            v |= eng->window[1].enable << 6;
            v |= eng->window[NDS_WINOBJ].enable << 7;
            return v;
        case R9_DISPCNT+2:
            v = eng->io.display_mode;
            v |= eng->io.obj.tile.boundary_1d << 4;
            v |= eng->io.hblank_free << 7;
            if (en == 0) {
                v |= this->ppu.io.display_block << 2;
                v |= eng->io.obj.bitmap.boundary_1d << 6;
            }
            return v;
        case R9_DISPCNT+3:
            v = eng->io.bg.extended_palettes << 6;
            v |= eng->io.obj.extended_palettes << 7;
            if (en == 0) {
                v |= eng->io.bg.character_base >> 16;
                v |= eng->io.bg.screen_base >> 13;
            }
            return v;
        case R_DISPSTAT+0: // DISPSTAT lo
            v = this->clock.ppu.vblank_active;
            v |= this->clock.ppu.hblank_active << 1;
            v |= (this->clock.ppu.y == this->ppu.io.vcount_at9) << 2;
            v |= this->ppu.io.vblank_irq_enable9 << 3;
            v |= this->ppu.io.hblank_irq_enable9 << 4;
            v |= this->ppu.io.vcount_irq_enable9 << 5;
            v |= (this->ppu.io.vcount_at9 & 0x100) >> 1; // hi bit of vcount_at
            return v;
        case R_DISPSTAT+1: // DISPSTAT hi
            v = this->ppu.io.vcount_at9 & 0xFF;
            return v;
        case R_VCOUNT+0: // VCOunt lo
            return this->clock.ppu.y & 0xFF;
        case R_VCOUNT+1:
            return this->clock.ppu.y >> 8;

        case R9_BG0CNT+0:
        case R9_BG1CNT+0:
        case R9_BG2CNT+0:
        case R9_BG3CNT+0: {
            u32 bgnum = (addr & 0b0110) >> 1;
            struct NDS_PPU_bg *bg = &eng->bg[bgnum];
            v = bg->priority;
            v |= bg->extrabits << 4;
            v |= (bg->character_base_block >> 12);
            v |= bg->mosaic_enable << 6;
            v |= bg->bpp8 << 7;
            return v; }

        case R9_BG0CNT+1:
        case R9_BG1CNT+1:
        case R9_BG2CNT+1:
        case R9_BG3CNT+1: {
            u32 bgnum = (addr & 0b0110) >> 1;
            struct NDS_PPU_bg *bg = &eng->bg[bgnum];
            v = bg->screen_base_block >> 11;
            v |= bg->display_overflow << 5;
            v |= bg->screen_size << 6;
            return v; }

        case R9_WININ+0:
            v = eng->window[0].active[1] << 0;
            v |= eng->window[0].active[2] << 1;
            v |= eng->window[0].active[3] << 2;
            v |= eng->window[0].active[4] << 3;
            v |= eng->window[0].active[0] << 4;
            v |= eng->window[0].active[5] << 5;
            return v;
        case R9_WININ+1:
            v = eng->window[1].active[1] << 0;
            v |= eng->window[1].active[2] << 1;
            v |= eng->window[1].active[3] << 2;
            v |= eng->window[1].active[4] << 3;
            v |= eng->window[1].active[0] << 4;
            v |= eng->window[1].active[5] << 5;
            return v;
        case R9_WINOUT+0:
            v = eng->window[NDS_WINOUTSIDE].active[1] << 0;
            v |= eng->window[NDS_WINOUTSIDE].active[2] << 1;
            v |= eng->window[NDS_WINOUTSIDE].active[3] << 2;
            v |= eng->window[NDS_WINOUTSIDE].active[4] << 3;
            v |= eng->window[NDS_WINOUTSIDE].active[0] << 4;
            v |= eng->window[NDS_WINOUTSIDE].active[5] << 5;
            return v;
        case R9_WINOUT+1:
            v = eng->window[NDS_WINOBJ].active[1] << 0;
            v |= eng->window[NDS_WINOBJ].active[2] << 1;
            v |= eng->window[NDS_WINOBJ].active[3] << 2;
            v |= eng->window[NDS_WINOBJ].active[4] << 3;
            v |= eng->window[NDS_WINOBJ].active[0] << 4;
            v |= eng->window[NDS_WINOBJ].active[5] << 5;
            return v;
        case R9_BLDCNT+0:
            v = eng->blend.targets_a[0] << 4;
            v |= eng->blend.targets_a[1] << 0;
            v |= eng->blend.targets_a[2] << 1;
            v |= eng->blend.targets_a[3] << 2;
            v |= eng->blend.targets_a[4] << 3;
            v |= eng->blend.targets_a[5] << 5;
            v |= eng->blend.mode << 6;
            return v;
        case R9_BLDCNT+1:
            v = eng->blend.targets_b[0] << 4;
            v |= eng->blend.targets_b[1] << 0;
            v |= eng->blend.targets_b[2] << 1;
            v |= eng->blend.targets_b[3] << 2;
            v |= eng->blend.targets_b[4] << 3;
            v |= eng->blend.targets_b[5] << 5;
            return v;
        case R9_BLDALPHA+0:
            return eng->blend.eva_a;
        case R9_BLDALPHA+1:
            return eng->blend.eva_b;

    }

    printf("\nUNKNOWN PPU RD9 ADDR:%08x sz:%d", addr, sz);
    return 0;
}

u32 NDS_PPU_read7_io8(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    u32 v;
   switch(addr) {
       case R_DISPSTAT+0: // DISPSTAT lo
           v = this->clock.ppu.vblank_active;
           v |= this->clock.ppu.hblank_active << 1;
           v |= (this->clock.ppu.y == this->ppu.io.vcount_at7) << 2;
           v |= this->ppu.io.vblank_irq_enable7 << 3;
           v |= this->ppu.io.hblank_irq_enable7 << 4;
           v |= this->ppu.io.vcount_irq_enable7 << 5;
           v |= (this->ppu.io.vcount_at7 & 0x100) >> 1; // hi bit of vcount_at
           return v;
       case R_DISPSTAT+1: // DISPSTAT hi
           v = this->ppu.io.vcount_at7 & 0xFF;
           return v;
       case R_VCOUNT+0: // VCOunt lo
           return this->clock.ppu.y & 0xFF;
       case R_VCOUNT+1:
           return this->clock.ppu.y >> 8;
   }
   printf("\nUnknown PPURD7IO8 addr:%08x", addr);
   return 0;
}

u32 NDS_PPU_read7_io(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    u32 v = NDS_PPU_read7_io8(this, addr, sz, access, has_effect);
    if (sz >= 2) v |= NDS_PPU_read7_io8(this, addr+1, sz, access, has_effect) << 8;
    if (sz == 4) {
        v |= NDS_PPU_read7_io8(this, addr+2, sz, access, has_effect) << 16;
        v |= NDS_PPU_read7_io8(this, addr+3, sz, access, has_effect) << 24;
    }
    return v;
}

u32 NDS_PPU_read9_io(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    u32 v = NDS_PPU_read9_io8(this, addr, sz, access, has_effect);
    if (sz >= 2) v |= NDS_PPU_read9_io8(this, addr+1, sz, access, has_effect) << 8;
    if (sz == 4) {
        v |= NDS_PPU_read9_io8(this, addr+2, sz, access, has_effect) << 16;
        v |= NDS_PPU_read9_io8(this, addr+3, sz, access, has_effect) << 24;
    }
    return v;
}

void NDS_PPU_write7_io(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    NDS_PPU_write7_io8(this, addr, sz, access, val & 0xFF);
    if (sz >= 2) NDS_PPU_write7_io8(this, addr+1, sz, access, (val >> 8) & 0xFF);
    if (sz == 4) {
        NDS_PPU_write7_io8(this, addr+2, sz, access, (val >> 16) & 0xFF);
        NDS_PPU_write7_io8(this, addr+3, sz, access, (val >> 24) & 0xFF);
    }
}
