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

static inline u32 c15to18(u32 color)
{
    u32 r = ((color << 1) & 0x3E);
    u32 g = ((color >> 4) & 0x3E);
    u32 b = ((color >> 9) & 0x3E);
    /*r |= (r >> 5);
    g |= (g >> 5);
    b |= (b >> 5);*/
    return r | (g << 6) | (b << 12);
}


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
        for (u32 bgnum = 0; bgnum < 4; bgnum++) {
            p->bg[bgnum].num = bgnum;
        }
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

static u64 read_vram_bg(struct NDS *this, struct NDSENG2D *eng, u32 addr, u32 sz)
{
    assert((addr >> 14) < 32);
    u8 *ptr = eng->memp.bg_vram[(addr >> 14) & 31];
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
    u8 *ptr = eng->memp.obj_vram[(addr >> 14) & 15];
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

static u32 read_pram_bg_ext(struct NDS *this, struct NDSENG2D *eng, u32 bgnum, u32 slotnum, u32 addr)
{
    if (!eng->memp.bg_extended_palette[slotnum & 3]) {
        static int a = 1;
        if (a) {
            printf("\nMISSED VRAM PAL READ!");
            a = 0;
        }
        return 0b1111110000011111; // glaring purple
    }
    addr &= 0x1FFF;
    return cR16(eng->memp.bg_extended_palette[slotnum & 3], addr);
}

static u32 read_pram_bg(struct NDS *this, struct NDSENG2D *eng, u32 addr, u32 sz)
{
    return cR[sz](eng->mem.bg_palette, addr & 0x1FF);
}

static u32 read_ext_palette_obj(struct NDS *this, struct NDSENG2D *eng, u32 palette, u32 index)
{
    u32 addr = 0x600 + ((palette << 5) | (index << 1)) + (0x200 * eng->num);
    addr &= 0x1FFF;
    if (eng->memp.obj_extended_palette) {
         u32 v = cR16(eng->memp.obj_extended_palette, addr);
         return v;
    }
    return 0;
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

static int obj_size[4][4][2] = {
        { { 8 , 8  }, { 16, 16 }, { 32, 32 }, { 64, 64 } },
        { { 16, 8  }, { 32, 8  }, { 32, 16 }, { 64, 32 } },
        { { 8 , 16 }, { 8 , 32 }, { 16, 32 }, { 32, 64 } },
        { { 8 , 8  }, { 8 , 8  }, { 8 , 8  }, { 8 , 8  } }
};

static inline void draw_obj_on_line(struct NDS *this, struct NDSENG2D *eng, u32 oam_offset, u32 prio_to_draw)
{
    i32 cycles_per_pixel;
    i32 pa, pb, pc, pd;

    u16 attr[3];

    attr[2] = read_oam(this, eng, oam_offset+4, 2);
    u32 priority = (attr[2] >> 10) & 3;
    if (priority != prio_to_draw) return;

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


    i32 y = attr[0] & 0xFF;
    u32 shape = (attr[0] >> 14) & 3;
    u32 mode = (attr[0] >> 10) & 3;
    u32 mosaic = (attr[0] >> 12) & 1;

    if (y >= 192) y -= 256;

    attr[1] = read_oam(this, eng, oam_offset+2, 2);
    i32 x = attr[1] & 0x1FF;
    u32 sz = (attr[1] >> 14) & 3;
    if (x & 0x100) x -= 512;

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

    i32 sprite_y = my_y - y;
    u32 tile_num = attr[2] & 0x3FF;
    u32 palette = (attr[2] >> 12);
    u32 flip_h = ((attr[1] >> 12) & 1) & (affine ^ 1);
    u32 flip_v = ((attr[1] >> 13) & 1) & (affine ^ 1);
    u32 bpp8 = (attr[0] >> 13) & 1;

    if (mosaic) {
        sprite_y = this->ppu.mosaic.obj.y_current - y;
    }

    u32 dbpp = bpp8;
    if (mode == 3) dbpp = 2;

    for (i32 lx = -half_width; lx <= half_width; lx++) {
        union NDS_PX mypx = {.has=1, .priority=priority, .sp_mosaic=mosaic, .dbg_mode=mode, .dbg_bpp = dbpp};
        eng->obj.drawing_cycles -= cycles_per_pixel;
        if (eng->obj.drawing_cycles < 1) return;
        i32 line_x = lx + x;
        if ((line_x < 0) || (line_x > 255)) continue;

        i32 tx = ((pa * lx + pb * sprite_y) >> 8) + real_half_width;
        i32 ty = ((pc * lx + pd * sprite_y) >> 8) + real_half_height;

        if ((tx < 0) || (ty < 0) || (tx >= width) || (ty >= height)) continue;

        if (flip_h) tx = width - tx - 1;
        if (flip_v) ty = height - ty - 1;
        u32 block_x = tx >> 3;
        u32 block_y = ty >> 3;
        u32 tile_x = tx & 7;
        u32 tile_y = ty & 7;

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
            mypx.has = (c >> 15) & 1;
            mypx.color = c15to18(c & 0x7FFF);
            mypx.alpha = palette;
            mypx.sp_translucent = palette < 15;
        }
        else if (bpp8) {
            if (eng->io.obj.tile.map_1d)
                tile_addr = (tile_num << eng->io.obj.tile.boundary_1d) + block_y * (width >> 2);
            else
                tile_addr = (tile_num & 0x3FE) + block_y * 32;

            tile_addr = ((tile_addr + (block_x << 1)) << 5) + (tile_y << 3) + tile_x;
            c = read_vram_obj(this, eng, tile_addr, 1);
            mypx.has = c != 0;
            mypx.color = c15to18(read_pram_obj2(this, eng, 0, c));
            mypx.sp_translucent = mode == 1;
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
            mypx.has = c != 0;

            c = read_pram_obj(this, eng, ((palette << 4) | c) << 1, 2);

            mypx.color = c15to18(c);
            mypx.sp_translucent = mode == 1;
        }
        union NDS_PX *opx = &eng->obj.line[line_x];

        if (mypx.has) {
            if (mode == 2) eng->window[NDS_WINOBJ].inside[line_x] = 1;
            else opx->u = mypx.u;
        }
        else {
            if (!opx->has)
                opx->u = mypx.u;
        }
    }
}

static void draw_obj_line(struct NDS *this, struct NDSENG2D *eng)
{
    //eng->obj.drawing_cycles = eng->io.hblank_free ? 1530 : 2124;
    eng->obj.drawing_cycles = 10000;

    memset(eng->obj.line, 0, sizeof(eng->obj.line));
    struct NDS_PPU_window *w = &eng->window[NDS_WINOBJ];
    memset(&w->inside, 0, sizeof(w->inside));

    if (!eng->obj.enable) return;
    // (GBA) Each OBJ takes:
    // n*1 cycles per pixel
    // 10 + n*2 per pixel if rotated/scaled
    for (i32 prio = 3; prio >= 0; prio--) {
        for (i32 i = 127; i >= 0; i--) {
            if (eng->obj.drawing_cycles < 0) {
                //printf("\nNO DRAW CYCLES LEFT!");
                return;
            }
            draw_obj_on_line(this, eng, i * 8, prio);
        }
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

static inline u32 C18to15(u32 c)
{
    u32 b = (c >> 13) & 0x1F;
    u32 g = (c >> 7) & 0x1F;
    u32 r = (c >> 1) & 0x1F;
    return r | (g << 5) | (b << 10);
    //return (((c >> 13) & 0x1F) << 10) | (((c >> 7) & 0x1F) << 5) | ((c >> 1) & 0x1F);
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
        bg->line[x].color = re_line->rgb[x];
        bg->line[x].has = re_line->has[x];
        bg->line[x].priority = bg->priority;
        bg->line[x].alpha = re_line->alpha[x];
    }
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4146) // unary minus operator applied to unsigned type, result still unsigned
#endif

static void draw_bg_line_normal(struct NDS *this, struct NDSENG2D *eng, u32 bgnum)
{
    struct NDS_PPU_bg *bg = &eng->bg[bgnum];
    memset(bg->line, 0, sizeof(bg->line));
    if (!bg->enable) {
        return;
    }
    u32 expal_slot = bgnum | (bg->ext_pal_slot << 1);

    i32 line = bg->mosaic_enable ? bg->mosaic_y : this->clock.ppu.y;
    line += bg->vscroll;

    i32 draw_x = -(bg->hscroll % 8);
    i32 grid_x = bg->hscroll / 8;
    i32 grid_y = line >> 3;
    i32 tile_y = line & 7;

    i32 screen_x = (grid_x >> 5) & 1;
    i32 screen_y = (grid_y >> 5) & 1;

    u32 base = eng->io.bg.screen_base + bg->screen_base_block + (grid_y % 32) * 64;
    union NDS_PX *buffer = bg->line;
    i32 last_encoder = -1;
    u16 encoder;

    grid_x &= 31;

    static const i32 sxm[4] = { 0, 2048, 0, 2048};
    static const i32 sym[4] = { 0, 0, 2048, 4096};
    u32 base_adjust = sxm[bg->screen_size];
    base += (screen_x * sxm[bg->screen_size]) + (screen_y * sym[bg->screen_size]);

    union NDS_PX tile[8] = {};
    if (screen_x == 1) base_adjust *= -1;
    u32 tile_base = bg->character_base_block + eng->io.bg.character_base;
    do {
        do {
            encoder = read_vram_bg(this, eng, base + grid_x++ * 2, 2);
            if (encoder != last_encoder) {
                int number = encoder & 0x3FF;
                int palette = encoder >> 12;
                i32 flip_x = (encoder >> 10) & 1;
                i32 flip_y = (encoder >> 11) & 1;
                i32 _tile_y = tile_y ^ (7 * flip_y);
                i32 xxor = 7 * flip_x;

                if (bg->bpp8) { // 8bpp
                    u64 data = read_vram_bg(this, eng, tile_base + (number << 6 | _tile_y << 3), 8);
                    for (u32 ix = 0; ix < 8; ix++) {
                        u32 mx = ix ^ xxor;
                        u32 index = data & 0xFF;
                        data >>= 8;

                        if (index == 0) {
                            tile[mx].has = 0;
                        }
                        else {
                            u32 c;

                            if (eng->io.bg.extended_palettes)
                                c = read_pram_bg_ext(this, eng, expal_slot, bgnum, palette << 9 | index << 1);
                            else
                                c = read_pram_bg(this, eng, index << 1, 2);
                            tile[mx].has = 1;
                            tile[mx].priority = bg->priority;
                            tile[mx].color = c15to18(c);
                        }
                    }
                }
                else { // 4bpp
                    u32 data = read_vram_bg(this, eng, tile_base + (number << 5 | _tile_y << 2), 4);
                    for (u32 ix = 0; ix < 8; ix++) {
                        u32 mx = ix ^ xxor;
                        u32 index = data & 15;
                        data >>= 4;
                        if (index == 0) {
                            tile[mx].has = 0;
                        }
                        else {
                            tile[mx].has = 1;
                            tile[mx].priority = bg->priority;
                            u32 c = read_pram_bg(this, eng, palette << 5 | index << 1, 2);
                            tile[mx].color = c15to18(c);
                        }
                    }
                }

                last_encoder = encoder;
            }

            if (draw_x >= 0 && draw_x <= 248) {
                memcpy(&buffer[draw_x], &tile[0], sizeof(union NDS_PX) * 8);
                draw_x += 8;
            }
            else {
                int x = 0;
                int max = 8;

                if (draw_x < 0) {
                    x = -draw_x;
                    draw_x = 0;
                    for (; x < max; x++) {
                        memcpy(&buffer[draw_x++], &tile[x], sizeof(union NDS_PX));
                    }
                } else {
                    max -= draw_x - 248;
                    for (; x < max; x++) {
                        memcpy(&buffer[draw_x++], &tile[x], sizeof(union NDS_PX));
                    }
                    break;
                }
            }
        } while(grid_x< 32);

        base += base_adjust;
        base_adjust *= -1;
        grid_x = 0;

    } while(draw_x < 256);
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

static void affine_line_start(struct NDS *this, struct NDSENG2D *eng, struct NDS_PPU_bg *bg)
{
    if (!bg->enable) {
        if (bg->mosaic_y != bg->last_y_rendered) {
            bg->x_lerp += bg->pb * eng->mosaic.bg.vsize;
            bg->y_lerp += bg->pd * eng->mosaic.bg.vsize;
            bg->last_y_rendered = bg->mosaic_y;
        }
    }
}

static void affine_line_end(struct NDS *this, struct NDSENG2D *eng, struct NDS_PPU_bg *bg)
{
    if (bg->mosaic_y != bg->last_y_rendered) {
        bg->x_lerp += bg->pb * eng->mosaic.bg.vsize;
        bg->y_lerp += bg->pd * eng->mosaic.bg.vsize;
        bg->last_y_rendered = bg->mosaic_y;
    }
}

typedef void (*affinerenderfunc)(struct NDS *, struct NDSENG2D *, struct NDS_PPU_bg *, i32 x, i32 y, union NDS_PX *, void *);

// map_base, block_width, tile_base

struct affine_normal_xtradata {
    u32 map_base, tile_base;
    i32 block_width;
    i32 width, height;
};

static void affine_normal(struct NDS *this, struct NDSENG2D *eng, struct NDS_PPU_bg *bg, i32 x, i32 y, union NDS_PX *out, void *xtra)
{
    struct affine_normal_xtradata *xd = (struct affine_normal_xtradata *)xtra;
    u32 tile_number = read_vram_bg(this, eng, xd->map_base + (y >> 3) * xd->block_width + (x >> 3), 1);

    u32 addr = (xd->tile_base + tile_number * 64);
    u32 c = read_vram_bg(this, eng, addr + ((y & 7) << 3) + (x & 7), 1);

    out->has = 1;
    out->color = 0x7FFF;
    if (c == 0) {
        out->has = 0;
    }
    else {
        out->has = 1;
        out->priority = bg->priority;
        out->color = read_pram_bg(this, eng, c << 1, 2);
    }
}

static void render_affine_loop(struct NDS *this, struct NDSENG2D *eng, struct NDS_PPU_bg *bg, i32 width, i32 height, affinerenderfunc render_func, void *xtra)
{
    i32 fx = bg->x_lerp;
    i32 fy = bg->y_lerp;
    for (i64 screen_x = 0; screen_x < 256; screen_x++) {
        i32 px = fx >> 8;
        i32 py = fy >> 8;
        fx += bg->pa;
        fy += bg->pc;

        if (bg->display_overflow) { // wraparound
            if (px >= width)
                px %= width;
            else if (px < 0)
                px = width + (px % width);

            if (py >= height)
                py %= height;
            else if (py < 0)
                py = height + (py % height);
        }
        else if ((px < 0) || (py < 0) || (px >= width) || (py >= height)){ // clip/transparent
            continue;
        }
        render_func(this, eng, bg, px, py, &bg->line[screen_x], xtra);
    }
    affine_line_end(this, eng, bg);
}

static void affine_rotodc(struct NDS *this, struct NDSENG2D *eng, struct NDS_PPU_bg *bg, i32 x, i32 y, union NDS_PX *out, void *xtra)
{
    struct affine_normal_xtradata *xd = (struct affine_normal_xtradata *)xtra;

    u32 color = read_vram_bg(this, eng, bg->screen_base_block + (y * xd->width + x) * 2, 2);
    if (color & 0x8000) {
        out->has = 1;
        out->color = c15to18(color & 0x7FFF);
        out->priority = bg->priority;
    } else {
        out->has = 0;
    }
}

static void affine_rotobpp8(struct NDS *this, struct NDSENG2D *eng, struct NDS_PPU_bg *bg, i32 x, i32 y, union NDS_PX *out, void *xtra)
{
    struct affine_normal_xtradata *xd = (struct affine_normal_xtradata *)xtra;

    u32 color = read_vram_bg(this, eng, bg->screen_base_block+ y * xd->width + x, 1);
    if (color) {
        out->has = 1;
        out->color = c15to18(read_pram_bg(this, eng, color << 1, 2));
        out->priority = bg->priority;
    } else {
        out->has = 0;
    }
}

static void affine_rotobpp16(struct NDS *this, struct NDSENG2D *eng, struct NDS_PPU_bg *bg, i32 x, i32 y, union NDS_PX *out, void *xtra)
{
    struct affine_normal_xtradata *xd = (struct affine_normal_xtradata *)xtra;
    u32 encoder = read_vram_bg(this, eng, xd->map_base + ((y >> 3) * xd->block_width + (x >> 3)) * 2, 2);
    u32 number = encoder & 0x3FF;
    u32 palette = encoder >> 12;
    u32 tile_x = x & 7;
    u32 tile_y = y & 7;

    tile_x ^= ((encoder >> 10) & 1) * 7;
    tile_y ^= ((encoder >> 11) & 1) * 7;

    u32 addr = xd->tile_base + number * 64;
    u8 index = read_vram_bg(this, eng, addr + (tile_y << 3) + tile_x, 1);
    if (index == 0) {
        out->has = 0;
    }
    else {
        u32 c;

        if (eng->io.bg.extended_palettes)
            out->color = c15to18(read_pram_bg_ext(this, eng, bg->num, bg->num, index << 1));
        else
            out->color = c15to18(read_pram_bg(this, eng, index << 1, 2));
        out->has = 1;
        out->priority = bg->priority;
    }
}


static void draw_bg_line_extended(struct NDS *this, struct NDSENG2D *eng, u32 bgnum)
{
    struct NDS_PPU_bg *bg = &eng->bg[bgnum];
    memset(bg->line, 0, sizeof(bg->line));
    if (!bg->enable) { return; }

    struct affine_normal_xtradata xtra;
    if (bg->bpp8) {
        static const i32 sz[4][2] = { {128, 128}, {256, 256}, {512, 256}, {512, 512}};
        i32 width = sz[bg->screen_size][0];
        i32 height = sz[bg->screen_size][1];
        xtra.width = width;
        xtra.height = height;
        if ((bg->character_base_block >> 14) & 1) {
            // Rotate scale direct-color
            render_affine_loop(this, eng, bg, width, height, &affine_rotodc, &xtra);
        }
        else {
            render_affine_loop(this, eng, bg, width, height, &affine_rotobpp8, &xtra);
        }
    }
    else {
        // rotoscale 16bpp
        i32 size = 128 << bg->screen_size;
        i32 block_width = 16 << bg->screen_size;
        xtra.block_width = block_width;
        xtra.map_base = eng->io.bg.screen_base + bg->screen_base_block;
        xtra.tile_base = eng->io.bg.character_base + bg->character_base_block;

        render_affine_loop(this, eng, bg, size, size, &affine_rotobpp16, &xtra);
    }



}


static void draw_bg_line_affine(struct NDS *this, struct NDSENG2D *eng, u32 bgnum)
{
    struct NDS_PPU_bg *bg = &eng->bg[bgnum];

    affine_line_start(this, eng, bg);
    memset(bg->line, 0, sizeof(bg->line));
    if (!bg->enable) {
        return;
    }

    struct affine_normal_xtradata xtra;
    xtra.map_base = eng->io.bg.screen_base + bg->screen_base_block;
    xtra.tile_base = eng->io.bg.character_base + bg->character_base_block;
    xtra.block_width = 16 << bg->screen_size;

    render_affine_loop(this, eng, bg, eng->bg[bgnum].hpixels, eng->bg[bgnum].vpixels, &affine_normal, &xtra);

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
        union NDS_PX *src;
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
    union NDS_PX *src = &eng->obj.line[0];
    u32 mosaic_counter = 0;
    for (u32 x = 0; x < 256; x++) {
        union NDS_PX *current = &eng->obj.line[x];
        if (!current->sp_mosaic || !src->sp_mosaic || (mosaic_counter == 0))  {
            src = current;
        }
        else {
            current->has = src->has;
            current->color = src->color;
            current->priority = src->priority;
            current->sp_translucent = src->sp_translucent;
            current->sp_mosaic = src->sp_mosaic;
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
        if (eng->window[NDS_WINOBJ].enable && eng->window[NDS_WINOBJ].inside[x]) active_window = &eng->window[NDS_WINOBJ];
        if (eng->window[NDS_WIN1].enable && eng->window[NDS_WIN1].inside[x]) active_window = &eng->window[NDS_WIN1];
        if (eng->window[NDS_WIN0].enable && eng->window[NDS_WIN0].inside[x]) active_window = &eng->window[NDS_WIN0];
    }
    return active_window;
}

static void find_targets_and_priorities(u32 bg_enables[6], union NDS_PX *layers[6], u32 *layer_a_out, u32 *layer_b_out, i32 x, u32 *actives)
{
/*
 * MelonDS algorithm:
 * for (priority = 3; priority >= 0; priority--) {
 *   for (u32 bg = 3; bg >= 0; bg--) {
 *       draw_bg(priority);
 *   }
 *   for each pixel:
 *       if sprite has pixel at this priority, overwrite.
 *
 *   on ANYTHING overwrite ANYTHING ELSE, move target right
 *
 *   so, priority in same level works like this:
 *      sprite, bg0, bg1, bg2, bg3, backdrop
 *
 *  this does the same thing but with only 1 draw pass
 */
    // Outputs here. 5 = backdrop, 4-1 = bg 3-0, 0 = sprite.
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

static void calculate_windows(struct NDS *this, struct NDSENG2D *eng)
{
    //if (!force && !this->ppu.window[0].enable && !this->ppu.window[1].enable && !this->ppu.window[GBA_WINOBJ].enable) return;

    // Calculate windows...
    calculate_windows_vflags(this, eng);
    if (this->clock.ppu.y >= 192) return;
    for (u32 wn = 0; wn < 2; wn++) {
        struct NDS_PPU_window *w = &eng->window[wn];
        if (!w->enable) {
            memset(&w->inside, 0, sizeof(w->inside));
            continue;
        }

        for (u32 x = 0; x < 256; x++) {
            if (x == w->left) w->h_flag = 1;
            if (x == w->right) w->h_flag = 0;

            w->inside[x] =  w->h_flag & w->v_flag;
        }
    }

    // Now take care of the outside window
    struct NDS_PPU_window *w0 = &eng->window[NDS_WIN0];
    struct NDS_PPU_window *w1 = &eng->window[NDS_WIN1];
    struct NDS_PPU_window *ws = &eng->window[NDS_WINOBJ];
    u32 w0r = w0->enable;
    u32 w1r = w1->enable;
    u32 wsr = ws->enable;
    struct NDS_PPU_window *w = &eng->window[NDS_WINOUTSIDE];
    memset(w->inside, 0, sizeof(w->inside));
    for (u32 x = 0; x < 256; x++) {
        u32 w0i = w0r & w0->inside[x];
        u32 w1i = w1r & w1->inside[x];
        u32 wsi = wsr & ws->inside[x];
        w->inside[x] = !(w0i || w1i || wsi);
    }

    /*struct GBA_PPU_window *wo = &eng->window[NDS_WINOUTSIDE];
    struct GBA_DBG_line *l = &this->dbg_info.line[this->clock.ppu.y];
    for (u32 x = 0; x < 240; x++) {
        l->window_coverage[x] = w0->inside[x] | (w1->inside[x] << 1) | (ws->inside[x] << 2) | (wo->inside[x] << 3);
    }*/
}


static void output_pixel(struct NDS *this, struct NDSENG2D *eng, u32 x, u32 obj_enable, u32 bg_enables[4]) {
    // Find which window applies to us.
    u32 default_active[6] = {1, 1, 1, 1, 1, 1}; // Default to active if no window.

    u32 *actives = default_active;

    struct NDS_PPU_window *active_window = get_active_window(this, eng, x);
    if (active_window) actives = active_window->active;

    union NDS_PX *sp_px = &eng->obj.line[x];
    union NDS_PX empty_px;
    empty_px.color=c15to18(read_pram_bg(this, eng, 0, 2));
    empty_px.priority=4;
    empty_px.sp_translucent=0;
    empty_px.has=1;

    sp_px->has &= obj_enable;

    union NDS_PX *layers[6] = {
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

    union NDS_PX *target_a = layers[target_a_layer];
    union NDS_PX *target_b = layers[target_b_layer];

    u32 blend_b = target_b->color;
    u32 output_color = target_a->color;
    if (actives[5] || (target_a->has && target_a->sp_translucent &&
                                  eng->blend.targets_b[target_b_layer])) { // backdrop is contained in active for the highest-priority window OR above is a semi-transparent sprite & below is a valid target
        if (target_a->has && target_a->sp_translucent &&
            eng->blend.targets_b[target_b_layer]) { // above is semi-transparent sprite and below is a valid target
            output_color = nds_alpha(output_color, blend_b, eng->blend.use_eva_a, eng->blend.use_eva_b);
        } else if (mode == 1 && eng->blend.targets_a[target_a_layer] &&
                   eng->blend.targets_b[target_b_layer]) { // mode == 1, both are valid
            output_color = nds_alpha(output_color, blend_b, eng->blend.use_eva_a, eng->blend.use_eva_b);
        } else if (mode == 2 && eng->blend.targets_a[target_a_layer]) { // mode = 2, A is valid
            output_color = nds_brighten(output_color, (i32) eng->blend.use_bldy);
        } else if (mode == 3 && eng->blend.targets_a[target_a_layer]) { // mode = 3, B is valid
            output_color = nds_darken(output_color, (i32) eng->blend.use_bldy);
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
    calculate_windows(this, eng);
    apply_mosaic(this, eng);
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
    memset(eng->bg[3].line, 0, sizeof(eng->bg[3].line));
    calculate_windows(this, eng);
    apply_mosaic(this, eng);

    u32 bg_enables[4] = {eng->bg[0].enable, eng->bg[1].enable, eng->bg[2].enable, 0};
    //memset(eng->line_px, 0x50, sizeof(eng->line_px));
    for (u32 x = 0; x < 256; x++) {
        output_pixel(this, eng, x, eng->obj.enable, &bg_enables[0]);
    }
}

static void draw_line2(struct NDS *this, struct NDSENG2D *eng, struct NDS_DBG_line *l)
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
    draw_bg_line_affine(this, eng, 3);
    calculate_windows(this, eng);
    apply_mosaic(this, eng);

    u32 bg_enables[4] = {eng->bg[0].enable, eng->bg[1].enable, eng->bg[2].enable, eng->bg[3].enable};
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
    calculate_windows(this, eng);
    apply_mosaic(this, eng);

    u32 bg_enables[4] = {eng->bg[0].enable, eng->bg[1].enable, eng->bg[2].enable, eng->bg[3].enable};
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
    calculate_windows(this, eng);
    apply_mosaic(this, eng);

    u32 bg_enables[4] = {eng->bg[0].enable, eng->bg[1].enable, eng->bg[2].enable, eng->bg[3].enable};
    //memset(eng->line_px, 0x50, sizeof(eng->line_px));
    for (u32 x = 0; x < 256; x++) {
        output_pixel(this, eng, x, eng->obj.enable, &bg_enables[0]);
    }

}

//             u32 blend_r = (a_r, a_a, b_r, b_a, eva, evb);
static inline u32 capblend(u32 a_i, u32 a_a, u32 b_i, u32 b_a, u32 eva, u32 evb)
{
/*
Dest_Intensity = (  (SrcA_Intensitity * SrcA_Alpha * EVA)
                   + (SrcB_Intensitity * SrcB_Alpha * EVB) ) / 16 */
    u32 i = ((a_i * a_a * eva) + (b_i * b_a * evb)) >> 4;
    if (i > 31) i = 31;
    return i;
}

static const u32 VRAM_offsets[9] = {
        0x00000, // A - 128KB
        0x20000, // B - 128KB
        0x40000, // C - 128KB
        0x60000, // D - 128KB
};

static void run_dispcap(struct NDS *this)
{
    if (!this->ppu.io.DISPCAPCNT.capture_enable) return;
    // 0=128x128, 1=256x64, 2=256x128, 3=256x192 dots

    u32 vram_write_block = this->ppu.io.DISPCAPCNT.vram_write_block;
    if (this->mem.vram.io.bank[vram_write_block].mst != 0) {
        static int a = 1;
        if (a) {
            printf("\nWARN: CANT OUTPUT TO UNMAPPED BLOCK...!?");
            a = 0;
        }
        return;
    }
    static const int csize[4][2] = {{128, 128}, {256, 64}, {256, 128}, {256, 192} };

    u32 y_size = csize[this->ppu.io.DISPCAPCNT.capture_size][1];
    if (y_size < this->clock.ppu.y) return;
    u32 x_size = csize[this->ppu.io.DISPCAPCNT.capture_size][0];

    // Get source A and B
    struct NDSENG2D *eng = &this->ppu.eng2d[0];
    static const int needab[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, 1}};
    u32 need_a = needab[this->ppu.io.DISPCAPCNT.capture_source][0];
    u32 need_b = needab[this->ppu.io.DISPCAPCNT.capture_source][1];
    if (need_a) {
        // 0 = full PPUA output  eng->line_px
        // 1 = RE output only re->output 6 to 5
        if (!this->ppu.io.DISPCAPCNT.source_a) { // full PPUA output
            for (u32 x = 0; x < 256; x++) {
                this->ppu.line_a[x] = C18to15(eng->line_px[x]);
            }
        }
        else {
            for (u32 x = 0; x < 256; x++) {
                this->ppu.line_a[x] = C18to15(this->re.out.linebuffer[this->clock.ppu.y].rgb[x]);
            }
        }
    }
    if (need_b) {
        // 0=VRAM
        // 1=Main Memory Display FIFO)
        static int a = 1;
        if (a) {
            printf("\nWARN NEED CAP B BUT NO SUPPORT!");
            a = 0;
        }
    }

    u16 *src;
    if (need_a) {
        src = this->ppu.line_a;
    }
    else if (need_b) {
        src = this->ppu.line_b;
    }
    else { // need_a && need_b
        // blend together!
        u32 eva = this->ppu.io.DISPCAPCNT.eva;
        u32 evb = this->ppu.io.DISPCAPCNT.evb;
        src = this->ppu.line_a;

        for (u32 x = 0; x < x_size; x++) {
            u32 fa = this->ppu.line_a[x];
            u32 fb = this->ppu.line_b[x];
            u32 a_r = fa & 0x1F;
            u32 a_g = (fa >> 5) & 0x1F;
            u32 a_b = (fa >> 10) & 0x1F;
            u32 a_a = (fa >> 15) & 1;
            u32 b_r = fb & 0x1F;
            u32 b_g = (fb >> 5) & 0x1F;
            u32 b_b = (fb >> 10) & 0x1F;
            u32 b_a = (fb >> 15) & 1;

            a_a = 1; // TODO: ?

            u32 blend_r = capblend(a_r, a_a, b_r, b_a, eva, evb);
            u32 blend_g = capblend(a_g, a_a, b_g, b_a, eva, evb);
            u32 blend_b = capblend(a_b, a_a, b_b, b_a, eva, evb);
            // Dest_Alpha = (SrcA_Alpha AND (EVA>0)) OR (SrcB_Alpha AND EVB>0))
            u32 blend_a = 0x8000 * ((a_a && (eva>0)) | (a_b && (evb>0)));
            this->ppu.line_a[x] = blend_r | (blend_g << 5) | (blend_b << 10) | blend_a;
        }
    }

    u32 offset = (x_size * this->clock.ppu.y) << 1;
    offset += VRAM_offsets[vram_write_block];
    memcpy(this->mem.vram.data + offset, src, x_size * 2);
    memcpy(this->dbg_info.eng[0].line[this->clock.ppu.y].dispcap_px, src, x_size * 2);
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
        case 2:
            draw_line2(this, eng, l);
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

    for (u32 bgnum = 0; bgnum < 4; bgnum++) {
        memcpy(this->dbg_info.eng[eng_num].line[this->clock.ppu.y].bg[bgnum].buf,
               eng->bg[bgnum].line, sizeof(union NDS_PX) * 256);
    }
    memcpy(this->dbg_info.eng[eng_num].line[this->clock.ppu.y].sprite_buf, eng->obj.line,
           sizeof(union NDS_PX) * 256);

    // Then we will pixel output it to the screen...
    u32 *line_output = this->ppu.cur_output + (this->clock.ppu.y * OUT_WIDTH);
    if (eng_num ^ this->ppu.io.display_swap ^ 1) line_output += (192 * OUT_WIDTH);

    u32 display_mode = eng->io.display_mode;
    if (eng_num == 1) display_mode &= 1;
    switch(display_mode) {
        case 0: // WHITE!
            memset(line_output, 0xFF, 1024);
            break;
        case 1: // NORMAL!
            memcpy(line_output, eng->line_px, 1024);
            break;
        case 2: { // VRAM!
            u32 block_num = this->ppu.io.display_block << 17;
            u16 *line_input = (u16 *)(this->mem.vram.data + block_num);
            line_input += this->clock.ppu.y << 8; // * 256
            for(u32 x = 0; x < 256; x++) {
                line_output[x] = c15to18(line_input[x]);
            }
            break; }
        case 3: { //Main RAM
            u16 *line_input = (u16 *)this->mem.RAM;
            line_input += this->clock.ppu.y << 8; // * 256
            for(u32 x = 0; x < 256; x++) {
                line_output[x] = c15to18(line_input[x]);
            }
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

        if (this->clock.ppu.y == 0) {
            this->ppu.doing_capture = this->ppu.io.DISPCAPCNT.capture_enable;
        }

        if (this->dbg.events.view.vec) {
            debugger_report_line(this->dbg.interface, this->clock.ppu.y);
        }

        for (u32 ppun = 0; ppun < 2; ppun++) {
            struct NDSENG2D *eng = &this->ppu.eng2d[ppun];
            for (u32 bgnum = 0; bgnum < 4; bgnum++) {
                struct NDS_PPU_bg *bg = &eng->bg[bgnum];
                if ((this->clock.ppu.y == 0) || bg->x_written) {
                    bg->x_lerp = bg->x;
                }

                if ((this->clock.ppu.y == 0) || (bg->y_written)) {
                    bg->y_lerp = bg->y;
                }
                bg->x_written = bg->y_written = 0;
            }
        }

        if ((this->ppu.io.vcount_at7 == this->clock.ppu.y) && this->ppu.io.vcount_irq_enable7) NDS_update_IF7(this, NDS_IRQ_VMATCH);
        if ((this->ppu.io.vcount_at9 == this->clock.ppu.y) && this->ppu.io.vcount_irq_enable9) NDS_update_IF9(this, NDS_IRQ_VMATCH);
    }
    else {
        // Start of hblank. Now draw line!
        if (this->clock.ppu.y < 192) {
            draw_line(this, 0);
            draw_line(this, 1);
            if (this->ppu.doing_capture) run_dispcap(this);
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
    this->clock.ppu.y = 0;
    this->clock.frame_start_cycle = NDS_clock_current7(this);
    this->ppu.cur_output = ((u32 *) this->ppu.display->output[this->ppu.display->last_written ^ 1]);
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
    }

    NDS_trigger_dma9_if(this, NDS_DMA_START_OF_DISPLAY);
}


void NDS_PPU_vblank(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct NDS *this = (struct NDS *)ptr;
    //printf("\nvblank %lld: line %d cyc:%lld", key, this->clock.ppu.y, NDS_clock_current7(this));

    this->clock.ppu.vblank_active = key;
    if (key) { // line 192
        if (this->ppu.io.vblank_irq_enable7) NDS_update_IF7(this, NDS_IRQ_VBLANK);
        if (this->ppu.io.vblank_irq_enable9) NDS_update_IF9(this, NDS_IRQ_VBLANK);
        NDS_check_dma7_at_vblank(this);
        NDS_check_dma9_at_vblank(this);
        NDS_GE_vblank_up(this);
        this->ppu.io.DISPCAPCNT.capture_enable = 0;
        this->ppu.doing_capture = 0;
    }
    // else { // line 0
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

        case R9_DISPCAPCNT+0: this->ppu.io.DISPCAPCNT.u = (this->ppu.io.DISPCAPCNT.u & 0xFFFFFF00) | (val & 0x7F); return;
        case R9_DISPCAPCNT+1: this->ppu.io.DISPCAPCNT.u = (this->ppu.io.DISPCAPCNT.u & 0xFFFF00FF) | ((val & 0x7F) << 8); return;
        case R9_DISPCAPCNT+2: this->ppu.io.DISPCAPCNT.u = (this->ppu.io.DISPCAPCNT.u & 0xFF00FFFF) | ((val & 0x3F) << 16); return;
        case R9_DISPCAPCNT+3: this->ppu.io.DISPCAPCNT.u = (this->ppu.io.DISPCAPCNT.u & 0x00FFFFFF) | ((val & 0xEF) << 24); return;

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
            /*if ((val & 7) != eng->io.bg_mode) {
                printf("\neng%d NEW BG MODE: %d", en, val & 7);
                dbgloglog(NDS_CAT_PPU_BG_MODE, DBGLS_INFO, "eng%d new BG mode:%d", en, eng->io.bg_mode);
            }*/
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
                this->ppu.io.display_block = ((val >> 2) & 3);
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
            bg->character_base_block = ((val >> 2) & 7) << 14;
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
            bg->screen_base_block = ((val >> 0) & 31) << 11;
            if (bgnum >= 2) {
                bg->display_overflow = (val >> 5) & 1;
                bg->ext_pal_slot = 0;
            }
            else {
                bg->ext_pal_slot = (val >> 5) & 1;
                bg->display_overflow = 0;
            }
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

        case R9_BG0HOFS+0:
        case R9_BG0HOFS+1:
        case R9_BG0VOFS+0:
        case R9_BG0VOFS+1:
        case R9_BG1HOFS+0:
        case R9_BG1HOFS+1:
        case R9_BG1VOFS+0:
        case R9_BG1VOFS+1:
        case R9_BG2HOFS+0:
        case R9_BG2HOFS+1:
        case R9_BG2VOFS+0:
        case R9_BG2VOFS+1:
        case R9_BG3HOFS+0:
        case R9_BG3HOFS+1:
        case R9_BG3VOFS+0:
        case R9_BG3VOFS+1:
        case R9_BLDY+0:
        case R9_BLDY+1:
            return 0;
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
