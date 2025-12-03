//
// Created by . on 12/4/24.
//

#include <cassert>
#include <cstdio>
#include <cstring>

#include "helpers/debug.h"
#include "gba_bus.h"
#include "helpers/color.h"
#include "helpers/multisize_memaccess.cpp"

#include "gba_ppu.h"
#include "gba_debugger.h"

namespace GBA::PPU {

#define MASTER_CYCLES_PER_SCANLINE 1232
#define HBLANK_CYCLES 226
#define MASTER_CYCLES_BEFORE_HBLANK (MASTER_CYCLES_PER_SCANLINE - HBLANK_CYCLES)
#define MASTER_CYCLES_PER_FRAME (228 * MASTER_CYCLES_PER_SCANLINE)
#define SCANLINE_HBLANK 1006

static constexpr u32 maskalign[5] = {0, 0xFFFFFFFF, 0xFFFFFFFE, 0, 0xFFFFFFFC};


core::core(GBA::core *parent) : gba(parent) {
}

void core::reset()
{
    mosaic.bg.hsize = mosaic.bg.vsize = 1;
    mosaic.obj.hsize = mosaic.obj.vsize = 1;
    mbg[2].pa = 1 << 8; mbg[2].pd = 1 << 8;
    mbg[3].pa = 1 << 8; mbg[3].pd = 1 << 8;
}

void core::vblank(void *ptr, u64 val, u64 clock, u32 jitter)
{
    //pprint_palette_ram();
    auto *gba = static_cast<GBA::core *>(ptr);
    if (val) {
        gba->clock.ppu.vblank_active = true;
        gba->check_dma_at_vblank();
        const u32 old_IF = gba->io.IF;
        gba->io.IF |= gba->ppu.io.vblank_irq_enable && val;
        if (old_IF != gba->io.IF) {
            // TODO: reset this
            //DBG_EVENT(DBG_GBA_EVENT_SET_VBLANK_IRQ);
            gba->eval_irqs();
        }
    }
    else {
        gba->clock.ppu.vblank_active = false;
    }
}

#define OUT_WIDTH 240

static void get_obj_tile_size(u8 sz, u32 shape, u32 *htiles, u32 *vtiles)
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
        default:
    }
    printf("\nHEY! INVALID SHAPE %d", shape);
    *htiles = 1;
    *vtiles = 1;
#undef T
}

// Thanks tonc!
static u32 se_index_fast(const u32 tx, const u32 ty, u32 bgcnt) {
    u32 n = tx + ty * 32;
    if (tx >= 32)
        n += 0x03E0;
    if (ty >= 32 && (bgcnt == 3))
        n += 0x0400;
    return n;
}

void core::get_affine_bg_pixel(u32 bgnum, const BG &bg, const i32 px, const i32 py, PX *opx)
{
    // Now px and py represent a number inside 0...hpixels and 0...vpixels
    const u32 block_x = px >> 3;
    const u32 block_y = py >> 3;
    const u32 line_in_tile = py & 7;

    const u32 tile_width = bg.htiles;

    const u32 screenblock_addr = bg.screen_base_block + block_x + (block_y * tile_width);
    const u32 tile_num = static_cast<u8 *>(VRAM)[screenblock_addr];

    // so now, grab that tile...
    const u32 tile_start_addr = bg.character_base_block + (tile_num * 64);
    const u32 line_start_addr = tile_start_addr + (line_in_tile * 8);
    const u8 *ptr = static_cast<u8 *>(VRAM) + line_start_addr;
    const u8 color = ptr[px & 7];
    if (color != 0) {
        opx->has = true;
        opx->color = palette_RAM[color];
        opx->priority = bg.priority;
    }
    else {
        opx->has = false;
    }
}

// get color from (px,py)
void core::get_affine_sprite_pixel(u32 mode, i32 px, i32 py, u32 tile_num, u32 htiles, u32 vtiles,  bool bpp8, u32 palette, u32 priority, u32 obj_mapping_1d, u32 dsize, i32 screen_x, u32 blended, PPU::window *w)
{
    i32 hpixels = static_cast<i32>(htiles * 8);
    i32 vpixels = static_cast<i32>(vtiles * 8);
    if (dsize) {
        hpixels >>= 1;
        vpixels >>= 1;
    }
    px += (hpixels >> 1);
    py += (vpixels >> 1);
    if ((px < 0) || (py < 0)) return;
    if ((px >= hpixels) || (py >= vpixels)) return;

    const u32 block_x = px >> 3;
    const u32 block_y = py >> 3;
    const u32 tile_x = px & 7;
    const u32 tile_y = py & 7;
    if (obj_mapping_1d) {
        //1D!
        tile_num += (((htiles >> dsize) << bpp8) * block_y);
    }
    else {
        tile_num += block_y << 5;
    }
    tile_num += block_x << bpp8;
    tile_num &= 0x3FF;
    if (bpp8) tile_num &= 0x3FE;

    const u32 tile_base_addr = 0x10000 + (32 * tile_num);
    const u32 tile_line_addr = tile_base_addr + (tile_y << (2 + bpp8));
    const u32 tile_px_addr = bpp8 ? (tile_line_addr + tile_x) : (tile_line_addr + (tile_x >> 1));
    u8 c = VRAM[tile_px_addr];

    if (!bpp8) {
        if (tile_x & 1) c >>= 4;
        c &= 15;
    }
    else palette = 0x100;

    if (c != 0) {
        switch(mode) {
            case 1:
            case 0: {
                PX *opx = &obj.line[screen_x];
                if (!opx->has) {
                    opx->has = true;
                    opx->priority = priority;
                    opx->color = palette_RAM[c + palette];
                    opx->translucent_sprite = blended;
                }
                break; }
            case 2: {
                w->inside[screen_x] = true;
            break; }
            default:
        }
    }
}

u32 core::get_sprite_tile_addr(u32 tile_num, u32 htiles, u32 block_y, u32 line_in_tile, bool bpp8, u32 d1) const
{
    if (io.obj_mapping_1d) {
        tile_num += ((htiles << bpp8) * block_y);
    }
    else {
        tile_num += block_y << 5;
    }

    // Advance by block x. NOT!
    //tile_num += block_x << bpp8;

    if (bpp8) tile_num &= 0x3FE;
    tile_num &= 0x3FF;

    // Now get pointer to that tile
    u32 tile_base_addr = 0x10000 + (32 * tile_num);
    u32 tile_line_addr = tile_base_addr + (line_in_tile << (2 + bpp8));
    return tile_line_addr;
}

void core::draw_sprite_affine(const u16 *ptr, PPU::window *w, u32 num)
{
    obj.drawing_cycles -= 1;
    if (obj.drawing_cycles < 1) return;
    bool m_mosaic = (ptr[0] >> 12) & 1;
    const i32 my_y = static_cast<i32>(m_mosaic ? mosaic.obj.y_current : gba->clock.ppu.y);

    const u32 dsize = (ptr[0] >> 9) & 1;
    u32 htiles, vtiles;
    const u32 shape = (ptr[0] >> 14) & 3; // 0 = square, 1 = horiozontal, 2 = vertical
    const u32 sz = (ptr[1] >> 14) & 3;
    get_obj_tile_size(sz, shape, &htiles, &vtiles);
    if (dsize) {
        htiles *= 2;
        vtiles *= 2;
    }

    i32 y_min = ptr[0] & 0xFF;
    const i32 y_max = (y_min + (static_cast<i32>(vtiles) * 8)) & 255;
    if(y_max < y_min)
        y_min -= 256;
    if(my_y < y_min || my_y >= y_max) {
        return;
    }

    const u32 mode = (ptr[0] >> 10) & 3;
    const u32 blended = mode == 1;

    const u32 tile_num = ptr[2] & 0x3FF;
    const bool bpp8 = (ptr[0] >> 13) & 1;
    u32 x = ptr[1] & 0x1FF;
    x = SIGNe9to32(x);
    const u32 pgroup = (ptr[1] >> 9) & 31;
    const u32 pbase = (pgroup * 0x20) >> 1;
    const u16 *pbase_ptr = reinterpret_cast<u16 *>(OAM) + pbase;
    const i32 pa = SIGNe16to32(pbase_ptr[3]);
    const i32 pb = SIGNe16to32(pbase_ptr[7]);
    const i32 pc = SIGNe16to32(pbase_ptr[11]);
    const i32 pd = SIGNe16to32(pbase_ptr[15]);
    obj.drawing_cycles -= 9;
    if (obj.drawing_cycles < 0) return;

    const u32 priority = (ptr[2] >> 10) & 3;
    const u32 palette = bpp8 ? 0x100 : (0x100 + (((ptr[2] >> 12) & 15) << 4));

    const i32 line_in_sprite = my_y - y_min;
    //i32 x_origin = x - (htiles << 2);

    i32 screen_x = static_cast<i32>(x);
    const i32 iy = static_cast<i32>(line_in_sprite) - (static_cast<i32>(vtiles) * 4);
    const i32 hwidth = static_cast<i32>(htiles) * 4;
    for (i32 ix=-hwidth; ix < hwidth; ix++) {
        const i32 px = (pa*ix + pb*iy)>>8;    // get x texture coordinate
        const i32 py = (pc*ix + pd*iy)>>8;    // get y texture coordinate
        if ((screen_x >= 0) && (screen_x < 240))
            get_affine_sprite_pixel(mode, px, py, tile_num, htiles, vtiles, bpp8, palette, priority,
                                    io.obj_mapping_1d, dsize, screen_x, blended, w);   // get color from (px,py)
        screen_x++;
        obj.drawing_cycles -= 2;
        if (obj.drawing_cycles < 0) return;
    }
}

void core::output_sprite_8bpp(const u8 *tptr, const u32 mode, const i32 screen_x, const u32 priority, const bool hflip, const bool m_mosaic, PPU::window *w) {
    for (i32 tile_x = 0; tile_x < 8; tile_x++) {
        i32 sx;
        if (hflip) sx = (7 - tile_x) + screen_x;
        else sx = tile_x + screen_x;
        if ((sx >= 0) && (sx < 240)) {
            obj.drawing_cycles -= 1;
            PX *opx = &obj.line[sx];
            if ((mode > 1) || (!opx->has) || (priority < opx->priority)) {
                const u8 c = tptr[tile_x];
                switch (mode) {
                    case 1:
                    case 0:
                        if (c != 0) {
                            opx->has = true;
                            opx->color = palette_RAM[0x100 + c];
                            opx->translucent_sprite = mode;
                        }
                        opx->priority = priority;
                        opx->mosaic_sprite = m_mosaic;
                        break;
                    case 2: { // OBJ window
                        if (c != 0) w->inside[sx] = true;
                        break;
                    }
                    default:
                }
            }
        }
        if (obj.drawing_cycles < 1) return;
    }
}


void core::output_sprite_4bpp(const u8 *tptr, const u32 mode, const i32 screen_x, const u32 priority, const bool hflip, const u32 palette, const bool m_mosaic, PPU::window *w) {
    for (i32 tile_x = 0; tile_x < 4; tile_x++) {
        i32 sx;
        if (hflip) sx = (7 - (tile_x * 2)) + screen_x;
        else sx = (tile_x * 2) + screen_x;
        u8 data = tptr[tile_x];
        for (u32 i = 0; i < 2; i++) {
            obj.drawing_cycles -= 1;
            if ((sx >= 0) && (sx < 240)) {
                PX *opx = &obj.line[sx];
                u32 c = data & 15;
                data >>= 4;
                if ((mode > 1) || (!opx->has) || (priority < opx->priority)) {
                    switch (mode) {
                        case 1:
                        case 0: {
                            if (c != 0) {
                                opx->has = true;
                                opx->color = palette_RAM[c + palette];
                                opx->translucent_sprite = mode;
                            }
                            opx->priority = priority;
                            opx->mosaic_sprite = m_mosaic;
                            break;
                        }
                        case 2: {
                            if (c != 0) w->inside[sx] = true;
                            break;
                        }
                        default:
                    }
                }
            }
            if (obj.drawing_cycles < 1) return;
            if (hflip) sx--;
            else sx++;
        }
    }
}

void core::draw_sprite_normal(const u16 *ptr, PPU::window *w)
{
    // 1 cycle to evaluate and 1 cycle per pixel
    obj.drawing_cycles -= 1;
    if (obj.drawing_cycles < 1) return;

    const bool obj_disable = (ptr[0] >> 9) & 1;
    if (obj_disable) return;

    const u32 shape = (ptr[0] >> 14) & 3; // 0 = square, 1 = horiozontal, 2 = vertical
    const u32 sz = (ptr[1] >> 14) & 3;
    u32 htiles, vtiles;
    get_obj_tile_size(sz, shape, &htiles, &vtiles);

    i32 y_min = ptr[0] & 0xFF;
    const i32 y_max = (y_min + (static_cast<i32>(vtiles) * 8)) & 255;
    if(y_max < y_min)
        y_min -= 256;

    const bool m_mosaic = (ptr[0] >> 12) & 1;
    const i32 my_y = static_cast<i32>(m_mosaic ? mosaic.obj.y_current : gba->clock.ppu.y);

    if(my_y < y_min || my_y >= y_max) {
        return;
    }

    // Clip sprite

    u32 tile_num = ptr[2] & 0x3FF;

    const u32 mode = (ptr[0] >> 10) & 3;
    const bool bpp8 = (ptr[0] >> 13) & 1;
    u32 x = ptr[1] & 0x1FF;
    x = SIGNe9to32(x);
    const bool hflip = (ptr[1] >> 12) & 1;
    const bool vflip = (ptr[1] >> 13) & 1;

    const u32 priority = (ptr[2] >> 10) & 3;
    const u32 palette = bpp8 ? 0 : (0x100 + (((ptr[2] >> 12) & 15) << 4));
    if (bpp8) tile_num &= 0x3FE;

    // OK we got all the attributes. Let's draw it!
    i32 line_in_sprite = my_y - y_min;
    //printf("\nPPU LINE %d LINE IN SPR:%d Y:%d", gba->clock.ppu.y, line_in_sprite, y);
    if (vflip) line_in_sprite = static_cast<i32>((((vtiles * 8) - 1) - line_in_sprite));
    const u32 tile_y_in_sprite = line_in_sprite >> 3; // /8
    const u32 line_in_tile = line_in_sprite & 7;

    // OK so we know which line
    // We have two possibilities; 1d or 2d layout
    u32 tile_addr = get_sprite_tile_addr(tile_num, htiles, tile_y_in_sprite, line_in_tile, bpp8, io.obj_mapping_1d);
    if (hflip) tile_addr += (htiles - 1) * (32 << bpp8);

    i32 screen_x = static_cast<i32>(x);
    for (u32 tile_xs = 0; tile_xs < htiles; tile_xs++) {
        const u8 *tptr = static_cast<u8 *>(VRAM) + tile_addr;
        if (hflip) tile_addr -= (32 << bpp8);
        else tile_addr += (32 << bpp8);
        if (bpp8) output_sprite_8bpp(tptr, mode, screen_x, priority, hflip, m_mosaic, w);
        else output_sprite_4bpp(tptr, mode, screen_x, priority, hflip, palette, m_mosaic, w);
        screen_x += 8;
        if (obj.drawing_cycles < 1) return;
    }
}


void core::draw_obj_line()
{
    obj.drawing_cycles = io.hblank_free ? 954 : 1210;

    memset(obj.line, 0, sizeof(obj.line));
    PPU::window *w = &window[WINOBJ];
    memset(&w->inside, 0, sizeof(w->inside));

    if (!obj.enable) return;
    // Each OBJ takes:
    // n*1 cycles per pixel
    // 10 + n*2 per pixel if rotated/scaled
    for (u32 i = 0; i < 128; i++) {
        const u16 *ptr = reinterpret_cast<u16 *>(OAM) + (i * 4);
        const u32 affine = (ptr[0] >> 8) & 1;

        if (affine) draw_sprite_affine(ptr, w, i);
        else draw_sprite_normal(ptr, w);
    }
}

void core::fetch_bg_slice(const BG &bg, const u32 block_x, const u32 vpos, PX px[], const u32 screen_x)
{
    const u32 block_y = vpos >> 3;
    const u32 screenblock_addr = bg.screen_base_block + (se_index_fast(block_x, block_y, bg.screen_size) << 1);

    const u16 attr = *reinterpret_cast<u16 *>(static_cast<u8 *>(VRAM) + screenblock_addr);
    const u32 tile_num = attr & 0x3FF;
    const bool hflip = (attr >> 10) & 1;
    const bool vflip = (attr >> 11) & 1;
    const u32 palbank = bg.bpp8 ? 0 : ((attr >> 12) & 15) << 4;

    u32 line_in_tile = vpos & 7;
    if (vflip) line_in_tile = 7 - line_in_tile;

    const u32 tile_bytes = bg.bpp8 ? 64 : 32;
    const u32 line_size = bg.bpp8 ? 8 : 4;
    const u32 tile_start_addr = bg.character_base_block + (tile_num * tile_bytes);
    const u32 line_addr = tile_start_addr + (line_in_tile * line_size);
    if (line_addr >= 0x10000) return; // hardware doesn't draw from up there
    const u8 *ptr = static_cast<u8 *>(VRAM) + line_addr;

    if (bg.bpp8) {
        u32 mx = hflip ? 7 : 0;
        for (u32 ex = 0; ex < 8; ex++) {
            const u8 data = ptr[mx];
            PX *p = &px[ex];
            if ((screen_x + mx) < 240) {
                if (data != 0) {
                    p->has = true;
                    p->color = palette_RAM[data];
                    p->priority = bg.priority;
                } else
                    p->has = false;
            }
            if (hflip) mx--;
            else mx++;
        }
    }
    else {
        u32 mx = 0;
        if (hflip) mx = 7;
        for (u32 ex = 0; ex < 4; ex++) {
            u8 data = *ptr;
            ptr++;
            for (u32 i = 0; i < 2; i++) {
                u16 c = data & 15;
                data >>= 4;
                PX *p = &px[mx];
                if ((screen_x + mx) < 240) {
                    if (c != 0) {
                        p->has = true;
                        p->color = palette_RAM[c + palbank];
                        p->priority = bg.priority;
                    } else
                        p->has = false;
                }
                if (hflip) mx--;
                else mx++;
            }
        }
    }
}

void core::apply_mosaic()
{
    // This function updates vertical mosaics, and applies horizontal.
    if (mosaic.bg.y_counter == 0) {
        mosaic.bg.y_current = gba->clock.ppu.y;
    }
    for (auto &bg : mbg) {
        if (!bg.enable) continue;
        if (bg.mosaic_enable) bg.mosaic_y = mosaic.bg.y_current;
        else bg.mosaic_y = gba->clock.ppu.y + 1;
    }
    mosaic.bg.y_counter = (mosaic.bg.y_counter + 1) % mosaic.bg.vsize;

    if (mosaic.obj.y_counter == 0) {
        mosaic.obj.y_current = gba->clock.ppu.y;
    }
    mosaic.obj.y_counter = (mosaic.obj.y_counter + 1) % mosaic.obj.vsize;


    // Now do horizontal blend
    for (auto & bg : mbg) {
        if (!bg.enable || !bg.mosaic_enable) continue;
        u32 mosaic_counter = 0;
        PX *src;
        for (u32 x = 0; x < 240; x++) {
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
    for (auto & x : obj.line) {
        if (!x.mosaic_sprite || !src->mosaic_sprite || (mosaic_counter == 0))  {
            src = &x;
        }
        else {
            x.has = src->has;
            x.color = src->color;
            x.priority = src->priority;
            x.translucent_sprite = src->translucent_sprite;
            x.mosaic_sprite = src->mosaic_sprite;
        }
        mosaic_counter = (mosaic_counter + 1) % mosaic.obj.hsize;
    }
}

void core::calculate_windows_vflag()
{
    for (u32 wn = 0; wn < 2; wn++) {
        PPU::window *w = &window[wn];
        if  (gba->clock.ppu.y == w->top) w->v_flag = true;
        if  (gba->clock.ppu.y == w->bottom) w->v_flag = false;
    }
}

void core::calculate_windows(bool in_vblank)
{
    //if (!force && !window[0].enable && !window[1].enable && !window[WINOBJ].enable) return;

    // Calculate windows...
    calculate_windows_vflag();
    if  (gba->clock.ppu.y >= 160) return;
    for (u32 wn = 0; wn < 2; wn++) {
        PPU::window *w = &window[wn];
        if (!w->enable) {
            memset(&w->inside, 0, sizeof(w->inside));
            continue;
        }

        for (u32 x = 0; x < 240; x++) {
            if (x == w->left) w->h_flag = true;
            if (x == w->right) w->h_flag = false;

            w->inside[x] =  w->h_flag & w->v_flag;
        }
    }

    // Now take care of the outside window
    const PPU::window *w0 = &window[WIN0];
    const PPU::window *w1 = &window[WIN1];
    const PPU::window *ws = &window[WINOBJ];
    const u32 w0r = w0->enable;
    const u32 w1r = w1->enable;
    const u32 wsr = ws->enable;
    PPU::window *w = &window[WINOUTSIDE];
    memset(w->inside, 0, sizeof(w->inside));
    for (u32 x = 0; x < 240; x++) {
        const u32 w0i = w0r & w0->inside[x];
        const u32 w1i = w1r & w1->inside[x];
        const u32 wsi = wsr & ws->inside[x];
        w->inside[x] = !(w0i || w1i || wsi);
    }

    PPU::window *wo = &window[WINOUTSIDE];
    auto *l = &gba->dbg_info.line[gba->clock.ppu.y];
    for (u32 x = 0; x < 240; x++) {
        l->window_coverage[x] = w0->inside[x] | (w1->inside[x] << 1) | (ws->inside[x] << 2) | (wo->inside[x] << 3);
    }
}

PPU::window *core::get_active_window(u32 x)
{
    PPU::window *active_window = nullptr;
    if (window[WIN0].enable || window[WIN1].enable || window[WINOBJ].enable) {
        active_window = &window[WINOUTSIDE];
        if (window[WINOBJ].enable && window[WINOBJ].inside[x]) active_window = &window[WINOBJ];
        if (window[WIN1].enable && window[WIN1].inside[x]) active_window = &window[WIN1];
        if (window[WIN0].enable && window[WIN0].inside[x]) active_window = &window[WIN0];
    }
    return active_window;
}

#define GBACTIVE_OBJ 0
#define GBACTIVE_BG0 1
#define GBACTIVE_BG1 2
#define GBACTIVE_BG3 3
#define GBACTIVE_BG4 4
#define GBACTIVE_SFX 5

void core::affine_line_start(BG &bg, i32 *fx, i32 *fy) const
{
    if (!bg.enable) {
        if (bg.mosaic_y != bg.last_y_rendered) {
            bg.x_lerp += bg.pb * static_cast<i32>(mosaic.bg.vsize);
            bg.y_lerp += bg.pd * static_cast<i32>(mosaic.bg.vsize);
            bg.last_y_rendered = bg.mosaic_y;
        }
        return;
    }
    *fx = bg.x_lerp;
    *fy = bg.y_lerp;
}

void core::affine_line_end(BG &bg) const
{
    if (bg.mosaic_y != bg.last_y_rendered) {
        bg.x_lerp += bg.pb * static_cast<i32>(mosaic.bg.vsize);
        bg.y_lerp += bg.pd * static_cast<i32>(mosaic.bg.vsize);
        bg.last_y_rendered = bg.mosaic_y;
    }
}

void core::draw_bg_line_affine(u32 bgnum)
{
    BG &bg = mbg[bgnum];
    memset(bg.line, 0, sizeof(bg.line));

    i32 fx, fy;
    affine_line_start(bg, &fx, &fy);
    if (!bg.enable) return;

    auto *dtl = &gba->dbg_info.bg_scrolls[bgnum];

    for (i64 screen_x = 0; screen_x < 240; screen_x++) {
        i32 px = fx >> 8;
        i32 py = fy >> 8;
        fx += bg.pa;
        fy += bg.pc;
        if (bg.display_overflow) { // wraparound
            px &= static_cast<i32>(bg.hpixels_mask);
            py &= static_cast<i32>(bg.vpixels_mask);
        }
        else { // clip/transparent
            if (px < 0) continue;
            if (py < 0) continue;
            if (px >= bg.hpixels) continue;
            if (py >= bg.vpixels) continue;
        }
        get_affine_bg_pixel(bgnum, bg, px, py, &bg.line[screen_x]);
        dtl->lines[(py << 7) + (px >> 3)] |= 1 << (px & 7);
    }
    affine_line_end(bg);
}

void core::draw_bg_line_normal(u32 bgnum)
{
    BG &bg = mbg[bgnum];
    memset(bg.line, 0, sizeof(bg.line));
    if (!bg.enable) return;
    // first do a fetch for fine scroll -1
    u32 hpos = bg.hscroll & bg.hpixels_mask;
    const u32 vpos = (bg.vscroll + bg.mosaic_y) & bg.vpixels_mask;
    const u32 fine_x = hpos & 7;
    u32 screen_x = 0;
    PX bgpx[8];
    //hpos = ((hpos >> 3) - 1) << 3;
    fetch_bg_slice(bg, hpos >> 3, vpos, bgpx, 0);
    auto *dbgl = &gba->dbg_info.line[gba->clock.ppu.y];
    // TODO HERE
    u8 *scroll_line = &gba->dbg_info.bg_scrolls[bgnum].lines[((bg.vscroll + gba->clock.ppu.y) & bg.vpixels_mask) * 128];
    u32 startx = fine_x;
    for (u32 i = startx; i < 8; i++) {
        bg.line[screen_x].color = bgpx[i].color;
        bg.line[screen_x].has = bgpx[i].has;
        bg.line[screen_x].priority = bgpx[i].priority;
        screen_x++;
        hpos = (hpos + 1) & bg.hpixels_mask;
        scroll_line[hpos >> 3] |= 1 << (hpos & 7);
    }

    while (screen_x < 240) {
        fetch_bg_slice(bg, hpos >> 3, vpos, &bg.line[screen_x], screen_x);
        if (screen_x <= 232) {
            scroll_line[hpos >> 3] = 0xFF;
        }
        else {
            for (u32 rx = screen_x; rx < 240; rx++) {
                scroll_line[hpos >> 3] |= 1 << (rx - screen_x);
            }
        }
        screen_x += 8;
        hpos = (hpos + 8) & bg.hpixels_mask;
    }
}

static void find_targets_and_priorities(const bool bg_enables[], PX *layers[], u32 *layer_a_out, u32 *layer_b_out, const bool *actives)
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

void core::output_pixel(u32 x, bool obj_enable, bool bg_enables[4], u16 *line_output) {
    // Find which window applies to us.
    bool default_active[6] = {true, true, true, true, true, true}; // Default to active if no window.

    bool *actives = default_active;
    PPU::window *active_window = get_active_window(x);
    if (active_window) actives = active_window->active;

    PX *sp_px = &obj.line[x];
    PX empty_px = {.color=palette_RAM[0], .priority=4, .has=true, .translucent_sprite=false};
    sp_px->has &= obj_enable;

    PX *layers[6] = {
            sp_px,
            &mbg[0].line[x],
            &mbg[1].line[x],
            &mbg[2].line[x],
            &mbg[3].line[x],
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
    const u32 mode = blend.mode;

    /* Find targets A and B */
    u32 target_a_layer, target_b_layer;
    find_targets_and_priorities(obg_enables, layers, &target_a_layer, &target_b_layer, actives);

    // Blending ONLY happens if the topmost pixel is a valid A target and the next-topmost is a valid B target
    // Brighten/darken only happen if topmost pixel is a valid A target

    const PX *target_a = layers[target_a_layer];
    const PX *target_b = layers[target_b_layer];

    const u32 blend_b = target_b->color;

    u32 output_color = target_a->color;
    if (actives[GBACTIVE_SFX] || (target_a->has && target_a->translucent_sprite &&
                                  blend.targets_b[target_b_layer])) { // backdrop is contained in active for the highest-priority window OR above is a semi-transparent sprite & below is a valid target
        if (target_a->has && target_a->translucent_sprite &&
            blend.targets_b[target_b_layer]) { // above is semi-transparent sprite and below is a valid target
            output_color = gba_alpha(output_color, blend_b, blend.use_eva_a, blend.use_eva_b);
        } else if (mode == 1 && blend.targets_a[target_a_layer] &&
                   blend.targets_b[target_b_layer]) { // mode == 1, both are valid
            output_color = gba_alpha(output_color, blend_b, blend.use_eva_a, blend.use_eva_b);
        } else if (mode == 2 && blend.targets_a[target_a_layer]) { // mode = 2, A is valid
            output_color = gba_brighten(output_color, static_cast<i32>(blend.use_bldy));
        } else if (mode == 3 && blend.targets_a[target_a_layer]) { // mode = 3, B is valid
            output_color = gba_darken(output_color, static_cast<i32>(blend.use_bldy));
        }
    }

    line_output[x] = output_color;
}

void core::draw_line0(u16 *line_output)
{
    draw_obj_line();
    draw_bg_line_normal(0);
    draw_bg_line_normal(1);
    draw_bg_line_normal(2);
    draw_bg_line_normal(3);
    apply_mosaic();
    calculate_windows(false);
    bool bg_enables[4] = {mbg[0].enable, mbg[1].enable, mbg[2].enable, mbg[3].enable};
    for (u32 x = 0; x < 240; x++) {
        output_pixel(x, obj.enable, &bg_enables[0], line_output);
    }
}

void core::draw_line1(u16 *line_output)
{
    draw_obj_line();
    draw_bg_line_normal(0);
    draw_bg_line_normal(1);
    draw_bg_line_affine(2);
    apply_mosaic();
    memset(&mbg[3].line, 0, sizeof(mbg[3].line));

    calculate_windows(false);
    bool bg_enables[4] = {mbg[0].enable, mbg[1].enable, mbg[2].enable, false};
    for (u32 x = 0; x < 240; x++) {
        output_pixel(x, obj.enable, &bg_enables[0], line_output);
    }
}

void core::draw_line2(u16 *line_output)
{
    draw_obj_line();
    memset(&mbg[0].line, 0, sizeof(mbg[0].line));
    memset(&mbg[1].line, 0, sizeof(mbg[1].line));
    draw_bg_line_affine(2);
    draw_bg_line_affine(3);
    calculate_windows(false);
    apply_mosaic();
    bool bg_enables[4] = {false, false, mbg[2].enable, mbg[3].enable};
    for (u32 x = 0; x < 240; x++) {
        output_pixel(x, obj.enable, &bg_enables[0], line_output);
    }
}

void core::draw_bg_line3()
{
    BG &bg = mbg[2];
    memset(bg.line, 0, sizeof(bg.line));

    i32 fx, fy;
    affine_line_start(bg, &fx, &fy);
    if (!bg.enable) return;
    for (u32 x = 0; x < 240; x++) {
        const i32 tx = fx >> 8;
        const i32 ty = fy >> 8;
        fx += bg.pa;
        fy += bg.pc;
        if ((tx < 0) || (ty < 0) || (tx >= 240) || (ty >= 160)) continue;

        const u16 *line_input = reinterpret_cast<u16 *>(VRAM) + (ty * 240);
        bg.line[x].color = line_input[tx];
        bg.line[x].has = (*line_input) != 0;
        bg.line[x].priority = bg.priority;
    }

    affine_line_end(bg);
}

void core::draw_bg_line4()
{
    BG &bg = mbg[2];
    memset(bg.line, 0, sizeof(bg.line));

    i32 fx, fy;
    affine_line_start(bg, &fx, &fy);
    if (!bg.enable) return;


    assert(io.frame < 2);
    const u32 base_addr = 0xA000 * io.frame;
    //if  (gba->clock.ppu.y == 50) printf("\nF:%lld L:%d BIT:%d", gba->clock.master_frame, gba->clock.ppu.y, io.frame);
    PX *px = &bg.line[0];

    for (u32 x = 0; x < 240; x++) {
        const i32 tx = fx >> 8;
        const i32 ty = fy >> 8;
        fx += bg.pa;
        fy += bg.pc;
        if ((tx < 0) || (ty < 0) || (tx >= 240) || (ty >= 160)) continue;

        auto *line_input = static_cast<u8 *>(VRAM) + base_addr + (ty * 240);
        const u32 color = palette_RAM[line_input[tx]];
        px->has = line_input[tx] != 0;
        px->priority = bg.priority;
        px->color = color;

        px++;
    }
    affine_line_end(bg);
}

void core::draw_bg_line5()
{
    BG &bg = mbg[2];
    memset(bg.line, 0, sizeof(bg.line));

    i32 fx, fy;
    affine_line_start(bg, &fx, &fy);
    if (!bg.enable) return;

    u32 base_addr = 0xA000 * io.frame;
    PX *px = &bg.line[0];
    for (u32 x = 0; x < 240; x++) {
        const i32 tx = fx >> 8;
        const i32 ty = fy >> 8;
        fx += bg.pa;
        fy += bg.pc;
        if ((tx < 0) || (ty < 0) || (tx >= 160) || (ty >= 128)) continue;

        auto *line_input = reinterpret_cast<u16 *>(static_cast<u8 *>(VRAM) + base_addr + (ty * 320));
        px->color = palette_RAM[line_input[x]];
        px->has = px->color != 0;
        px->priority = bg.priority;

        px++;
    }
    affine_line_end(bg);
}

void core::draw_line3(u16 *line_output)
{
    draw_obj_line();
    memset(&mbg[0].line, 0, sizeof(mbg[0].line));
    memset(&mbg[1].line, 0, sizeof(mbg[1].line));
    draw_bg_line3();
    memset(&mbg[3].line, 0, sizeof(mbg[3].line));
    calculate_windows(false);
    apply_mosaic();

    bool bg_enables[4] = {false, false, mbg[2].enable, false};
    for (u32 x = 0; x < 240; x++) {
        output_pixel(x, obj.enable, &bg_enables[0], line_output);
    }
}

void core::draw_line4(u16 *line_output)
{
    draw_obj_line();
    memset(&mbg[0].line, 0, sizeof(mbg[0].line));
    memset(&mbg[1].line, 0, sizeof(mbg[1].line));
    draw_bg_line4();
    memset(&mbg[3].line, 0, sizeof(mbg[3].line));
    calculate_windows(false);
    apply_mosaic();

    bool bg_enables[4] = {false, false, mbg[2].enable, false};
    for (u32 x = 0; x < 240; x++) {
        output_pixel(x, obj.enable, &bg_enables[0], line_output);
    }
}

void core::draw_line5(u16 *line_output)
{
    draw_obj_line();
    memset(&mbg[0].line, 0, sizeof(mbg[0].line));
    memset(&mbg[1].line, 0, sizeof(mbg[1].line));
    draw_bg_line5();
    memset(&mbg[3].line, 0, sizeof(mbg[3].line));
    calculate_windows(false);
    apply_mosaic();

    bool bg_enables[4] = {false, false, mbg[2].enable, false};
    for (u32 x = 0; x < 240; x++) {
        output_pixel(x, obj.enable, &bg_enables[0], line_output);
    }
}

void core::hblank(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *ppu = static_cast<core *>(ptr);
    ppu->do_hblank(key);
}

void core::do_hblank(bool in_hblank) {
    gba->clock.ppu.hblank_active = in_hblank;

    if (in_hblank) { // mid-scanline
        if  (gba->clock.ppu.y < 160) {
            // Copy debug data
            auto *l = &gba->dbg_info.line[gba->clock.ppu.y];
            l->bg_mode = io.bg_mode;
            for (u32 bgn = 0; bgn < 4; bgn++) {
                BG &bg = mbg[bgn];
                auto *mdbg = &l->bg[bgn];
                mdbg->htiles = bg.htiles;
                mdbg->vtiles = bg.vtiles;
                mdbg->hscroll = bg.hscroll;
                mdbg->vscroll = bg.vscroll;
                mdbg->display_overflow = bg.display_overflow;
                mdbg->screen_base_block = bg.screen_base_block;
                mdbg->character_base_block = bg.character_base_block;
                mdbg->bpp8 = bg.bpp8;
                mdbg->priority = bg.priority;
                if (bgn >= 2) {
                    mdbg->hpos = bg.x;
                    mdbg->vpos = bg.y;
                    mdbg->pa = bg.pa;
                    mdbg->pb = bg.pb;
                    mdbg->pc = bg.pc;
                    mdbg->pd = bg.pd;
                }
            }

            // Actually draw the line
            u16 *line_output = cur_output +  (gba->clock.ppu.y * OUT_WIDTH);
            if (io.force_blank) {
                memset(line_output, 0xFF, 480);
                return;
            }
            memset(line_output, 0, 480);
            switch (io.bg_mode) {
                case 0:
                    draw_line0(line_output);
                    break;
                case 1:
                    draw_line1(line_output);
                    break;
                case 2:
                    draw_line2(line_output);
                    break;
                case 3:
                    draw_line3(line_output);
                    break;
                case 4:
                    draw_line4(line_output);
                    break;
                case 5:
                    draw_line5(line_output);
                    break;
                default:
                    assert(1 == 2);
            }
        } else {
            calculate_windows_vflag();
        }

        // It's cleared at cycle 0 and set at cycle 1007
        u32 old_IF = gba->io.IF;
        gba->io.IF |= io.hblank_irq_enable << 1;
        if (old_IF != gba->io.IF) {
            gba->eval_irqs(); // TODO: fix DBG_EVENT again...
            DBG_EVENT(DBG_GBA_EVENT_SET_HBLANK_IRQ);
        }

        // Check if we have any DMA transfers that need to go...
        gba->check_dma_at_hblank();
    }
    else { // end of scanline/new scanline
        // GBA_PPU_finish_scanline()
        u32 old_IF = gba->io.IF;
        gba->io.IF |= ((io.vcount_at == gba->clock.ppu.y) && io.vcount_irq_enable) << 2;
        if (old_IF != gba->io.IF) {
            //DBG_EVENT(DBG_GBA_EVENT_SET_LINECOUNT_IRQ); // TODO: this!
            gba->eval_irqs();
        }

        gba->clock.ppu.y++;
        if (gba->dbg.events.view.vec) {
            debugger_report_line(gba->dbg.interface, static_cast<i32>(gba->clock.ppu.y));
        }

        // ==160 check DMA at vblank
        // == 227, vblank_active = 0
        // == 228, new_frame() goes here. whcih would set y==0
        // THEN
        //  hblank(0)

        gba->clock.ppu.scanline_start = gba->clock.master_cycle_count;
        // IF Y == 0 reset mosaics goes here
        if  (gba->clock.ppu.y < 160) {
            auto *dbgl = &gba->dbg_info.line[gba->clock.ppu.y];
            for (u32 i = 2; i < 4; i++) {
                BG &bg = mbg[i];
                auto &dbgbg = dbgl->bg[i];
                if ( (gba->clock.ppu.y == 1) || bg.x_written) {
                    bg.x_lerp = bg.x;
                    dbgbg.reset_x = 1;
                }
                dbgbg.x_lerp = bg.x_lerp;

                if ( (gba->clock.ppu.y == 1) || (bg.y_written)) {
                    bg.y_lerp = bg.y;
                    dbgbg.reset_y = 1;
                }
                dbgbg.y_lerp = bg.y_lerp;
                bg.x_written = bg.y_written = false;
            }
        }
    }
}

void core::schedule_frame(void *ptr, u64 key, u64 clock, u32 jitter) {
    auto *th = static_cast<core *>(ptr);
    i64 cur_clock = static_cast<i64>(clock - jitter);
    th->do_schedule_frame(cur_clock);
}

void core::do_schedule_frame(i64 cur_clock)
{
    i64 start_clock = cur_clock;
    for (u32 line = 0; line < 228; line++) {
        if (line == 160) {
            gba->scheduler.only_add_abs(cur_clock, 1, gba, &vblank, nullptr);
        }
        if (line == 227) {
            gba->scheduler.only_add_abs(cur_clock, 0, gba, &vblank, nullptr);
        }

        gba->scheduler.only_add_abs(cur_clock+MASTER_CYCLES_BEFORE_HBLANK, 1, this, &hblank, nullptr);
        gba->scheduler.only_add_abs_w_tag(cur_clock+MASTER_CYCLES_PER_SCANLINE, 0, this, &hblank, nullptr, 1);
        cur_clock += MASTER_CYCLES_PER_SCANLINE;
    }
    gba->scheduler.only_add_abs_w_tag(start_clock+MASTER_CYCLES_PER_FRAME, 0, this, &schedule_frame, nullptr, 2);

    // Do new-frame stuff
    for (auto & bg : mbg) {
        bg.mosaic_y = 0;
        bg.last_y_rendered = 159;
    }
    mosaic.bg.y_counter = 0;
    mosaic.bg.y_current = 0;
    mosaic.obj.y_counter = 0;
    mosaic.obj.y_current = 0;
    memset(gba->dbg_info.bg_scrolls, 0, sizeof(gba->dbg_info.bg_scrolls));

    debugger_report_frame(gba->dbg.interface);
    gba->clock.ppu.y = 0;
    cur_output = static_cast<u16 *>(display->output[display->last_written ^ 1]);
    display->last_written ^= 1;
    gba->clock.master_frame++;
    process_button_IRQ();
}


u32 core::read_invalid(u32 addr, u8 sz, u8 access, bool has_effect)
{
    printf("\nREAD UNKNOWN PPU ADDR:%08x sz:%d", addr, sz);
    return gba->open_bus_byte(addr);
}

void core::write_invalid(u32 addr, u8 sz, u8 access, u32 val)
{
    printf("\nWRITE UNKNOWN PPU ADDR:%08x sz:%d DATA:%08x", addr, sz, val);
    //dbg.var++;
    //if (dbg.var > 5) dbg_break("too many ppu write", gba->clock.master_cycle_count);
}

u32 core::mainbus_read_palette(GBA::core *gba, u32 addr, u8 sz, u8 access, bool has_effect)
{
    auto *th = &gba->ppu;
    addr &= maskalign[sz];

    gba->waitstates.current_transaction++;
    if (sz == 4) {
        gba->waitstates.current_transaction++;
    }
    //if (addr < 0x05000400)
    return cR[sz](th->palette_RAM, addr & 0x3FF);

    //return GBA_PPU_read_invalid(addr, sz, access, has_effect);
}

u32 core::mainbus_read_VRAM(GBA::core *gba, u32 addr, u8 sz, u8 access, bool has_effect)
{
    auto *th = &gba->ppu;
    addr &= maskalign[sz];
    gba->waitstates.current_transaction += (sz == 4) ? 2 : 1;
    addr &= 0x1FFFF;
    if (addr < 0x18000)
        return cR[sz](th->VRAM, addr);
    else
        return cR[sz](th->VRAM, addr - 0x8000);
}

u32 core::mainbus_read_OAM(GBA::core *gba, u32 addr, u8 sz, u8 access, bool has_effect) {
    auto *th = &gba->ppu;
    addr &= maskalign[sz];
    gba->waitstates.current_transaction++;
    //if (addr < 0x07000400)
        return cR[sz](th->OAM, addr & 0x3FF);

    //return GBA_PPU_read_invalid(addr, sz, access, has_effect);
}

void core::mainbus_write_palette(GBA::core *gba, u32 addr, u8 sz, u8 access, u32 val)
{
    auto *th = &gba->ppu;
    gba->waitstates.current_transaction += (sz == 4) ? 2 : 1;
    addr &= maskalign[sz];

    if (sz == 1) { sz = 2; val = (val & 0xFF) | ((val << 8) & 0xFF00); }

    return cW[sz](th->palette_RAM, addr & 0x3FF, val);
}

void core::mainbus_write_VRAM(GBA::core *gba, u32 addr, u8 sz, u8 access, u32 val)
{
    auto *th = &gba->ppu;
    DBG_EVENT_TH(DBG_GBA_EVENT_WRITE_VRAM);
    addr &= maskalign[sz];
    gba->waitstates.current_transaction += (sz == 4) ? 2 : 1;

    u32 vram_boundary = th->io.bg_mode >= 3 ? 0x14000 : 0x10000;
    u32 mask = sz == 4 ? 0xFFFFFFFF : (sz == 2 ? 0xFFFF : 0xFF);
    val &= mask;
    addr &= 0x1FFFF;
    if (addr >= vram_boundary) {
        if (sz != 1) {
            if (addr >= 0x18000) {
                addr &= ~0x8000;

                if (addr < vram_boundary) {
                    return;
                }
            }
            return cW[sz](th->VRAM, addr, val);
        }
        return;
    }
    else {
        if (sz == 1) {
            addr &= 0x1FFFE;
            sz = 2;
            val = (val << 8) | val;
        }
        return cW[sz](th->VRAM, addr, val);
    }

    th->write_invalid(addr, sz, access, val);
}

void core::mainbus_write_OAM(GBA::core *gba, u32 addr, u8 sz, u8 access, u32 val)
{
    auto *th = &gba->ppu;
    gba->waitstates.current_transaction++;
    addr &= maskalign[sz];
    if (sz == 1) return;
    //if (addr < 0x07000400)
    return cW[sz](th->OAM, addr & 0x3FF, val);

    //GBA_PPU_write_invalid(addr, sz, access, val);
}

#define BG3PA 0x04000030
#define BG3PB 0x04000032
#define BG3PC 0x04000034
#define BG3PD 0x04000036

#define WIN0H 0x04000040
#define WIN1H 0x04000042
#define WIN0V 0x04000044
#define WIN1V 0x04000046

#define WININ 0x04000048
#define WINOUT 0x0400004A

#define MOSAIC 0x0400004C
#define BLDCNT 0x04000050
#define BLDALPHA 0x04000052
#define BLDY 0x04000054

u32 core::mainbus_read_IO(GBA::core *gba, u32 addr, u8 sz, u8 access, bool has_effect)
{
    auto *th = &gba->ppu;
    u32 v = 0;
    addr &= maskalign[sz];
    switch(addr) {
        case 0x04000000: // DISPCTRL lo DISPCNT
            v = th->io.bg_mode;
            v |= (th->io.frame) << 4;
            v |= (th->io.hblank_free) << 5;
            v |= (th->io.obj_mapping_1d) << 6;
            v |= (th->io.force_blank) << 7;
            return v;
        case 0x04000001: // DISPCTRL hi
            v = th->mbg[0].enable;
            v |= th->mbg[1].enable << 1;
            v |= th->mbg[2].enable << 2;
            v |= th->mbg[3].enable << 3;
            v |= th->obj.enable << 4;
            v |= th->window[0].enable << 5;
            v |= th->window[1].enable << 6;
            v |= th->window[WINOBJ].enable << 7;
            return v;
        case 0x04000004: // DISPSTAT lo
            v = gba->clock.ppu.vblank_active;
            v |= gba->clock.ppu.hblank_active << 1;
            v |=  (gba->clock.ppu.y == th->io.vcount_at) << 2;
            v |= th->io.vblank_irq_enable << 3;
            v |= th->io.hblank_irq_enable << 4;
            v |= th->io.vcount_irq_enable << 5;
            return v;
        case 0x04000005: // DISPSTAT hi
            v = th->io.vcount_at;
            return v;
        case 0x04000006: // VCNT lo
            return gba->clock.ppu.y;
        case 0x04000007: // VCNT hi
            return 0;
        case 0x04000008: // BG control lo
        case 0x0400000A:
        case 0x0400000C:
        case 0x0400000E: {
            const u32 bgnum = (addr & 0b0110) >> 1;
            const BG &bg = th->mbg[bgnum];
            v = bg.priority;
            v |= bg.extrabits << 4;
            v |= (bg.character_base_block >> 12);
            v |= bg.mosaic_enable << 6;
            v |= bg.bpp8 << 7;
            return v; }
        case 0x04000009: // BG control
        case 0x0400000B:
        case 0x0400000D:
        case 0x0400000F: {
            const u32 bgnum = (addr & 0b0110) >> 1;
            const BG &bg = th->mbg[bgnum];
            v = bg.screen_base_block >> 11;
            if (bgnum >= 2) v |= bg.display_overflow << 5;
            v |= bg.screen_size << 6;
            return v; }

        case WININ:
            v = th->window[0].active[1] << 0;
            v |= th->window[0].active[2] << 1;
            v |= th->window[0].active[3] << 2;
            v |= th->window[0].active[4] << 3;
            v |= th->window[0].active[0] << 4;
            v |= th->window[0].active[5] << 5;
            return v;
        case WININ+1:
            v = th->window[1].active[1] << 0;
            v |= th->window[1].active[2] << 1;
            v |= th->window[1].active[3] << 2;
            v |= th->window[1].active[4] << 3;
            v |= th->window[1].active[0] << 4;
            v |= th->window[1].active[5] << 5;
            return v;
        case WINOUT:
            v = th->window[WINOUTSIDE].active[1] << 0;
            v |= th->window[WINOUTSIDE].active[2] << 1;
            v |= th->window[WINOUTSIDE].active[3] << 2;
            v |= th->window[WINOUTSIDE].active[4] << 3;
            v |= th->window[WINOUTSIDE].active[0] << 4;
            v |= th->window[WINOUTSIDE].active[5] << 5;
            return v;
        case WINOUT+1:
            v = th->window[WINOBJ].active[1] << 0;
            v |= th->window[WINOBJ].active[2] << 1;
            v |= th->window[WINOBJ].active[3] << 2;
            v |= th->window[WINOBJ].active[4] << 3;
            v |= th->window[WINOBJ].active[0] << 4;
            v |= th->window[WINOBJ].active[5] << 5;
            return v;
        case BLDCNT:
            v = th->blend.targets_a[0] << 4;
            v |= th->blend.targets_a[1] << 0;
            v |= th->blend.targets_a[2] << 1;
            v |= th->blend.targets_a[3] << 2;
            v |= th->blend.targets_a[4] << 3;
            v |= th->blend.targets_a[5] << 5;
            v |= th->blend.mode << 6;
            return v;
        case BLDCNT+1:
            v = th->blend.targets_b[0] << 4;
            v |= th->blend.targets_b[1] << 0;
            v |= th->blend.targets_b[2] << 1;
            v |= th->blend.targets_b[3] << 2;
            v |= th->blend.targets_b[4] << 3;
            v |= th->blend.targets_b[5] << 5;
            return v;
        case BLDALPHA:
            return th->blend.eva_a;
        case BLDALPHA+1:
            return th->blend.eva_b;


        case 0x04000002:
        case 0x04000003:
        case 0x04000010:
        case 0x04000011:
        case 0x04000012:
        case 0x04000013:
        case 0x04000014:
        case 0x04000015:
        case 0x04000016:
        case 0x04000017:
        case 0x04000018:
        case 0x04000019:
        case 0x0400001A:
        case 0x0400001B:
        case 0x0400001C:
        case 0x0400001D:
        case 0x0400001E:
        case 0x0400001F:
        case 0x04000020:
        case 0x04000021:
        case 0x04000022:
        case 0x04000023:
        case 0x04000024:
        case 0x04000025:
        case 0x04000026:
        case 0x04000027:
        case 0x04000028:
        case 0x04000029:
        case 0x0400002A:
        case 0x0400002B:
        case 0x0400002C:
        case 0x0400002D:
        case 0x0400002E:
        case 0x0400002F:
        case 0x04000030:
        case 0x04000031:
        case 0x04000032:
        case 0x04000033:
        case 0x04000034:
        case 0x04000035:
        case 0x04000036:
        case 0x04000037:
        case 0x04000038:
        case 0x04000039:
        case 0x0400003A:
        case 0x0400003B:
        case 0x0400003C:
        case 0x0400003D:
        case 0x0400003E:
        case 0x0400003F:
        case 0x0400004E:
        case 0x0400004F:
        case 0x04000054:
        case 0x04000055:
        case 0x04000056:
        case 0x04000057:
        case 0x04000058:
        case 0x04000059:
        case 0x0400005A:
        case 0x0400005B:
        case 0x0400005C:
        case 0x0400005D:
        case 0x0400005E:
        case 0x0400005F:
        case WIN0H:
        case WIN0H+1:
        case WIN1H:
        case WIN1H+1:
        case WIN0V:
        case WIN0V+1:
        case WIN1V:
        case WIN1V+1:
        case MOSAIC:
        case MOSAIC+1:
            return gba->open_bus_byte(addr);
        default:
    }
    return th->read_invalid(addr, sz, access, has_effect);
}

void core::calc_screen_size(u32 num, u32 mode)
{
    if (mode >= 3) return;
    BG &bg = mbg[num];
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
    default:
        NOGOHERE;
    }
#undef T
    bg.htiles_mask = bg.htiles - 1;
    bg.vtiles_mask = bg.vtiles - 1;
    bg.hpixels = bg.htiles << 3;
    bg.vpixels = bg.vtiles << 3;
    bg.hpixels_mask = bg.hpixels - 1;
    bg.vpixels_mask = bg.vpixels - 1;
}

void core::update_bg_x(u32 bgnum, u32 which, u32 val)
{
    u32 v = mbg[bgnum].x & 0x0FFFFFFF;
    val &= 0xFF;
    switch(which) {
        case 0: v = (v & 0x0FFFFF00) | val; break;
        case 1: v = (v & 0x0FFF00FF) | (val << 8); break;
        case 2: v = (v & 0x0F00FFFF) | (val << 16); break;
        case 3: v = (v & 0x00FFFFFF) | (val << 24); break;
        default:
            NOGOHERE;
    }
    if ((v >> 27) & 1) v |= 0xF0000000;
    mbg[bgnum].x = static_cast<i32>(v);
    mbg[bgnum].x_written = true;
    //printf("\nline:%d X%d update:%08x", gba->clock.ppu.y, bgnum, v);
}

void core::update_bg_y(u32 bgnum, u32 which, u32 val)
{
    u32 v = mbg[bgnum].y & 0x0FFFFFFF;
    val &= 0xFF;
    switch(which) {
        case 0: v = (v & 0x0FFFFF00) | val; break;
        case 1: v = (v & 0x0FFF00FF) | (val << 8); break;
        case 2: v = (v & 0x0F00FFFF) | (val << 16); break;
        case 3: v = (v & 0x00FFFFFF) | (val << 24); break;
        default: NOGOHERE;
    }
    if ((v >> 27) & 1) v |= 0xF0000000;
    mbg[bgnum].y = static_cast<i32>(v);
    mbg[bgnum].y_written = true;
    //printf("\nline:%d Y%d update:%08x", gba->clock.ppu.y, bgnum, v);
}

void core::mainbus_write_IO(GBA::core *gba, u32 addr, u8 sz, u8 access, u32 val) {
    auto *th = &gba->ppu;
    addr &= maskalign[sz];
    if ((addr >= 0x04000020) && (addr < 0x04000040)) {
        DBG_EVENT_TH(DBG_GBA_EVENT_WRITE_AFFINE_REGS);
    }
    switch(addr) {
        case 0x04000000: {// DISPCNT lo
            //printf("\nDISPCNT WRITE %04x", val);
            u32 new_mode = val & 7;
            if (new_mode >= 6) {
                printf("\nILLEGAL BG MODE:%d", new_mode);
                dbg_break("ILLEGAL BG MODE", gba->clock.master_cycle_count);
            }
            else {
                //if (new_mode != ppu->io.bg_mode) printf("\nBG MODE:%d LINE:%d", val & 7, gba->clock.ppu.y);
            }

            th->io.bg_mode = new_mode;
            th->calc_screen_size(0, th->io.bg_mode);
            th->calc_screen_size(1, th->io.bg_mode);
            th->calc_screen_size(2, th->io.bg_mode);
            th->calc_screen_size(3, th->io.bg_mode);

            th->io.frame = (val >> 4) & 1;
            th->io.hblank_free = (val >> 5) & 1;
            th->io.obj_mapping_1d = (val >> 6) & 1;
            th->io.force_blank = (val >> 7) & 1;
            return; }
        case 0x04000001: { // DISPCNT hi
            th->mbg[0].enable = (val >> 0) & 1;
            th->mbg[1].enable = (val >> 1) & 1;
            th->mbg[2].enable = (val >> 2) & 1;
            th->mbg[3].enable = (val >> 3) & 1;
            th->obj.enable = (val >> 4) & 1;
            th->window[0].enable = (val >> 5) & 1;
            th->window[1].enable = (val >> 6) & 1;
            th->window[WINOBJ].enable = (val >> 7) & 1;
            /*printf("\nBGs 0:%d 1:%d 2:%d 3:%d obj:%d window0:%d window1:%d force_hblank:%d",
                   bg[0].enable, bg[1].enable, bg[2].enable, bg[3].enable,
                   obj.enable, window[0].enable, window[1].enable, io.force_blank
                   );
            printf("\nOBJ mapping 2d:%d", io.obj_mapping_2d);*/
            return; }
        case 0x04000002:
            printf("\nGREEN SWAP? %d", val);
            return;
        case 0x04000003:
            printf("\nGREEN SWAP2? %d", val);
            return;
        case 0x04000004: {// DISPSTAT
            th->io.vblank_irq_enable = (val >> 3) & 1;
            th->io.hblank_irq_enable = (val >> 4) & 1;
            th->io.vcount_irq_enable = (val >> 5) & 1;
            return; }
        case 0x04000005: th->io.vcount_at = val; return;
        case 0x04000006: // not used
        case 0x04000007: return; // not used

        case 0x04000008: // BG control
        case 0x0400000A:
        case 0x0400000C:
        case 0x0400000E: { // BGCNT lo
            const u32 bgnum = (addr & 0b0110) >> 1;
            BG &bg = th->mbg[bgnum];
            bg.priority = val & 3;
            bg.extrabits = (val >> 4) & 3;
            bg.character_base_block = ((val >> 2) & 3) << 14;
            bg.mosaic_enable = (val >> 6) & 1;
            bg.bpp8 = (val >> 7) & 1;
            return; }
        case 0x04000009: // BG control // BGCNT hi
        case 0x0400000B:
        case 0x0400000D:
        case 0x0400000F: {
            const u32 bgnum = (addr & 0b0110) >> 1;
            BG &bg = th->mbg[bgnum];
            bg.screen_base_block = ((val >> 0) & 31) << 11;
            if (bgnum >= 2) bg.display_overflow = (val >> 5) & 1;
            bg.screen_size = (val >> 6) & 3;
            th->calc_screen_size(bgnum, th->io.bg_mode);
            return; }
#define BG2 2
#define BG3 3
        case 0x04000010: th->mbg[0].hscroll = (th->mbg[0].hscroll & 0xFF00) | val; return;
        case 0x04000011: th->mbg[0].hscroll = (th->mbg[0].hscroll & 0x00FF) | (val << 8); return;
        case 0x04000012: th->mbg[0].vscroll = (th->mbg[0].vscroll & 0xFF00) | val; return;
        case 0x04000013: th->mbg[0].vscroll = (th->mbg[0].vscroll & 0x00FF) | (val << 8); return;
        case 0x04000014: th->mbg[1].hscroll = (th->mbg[1].hscroll & 0xFF00) | val; return;
        case 0x04000015: th->mbg[1].hscroll = (th->mbg[1].hscroll & 0x00FF) | (val << 8); return;
        case 0x04000016: th->mbg[1].vscroll = (th->mbg[1].vscroll & 0xFF00) | val; return;
        case 0x04000017: th->mbg[1].vscroll = (th->mbg[1].vscroll & 0x00FF) | (val << 8); return;
        case 0x04000018: th->mbg[2].hscroll = (th->mbg[2].hscroll & 0xFF00) | val; return;
        case 0x04000019: th->mbg[2].hscroll = (th->mbg[2].hscroll & 0x00FF) | (val << 8); return;
        case 0x0400001A: th->mbg[2].vscroll = (th->mbg[2].vscroll & 0xFF00) | val; return;
        case 0x0400001B: th->mbg[2].vscroll = (th->mbg[2].vscroll & 0x00FF) | (val << 8); return;
        case 0x0400001C: th->mbg[3].hscroll = (th->mbg[3].hscroll & 0xFF00) | val; return;
        case 0x0400001D: th->mbg[3].hscroll = (th->mbg[3].hscroll & 0x00FF) | (val << 8); return;
        case 0x0400001E: th->mbg[3].vscroll = (th->mbg[3].vscroll & 0xFF00) | val; return;
        case 0x0400001F: th->mbg[3].vscroll = (th->mbg[3].vscroll & 0x00FF) | (val << 8); return;

        case 0x04000020: th->mbg[2].pa = static_cast<i32>((th->mbg[2].pa & 0xFFFFFF00) | val); return;
        case 0x04000021: th->mbg[2].pa = static_cast<i32>((th->mbg[2].pa & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000)); return;
        case 0x04000022: th->mbg[2].pb = static_cast<i32>((th->mbg[2].pb & 0xFFFFFF00) | val); return;
        case 0x04000023: th->mbg[2].pb = static_cast<i32>((th->mbg[2].pb & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000)); return;
        case 0x04000024: th->mbg[2].pc = static_cast<i32>((th->mbg[2].pc & 0xFFFFFF00) | val); return;
        case 0x04000025: th->mbg[2].pc = static_cast<i32>((th->mbg[2].pc & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000)); return;
        case 0x04000026: th->mbg[2].pd = static_cast<i32>((th->mbg[2].pd & 0xFFFFFF00) | val); return;
        case 0x04000027: th->mbg[2].pd = static_cast<i32>((th->mbg[2].pd & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000)); return;
        case 0x04000028: th->update_bg_x(BG2, 0, val); return;
        case 0x04000029: th->update_bg_x(BG2, 1, val); return;
        case 0x0400002A: th->update_bg_x(BG2, 2, val); return;
        case 0x0400002B: th->update_bg_x(BG2, 3, val); return;
        case 0x0400002C: th->update_bg_y(BG2, 0, val); return;
        case 0x0400002D: th->update_bg_y(BG2, 1, val); return;
        case 0x0400002E: th->update_bg_y(BG2, 2, val); return;
        case 0x0400002F: th->update_bg_y(BG2, 3, val); return;

        case BG3PA:   th->mbg[3].pa = static_cast<i32>((th->mbg[3].pa & 0xFFFFFF00) | val); return;
        case BG3PA+1: th->mbg[3].pa = static_cast<i32>((th->mbg[3].pa & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000)); return;
        case BG3PB:   th->mbg[3].pb = static_cast<i32>((th->mbg[3].pb & 0xFFFFFF00) | val); return;
        case BG3PB+1: th->mbg[3].pb = static_cast<i32>((th->mbg[3].pb & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000)); return;
        case BG3PC:   th->mbg[3].pc = static_cast<i32>((th->mbg[3].pc & 0xFFFFFF00) | val); return;
        case BG3PC+1: th->mbg[3].pc = static_cast<i32>((th->mbg[3].pc & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000)); return;
        case BG3PD:   th->mbg[3].pd = static_cast<i32>((th->mbg[3].pd & 0xFFFFFF00) | val); return;
        case BG3PD+1: th->mbg[3].pd = static_cast<i32>((th->mbg[3].pd & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000)); return;
        case 0x04000038: th->update_bg_x(BG3, 0, val); return;
        case 0x04000039: th->update_bg_x(BG3, 1, val); return;
        case 0x0400003A: th->update_bg_x(BG3, 2, val); return;
        case 0x0400003B: th->update_bg_x(BG3, 3, val); return;
        case 0x0400003C: th->update_bg_y(BG3, 0, val); return;
        case 0x0400003D: th->update_bg_y(BG3, 1, val); return;
        case 0x0400003E: th->update_bg_y(BG3, 2, val); return;
        case 0x0400003F: th->update_bg_y(BG3, 3, val); return;

        case WIN0H:   th->window[0].right = static_cast<i32>(val); return;
        case WIN0H+1: th->window[0].left = static_cast<i32>(val); return;
        case WIN1H:   th->window[1].right = static_cast<i32>(val); return;
        case WIN1H+1: th->window[1].left = static_cast<i32>(val); return;
        case WIN0V:   th->window[0].bottom = static_cast<i32>(val); return;
        case WIN0V+1: th->window[0].top = static_cast<i32>(val); return;
        case WIN1V:   th->window[1].bottom = static_cast<i32>(val); return;
        case WIN1V+1: th->window[1].top = static_cast<i32>(val); return;

        case WININ:
            th->window[0].active[1] = (val >> 0) & 1;
            th->window[0].active[2] = (val >> 1) & 1;
            th->window[0].active[3] = (val >> 2) & 1;
            th->window[0].active[4] = (val >> 3) & 1;
            th->window[0].active[0] = (val >> 4) & 1;
            th->window[0].active[5] = (val >> 5) & 1;
            return;
        case WININ+1:
            th->window[1].active[1] = (val >> 0) & 1;
            th->window[1].active[2] = (val >> 1) & 1;
            th->window[1].active[3] = (val >> 2) & 1;
            th->window[1].active[4] = (val >> 3) & 1;
            th->window[1].active[0] = (val >> 4) & 1;
            th->window[1].active[5] = (val >> 5) & 1;
            return;
        case WINOUT:
            th->window[WINOUTSIDE].active[1] = (val >> 0) & 1;
            th->window[WINOUTSIDE].active[2] = (val >> 1) & 1;
            th->window[WINOUTSIDE].active[3] = (val >> 2) & 1;
            th->window[WINOUTSIDE].active[4] = (val >> 3) & 1;
            th->window[WINOUTSIDE].active[0] = (val >> 4) & 1;
            th->window[WINOUTSIDE].active[5] = (val >> 5) & 1;
            return;
        case WINOUT+1:
            th->window[WINOBJ].active[1] = (val >> 0) & 1;
            th->window[WINOBJ].active[2] = (val >> 1) & 1;
            th->window[WINOBJ].active[3] = (val >> 2) & 1;
            th->window[WINOBJ].active[4] = (val >> 3) & 1;
            th->window[WINOBJ].active[0] = (val >> 4) & 1;
            th->window[WINOBJ].active[5] = (val >> 5) & 1;
            return;
        case 0x0400004C:
            th->mosaic.bg.hsize = (val & 15) + 1;
            th->mosaic.bg.vsize = ((val >> 4) & 15) + 1;
            return;
        case 0x0400004D:
            th->mosaic.obj.hsize = (val & 15) + 1;
            th->mosaic.obj.vsize = ((val >> 4) & 15) + 1;
            return;
#define BT_BG0 0
#define BT_BG1 1
#define BT_BG2 2
#define BT_BG3 3
#define BT_OBJ 4
#define BT_BD 5

        case BLDCNT:
            th->blend.mode = (val >> 6) & 3;
            // sp, bg0, bg1, bg2, bg3, bd
            th->blend.targets_a[0] = (val >> 4) & 1;
            th->blend.targets_a[1] = (val >> 0) & 1;
            th->blend.targets_a[2] = (val >> 1) & 1;
            th->blend.targets_a[3] = (val >> 2) & 1;
            th->blend.targets_a[4] = (val >> 3) & 1;
            th->blend.targets_a[5] = (val >> 5) & 1;
            return;
        case BLDCNT+1:
            th->blend.targets_b[0] = (val >> 4) & 1;
            th->blend.targets_b[1] = (val >> 0) & 1;
            th->blend.targets_b[2] = (val >> 1) & 1;
            th->blend.targets_b[3] = (val >> 2) & 1;
            th->blend.targets_b[4] = (val >> 3) & 1;
            th->blend.targets_b[5] = (val >> 5) & 1;
            return;

        case BLDALPHA:
            th->blend.eva_a = val & 31;
            th->blend.use_eva_a = th->blend.eva_a;
            if (th->blend.use_eva_a > 16) th->blend.use_eva_a = 16;
            return;
        case BLDALPHA+1:
            th->blend.eva_b = val & 31;
            th->blend.use_eva_b = th->blend.eva_b;
            if (th->blend.use_eva_b > 16) th->blend.use_eva_b = 16;
            return;
        case 0x04000054:
            th->blend.bldy = val & 31;
            th->blend.use_bldy = th->blend.bldy;
            if (th->blend.use_bldy > 16) th->blend.use_bldy = 16;
            return;
        case 0x04000055:
            // TODO: support this stuff
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
        default:
#undef BG2
#undef BG3
    }

    th->write_invalid(addr, sz, access, val);
}

}