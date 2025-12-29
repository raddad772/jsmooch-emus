//
// Created by . on 1/18/25.
//
#include <cstring>
#include <cassert>

#include "helpers/multisize_memaccess.cpp"

#include "nds_bus.h"
#include "nds_dma.h"
#include "nds_regs.h"
#include "nds_irq.h"
#include "nds_ppu.h"
#include "nds_debugger.h"
#include "helpers/color.h"

namespace NDS::PPU {

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


core::core(NDS::core *parent) : bus(parent)
{
    for (auto & eng : eng2d) {

        for (u32 bgnum = 0; bgnum < 4; bgnum++) {
            eng.BG[bgnum].num = bgnum;
        }
    }
    eng2d[0].io.bg.do_3d = false;
    eng2d[1].num = 1;
}

void core::reset()
{
    for (auto &p : eng2d) {
        p.mosaic.bg.hsize = p.mosaic.bg.vsize = 1;
        p.mosaic.obj.hsize = p.mosaic.obj.vsize = 1;
        p.BG[2].pa = 1 << 8; p.BG[2].pd = 1 << 8;
        p.BG[3].pa = 1 << 8; p.BG[3].pd = 1 << 8;
    }
}

u64 ENG2D::read_vram_bg(const u32 addr, const u8 sz) const {
    assert((addr >> 14) < 32);
    u8 *ptr = memp.bg_vram[(addr >> 14) & 31];
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

u32 ENG2D::read_vram_obj(const u32 addr, const u8 sz) const {
    assert((addr >> 14) < 16);
    u8 *ptr = memp.obj_vram[(addr >> 14) & 15];
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

u32 ENG2D::read_pram_bg_ext(const u32 slotnum, u32 addr) const {
    if (!memp.bg_extended_palette[slotnum & 3]) {
        static int a = 1;
        if (a) {
            printf("\nMISSED VRAM PAL READ!");
            a = 0;
        }
        return 0b1111110000011111; // glaring purple
    }
    addr &= 0x1FFF;
    return cR16(memp.bg_extended_palette[slotnum & 3], addr);
}

u32 ENG2D::read_pram_bg(u32 addr, u8 sz) const {
    return cR[sz](mem.bg_palette, addr & 0x1FF);
}

u32 ENG2D::read_ext_palette_obj(u32 palette, u32 index) const
{
    u32 addr = 0x600 + ((palette << 5) | (index << 1)) + (0x200 * num);
    addr &= 0x1FFF;
    if (memp.obj_extended_palette) {
         u32 v = cR16(memp.obj_extended_palette, addr);
         return v;
    }
    return 0;
}

u32 ENG2D::read_pram_obj2(const u32 palette, const u32 index) const {
    if (io.obj.extended_palettes)
        return read_ext_palette_obj(palette, index) & 0x7FFF;
    const u32 addr = ((palette << 5) | (index << 1)) & 0x1FF;
    const u32 v = cR16(mem.obj_palette, addr) & 0x7FFF;
    return v;
}

u32 ENG2D::read_pram_obj(u32 addr, u8 sz) const {
    return cR[sz](mem.obj_palette, addr & 0x1FF);
}

u32 ENG2D::read_oam(u32 addr, u8 sz) const {
    return cR[sz](mem.oam, addr & 0x3FF);
}

#define OUT_WIDTH 256

void core::calculate_windows_vflags(ENG2D &eng)
{
    for (u32 wn = 0; wn < 2; wn++) {
        WINDOW *w = &eng.window[wn];
        u32 y = bus->clock.ppu.y & 0xFF;
        if (y == w->top) w->v_flag = 1;
        if (y == w->bottom) w->v_flag = 0;
    }
}

static constexpr int obj_size[4][4][2] = {
        { { 8 , 8  }, { 16, 16 }, { 32, 32 }, { 64, 64 } },
        { { 16, 8  }, { 32, 8  }, { 32, 16 }, { 64, 32 } },
        { { 8 , 16 }, { 8 , 32 }, { 16, 32 }, { 32, 64 } },
        { { 8 , 8  }, { 8 , 8  }, { 8 , 8  }, { 8 , 8  } }
};

void ENG2D::draw_obj_on_line(u32 oam_offset, u32 prio_to_draw)
{
    i32 cycles_per_pixel;
    i32 pa, pb, pc, pd;

    u16 attr[3];

    attr[2] = read_oam(oam_offset+4, 2);
    u32 priority = (attr[2] >> 10) & 3;
    if (priority != prio_to_draw) return;

    attr[0] = read_oam(oam_offset, 2);
    u32 affine = (attr[0] >> 8) & 1;
    u32 b9 = (attr[0] >> 9) & 1;

    if (!affine && b9) {
        obj.drawing_cycles--;
        return;
    }
    cycles_per_pixel = affine ? 2 : 1;
    obj.drawing_cycles -= affine ? 10 : 1;
    if (obj.drawing_cycles < 0) return;


    i32 y = attr[0] & 0xFF;
    u32 shape = (attr[0] >> 14) & 3;
    u32 mode = (attr[0] >> 10) & 3;
    u32 m_mosaic = (attr[0] >> 12) & 1;

    if (y >= 192) y -= 256;

    attr[1] = read_oam(oam_offset+2, 2);
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
    i32 my_y = ppu->bus->clock.ppu.y;
    if (my_y < (y - half_height) || my_y >= (y + half_height)){
        return;
    }

    if (affine) {
        u32 pgroup = ((attr[1] >> 9) & 31) << 5;
        pa = read_oam(pgroup + 6, 2);
        pb = read_oam(pgroup + 14, 2);
        pc = read_oam(pgroup + 22, 2);
        pd = read_oam(pgroup + 30, 2);
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

    if (m_mosaic) {
        sprite_y = ppu->mosaic.obj.y_current - y;
    }

    u32 dbpp = bpp8;
    if (mode == 3) dbpp = 2;

    for (i32 lx = -half_width; lx <= half_width; lx++) {
        PX mypx = {.has=1, .priority=priority, .sp_mosaic=m_mosaic, .dbg_mode=mode, .dbg_bpp = dbpp};
        obj.drawing_cycles -= cycles_per_pixel;
        if (obj.drawing_cycles < 1) return;
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
            if (io.obj.bitmap.map_1d) {
                u32 addr = (tile_num * (64 << io.obj.bitmap.boundary_1d) + ty * width + tx) << 1;
                c = read_vram_obj(addr, 2);
            }
            else {
                u32 mask = (16 << io.obj.bitmap.dim_2d) - 1;
                u32 addr = ((tile_num & ~mask) * 64 + (tile_num & mask) * 8 + ty * (128 << io.obj.bitmap.dim_2d) + tx) << 1;
                c = read_vram_obj(addr, 2);
            }
            mypx.has = (c >> 15) & 1;
            mypx.color = c15to18(c & 0x7FFF);
            mypx.alpha = palette;
            mypx.sp_translucent = palette < 15;
        }
        else if (bpp8) {
            if (io.obj.tile.map_1d)
                tile_addr = (tile_num << io.obj.tile.boundary_1d) + block_y * (width >> 2);
            else
                tile_addr = (tile_num & 0x3FE) + block_y * 32;

            tile_addr = ((tile_addr + (block_x << 1)) << 5) + (tile_y << 3) + tile_x;
            c = read_vram_obj(tile_addr, 1);
            mypx.has = c != 0;
            mypx.color = c15to18(read_pram_obj2(0, c));
            mypx.sp_translucent = mode == 1;
        }
        else { // 4bpp
            if (io.obj.tile.map_1d)
                tile_addr = (tile_num << io.obj.tile.boundary_1d) + block_y * (width >> 3);
            else
                tile_addr = tile_num + block_y * 32;

            tile_addr = ((tile_addr + block_x) << 5) + ((tile_y << 2) | (tile_x >> 1));
            c = read_vram_obj(tile_addr, 1);
            if (tile_x & 1) c >>= 4;
            c &= 15;
            mypx.has = c != 0;

            c = read_pram_obj(((palette << 4) | c) << 1, 2);

            mypx.color = c15to18(c);
            mypx.sp_translucent = mode == 1;
        }
        PX *opx = &obj.line[line_x];

        if (mypx.has) {
            if (mode == 2) window[WINOBJ].inside[line_x] = 1;
            else opx->u = mypx.u;
        }
        else {
            if (!opx->has)
                opx->u = mypx.u;
        }
    }
}

void ENG2D::draw_obj_line()
{
    //obj.drawing_cycles = io.hblank_free ? 1530 : 2124;
    obj.drawing_cycles = 10000;

    memset(obj.line, 0, sizeof(obj.line));
    WINDOW *w = &window[WINOBJ];
    memset(&w->inside, 0, sizeof(w->inside));

    if (!obj.enable) return;
    // (GBA) Each OBJ takes:
    // n*1 cycles per pixel
    // 10 + n*2 per pixel if rotated/scaled
    for (i32 prio = 3; prio >= 0; prio--) {
        for (i32 i = 127; i >= 0; i--) {
            if (obj.drawing_cycles < 0) {
                //printf("\nNO DRAW CYCLES LEFT!");
                return;
            }
            draw_obj_on_line(i * 8, prio);
        }
    }
}

static inline u32 C18to15(u32 c)
{
    u32 b = (c >> 13) & 0x1F;
    u32 g = (c >> 7) & 0x1F;
    u32 r = (c >> 1) & 0x1F;
    return r | (g << 5) | (b << 10);
    //return (((c >> 13) & 0x1F) << 10) | (((c >> 7) & 0x1F) << 5) | ((c >> 1) & 0x1F);
}

void ENG2D::draw_3d_line(u32 bgnum)
{
    u32 line_num = ppu->bus->clock.ppu.y;
    if (line_num > 191) return;
    // Copy line over
    MBG &bg = BG[bgnum];
    memset(bg.line, 0, sizeof(bg.line));
    auto &re_line = ppu->bus->re.out.linebuffer[line_num];
    for (u32 x = 0; x < 256; x++) {
        bg.line[x].color = re_line.rgb[x];
        bg.line[x].has = re_line.has[x];
        bg.line[x].priority = bg.priority;
        bg.line[x].alpha = re_line.alpha[x];
    }
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4146) // unary minus operator applied to unsigned type, result still unsigned
#endif

void ENG2D::draw_bg_line_normal(u32 bgnum)
{
    MBG &bg = BG[bgnum];
    memset(bg.line, 0, sizeof(bg.line));
    if (!bg.enable) {
        return;
    }
    u32 expal_slot = bgnum | (bg.ext_pal_slot << 1);

    i32 line = bg.mosaic_enable ? bg.mosaic_y : ppu->bus->clock.ppu.y;
    line += bg.vscroll;

    i32 draw_x = -(bg.hscroll % 8);
    i32 grid_x = bg.hscroll / 8;
    i32 grid_y = line >> 3;
    i32 tile_y = line & 7;

    i32 screen_x = (grid_x >> 5) & 1;
    i32 screen_y = (grid_y >> 5) & 1;

    u32 base = io.bg.screen_base + bg.screen_base_block + (grid_y % 32) * 64;
    PX *buffer = bg.line;
    i32 last_encoder = -1;
    u16 encoder;

    grid_x &= 31;

    static constexpr i32 sxm[4] = { 0, 2048, 0, 2048};
    static constexpr i32 sym[4] = { 0, 0, 2048, 4096};
    u32 base_adjust = sxm[bg.screen_size];
    base += (screen_x * sxm[bg.screen_size]) + (screen_y * sym[bg.screen_size]);

    PX tile[8] = {};
    if (screen_x == 1) base_adjust *= -1;
    u32 tile_base = bg.character_base_block + io.bg.character_base;
    do {
        do {
            encoder = read_vram_bg(base + grid_x++ * 2, 2);
            if (encoder != last_encoder) {
                int number = encoder & 0x3FF;
                int palette = encoder >> 12;
                i32 flip_x = (encoder >> 10) & 1;
                i32 flip_y = (encoder >> 11) & 1;
                i32 _tile_y = tile_y ^ (7 * flip_y);
                i32 xxor = 7 * flip_x;

                if (bg.bpp8) { // 8bpp
                    u64 data = read_vram_bg(tile_base + (number << 6 | _tile_y << 3), 8);
                    for (u32 ix = 0; ix < 8; ix++) {
                        u32 mx = ix ^ xxor;
                        u32 index = data & 0xFF;
                        data >>= 8;

                        if (index == 0) {
                            tile[mx].has = 0;
                        }
                        else {
                            u32 c;

                            if (io.bg.extended_palettes)
                                c = read_pram_bg_ext(expal_slot, palette << 9 | index << 1);
                            else
                                c = read_pram_bg(index << 1, 2);
                            tile[mx].has = 1;
                            tile[mx].priority = bg.priority;
                            tile[mx].color = c15to18(c);
                        }
                    }
                }
                else { // 4bpp
                    u32 data = read_vram_bg(tile_base + (number << 5 | _tile_y << 2), 4);
                    for (u32 ix = 0; ix < 8; ix++) {
                        u32 mx = ix ^ xxor;
                        u32 index = data & 15;
                        data >>= 4;
                        if (index == 0) {
                            tile[mx].has = 0;
                        }
                        else {
                            tile[mx].has = 1;
                            tile[mx].priority = bg.priority;
                            u32 c = read_pram_bg(palette << 5 | index << 1, 2);
                            tile[mx].color = c15to18(c);
                        }
                    }
                }

                last_encoder = encoder;
            }

            if (draw_x >= 0 && draw_x <= 248) {
                memcpy(&buffer[draw_x], &tile[0], sizeof(PX) * 8);
                draw_x += 8;
            }
            else {
                int x = 0;
                int max = 8;

                if (draw_x < 0) {
                    x = -draw_x;
                    draw_x = 0;
                    for (; x < max; x++) {
                        memcpy(&buffer[draw_x++], &tile[x], sizeof(PX));
                    }
                } else {
                    max -= draw_x - 248;
                    for (; x < max; x++) {
                        memcpy(&buffer[draw_x++], &tile[x], sizeof(PX));
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

void ENG2D::affine_line_start(MBG &bg)
{
    if (!bg.enable) {
        if (bg.mosaic_y != bg.last_y_rendered) {
            bg.x_lerp += bg.pb * mosaic.bg.vsize;
            bg.y_lerp += bg.pd * mosaic.bg.vsize;
            bg.last_y_rendered = bg.mosaic_y;
        }
    }
}

void ENG2D::affine_line_end(MBG &bg)
{
    if (bg.mosaic_y != bg.last_y_rendered) {
        bg.x_lerp += bg.pb * mosaic.bg.vsize;
        bg.y_lerp += bg.pd * mosaic.bg.vsize;
        bg.last_y_rendered = bg.mosaic_y;
    }
}


// map_base, block_width, tile_base

struct affine_normal_xtradata {
    u32 map_base{}, tile_base{};
    i32 block_width{};
    i32 width{}, height{};
};

void ENG2D::affine_normal(MBG &bg, i32 x, i32 y, PX &out, void *xtra) const
{
    auto &xd = *static_cast<affine_normal_xtradata *>(xtra);
    u32 tile_number = read_vram_bg(xd.map_base + (y >> 3) * xd.block_width + (x >> 3), 1);

    u32 addr = (xd.tile_base + tile_number * 64);
    u32 c = read_vram_bg(addr + ((y & 7) << 3) + (x & 7), 1);

    out.has = 1;
    out.color = 0x7FFF;
    if (c == 0) {
        out.has = 0;
    }
    else {
        out.has = 1;
        out.priority = bg.priority;
        out.color = read_pram_bg(c << 1, 2);
    }
}

void ENG2D::render_affine_loop(MBG &bg, i32 width, i32 height, affinerenderfunc render_func, void *xtra)
{
    i32 fx = bg.x_lerp;
    i32 fy = bg.y_lerp;
    for (i64 screen_x = 0; screen_x < 256; screen_x++) {
        i32 px = fx >> 8;
        i32 py = fy >> 8;
        fx += bg.pa;
        fy += bg.pc;

        if (bg.display_overflow) { // wraparound
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
        (this->*render_func)(bg, px, py, bg.line[screen_x], xtra);
    }
    affine_line_end(bg);
}

void ENG2D::affine_rotodc(MBG &bg, i32 x, i32 y, PX &out, void *xtra) const
{
    auto &xd = *static_cast<affine_normal_xtradata *>(xtra);

    u32 color = read_vram_bg(bg.screen_base_block + (y * xd.width + x) * 2, 2);
    //printf("\nY:%d x:%d addr:%04x", y, x, bg.screen_base_block + (y * xd.width + x) * 2);
    if (color & 0x8000) {
        out.has = 1;
        out.color = c15to18(color & 0x7FFF);
        out.priority = bg.priority;
    }// else {
    //    out.has = 0;
    //}
}

void ENG2D::affine_rotobpp8(MBG &bg, i32 x, i32 y, PX &out, void *xtra) const
{
    auto &xd = *static_cast<affine_normal_xtradata *>(xtra);

    const u32 color = read_vram_bg(bg.screen_base_block+ y * xd.width + x, 1);
    if (color) {
        out.has = 1;
        out.color = c15to18(read_pram_bg(color << 1, 2));
        out.priority = bg.priority;
    } else {
        out.has = 0;
    }
}

void ENG2D::affine_rotobpp16(MBG &bg, i32 x, i32 y, PX &out, void *xtra) const
{
    auto &xd = *static_cast<affine_normal_xtradata *>(xtra);
    const u32 encoder = read_vram_bg(xd.map_base + ((y >> 3) * xd.block_width + (x >> 3)) * 2, 2);
    const u32 number = encoder & 0x3FF;
    u32 palette = encoder >> 12;
    u32 tile_x = x & 7;
    u32 tile_y = y & 7;

    tile_x ^= ((encoder >> 10) & 1) * 7;
    tile_y ^= ((encoder >> 11) & 1) * 7;

    const u32 addr = xd.tile_base + number * 64;
    const u8 index = read_vram_bg(addr + (tile_y << 3) + tile_x, 1);
    if (index == 0) {
        out.has = 0;
    }
    else {
        u32 c;

        if (io.bg.extended_palettes)
            out.color = c15to18(read_pram_bg_ext(bg.num, index << 1));
        else
            out.color = c15to18(read_pram_bg(index << 1, 2));
        out.has = 1;
        out.priority = bg.priority;
    }
}


void ENG2D::draw_bg_line_extended(u32 bgnum)
{
    MBG &bg = BG[bgnum];
    memset(bg.line, 0, sizeof(bg.line));
    if (!bg.enable) { return; }

    affine_normal_xtradata xtra;
    if (bg.bpp8) {
        static constexpr i32 sz[4][2] = { {128, 128}, {256, 256}, {512, 256}, {512, 512}};
        i32 width = sz[bg.screen_size][0];
        i32 height = sz[bg.screen_size][1];
        xtra.width = width;
        xtra.height = height;
        if ((bg.character_base_block >> 14) & 1) {
            // Rotate scale direct-color
            render_affine_loop(bg, width, height, &ENG2D::affine_rotodc, &xtra);
        }
        else {
            render_affine_loop(bg, width, height, &ENG2D::affine_rotobpp8, &xtra);
        }
    }
    else {
        // rotoscale 16bpp
        i32 size = 128 << bg.screen_size;
        i32 block_width = 16 << bg.screen_size;
        xtra.block_width = block_width;
        xtra.map_base = io.bg.screen_base + bg.screen_base_block;
        xtra.tile_base = io.bg.character_base + bg.character_base_block;

        render_affine_loop(bg, size, size, &ENG2D::affine_rotobpp16, &xtra);
    }
}


void ENG2D::draw_bg_line_affine(u32 bgnum)
{
    MBG &bg = BG[bgnum];

    affine_line_start(bg);
    memset(bg.line, 0, sizeof(bg.line));
    if (!bg.enable) {
        return;
    }

    affine_normal_xtradata xtra;
    xtra.map_base = io.bg.screen_base + bg.screen_base_block;
    xtra.tile_base = io.bg.character_base + bg.character_base_block;
    xtra.block_width = 16 << bg.screen_size;

    render_affine_loop(bg, BG[bgnum].hpixels, BG[bgnum].vpixels, &ENG2D::affine_normal, &xtra);

    affine_line_end(bg);
}

void ENG2D::apply_mosaic()
{
    // This function updates vertical mosaics, and applies horizontal.
    if (ppu->mosaic.bg.y_counter == 0) {
        ppu->mosaic.bg.y_current = ppu->bus->clock.ppu.y;
    }
    for (auto & bg : BG) {
        if (!bg.enable) continue;
        if (bg.mosaic_enable) bg.mosaic_y = ppu->mosaic.bg.y_current;
        else bg.mosaic_y = ppu->bus->clock.ppu.y + 1;
    }
    ppu->mosaic.bg.y_counter = (ppu->mosaic.bg.y_counter + 1) % mosaic.bg.vsize;

    if (ppu->mosaic.obj.y_counter == 0) {
        ppu->mosaic.obj.y_current = ppu->bus->clock.ppu.y;
    }
    ppu->mosaic.obj.y_counter = (ppu->mosaic.obj.y_counter + 1) % mosaic.obj.vsize;

    // Now do horizontal blend
    for (auto &bg : BG) {
        if (!bg.enable || !bg.mosaic_enable) continue;
        u32 mosaic_counter = 0;
        PX *src = &bg.line[0];
        for (u32 x = 0; x < 256; x++) {
            if (mosaic_counter == 0) {
                src = &bg.line[x];
            }
            else {
                bg.line[x].has = src->has;
                bg.line[x].color = src->color;
                bg.line[x].priority = src->priority;
            }
            mosaic_counter = (mosaic_counter + 1) % mosaic.bg.hsize;
        }
    }

    // Now do sprites, which is a bit different
    PX *src = &obj.line[0];
    u32 mosaic_counter = 0;
    for (u32 x = 0; x < 256; x++) {
        PX *current = &obj.line[x];
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
        mosaic_counter = (mosaic_counter + 1) % mosaic.obj.hsize;
    }
}

#define NDSCTIVE_SFX 5

WINDOW *ENG2D::get_active_window(u32 x)
{
    WINDOW *active_window = nullptr;
    if (window[WIN0].enable || window[WIN1].enable || window[WINOBJ].enable) {
        active_window = &window[WINOUTSIDE];
        if (window[WINOBJ].enable && window[WINOBJ].inside[x]) active_window = &window[WINOBJ];
        if (window[WIN1].enable && window[WIN1].inside[x]) active_window = &window[WIN1];
        if (window[WIN0].enable && window[WIN0].inside[x]) active_window = &window[WIN0];
    }
    return active_window;
}

static void find_targets_and_priorities(bool bg_enables[6], PX *layers[6], u32 &layer_a_out, u32 &layer_b_out, i32 x, bool *actives)
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
    layer_a_out = 5;
    layer_b_out = 5;

    // Get highest priority 1st target
    for (i32 priority = 3; priority >= 0; priority--) {
        for (i32 layer_num = 4; layer_num >= 0; layer_num--) {
            if (actives[layer_num] && bg_enables[layer_num] && layers[layer_num]->has && (layers[layer_num]->priority == priority)) {
                layer_b_out = layer_a_out;
                layer_a_out = layer_num;
            }
        }
    }
}

void ENG2D::calculate_windows()
{
    //if (!force && !window[0].enable && !window[1].enable && !window[GBA_WINOBJ].enable) return;

    // Calculate windows...
    ppu->calculate_windows_vflags(*this);
    if (ppu->bus->clock.ppu.y >= 192) return;
    for (u32 wn = 0; wn < 2; wn++) {
        WINDOW *w = &window[wn];
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
    WINDOW &w0 = window[WIN0];
    WINDOW &w1 = window[WIN1];
    WINDOW &ws = window[WINOBJ];
    u32 w0r = w0.enable;
    u32 w1r = w1.enable;
    u32 wsr = ws.enable;
    WINDOW *w = &window[WINOUTSIDE];
    memset(w->inside, 0, sizeof(w->inside));
    for (u32 x = 0; x < 256; x++) {
        u32 w0i = w0r & w0.inside[x];
        u32 w1i = w1r & w1.inside[x];
        u32 wsi = wsr & ws.inside[x];
        w->inside[x] = !(w0i || w1i || wsi);
    }

    /*GBA_PPU_window *wo = &eng.window[WINOUTSIDE];
    GBA_DBG_line *l = &dbg_info.line[bus->clock.ppu.y];
    for (u32 x = 0; x < 240; x++) {
        l->window_coverage[x] = w0->inside[x] | (w1->inside[x] << 1) | (ws->inside[x] << 2) | (wo->inside[x] << 3);
    }*/
}


void ENG2D::output_pixel(u32 x, bool obj_enable, bool bg_enables[4]) {
    // Find which window applies to us.
    bool default_active[6] = {true, true, true, true, true, true}; // Default to active if no window.

    bool *actives = &default_active[0];

    WINDOW *active_window = get_active_window(x);
    if (active_window) actives = &active_window->active[0];

    PX *sp_px = &obj.line[x];
    PX empty_px;
    empty_px.color=c15to18(read_pram_bg(0, 2));
    empty_px.priority=4;
    empty_px.sp_translucent=0;
    empty_px.has=1;

    sp_px->has &= obj_enable;

    PX *layers[6] = {
            sp_px,
            &BG[0].line[x],
            &BG[1].line[x],
            &BG[2].line[x],
            &BG[3].line[x],
            &empty_px
    };
    bool obg_enables[6] = {
            obj_enable,
            bg_enables[0],
            bg_enables[1],
            bg_enables[2],
            bg_enables[3],
            true
    };
    u32 mode = blend.mode;

    /* Find targets A and B */
    u32 target_a_layer, target_b_layer;
    find_targets_and_priorities(obg_enables, layers, target_a_layer, target_b_layer, x, actives);

    // Blending ONLY happens if the topmost pixel is a valid A target and the next-topmost is a valid B target
    // Brighten/darken only happen if topmost pixel is a valid A target

    PX *target_a = layers[target_a_layer];
    PX *target_b = layers[target_b_layer];

    u32 blend_b = target_b->color;
    u32 output_color = target_a->color;
    if (actives[5] || (target_a->has && target_a->sp_translucent &&
                                  blend.targets_b[target_b_layer])) { // backdrop is contained in active for the highest-priority window OR above is a semi-transparent sprite & below is a valid target
        if (target_a->has && target_a->sp_translucent &&
            blend.targets_b[target_b_layer]) { // above is semi-transparent sprite and below is a valid target
            output_color = nds_alpha(output_color, blend_b, blend.use_eva_a, blend.use_eva_b);
        } else if (mode == 1 && blend.targets_a[target_a_layer] &&
                   blend.targets_b[target_b_layer]) { // mode == 1, both are valid
            output_color = nds_alpha(output_color, blend_b, blend.use_eva_a, blend.use_eva_b);
        } else if (mode == 2 && blend.targets_a[target_a_layer]) { // mode = 2, A is valid
            output_color = nds_brighten(output_color, static_cast<i32>(blend.use_bldy));
        } else if (mode == 3 && blend.targets_a[target_a_layer]) { // mode = 3, B is valid
            output_color = nds_darken(output_color, static_cast<i32>(blend.use_bldy));
        }
    }
    //if (output_color != 0) printf("\nOUTPUT %d", output_color);
    line_px[x] = 0x80000 | output_color;
}

void ENG2D::draw_line0(DBG_line *l)
{
    draw_obj_line();
    if ((num == 0) && (io.bg.do_3d)) {
        draw_3d_line(0);
    }
    else {
        draw_bg_line_normal(0);
    }
    draw_bg_line_normal(1);
    draw_bg_line_normal(2);
    draw_bg_line_normal(3);
    calculate_windows();
    apply_mosaic();
    bool bg_enables[4] = {BG[0].enable, BG[1].enable, BG[2].enable, BG[3].enable};
    for (u32 x = 0; x < 256; x++) {
        output_pixel(x, obj.enable, &bg_enables[0]);
    }
}

void ENG2D::draw_line1(DBG_line *l)
{
    draw_obj_line();
    if ((num == 0) && (io.bg.do_3d)) {
        draw_3d_line(0);
    }
    else {
        draw_bg_line_normal(0);
    }
    draw_bg_line_normal(1);
    draw_bg_line_affine(2);
    memset(BG[3].line, 0, sizeof(BG[3].line));
    calculate_windows();
    apply_mosaic();

    bool bg_enables[4] = {BG[0].enable, BG[1].enable, BG[2].enable, 0};
    //memset(line_px, 0x50, sizeof(line_px));
    for (u32 x = 0; x < 256; x++) {
        output_pixel(x, obj.enable, &bg_enables[0]);
    }
}

void ENG2D::draw_line2(DBG_line *l)
{
    draw_obj_line();
    if ((num == 0) && (io.bg.do_3d)) {
        draw_3d_line(0);
    }
    else {
        draw_bg_line_normal(0);
    }
    draw_bg_line_normal(1);
    draw_bg_line_affine(2);
    draw_bg_line_affine(3);
    calculate_windows();
    apply_mosaic();

    bool bg_enables[4] = {BG[0].enable, BG[1].enable, BG[2].enable, BG[3].enable};
    //memset(line_px, 0x50, sizeof(line_px));
    for (u32 x = 0; x < 256; x++) {
        output_pixel(x, obj.enable, &bg_enables[0]);
    }
}


void ENG2D::draw_line3(DBG_line *l)
{
    draw_obj_line();
    if ((num == 0) && (io.bg.do_3d)) {
        draw_3d_line(0);
    }
    else {
        draw_bg_line_normal(0);
    }
    draw_bg_line_normal(1);
    draw_bg_line_affine(2);
    draw_bg_line_extended(3);
    calculate_windows();
    apply_mosaic();

    bool bg_enables[4] = {BG[0].enable, BG[1].enable, BG[2].enable, BG[3].enable};
    //memset(line_px, 0x50, sizeof(line_px));
    for (u32 x = 0; x < 256; x++) {
        output_pixel(x, obj.enable, &bg_enables[0]);
    }
}

void ENG2D::draw_line5(DBG_line *l)
{
    draw_obj_line();
    if ((num == 0) && (io.bg.do_3d)) {
        draw_3d_line(0);
    }
    else {
        draw_bg_line_normal(0);
    }
    draw_bg_line_normal(1);
    draw_bg_line_extended(2);
    draw_bg_line_extended(3);
    calculate_windows();
    apply_mosaic();

    bool bg_enables[4] = {BG[0].enable, BG[1].enable, BG[2].enable, BG[3].enable};
    //memset(line_px, 0x50, sizeof(line_px));
    for (u32 x = 0; x < 256; x++) {
        output_pixel(x, obj.enable, &bg_enables[0]);
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

static constexpr u32 VRAM_offsets[9] = {
        0x00000, // A - 128KB
        0x20000, // B - 128KB
        0x40000, // C - 128KB
        0x60000, // D - 128KB
};

void core::run_dispcap()
{
    if (!io.DISPCAPCNT.capture_enable) return;
    // 0=128x128, 1=256x64, 2=256x128, 3=256x192 dots

    u32 vram_write_block = io.DISPCAPCNT.vram_write_block;
    if (bus->mem.vram.io.bank[vram_write_block].mst != 0) {
        static int a = 1;
        if (a) {
            printf("\nWARN: CANT OUTPUT TO UNMAPPED BLOCK...!?");
            a = 0;
        }
        return;
    }
    static constexpr int csize[4][2] = {{128, 128}, {256, 64}, {256, 128}, {256, 192} };

    u32 y_size = csize[io.DISPCAPCNT.capture_size][1];
    if (y_size < bus->clock.ppu.y) return;
    u32 x_size = csize[io.DISPCAPCNT.capture_size][0];

    // Get source A and B
    ENG2D &eng = eng2d[0];
    static constexpr bool needab[4][2] = {{true, false}, {false, true}, {true, true}, {true, true}};
    bool need_a = needab[io.DISPCAPCNT.capture_source][0];
    bool need_b = needab[io.DISPCAPCNT.capture_source][1];
    if (need_a) {
        // 0 = full PPUA output  eng.line_px
        // 1 = RE output only re->output 6 to 5
        if (!io.DISPCAPCNT.source_a) { // full PPUA output
            for (u32 x = 0; x < 256; x++) {
                line_a[x] = C18to15(eng.line_px[x]) | ((eng.line_px[x] >> 4) & 0x8000);;

            }
        }
        else {
            for (u32 x = 0; x < 256; x++) {
                line_a[x] = C18to15(bus->re.out.linebuffer[bus->clock.ppu.y].rgb[x]);
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
        src = line_a;
    }
    else if (need_b) {
        src = line_b;
    }
    else { // need_a && need_b
        // blend together!
        u32 eva = io.DISPCAPCNT.eva;
        u32 evb = io.DISPCAPCNT.evb;
        src = line_a;

        for (u32 x = 0; x < x_size; x++) {
            u32 fa = line_a[x];
            u32 fb = line_b[x];
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
            line_a[x] = blend_r | (blend_g << 5) | (blend_b << 10) | blend_a;
        }
    }

    u32 offset = (x_size * bus->clock.ppu.y) << 1;
    offset += VRAM_offsets[vram_write_block];
    memcpy(bus->mem.vram.data + offset, src, x_size * 2);
    memcpy(bus->dbg_info.eng[0].line[bus->clock.ppu.y].dispcap_px, src, x_size * 2);
}

void core::draw_line(u32 eng_num)
{
    ENG2D &eng = eng2d[eng_num];
    DBG_line *l = &bus->dbg_info.eng[eng_num].line[bus->clock.ppu.y];
    l->bg_mode = eng.io.bg_mode;
    // We have 2-4 display modes. They can be: WHITE, NORMAL, VRAM display, and "main memory display"
    // During this time, the 2d engine runs like normal!
    // So we will draw our lines...
    //if (eng.num == 1 && bus->clock.ppu.y == 100) { printf("\nline:100 bg_mode:%d what:%d bg0:%d bg1:%d bg2:%d bg3:%d", eng.io.bg_mode, eng.io.bg_mode, eng.BG[0].enable, eng.BG[1].enable, eng.BG[2].enable, eng.BG[3].enable); }
    switch(eng.io.bg_mode) {
        case 0:
            eng.draw_line0(l);
            break;
        case 1:
            eng.draw_line1(l);
            break;
        case 2:
            eng.draw_line2(l);
            break;
        case 3:
            eng.draw_line3(l);
            break;
        case 5:
            eng.draw_line5(l);
            break;
        default: {
            static int warned = 0;
            if (warned != eng.io.bg_mode) {
                printf("\nWARNING implement mode %d eng%d", eng.io.bg_mode, eng.num);
                warned = eng.io.bg_mode;
            }
        }
    }

    for (u32 bgnum = 0; bgnum < 4; bgnum++) {
        memcpy(bus->dbg_info.eng[eng_num].line[bus->clock.ppu.y].bg[bgnum].buf,
               eng.BG[bgnum].line, sizeof(PX) * 256);
    }
    memcpy(bus->dbg_info.eng[eng_num].line[bus->clock.ppu.y].sprite_buf, eng.obj.line,
           sizeof(PX) * 256);

    // Then we will pixel output it to the screen...
    u32 *line_output = cur_output + (bus->clock.ppu.y * OUT_WIDTH);
    if (eng_num ^ io.display_swap ^ 1) line_output += (192 * OUT_WIDTH);

    u32 display_mode = eng.io.display_mode;
    if (eng_num == 1) display_mode &= 1;
    switch(display_mode) {
        case 0: // WHITE!
            memset(line_output, 0xFF, 1024);
            break;
        case 1: // NORMAL!
            memcpy(line_output, eng.line_px, 1024);
            break;
        case 2: { // VRAM!
            u32 block_num = io.display_block << 17;
            u16 *line_input = reinterpret_cast<u16 *>(bus->mem.vram.data + block_num);
            line_input += bus->clock.ppu.y << 8; // * 256
            for(u32 x = 0; x < 256; x++) {
                line_output[x] = c15to18(line_input[x]);
            }
            break; }
        case 3: { //Main RAM
            u16 *line_input = reinterpret_cast<u16 *>(bus->mem.RAM);
            line_input += bus->clock.ppu.y << 8; // * 256
            for(u32 x = 0; x < 256; x++) {
                line_output[x] = c15to18(line_input[x]);
            }
            break; }
    }
}

void core::hblank(void *ptr, u64 key, u64 clock, u32 jitter) // Called at hblank time
{
    auto *bus = static_cast<NDS::core *>(ptr);
    auto &ppu = bus->ppu;
    bus->clock.ppu.hblank_active = key;
    if (key == 0) { // end hblank...new line!
        bus->clock.ppu.scanline_start = clock - jitter;

        bus->clock.ppu.y++;
        //printf("\nhblank %lld: line %d  cyc:%lld", key, bus->clock.ppu.y, clock.master_cycle_count7);

        if (bus->clock.ppu.y >= 263) {
            ppu.new_frame();
        }

        if (bus->clock.ppu.y == 0) {
            ppu.doing_capture = ppu.io.DISPCAPCNT.capture_enable;
        }

        if (bus->dbg.events.view.vec) {
            debugger_report_line(bus->dbg.interface, bus->clock.ppu.y);
        }

        for (auto &eng : ppu.eng2d) {
            for (auto &bg : eng.BG) {
                if ((bus->clock.ppu.y == 0) || bg.x_written) {
                    bg.x_lerp = bg.x;
                }

                if ((bus->clock.ppu.y == 0) || (bg.y_written)) {
                    bg.y_lerp = bg.y;
                }
                bg.x_written = bg.y_written = false;
            }
        }

        if ((ppu.io.vcount_at7 == bus->clock.ppu.y) && ppu.io.vcount_irq_enable7) bus->update_IF7(IRQ_VMATCH);
        if ((ppu.io.vcount_at9 == bus->clock.ppu.y) && ppu.io.vcount_irq_enable9) bus->update_IF9(IRQ_VMATCH);
    }
    else {
        // Start of hblank. Now draw line!
        if (bus->clock.ppu.y < 192) {
            ppu.draw_line(0);
            ppu.draw_line(1);
            if (ppu.doing_capture) ppu.run_dispcap();
        } else {
            ppu.calculate_windows_vflags(ppu.eng2d[0]);
            ppu.calculate_windows_vflags(ppu.eng2d[1]);
        }
        if (ppu.io.hblank_irq_enable7) bus->update_IF7(IRQ_HBLANK);
        if (ppu.io.hblank_irq_enable9) bus->update_IF9(IRQ_HBLANK);
        bus->check_dma9_at_hblank();
    }
}

#define MASTER_CYCLES_PER_FRAME 570716
void core::new_frame() {
    bus->clock.ppu.y = 0;
    bus->clock.frame_start_cycle = bus->clock.current7();
    cur_output = static_cast<u32 *>(display->output[display->last_written ^ 1]);
    display->last_written ^= 1;
    bus->clock.master_frame++;
    MBG *bg;

    for (auto &p : eng2d) {
        for (auto &mbg : p.BG) {
            mbg.mosaic_y = 0;
            mbg.last_y_rendered = 191;
        }
        mosaic.bg.y_counter = 0;
        mosaic.bg.y_current = 0;
        mosaic.obj.y_counter = 0;
        mosaic.obj.y_current = 0;
    }

    bus->trigger_dma9_if(DMA_START_OF_DISPLAY);
}


void core::vblank(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *bus = static_cast<NDS::core *>(ptr);
    auto &ppu = bus->ppu;
    //printf("\nvblank %lld: line %d cyc:%lld", key, bus->clock.ppu.y, NDS_clock.current7());

    bus->clock.ppu.vblank_active = key;
    if (key) { // line 192
        if (ppu.io.vblank_irq_enable7) bus->update_IF7(IRQ_VBLANK);
        if (ppu.io.vblank_irq_enable9) bus->update_IF9(IRQ_VBLANK);
        bus->check_dma7_at_vblank();
        bus->check_dma9_at_vblank();
        bus->ge.vblank_up();
        ppu.io.DISPCAPCNT.capture_enable = 0;
        ppu.doing_capture = false;
    }
    // else { // line 0
}

void core::write_2d_bg_palette(u32 eng_num, u32 addr, u8 sz, u32 val)
{
    cW[sz](eng2d[eng_num].mem.bg_palette, addr, val);
}

void core::write_2d_obj_palette(u32 eng_num, u32 addr, u8 sz, u32 val)
{
    cW[sz](eng2d[eng_num].mem.obj_palette, addr, val);
}

void ENG2D::calc_screen_sizeA(u32 bgnum) {
    MBG &bg = BG[num];
    if ((num == 0) && bg.do3d) {
        bg.hpixels = 256;
        bg.vpixels = 192;
        return;
    }

    static constexpr u32 sk[8][4] = {
            {1, 1, 1, 1}, // text  text  text  text
            {1, 1, 1, 2}, // text  text  text  affine
            {1, 1, 2, 2}, // text text affine affine
            {1, 1, 1, 3}, // text text text extend
            {1, 1, 2, 3}, // text text affine extend
            {1, 1, 3, 3}, // text text extend extend
            {1, 0, 4, 0},  // 3d empty big empty
            {0, 0, 0, 0},  // empty empty empty empty
    };
    u32 mk = sk[io.bg_mode][num];
    if (mk == SK_extended) { // "extend" so look at other bits...
        if (bg.bpp8) {
                u32 b = (bg.character_base_block >> 14) & 1;
                if (!b) {
                    mk = SK_rotscale_8bit;
                }
                else {
                    mk = SK_rotscale_direct;
                }
            }
        else {
            mk = SK_rotscale_16bit; // rot/scale with 16bit bgmap entries
        }
    }
    if ((num == 0) && (mk == 1)) {
        if (bg.do3d) mk = SK_3donly;
    }

    bg.kind = static_cast<SCREEN_KINDS>(mk);
    if (mk == 0) return;

/*
screen base however NOT used at all for Large screen bitmap mode
  bgcnt size  text     rotscal    bitmap   large bmp
  0           256x256  128x128    128x128  512x1024
  1           512x256  256x256    256x256  1024x512
  2           256x512  512x512    512x256  -
  3           512x512  1024x1024  512x512  - */
    if (mk == SK_text) {
        switch(bg.screen_size) {
            case 0: bg.htiles = 32; bg.vtiles = 32; break;
            case 1: bg.htiles = 64; bg.vtiles = 32; break;
            case 2: bg.htiles = 32; bg.vtiles = 64; break;
            case 3: bg.htiles = 64; bg.vtiles = 64; break;
        }
        bg.hpixels = bg.htiles * 8;
        bg.vpixels = bg.vtiles * 8;
    }
    else if (mk == SK_affine) {
        switch(bg.screen_size) {
            case 0: bg.htiles = 16; bg.vtiles = 16; break;
            case 1: bg.htiles = 32; bg.vtiles = 32; break;
            case 2: bg.htiles = 64; bg.vtiles = 64; break;
            case 3: bg.htiles = 128; bg.vtiles = 128; break;
        }
        bg.hpixels = bg.htiles * 8;
        bg.vpixels = bg.vtiles * 8;
    }
    else if ((mk == SK_rotscale_8bit) || (mk == SK_rotscale_16bit) || (mk == SK_rotscale_direct)) {
        switch(bg.screen_size) {
            case 0: bg.hpixels=128; bg.vpixels=128; break;
            case 1: bg.hpixels=256; bg.vpixels=256; break;
            case 2: bg.hpixels=512; bg.vpixels=256; break;
            case 3: bg.hpixels=512; bg.vpixels=512; break;
        }
    }

    bg.htiles_mask = bg.htiles - 1;
    bg.vtiles_mask = bg.vtiles - 1;
    bg.hpixels_mask = bg.hpixels - 1;
    bg.vpixels_mask = bg.vpixels - 1;
}

void ENG2D::calc_screen_sizeB(u32 bgnum)
{
    MBG &bg = BG[bgnum];
    u32 mode = io.bg_mode;
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
    bg.kind = static_cast<SCREEN_KINDS>(sk[mode][num]);
    if (mode >= 3) return;
    // mode0 they're all the same no scaling
    u32 scales = mode >= 1 ? (num > 1) :  0;
    // mode1 bg 0-1 no scaling, 2 scaling
    // mode2 is 2 and 3
#define T(scales, ssize, hrs, vrs) case (((scales) << 2) | (ssize)): bg.htiles = hrs; bg.vtiles = vrs; break
    switch((scales << 2) | bg.screen_size) {
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
    bg.htiles_mask = bg.htiles - 1;
    bg.vtiles_mask = bg.vtiles - 1;
    bg.hpixels = bg.htiles << 3;
    bg.vpixels = bg.vtiles << 3;
    bg.hpixels_mask = bg.hpixels - 1;
    bg.vpixels_mask = bg.vpixels - 1;
}

void ENG2D::calc_screen_size(u32 bgnum)
{
    if (num == 0) calc_screen_sizeA(bgnum);
    else calc_screen_sizeB(bgnum);
}

void MBG::update_x(u32 which, u32 val)
{
    i32 v = x & 0x0FFFFFFF;
    val &= 0xFF;
    switch(which) {
        case 0: v = (v & 0x0FFFFF00) | val; break;
        case 1: v = (v & 0x0FFF00FF) | (val << 8); break;
        case 2: v = (v & 0x0F00FFFF) | (val << 16); break;
        case 3: v = (v & 0x00FFFFFF) | (val << 24); break;
    }
    if ((v >> 27) & 1) v |= 0xF0000000;
    x = v;
    x_written = true;
}

void MBG::update_y(u32 which, u32 val)
{
    u32 v = y & 0x0FFFFFFF;
    val &= 0xFF;
    switch(which) {
        case 0: v = (v & 0x0FFFFF00) | val; break;
        case 1: v = (v & 0x0FFF00FF) | (val << 8); break;
        case 2: v = (v & 0x0F00FFFF) | (val << 16); break;
        case 3: v = (v & 0x00FFFFFF) | (val << 24); break;
    }
    if ((v >> 27) & 1) v |= 0xF0000000;
    y = v;
    y_written = true;
}

static constexpr u32 boundary_to_stride[4] = {32, 64, 128, 256};
static constexpr u32 boundary_to_stride_bitmap[2] = { 32, 64}; // ??

void core::write7_io8(u32 addr, u8 sz, u8 access, u32 val) {
    u32 en = 0;
    if (addr >= 0x04001000) {
        addr -= 0x1000;
        en = 1;
    }
    switch(addr) {
        case R_DISPSTAT+0:
            io.vblank_irq_enable7 = (val >> 3) & 1;
            io.hblank_irq_enable7 = (val >> 4) & 1;
            io.vcount_irq_enable7 = (val >> 5) & 1;
            io.vcount_at7 = (io.vcount_at7 & 0xFF) | ((val & 0x80) << 1);
            return;
        case R_DISPSTAT+1:
            io.vcount_at7 = (io.vcount_at7 & 0x100) | val;
            return;
    }
    printf("\nUNKNOWN PPU WR7 ADDR:%08x sz:%d VAL:%08x", addr, sz, val);
}

void core::write9_io8(u32 addr, u8 sz, u8 access, u32 val)
{
    u32 en = 0;
    if (addr >= 0x04001000) {
        addr -= 0x1000;
        en = 1;
    }
    ENG2D &eng = eng2d[en];
    switch(addr) {
        case R9_DISP3DCNT+0:
            bus->re.io.DISP3DCNT.u = (bus->re.io.DISP3DCNT.u & 0xFF00) | (val & 0xFF);
            return;
        case R9_DISP3DCNT+1:
            // bits 5 and 6 are acks
            if (val & 0x10) { // ack rdlines underflow
                bus->re.io.DISP3DCNT.rdlines_underflow = 0;
            }
            if (val & 0x20) {
                bus->re.io.DISP3DCNT.poly_vtx_ram_overflow = 0;
            }
            bus->re.io.DISP3DCNT.u = (bus->re.io.DISP3DCNT.u & 0xFF) | ((val & 0b01001111) << 8);
            return;

        case R_VCOUNT+0: // read-only register...
        case R_VCOUNT+1:
            return;
        case R9_DISP3DCNT+2:
        case R9_DISP3DCNT+3: return;

        case R9_DISPCAPCNT+0: io.DISPCAPCNT.u = (io.DISPCAPCNT.u & 0xFFFFFF00) | (val & 0x7F); return;
        case R9_DISPCAPCNT+1: io.DISPCAPCNT.u = (io.DISPCAPCNT.u & 0xFFFF00FF) | ((val & 0x7F) << 8); return;
        case R9_DISPCAPCNT+2: io.DISPCAPCNT.u = (io.DISPCAPCNT.u & 0xFF00FFFF) | ((val & 0x3F) << 16); return;
        case R9_DISPCAPCNT+3: io.DISPCAPCNT.u = (io.DISPCAPCNT.u & 0x00FFFFFF) | ((val & 0xEF) << 24); return;

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
            io.vblank_irq_enable9 = (val >> 3) & 1;
            io.hblank_irq_enable9 = (val >> 4) & 1;
            io.vcount_irq_enable9 = (val >> 5) & 1;
            io.vcount_at9 = (io.vcount_at9 & 0xFF) | ((val & 0x80) << 1);
            return;
        case R_DISPSTAT+1:
            io.vcount_at9 = (io.vcount_at9 & 0x100) | val;
            return;
        case R9_DISPCNT+0:
            /*if ((val & 7) != eng.io.bg_mode) {
                printf("\neng%d NEW BG MODE: %d", en, val & 7);
                dbgloglog(NDS_CAT_PPU_BG_MODE, DBGLS_INFO, "eng%d new BG mode:%d", en, eng.io.bg_mode);
            }*/
            eng.io.bg_mode = val & 7;
            if (en == 0) {
                eng.io.bg.do_3d = (val >> 3) & 1;
                eng.BG[0].do3d = eng.io.bg.do_3d;
            }
            eng.io.obj.tile.map_1d = (val >> 4) & 1;
            eng.io.obj.bitmap.dim_2d = (val >> 5) & 1;
            eng.io.obj.bitmap.map_1d = (val >> 6) & 1;
            eng.io.force_blank = (val >> 7) & 1;
            eng.calc_screen_size(0);
            eng.calc_screen_size(1);
            eng.calc_screen_size(2);
            eng.calc_screen_size(3);
            return;
        case R9_DISPCNT+1:
            eng.BG[0].enable = (val >> 0) & 1;
            eng.BG[1].enable = (val >> 1) & 1;
            eng.BG[2].enable = (val >> 2) & 1;
            eng.BG[3].enable = (val >> 3) & 1;
            eng.obj.enable = (val >> 4) & 1;
            eng.window[0].enable = (val >> 5) & 1;
            eng.window[1].enable = (val >> 6) & 1;
            eng.window[WINOBJ].enable = (val >> 7) & 1;
            return;
        case R9_DISPCNT+2:
            eng.io.display_mode = val & 3;
            if ((en == 1) && (eng.io.display_mode > 1)) {
                printf("\nWARNING eng1 BAD DISPLAY MODE: %d", eng.io.display_mode);
            }
            eng.io.obj.tile.boundary_1d = (val >> 4) & 3;
            eng.io.obj.tile.stride_1d = boundary_to_stride[eng.io.obj.tile.boundary_1d];
            eng.io.hblank_free = (val >> 7) & 1;
            if (en == 0) {
                io.display_block = ((val >> 2) & 3);
                eng.io.obj.bitmap.boundary_1d = (val >> 6) & 1;
                eng.io.obj.bitmap.stride_1d = boundary_to_stride_bitmap[eng.io.obj.bitmap.boundary_1d];
            }
            return;
        case R9_DISPCNT+3:
            if (en == 0) {
                eng.io.bg.character_base = (val & 7) << 16;
                eng.io.bg.screen_base = ((val >> 3) & 7) << 16;
            }
            eng.io.bg.extended_palettes = (val >> 6) & 1;
            eng.io.obj.extended_palettes = (val >> 7) & 1;
            return;
        case R9_BG0CNT+0:
        case R9_BG1CNT+0:
        case R9_BG2CNT+0:
        case R9_BG3CNT+0: { // BGCNT lo
            u32 bgnum = (addr & 0b0110) >> 1;
            MBG &bg = eng.BG[bgnum];
            bg.priority = val & 3;
            bg.extrabits = (val >> 4) & 3;
            bg.character_base_block = ((val >> 2) & 7) << 14;
            bg.mosaic_enable = (val >> 6) & 1;
            bg.bpp8 = (val >> 7) & 1;
            if (eng.num == 0) eng.calc_screen_size(bgnum);
            return; }
        case R9_BG0CNT+1:
        case R9_BG1CNT+1:
        case R9_BG2CNT+1:
        case R9_BG3CNT+1: { // BGCNT lo
            u32 bgnum = (addr & 0b0110) >> 1;
            MBG &bg = eng.BG[bgnum];
            bg.screen_base_block = ((val >> 0) & 31) << 11;
            if (bgnum >= 2) {
                bg.display_overflow = (val >> 5) & 1;
                bg.ext_pal_slot = 0;
            }
            else {
                bg.ext_pal_slot = (val >> 5) & 1;
                bg.display_overflow = 0;
            }
            bg.screen_size = (val >> 6) & 3;
            eng.calc_screen_size(bgnum);
            return; }

#define SET16L(thing, to) thing = (thing & 0xFF00) | to
#define SET16H(thing, to) thing = (thing & 0xFF) | (to << 8)

        case R9_BG0HOFS+0: SET16L(eng.BG[0].hscroll, val); return;
        case R9_BG0HOFS+1: SET16H(eng.BG[0].hscroll, val); return;
        case R9_BG0VOFS+0: SET16L(eng.BG[0].vscroll, val); return;
        case R9_BG0VOFS+1: SET16H(eng.BG[0].vscroll, val); return;

        case R9_BG1HOFS+0: SET16L(eng.BG[1].hscroll, val); return;
        case R9_BG1HOFS+1: SET16H(eng.BG[1].hscroll, val); return;
        case R9_BG1VOFS+0: SET16L(eng.BG[1].vscroll, val); return;
        case R9_BG1VOFS+1: SET16H(eng.BG[1].vscroll, val); return;

        case R9_BG2HOFS+0: SET16L(eng.BG[2].hscroll, val); return;
        case R9_BG2HOFS+1: SET16H(eng.BG[2].hscroll, val); return;
        case R9_BG2VOFS+0: SET16L(eng.BG[2].vscroll, val); return;
        case R9_BG2VOFS+1: SET16H(eng.BG[2].vscroll, val); return;

        case R9_BG3HOFS+0: SET16L(eng.BG[3].hscroll, val); return;
        case R9_BG3HOFS+1: SET16H(eng.BG[3].hscroll, val); return;
        case R9_BG3VOFS+0: SET16L(eng.BG[3].vscroll, val); return;
        case R9_BG3VOFS+1: SET16H(eng.BG[3].vscroll, val); return;

#define BG2 2
#define BG3 3
        case R9_BG2PA+0: eng.BG[2].pa = (eng.BG[2].pa & 0xFFFFFF00) | val; return;
        case R9_BG2PA+1: eng.BG[2].pa = (eng.BG[2].pa & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case R9_BG2PB+0: eng.BG[2].pb = (eng.BG[2].pb & 0xFFFFFF00) | val; return;
        case R9_BG2PB+1: eng.BG[2].pb = (eng.BG[2].pb & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case R9_BG2PC+0: eng.BG[2].pc = (eng.BG[2].pc & 0xFFFFFF00) | val; return;
        case R9_BG2PC+1: eng.BG[2].pc = (eng.BG[2].pc & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case R9_BG2PD+0: eng.BG[2].pd = (eng.BG[2].pd & 0xFFFFFF00) | val; return;
        case R9_BG2PD+1: eng.BG[2].pd = (eng.BG[2].pd & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case R9_BG2X+0: eng.BG[BG2].update_x(0, val); return;
        case R9_BG2X+1: eng.BG[BG2].update_x(1, val); return;
        case R9_BG2X+2: eng.BG[BG2].update_x(2, val); return;
        case R9_BG2X+3: eng.BG[BG2].update_x(3, val); return;
        case R9_BG2Y+0: eng.BG[BG2].update_y(0, val); return;
        case R9_BG2Y+1: eng.BG[BG2].update_y(1, val); return;
        case R9_BG2Y+2: eng.BG[BG2].update_y(2, val); return;
        case R9_BG2Y+3: eng.BG[BG2].update_y(3, val); return;

        case R9_BG3PA+0: eng.BG[3].pa = (eng.BG[3].pa & 0xFFFFFF00) | val; return;
        case R9_BG3PA+1: eng.BG[3].pa = (eng.BG[3].pa & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case R9_BG3PB+0: eng.BG[3].pb = (eng.BG[3].pb & 0xFFFFFF00) | val; return;
        case R9_BG3PB+1: eng.BG[3].pb = (eng.BG[3].pb & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case R9_BG3PC+0: eng.BG[3].pc = (eng.BG[3].pc & 0xFFFFFF00) | val; return;
        case R9_BG3PC+1: eng.BG[3].pc = (eng.BG[3].pc & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case R9_BG3PD+0: eng.BG[3].pd = (eng.BG[3].pd & 0xFFFFFF00) | val; return;
        case R9_BG3PD+1: eng.BG[3].pd = (eng.BG[3].pd & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case R9_BG3X+0: eng.BG[BG3].update_x(0, val); return;
        case R9_BG3X+1: eng.BG[BG3].update_x(1, val); return;
        case R9_BG3X+2: eng.BG[BG3].update_x(2, val); return;
        case R9_BG3X+3: eng.BG[BG3].update_x(3, val); return;
        case R9_BG3Y+0: eng.BG[BG3].update_y(0, val); return;
        case R9_BG3Y+1: eng.BG[BG3].update_y(1, val); return;
        case R9_BG3Y+2: eng.BG[BG3].update_y(2, val); return;
        case R9_BG3Y+3: eng.BG[BG3].update_y(3, val); return;

        case R9_WIN0H:   eng.window[0].right = val; return;
        case R9_WIN0H+1: eng.window[0].left = val; return;
        case R9_WIN1H:   eng.window[1].right = val; return;
        case R9_WIN1H+1: eng.window[1].left = val; return;
        case R9_WIN0V:   eng.window[0].bottom = val; return;
        case R9_WIN0V+1: eng.window[0].top = val; return;
        case R9_WIN1V:   eng.window[1].bottom = val; return;
        case R9_WIN1V+1: eng.window[1].top = val; return;

        case R9_WININ:
            eng.window[0].active[1] = (val >> 0) & 1;
            eng.window[0].active[2] = (val >> 1) & 1;
            eng.window[0].active[3] = (val >> 2) & 1;
            eng.window[0].active[4] = (val >> 3) & 1;
            eng.window[0].active[0] = (val >> 4) & 1;
            eng.window[0].active[5] = (val >> 5) & 1;
            return;
        case R9_WININ+1:
            eng.window[1].active[1] = (val >> 0) & 1;
            eng.window[1].active[2] = (val >> 1) & 1;
            eng.window[1].active[3] = (val >> 2) & 1;
            eng.window[1].active[4] = (val >> 3) & 1;
            eng.window[1].active[0] = (val >> 4) & 1;
            eng.window[1].active[5] = (val >> 5) & 1;
            return;
        case R9_WINOUT:
            eng.window[WINOUTSIDE].active[1] = (val >> 0) & 1;
            eng.window[WINOUTSIDE].active[2] = (val >> 1) & 1;
            eng.window[WINOUTSIDE].active[3] = (val >> 2) & 1;
            eng.window[WINOUTSIDE].active[4] = (val >> 3) & 1;
            eng.window[WINOUTSIDE].active[0] = (val >> 4) & 1;
            eng.window[WINOUTSIDE].active[5] = (val >> 5) & 1;
            return;
        case R9_WINOUT+1:
            eng.window[WINOBJ].active[1] = (val >> 0) & 1;
            eng.window[WINOBJ].active[2] = (val >> 1) & 1;
            eng.window[WINOBJ].active[3] = (val >> 2) & 1;
            eng.window[WINOBJ].active[4] = (val >> 3) & 1;
            eng.window[WINOBJ].active[0] = (val >> 4) & 1;
            eng.window[WINOBJ].active[5] = (val >> 5) & 1;
            return;
        case R9_MOSAIC+0:
            eng.mosaic.bg.hsize = (val & 15) + 1;
            eng.mosaic.bg.vsize = ((val >> 4) & 15) + 1;
            return;
        case R9_MOSAIC+1:
            eng.mosaic.obj.hsize = (val & 15) + 1;
            eng.mosaic.obj.vsize = ((val >> 4) & 15) + 1;
            return;
        case R9_BLDCNT:
            eng.blend.mode = (val >> 6) & 3;
            // sp, bg0, bg1, bg2, bg3, bd
            eng.blend.targets_a[0] = (val >> 4) & 1;
            eng.blend.targets_a[1] = (val >> 0) & 1;
            eng.blend.targets_a[2] = (val >> 1) & 1;
            eng.blend.targets_a[3] = (val >> 2) & 1;
            eng.blend.targets_a[4] = (val >> 3) & 1;
            eng.blend.targets_a[5] = (val >> 5) & 1;
            return;
        case R9_BLDCNT+1:
            eng.blend.targets_b[0] = (val >> 4) & 1;
            eng.blend.targets_b[1] = (val >> 0) & 1;
            eng.blend.targets_b[2] = (val >> 1) & 1;
            eng.blend.targets_b[3] = (val >> 2) & 1;
            eng.blend.targets_b[4] = (val >> 3) & 1;
            eng.blend.targets_b[5] = (val >> 5) & 1;
            return;
        case R9_BLDALPHA:
            eng.blend.eva_a = val & 31;
            eng.blend.use_eva_a = eng.blend.eva_a;
            if (eng.blend.use_eva_a > 16) eng.blend.use_eva_a = 16;
            return;
        case R9_BLDALPHA+1:
            eng.blend.eva_b = val & 31;
            eng.blend.use_eva_b = eng.blend.eva_b;
            if (eng.blend.use_eva_b > 16) eng.blend.use_eva_b = 16;
            return;
        case R9_BLDY+0:
            eng.blend.bldy = val & 31;
            eng.blend.use_bldy = eng.blend.bldy;
            if (eng.blend.use_bldy > 16) eng.blend.use_bldy = 16;
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

void core::write9_io(u32 addr, u8 sz, u8 access, u32 val)
{
    write9_io8(addr, sz, access, val & 0xFF);
    if (sz >= 2) write9_io8(addr+1, sz, access, (val >> 8) & 0xFF);
    if (sz == 4) {
        write9_io8(addr+2, sz, access, (val >> 16) & 0xFF);
        write9_io8(addr+3, sz, access, (val >> 24) & 0xFF);
    }
}

u32 core::read9_io8(u32 addr, u8 sz, u8 access, bool has_effect)
{
    u32 en = 0;
    if (addr >= 0x04001000) {
        addr -= 0x1000;
        en = 1;
    }
    ENG2D &eng = eng2d[en];

    u32 v;

    switch(addr) {
        case R9_DISP3DCNT+0:
            return bus->re.io.DISP3DCNT.u & 0xFF;
        case R9_DISP3DCNT+1:
            return (bus->re.io.DISP3DCNT.u >> 8) & 0xFF;

        case R9_DISPCNT+0:
            v = eng.io.bg_mode;
            v |= eng.BG[0].do3d << 3;
            v |= eng.io.obj.tile.map_1d << 4;
            v |= eng.io.obj.bitmap.dim_2d << 5;
            v |= eng.io.obj.bitmap.map_1d << 6;
            v |= eng.io.force_blank << 7;
            return v;
        case R9_DISPCNT+1:
            v = eng.BG[0].enable;
            v |= eng.BG[1].enable << 1;
            v |= eng.BG[2].enable << 2;
            v |= eng.BG[3].enable << 3;
            v |= eng.obj.enable << 4;
            v |= eng.window[0].enable << 5;
            v |= eng.window[1].enable << 6;
            v |= eng.window[WINOBJ].enable << 7;
            return v;
        case R9_DISPCNT+2:
            v = eng.io.display_mode;
            v |= eng.io.obj.tile.boundary_1d << 4;
            v |= eng.io.hblank_free << 7;
            if (en == 0) {
                v |= io.display_block << 2;
                v |= eng.io.obj.bitmap.boundary_1d << 6;
            }
            return v;
        case R9_DISPCNT+3:
            v = eng.io.bg.extended_palettes << 6;
            v |= eng.io.obj.extended_palettes << 7;
            if (en == 0) {
                v |= eng.io.bg.character_base >> 16;
                v |= eng.io.bg.screen_base >> 13;
            }
            return v;
        case R_DISPSTAT+0: // DISPSTAT lo
            v = bus->clock.ppu.vblank_active;
            v |= bus->clock.ppu.hblank_active << 1;
            v |= (bus->clock.ppu.y == io.vcount_at9) << 2;
            v |= io.vblank_irq_enable9 << 3;
            v |= io.hblank_irq_enable9 << 4;
            v |= io.vcount_irq_enable9 << 5;
            v |= (io.vcount_at9 & 0x100) >> 1; // hi bit of vcount_at
            return v;
        case R_DISPSTAT+1: // DISPSTAT hi
            v = io.vcount_at9 & 0xFF;
            return v;
        case R_VCOUNT+0: // VCOunt lo
            return bus->clock.ppu.y & 0xFF;
        case R_VCOUNT+1:
            return bus->clock.ppu.y >> 8;

        case R9_BG0CNT+0:
        case R9_BG1CNT+0:
        case R9_BG2CNT+0:
        case R9_BG3CNT+0: {
            u32 bgnum = (addr & 0b0110) >> 1;
            MBG &bg = eng.BG[bgnum];
            v = bg.priority;
            v |= bg.extrabits << 4;
            v |= (bg.character_base_block >> 12);
            v |= bg.mosaic_enable << 6;
            v |= bg.bpp8 << 7;
            return v; }

        case R9_BG0CNT+1:
        case R9_BG1CNT+1:
        case R9_BG2CNT+1:
        case R9_BG3CNT+1: {
            u32 bgnum = (addr & 0b0110) >> 1;
            MBG &bg = eng.BG[bgnum];
            v = bg.screen_base_block >> 11;
            v |= bg.display_overflow << 5;
            v |= bg.screen_size << 6;
            return v; }

        case R9_WININ+0:
            v = eng.window[0].active[1] << 0;
            v |= eng.window[0].active[2] << 1;
            v |= eng.window[0].active[3] << 2;
            v |= eng.window[0].active[4] << 3;
            v |= eng.window[0].active[0] << 4;
            v |= eng.window[0].active[5] << 5;
            return v;
        case R9_WININ+1:
            v = eng.window[1].active[1] << 0;
            v |= eng.window[1].active[2] << 1;
            v |= eng.window[1].active[3] << 2;
            v |= eng.window[1].active[4] << 3;
            v |= eng.window[1].active[0] << 4;
            v |= eng.window[1].active[5] << 5;
            return v;
        case R9_WINOUT+0:
            v = eng.window[WINOUTSIDE].active[1] << 0;
            v |= eng.window[WINOUTSIDE].active[2] << 1;
            v |= eng.window[WINOUTSIDE].active[3] << 2;
            v |= eng.window[WINOUTSIDE].active[4] << 3;
            v |= eng.window[WINOUTSIDE].active[0] << 4;
            v |= eng.window[WINOUTSIDE].active[5] << 5;
            return v;
        case R9_WINOUT+1:
            v = eng.window[WINOBJ].active[1] << 0;
            v |= eng.window[WINOBJ].active[2] << 1;
            v |= eng.window[WINOBJ].active[3] << 2;
            v |= eng.window[WINOBJ].active[4] << 3;
            v |= eng.window[WINOBJ].active[0] << 4;
            v |= eng.window[WINOBJ].active[5] << 5;
            return v;
        case R9_BLDCNT+0:
            v = eng.blend.targets_a[0] << 4;
            v |= eng.blend.targets_a[1] << 0;
            v |= eng.blend.targets_a[2] << 1;
            v |= eng.blend.targets_a[3] << 2;
            v |= eng.blend.targets_a[4] << 3;
            v |= eng.blend.targets_a[5] << 5;
            v |= eng.blend.mode << 6;
            return v;
        case R9_BLDCNT+1:
            v = eng.blend.targets_b[0] << 4;
            v |= eng.blend.targets_b[1] << 0;
            v |= eng.blend.targets_b[2] << 1;
            v |= eng.blend.targets_b[3] << 2;
            v |= eng.blend.targets_b[4] << 3;
            v |= eng.blend.targets_b[5] << 5;
            return v;
        case R9_BLDALPHA+0:
            return eng.blend.eva_a;
        case R9_BLDALPHA+1:
            return eng.blend.eva_b;

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

u32 core::read7_io8(u32 addr, u8 sz, u8 access, bool has_effect) const
{
    u32 v;
    switch(addr) {
       case R_DISPSTAT+0: // DISPSTAT lo
           v = bus->clock.ppu.vblank_active;
           v |= bus->clock.ppu.hblank_active << 1;
           v |= (bus->clock.ppu.y == io.vcount_at7) << 2;
           v |= io.vblank_irq_enable7 << 3;
           v |= io.hblank_irq_enable7 << 4;
           v |= io.vcount_irq_enable7 << 5;
           v |= (io.vcount_at7 & 0x100) >> 1; // hi bit of vcount_at
           return v;
       case R_DISPSTAT+1: // DISPSTAT hi
           v = io.vcount_at7 & 0xFF;
           return v;
       case R_VCOUNT+0: // VCOunt lo
           return bus->clock.ppu.y & 0xFF;
       case R_VCOUNT+1:
           return bus->clock.ppu.y >> 8;
   }
   printf("\nUnknown PPURD7IO8 addr:%08x", addr);
   return 0;
}

u32 core::read7_io(u32 addr, u8 sz, u8 access, bool has_effect) const
{
    u32 v = read7_io8(addr, sz, access, has_effect);
    if (sz >= 2) v |= read7_io8(addr+1, sz, access, has_effect) << 8;
    if (sz == 4) {
        v |= read7_io8(addr+2, sz, access, has_effect) << 16;
        v |= read7_io8(addr+3, sz, access, has_effect) << 24;
    }
    return v;
}

u32 core::read9_io(u32 addr, u8 sz, u8 access, bool has_effect)
{
    u32 v = read9_io8(addr, sz, access, has_effect);
    if (sz >= 2) v |= read9_io8(addr+1, sz, access, has_effect) << 8;
    if (sz == 4) {
        v |= read9_io8(addr+2, sz, access, has_effect) << 16;
        v |= read9_io8(addr+3, sz, access, has_effect) << 24;
    }
    return v;
}

void core::write7_io(u32 addr, u8 sz, u8 access, u32 val)
{
    write7_io8(addr, sz, access, val & 0xFF);
    if (sz >= 2) write7_io8(addr+1, sz, access, (val >> 8) & 0xFF);
    if (sz == 4) {
        write7_io8(addr+2, sz, access, (val >> 16) & 0xFF);
        write7_io8(addr+3, sz, access, (val >> 24) & 0xFF);
    }
}
}