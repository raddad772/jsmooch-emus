//
// Created by . on 12/4/24.
//

#include <cstring>
#include <cassert>
#include <cstdlib>

#include "helpers/color.h"
#include "nds.h"
#include "nds_bus.h"
#include "nds_debugger.h"

namespace NDS {
#define PAL_BOX_SIZE 10
#define PAL_BOX_SIZE_W_BORDER 11

static void render_image_view_palette(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    auto *th = static_cast<core *>(ptr);
    if (th->clock.master_frame == 0) return;
    image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = static_cast<u32 *>(iv->img_buf[iv->draw_which_buf].ptr);
    memset(outbuf, 0, out_width * (((16 * PAL_BOX_SIZE_W_BORDER) * 5) + 2));
    u32 which = dview->options[0].radiogroup.value;

    // We get a 32x32 playground, ish.
    // That's 1000 colors, but extended palettes have 4000.

    if (which == 0) {

        for (u32 eng_num = 0; eng_num < 2; eng_num++) {
            auto &eng = th->ppu.eng2d[eng_num];
            u32 upper_row = 0;
            u32 offset = 0;
            u32 y_offset = 0;
            u32 x_offset = eng_num * ((16 * PAL_BOX_SIZE_W_BORDER) + 1);
            // iv->width = ((16 * PAL_BOX_SIZE_W_BORDER * 2) + 2);

            for (u32 lohi = 0; lohi < 0x200; lohi += 0x100) { // lohi = bg or OBJ palettes
                u16 *curpal = lohi == 0 ? reinterpret_cast<u16 *>(eng.mem.bg_palette) : reinterpret_cast<u16 *>(eng.mem.obj_palette);
                for (u32 palette = 0; palette < 16; palette++) {
                    for (u32 color = 0; color < 16; color++) {
                        u32 y_top = y_offset + offset + upper_row;
                        u32 x_left = x_offset + (color * PAL_BOX_SIZE_W_BORDER);
                        u32 c = gba_to_screen(curpal[(palette << 4) + color]);
                        for (u32 y = 0; y < PAL_BOX_SIZE; y++) {
                            u32 *box_ptr = (outbuf + ((y_top + y) * out_width) + x_left);
                            for (u32 x = 0; x < PAL_BOX_SIZE; x++) {
                                box_ptr[x] = c;
                            }
                        }
                    }
                    y_offset += PAL_BOX_SIZE;
                }
                offset += 2;
            }
        }
    }
    else {
        u32 eng_num;
        u32 slot_num;
        /*u8 *ptr;
        switch(which) {
            case 0:

        }*/
    }
}

#define PAL_BOX_SIZE 10
#define PAL_BOX_SIZE_W_BORDER 11

static void setup_image_view_palettes(core *th, debugger_interface *dbgr)
{
    debugger_view *dview;
    th->dbg.image_views.palettes = dbgr->make_view(dview_image);
    dview = &th->dbg.image_views.palettes.get();
    image_view *iv = &dview->image;

    iv->width = (64 * PAL_BOX_SIZE_W_BORDER) + 10;
    iv->height = (64 * PAL_BOX_SIZE_W_BORDER) + 10;

    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = ivec2( 0, 0 );
    iv->viewport.p[1] = ivec2( iv->width, iv->height );

    iv->update_func.ptr = th;
    iv->update_func.func = &render_image_view_palette;
    snprintf(iv->label, sizeof(iv->label), "Palettes Viewer");

    debugger_widget &rg = debugger_widgets_add_radiogroup(dview->options, "Which", 1, 0, 0);
    rg.radiogroup.add_button("Regular", 0, 1);
    rg.radiogroup.add_button("EngA Ext.BG", 1, 1);
    rg.radiogroup.add_button("EngA Ext.OBJ", 2, 1);
    rg.radiogroup.add_button("EngB Ext.BG", 3, 1);
    rg.radiogroup.add_button("EngB Ext.OBJ", 4, 1);
}

static void draw_line(u32 *outbuf, u32 out_width, u32 out_height, i32 x0, i32 y0, i32 x1, i32 y1)
{
    i32 dx =  abs (x1 - x0), sx = x0 < x1 ? 1 : -1;
    i32 dy = -abs (y1 - y0), sy = y0 < y1 ? 1 : -1;
    i32 err = dx + dy, e2; /* error value e_xy */

    for (;;){  /* loop */
        if ((x0 >= 0) && (y0 >= 0) && (x0 < out_width) && (y0 < out_height)) outbuf[(out_width * y0) + x0] = 0xFFFFFFFF;
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; } /* e_xy+e_x > 0 */
        if (e2 <= dx) { err += dx; y0 += sy; } /* e_xy+e_y < 0 */
    }
}

static void render_image_view_re_wireframe(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    auto *th = static_cast<core *>(ptr);
    if (th->clock.master_frame == 0) return;

    image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = static_cast<u32 *>(iv->img_buf[iv->draw_which_buf].ptr);
    memset(outbuf, 0, out_width * 4 * 192);
    auto *b = &th->ge.buffers[th->ge.ge_has_buffer ^ 1];

    debugger_widget_textbox *tb = &dview->options[0].textbox;
    tb->clear();
    tb->sprintf( "#poly:%d", b->polygon_index);

    for (u32 i = 0; i < b->polygon_index; i++) {
        auto *p = &b->polygon[i];
        if (p->attr.mode > 2) continue;
        GFX::VTX_list_node *v0, *v1;
        v1 = p->vertex_list.first;
        for (u32 vn = 1; vn < p->vertex_list.len+1; vn++) {
            v0 = v1;
            v1 = v1->next;
            if (!v1) v1 = p->vertex_list.first;

            GFX::VTX_list_node *n0, *n1;
            if (v0->data.xyzw[1] < v1->data.xyzw[1]) {
                n0 = v0;
                n1 = v1;
            }
            else {
                n0 = v1;
                n1 = v0;
            }
            draw_line(outbuf, out_width, 192, n0->data.xyzw[0], n0->data.xyzw[1], n1->data.xyzw[0], n1->data.xyzw[1]);
        }
    }
}

static inline u32 C18to15(u32 c)
{
    return (((c >> 13) & 0x1F) << 10) | (((c >> 7) & 0x1F) << 5) | ((c >> 1) & 0x1F);
}

static void render_image_view_re_output(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    auto *th = static_cast<core *>(ptr);
    if (th->clock.master_frame == 0) return;

    image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = static_cast<u32 *>(iv->img_buf[iv->draw_which_buf].ptr);
    memset(outbuf, 0, out_width * 4 * 192);
    auto *b = &th->ge.buffers[th->ge.ge_has_buffer ^ 1];

    for (u32 y = 0; y < 192; y++) {
        auto *lbuf = &th->re.out.linebuffer[y];
        u32 *out_line = outbuf + (y * out_width);
        for (u32 x = 0; x < 256; x++) {
            out_line[x] = nds_to_screen(lbuf->rgb[x]);
        }
    }
}

static void set_info_A(char *mapstr, u32 mst, u32 ofs, u32 *mapaddr_start, u32 *mapaddr_end)
{
    switch(mst) {
        case 0:
            snprintf(mapstr, 50, "arm9");
            *mapaddr_start = 0x06800000;
            break;
        case 1:
            snprintf(mapstr, 50, "arm9+engA BG VRAM");
            *mapaddr_start = 0x20000 * ofs;
            break;
        case 2:
            snprintf(mapstr, 50, "arm9+engA OBJ VRAM");
            *mapaddr_start = 0x20000 * (ofs & 1);
            break;
        case 3:
            snprintf(mapstr, 50, "3d texture");
            *mapaddr_start = 0x20000 * ofs;
            break;
    }
    *mapaddr_end = *mapaddr_start + 0x1FFFF;
}

static void set_info_B(char *mapstr, u32 mst, u32 ofs, u32 *mapaddr_start, u32 *mapaddr_end) {
    switch (mst) {
        case 0:
            snprintf(mapstr, 50, "arm9");
            *mapaddr_start = 0x06820000;
            break;
        case 1:
            snprintf(mapstr, 50, "arm9+engA BG VRAM");
            *mapaddr_start = 0x20000 * ofs;
            break;
        case 2:
            snprintf(mapstr, 50, "arm9+engA OBJ VRAM");
            *mapaddr_start = 0x20000 * (ofs & 1);
            break;
        case 3:
            snprintf(mapstr, 50, "3d texture");
            *mapaddr_start = 0x20000 * ofs;
            break;
    }
    *mapaddr_end = *mapaddr_start + 0x1FFFF;
}

static void set_info_C(char *mapstr, u32 mst, u32 ofs, u32 *mapaddr_start, u32 *mapaddr_end) {
    switch (mst) {
        case 0:
            snprintf(mapstr, 50, "arm9");
            *mapaddr_start = 0x06840000;
            break;
        case 1:
            snprintf(mapstr, 50, "arm9+engA BG VRAM");
            *mapaddr_start = 0x20000 * ofs;
            break;
        case 2:
            snprintf(mapstr, 50, "arm7");
            *mapaddr_start = 0x06000000 + (0x20000 * (ofs & 1));
            break;
        case 3:
            snprintf(mapstr, 50, "3d texture");
            *mapaddr_start = 0x20000 * ofs;
            break;
        case 4:
            snprintf(mapstr, 50, "arm9+engB BG VRAM");
            *mapaddr_start = 0;
            break;
    }
    *mapaddr_end = *mapaddr_start + 0x1FFFF;
}

static void set_info_D(char *mapstr, u32 mst, u32 ofs, u32 *mapaddr_start, u32 *mapaddr_end) {
    switch (mst) {
        case 0:
            snprintf(mapstr, 50, "arm9");
            *mapaddr_start = 0x06860000;
            break;
        case 1:
            snprintf(mapstr, 50, "arm9+engA BG VRAM");
            *mapaddr_start = 0x20000 * ofs;
            break;
        case 2:
            snprintf(mapstr, 50, "arm7");
            *mapaddr_start = 0x06000000 + (0x20000 * (ofs & 1));
            break;
        case 3:
            snprintf(mapstr, 50, "3d texture");
            *mapaddr_start = 0x20000 * ofs;
            break;
        case 4:
            snprintf(mapstr, 50, "arm9+engB OBJ VRAM");
            *mapaddr_start = 0;
            break;
    }
    *mapaddr_end = *mapaddr_start + 0x1FFFF;
}

static void set_info_E(char *mapstr, u32 mst, u32 ofs, u32 *mapaddr_start, u32 *mapaddr_end) {
    switch (mst) {
        case 0:
            snprintf(mapstr, 50, "arm9");
            *mapaddr_start = 0x06880000;
            break;
        case 1:
            snprintf(mapstr, 50, "arm9+engA BG VRAM");
            *mapaddr_start = 0;
            break;
        case 2:
            snprintf(mapstr, 50, "arm9+engA OBJ VRAM");
            *mapaddr_start = 0;
            break;
        case 3:
            snprintf(mapstr, 50, "3d palette 0-3");
            *mapaddr_start = 0;
            break;
        case 4:
            snprintf(mapstr, 50, "engA BG extended palette 0-3");
            *mapaddr_start = 0;
            *mapaddr_end = 0x7FFF;
            return;
    }
    *mapaddr_end = *mapaddr_start + 0xFFFF;
}

static void set_info_F(char *mapstr, u32 mst, u32 ofs, u32 *mapaddr_start, u32 *mapaddr_end) {
    switch (mst) {
        case 0:
            snprintf(mapstr, 50, "arm9");
            *mapaddr_start = 0x06890000;
            break;
        case 1:
            snprintf(mapstr, 50, "arm9+engA BG VRAM");
            *mapaddr_start =  (0x4000*(ofs & 1)) + (0x10000 * ((ofs >> 1) & 1));
            break;
        case 2:
            snprintf(mapstr, 50, "arm9+engA OBJ VRAM");
            *mapaddr_start = (0x4000*(ofs & 1)) + (0x10000 * ((ofs >> 1) & 1));
            break;
        case 3:
            snprintf(mapstr, 50, "3d palette %d", (ofs & 1) + ((ofs & 2) << 1));
            *mapaddr_start = 0x4000 * ((ofs & 1) + ((ofs & 2) << 1));
            break;
        case 4: {
            char *ptr = mapstr;
            ptr += snprintf(ptr, 50-(ptr-mapstr), "engA BG extended palette ");
            if ((ofs & 1) == 0) {
                ptr += snprintf(ptr, 50-(ptr-mapstr), "slot 0-1");
                *mapaddr_start = 0;
            }
            else {
                ptr += snprintf(ptr, 50-(ptr-mapstr), "slot 2-3");
                *mapaddr_start = 0x4000;
            }
            break; }
        case 5:
            snprintf(mapstr, 50, "engA OBJ extended palette");
            *mapaddr_start = 0;
            *mapaddr_end = 0x1FFF;
            return;
    }
    *mapaddr_end = *mapaddr_start + 0x3FFF;
}

static void set_info_G(char *mapstr, u32 mst, u32 ofs, u32 *mapaddr_start, u32 *mapaddr_end) {
    switch (mst) {
        case 0:
            snprintf(mapstr, 50, "arm9");
            *mapaddr_start = 0x06894000;
            break;
        case 1:
            snprintf(mapstr, 50, "arm9+engA BG VRAM");
            *mapaddr_start =  (0x4000 * (ofs & 1)) + (0x10000 * ((ofs >> 1) & 1));
            break;
        case 2:
            snprintf(mapstr, 50, "arm9+engA OBJ VRAM");
            *mapaddr_start = (0x4000 * (ofs & 1)) + (0x10000 * ((ofs >> 1) & 1));
            break;
        case 3:
            snprintf(mapstr, 50, "3d palette %d", ((ofs & 1) + ((ofs & 2) << 1)));
            *mapaddr_start = 0x4000 * ((ofs & 1) + ((ofs & 2) << 1));
            break;
        case 4: {
            char *ptr = mapstr;
            ptr += snprintf(ptr, 50-(ptr-mapstr), "engA BG extended palette ");
            if ((ofs & 1) == 0) {
                ptr += snprintf(ptr, 50-(ptr-mapstr), "slot 0-1");
                *mapaddr_start = 0;
            }
            else {
                ptr += snprintf(ptr, 50-(ptr-mapstr), "slot 2-3");
                *mapaddr_start = 0x4000;
            }
            break; }
        case 5:
            snprintf(mapstr, 50, "engA OBJ extended palette");
            *mapaddr_start = 0;
            *mapaddr_end = 0x1FFF;
            return;
    }
    *mapaddr_end = *mapaddr_start + 0x3FFF;
}

static void set_info_H(char *mapstr, u32 mst, u32 ofs, u32 *mapaddr_start, u32 *mapaddr_end) {
    switch (mst) {
        case 0:
            snprintf(mapstr, 50, "arm9");
            *mapaddr_start = 0x06898000;
            break;
        case 1:
            snprintf(mapstr, 50, "arm9+engB BG VRAM");
            *mapaddr_start =  0;
            break;
        case 2:
            snprintf(mapstr, 50, "engB BG extended palette 0-3");
            *mapaddr_start = 0;
            break;
    }
    *mapaddr_end = *mapaddr_start + 0x7FFF;
}

static void set_info_I(char *mapstr, u32 mst, u32 ofs, u32 *mapaddr_start, u32 *mapaddr_end) {
    switch (mst) {
        case 0:
            snprintf(mapstr, 50, "arm9");
            *mapaddr_start = 0x068A0000;
            break;
        case 1:
            snprintf(mapstr, 50, "arm9+engB BG VRAM");
            *mapaddr_start =  0x8000;
            break;
        case 2:
            snprintf(mapstr, 50, "arm9+engB OBJ VRAM");
            *mapaddr_start =  0;
            break;
        case 3:
            snprintf(mapstr, 50, "engB OBJ extended palette");
            *mapaddr_start = 0;
            *mapaddr_end = 0x1FFF;
            return;
    }
    *mapaddr_end = *mapaddr_start + 0x3FFF;
}

static void classify_bg_kind(core *th, PPU::ENG2D *eng, u32 bgnum, jsm_string *js)
{
    auto *bg = &eng->BG[bgnum];

#define cl(...) { js->sprintf(__VA_ARGS__); return; }
    if ((eng->num == 0) && (bgnum == 0) && (bg->do3d)) cl("3d")
    if (bgnum < 2) cl("text")
    if (bgnum == 2) {
        switch(eng->io.bg_mode) {
            case 0:
            case 1:
            case 3: cl("text")
            case 2:
            case 4: cl("affine")
            case 5: cl("extended")
            case 6:
                if (eng->num == 0) cl("large")
                else cl("none")
            default:
                cl("none")
        }
    }
    else {
        switch(eng->io.bg_mode) {
            case 0:
                cl("text")
            case 1:
            case 2:
                cl("affine")
            case 3:
            case 4:
            case 5:
                cl("extended")
            default:
                cl("none")
        }
    }
#undef cl
}

static void print_layer_info(core *th, PPU::ENG2D *eng, u32 bgnum, debugger_widget_textbox *tb)
{
    if (bgnum < 4) {
        auto *bg = &eng->BG[bgnum];
        tb->sprintf( "\n-BG%d:", bgnum);
        if (bg->enable) tb->sprintf( "on ");
        else tb->sprintf( "off");
        tb->sprintf( "  kind:");
        classify_bg_kind(th, eng, bgnum, &tb->contents);
        tb->sprintf( "  mosaic:%d  screen_size:%d  8bpp:%d", bg->enable,bg->mosaic_enable, bg->screen_size, bg->bpp8);
        tb->sprintf( "\n---hscroll:%d  vscroll:%d  mos.x,y:%d,%d  ", bg->hscroll, bg->vscroll, th->ppu.eng2d[0].mosaic.bg.hsize, th->ppu.eng2d[0].mosaic.bg.vsize);
        if (bgnum < 2) {
            tb->sprintf( "ext_palette:%d", bg->ext_pal_slot);
        }
        else {
            tb->sprintf( "disp_overflow:%d", bg->display_overflow);
        }
        tb->sprintf( "  priority:%d",  bg->priority);
    }
    else {
        tb->sprintf( "\n-OBJ  on:%d  mosaic:%d", eng->obj.enable, eng->mosaic.obj.enable);
        tb->sprintf( "\n---ext_palettes:%d  tile_1d:%d  bitmap_1d:%d", eng->io.obj.extended_palettes, eng->io.obj.tile.map_1d, eng->io.obj.bitmap.map_1d);
    }
}

static void pprint_irqs(debugger_widget_textbox *tb, u32 ie) {
    for (u32 i = 0; i < 23; i++) {
        bool bit = (ie >> i) & 1;
        if (!bit) continue;
#define CC(num, str) case num: tb->sprintf("\n" str); break
        switch (i) {
            CC(0, "VBlank");
            CC(1, "HBlank");
            CC(2, "VMatch");
            CC(3, "Timer0");
            CC(4, "Timer1");
            CC(5, "Timer2");
            CC(6, "Timer3");
            CC(7, "Serial");
            CC(8, "DMA0");
            CC(9, "DMA1");
            CC(10, "DMA2");
            CC(11, "DMA3");
            CC(12, "KEYPAD");
            CC(16, "IPC SYNC");
            CC(17, "IPC SEND EMPTY");
            CC(18, "IPC RECV NOT EMPTY");
            CC(19, "CART DATA READY");
            CC(21, "GFX FIFO");
            CC(23, "SPI");
            default:
            tb->sprintf("\nUnknown bit %d", i);
            break;
        }
    }
#undef CC
}

static void render_image_view_sys_info(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    auto *th = static_cast<core *>(ptr);
    //memset(ptr, 0, out_width * 4 * 10);
    debugger_widget_textbox *tb = &dview->options[0].textbox;
    tb->clear();
    for (u32 ppun = 0; ppun < 2; ppun++) {
        auto &eng = th->ppu.eng2d[ppun];
        tb->sprintf( "eng%c  display_mode:%d  bg_mode:%d  bg_ext_pal:%d", ppun == 0 ? 'A' : 'B', eng.io.display_mode, eng.io.bg_mode, eng.io.bg.extended_palettes);
        tb->sprintf( "\n");
        if (ppun ^ th->ppu.io.display_swap) tb->sprintf( "top   ");
        else tb->sprintf( "bottom");
        if (ppun == 0) tb->sprintf( "  do_3d:%d", eng.io.bg.do_3d);

        for (u32 bgnum = 0; bgnum < 5; bgnum++) {
            print_layer_info(th, &eng, bgnum, tb);
        }


        tb->sprintf( "\n\n");
    }

    // Now do VRAM mappings
    char mapstr[50];
    u32 mapaddr_start, mapaddr_end;
    for (u32 bnum = 0; bnum < 9; bnum++) {
        u32 mst = th->mem.vram.io.bank[bnum].mst;
        u32 ofs = th->mem.vram.io.bank[bnum].ofs;

        switch(bnum) {
            case 0:
                set_info_A(mapstr, mst, ofs, &mapaddr_start, &mapaddr_end);
                break;
            case 1:
                set_info_B(mapstr, mst, ofs, &mapaddr_start, &mapaddr_end);
                break;
            case 2:
                set_info_C(mapstr, mst, ofs, &mapaddr_start, &mapaddr_end);
                break;
            case 3:
                set_info_D(mapstr, mst, ofs, &mapaddr_start, &mapaddr_end);
                break;
            case 4:
                set_info_E(mapstr, mst, ofs, &mapaddr_start, &mapaddr_end);
                break;
            case 5:
                set_info_F(mapstr, mst, ofs, &mapaddr_start, &mapaddr_end);
                break;
            case 6:
                set_info_G(mapstr, mst, ofs, &mapaddr_start, &mapaddr_end);
                break;
            case 7:
                set_info_H(mapstr, mst, ofs, &mapaddr_start, &mapaddr_end);
                break;
            case 8:
                set_info_I(mapstr, mst, ofs, &mapaddr_start, &mapaddr_end);
                break;
        }
        tb->sprintf( "\nVRAM %c  MST:%d  OFS:%d  mapping:%s  start:%x  end:%x", 'A' + bnum, mst, ofs, mapstr, mapaddr_start, mapaddr_end);

    }
    tb->sprintf( "\n\nTimers ARM9:");
    for (u32 i = 0; i < 4; i++) {
        float period_f = ((float)th->timer9[i].reload_ticks / 33000000.0f);
        tb->sprintf(  "\n%d: %f", i, period_f);
    }

    tb->sprintf( "\n\nTimers ARM7:");
    for (u32 i = 0; i < 4; i++) {
        float period_f = ((float)th->timer7[i].reload_ticks / 33000000.0f);
        tb->sprintf(  "\n%d: %f", i, period_f);
    }

    tb->sprintf("\n\nInterrupts ARM9:");
    pprint_irqs(tb, th->io.arm9.IE);
    tb->sprintf("\n\nInterrupts ARM7:");
    pprint_irqs(tb, th->io.arm7.IE);

}

static void render_image_view_dispcap(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width)
{
    auto *th = static_cast<core *>(ptr);
    if (th->clock.master_frame == 0) return;

    debugger_widget_radiogroup *engrg = &dview->options[0].radiogroup;
    debugger_widget_textbox *tb = &dview->options[1].textbox;

    image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = static_cast<u32 *>(iv->img_buf[iv->draw_which_buf].ptr);
    memset(outbuf, 0, out_width * 4 * 192);

    static const int csize[4][2] = {{128, 128}, {256, 64}, {256, 128}, {256, 192} };
    auto dc = th->ppu.io.DISPCAPCNT;

    tb->sprintf( "DISPCAP stats:");
    tb->sprintf( "\ncap_size:%d(%dx%d)   eva:%d  evb:%d", dc.capture_size, csize[dc.capture_size][0], csize[dc.capture_size][1], dc.eva, dc.evb);
    tb->sprintf( "\ncap_size:%d(%dx%d)", dc.capture_size, csize[dc.capture_size][0], csize[dc.capture_size][1]);


    tb->clear();
    for (u32 y = 0; y < 192; y++) {
        u32 *out_line = outbuf + (y * out_width);
        u16 *in_line = th->dbg_info.eng[0].line[y].dispcap_px;
        for (u32 x = 0; x < 256; x++) {
            out_line[x] = gba_to_screen(in_line[x]);
        }
    }
}

static void render_image_view_ppu_layers(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width)
{
    auto *th = static_cast<core *>(ptr);
    if (th->clock.master_frame == 0) return;
    debugger_widget_radiogroup *engrg = &dview->options[0].radiogroup;
    debugger_widget_radiogroup *layernum = &dview->options[1].radiogroup;
    debugger_widget_radiogroup *attrkind = &dview->options[2].radiogroup;
    debugger_widget_textbox *tb = &dview->options[3].textbox;

    image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = static_cast<u32 *>(iv->img_buf[iv->draw_which_buf].ptr);
    memset(outbuf, 0, out_width * 4 * 192);

    tb->clear();
    auto &eng = th->ppu.eng2d[engrg->value];
    print_layer_info(th, &eng, layernum->value, tb);

    for (u32 y = 0; y < 192; y++) {
        //LINEBUFFER *lbuf = &th->re.out.linebuffer[y];
        u32 *out_line = outbuf + (y * out_width);
        PPU::PX *px_line;
        if (layernum->value < 4) {
            px_line = th->dbg_info.eng[engrg->value].line[y].bg[layernum->value].buf;
        }
        else {
            px_line = th->dbg_info.eng[engrg->value].line[y].sprite_buf;
        }
        for (u32 x = 0; x < 256; x++) {
            switch(attrkind->value) {
                case 0:
                    if (px_line[x].has) {
                        u32 c = px_line[x].color;
                        out_line[x] = nds_to_screen(c);
                    }
                    else {
                        out_line[x] = 0xFF000000;
                    }
                    break;
                case 1:
                    out_line[x] = 0xFF000000 | (px_line[x].has * 0xFFFFFF);
                    break;
                case 2: {
                    u32 v = px_line[x].priority + 1;
                    assert(px_line[x].priority < 5);
                    float f = ((float)v) / 5.0f;
                    f *= 255.0f;
                    v = (u32)f;
                    out_line[x] = v | (v << 8) | (v << 16) | 0xFF000000;
                    break; }
                case 3: {
                    if (px_line[x].has) {
                        u32 v = px_line[x].dbg_mode;
                        // red   purple     yellow     white
                        static const u32 mode_colors[4] = {0x0000FF, 0xFF00FF, 0x00FFFF, 0xFFFFFF};
                        out_line[x] = 0xFF000000 | mode_colors[v];
                    }
                    break; }
                case 4: { // sp_translucent
                    if (px_line[x].sp_translucent) {
                        out_line[x] = 0xFFFFFFFF;
                    }
                    break; }
                case 5: { // BPP. 4=red, 8=green, 16=blue
                    if (px_line[x].has) {
                        static const u32 mode_colors[4] = {0xFF0000FF, 0xFF00FF00, 0xFFFF0000, 0xFFFFFFFF};
                        out_line[x] = mode_colors[px_line[x].dbg_bpp];
                    }
                    break; }

            }
        }
    }
}


static void render_image_view_re_attr(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    auto *th = static_cast<core *>(ptr);
    if (th->clock.master_frame == 0) return;
    debugger_widget_radiogroup *attr_kind = &dview->options[0].radiogroup;

    u32 tm_colors[8] = {
            0xFF000000, // no texture/color
            0xFFFF0000, // color 1, blue. single tri / untextured
            0xFF0000FF, // color 2, red. single quad / modulation
            0xFF00FF00, // color 3, green. triangle strip / decal
            0xFFFFFF00, // color 4, teal. quad strip / toon
            0xFFFF00FF, // color 5, purple / highlight
            0xFF00FFFF, // color 6, yellow / shadow
            0xFFFFFFFF, // color 7, white
    };

    debugger_widget_colorkey *colorkey = &dview->options[1].colorkey;

    switch(attr_kind->value) {
        case 0: // Texture format
            colorkey->set_title("Texture Format");
            colorkey->add_item("No texture", tm_colors[0]);
            colorkey->add_item("A3I5", tm_colors[1]);
            colorkey->add_item("2bpp", tm_colors[2]);
            colorkey->add_item("4bpp", tm_colors[3]);
            colorkey->add_item("8bpp", tm_colors[4]);
            colorkey->add_item("Compressed", tm_colors[5]);
            colorkey->add_item("A5I3", tm_colors[6]);
            colorkey->add_item("Direct color", tm_colors[7]);
            break;
        case 1: // Vertex submit mode
            colorkey->set_title("Vertex submission mode");
            colorkey->add_item("Triangle", tm_colors[1]);
            colorkey->add_item("Quad", tm_colors[2]);
            colorkey->add_item("Triangle strip", tm_colors[3]);
            colorkey->add_item("Quad strip", tm_colors[4]);
            break;
        case 2: // Pixel shading mode
            colorkey->set_title("Pixel shading mode");
            colorkey->add_item("Untextured", tm_colors[1]);
            colorkey->add_item("Modulation", tm_colors[2]);
            colorkey->add_item("Decal", tm_colors[3]);
            colorkey->add_item("Toon", tm_colors[4]);
            colorkey->add_item("Highlight", tm_colors[5]);
            colorkey->add_item("Shadow", tm_colors[6]);
            break;
    }

    image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = static_cast<u32 *>(iv->img_buf[iv->draw_which_buf].ptr);
    memset(outbuf, 0, out_width * 4 * 192);

    for (u32 y = 0; y < 192; y++) {
        auto *lbuf = &th->re.out.linebuffer[y];
        u32 *out_line = outbuf + (y * out_width);
        for (u32 x = 0; x < 256; x++) {
            switch(attr_kind->value) {
                case 0: {
                    auto tp = lbuf->tex_param[x];
                    out_line[x] = tm_colors[tp.format];
                    break; }
                case 1: {
                    auto ea = lbuf->extra_attr[x];
                    if (ea.has_px)
                        out_line[x] = tm_colors[ea.vertex_mode];
                    break; }
                case 2: {
                    auto ea = lbuf->extra_attr[x];
                    if (ea.has_px)
                        out_line[x] = tm_colors[ea.shading_mode];
                    break; }
                default:
                    printf("\n!?");
            }
        }
    }
}


static void setup_cpu_trace(debugger_interface *dbgr, core *th)
{
    cvec_ptr p = dbgr->make_view(dview_trace);
    debugger_view *dview = &p.get();
    trace_view *tv = &dview->trace;
    snprintf(tv->name, sizeof(tv->name), "Trace");
    tv->add_col("Arch", 5);
    tv->add_col("Cycle#", 10);
    tv->add_col("Addr", 8);
    tv->add_col("Opcode", 8);
    tv->add_col("Disassembly", 45);
    tv->add_col("Context", 32);
    tv->autoscroll = 1;
    tv->display_end_top = 0;

    th->arm7.dbg.tvptr = tv;
    th->arm9.dbg.tvptr = tv;
}

static void setup_dbglog(debugger_interface *dbgr, core *th)
{
    cvec_ptr p = dbgr->make_view(dview_dbglog);
    debugger_view *dview = &p.get();
    dbglog_view &dv = dview->dbglog;
    th->dbg.dvptr = &dv;
    snprintf(dv.name, sizeof(dv.name), "Trace");
    dv.has_extra = true;

    dbglog_category_node &root = dv.get_category_root();
    root.children.reserve(10);
    dbglog_category_node &arm7 = root.add_node(dv, "ARM7TDMI", nullptr, 0, 0);
    arm7.children.reserve(10);
    arm7.add_node(dv, "Instruction Trace", "ARM7", NDS_CAT_ARM7_INSTRUCTION, 0x80FF80);
    th->arm7.trace.dbglog.view = &dv;
    th->arm7.trace.dbglog.id = NDS_CAT_ARM7_INSTRUCTION;
    arm7.add_node(dv, "Halt", "ARM7.H", NDS_CAT_ARM7_HALT, 0xA0AF80);

    dbglog_category_node &arm9 = root.add_node(dv, "ARM946ES", nullptr, 0, 0);
    arm9.children.reserve(10);
    arm9.add_node(dv, "Instruction Trace", "ARM9", NDS_CAT_ARM9_INSTRUCTION, 0x8080FF);
    th->arm9.dbg.dvptr = &dv;
    th->arm9.dbg.dv_id = NDS_CAT_ARM9_INSTRUCTION;
    dbglog_category_node &cart = root.add_node(dv, "Cart", nullptr, 0, 0);
    cart.children.reserve(10);
    cart.add_node(dv, "Read Start", "Cart.read", NDS_CAT_CART_READ_START, 0xFFFFFF);
    cart.add_node(dv, "Read Complete", "Cart.read.", NDS_CAT_CART_READ_COMPLETE, 0xFFFFFF);

    dbglog_category_node &dma = root.add_node(dv, "DMA", nullptr, 0, 0);
    dma.children.reserve(10);
    dma.add_node(dv, "DMA Start", "dma.start", NDS_CAT_DMA_START, 0xFFFFFF);

    dbglog_category_node &ppu = root.add_node(dv, "PPU", nullptr, 0, 0);
    ppu.children.reserve(10);
    ppu.add_node(dv, "Register Writes", "PPU.regw", NDS_CAT_PPU_REG_WRITE, 0xFFFFFF);
    ppu.add_node(dv, "BG mode changes", "PPU.BG+", NDS_CAT_PPU_BG_MODE, 0xFFFFFF);
    ppu.add_node(dv, "Misc.", "PPU.misc", NDS_CAT_PPU_MISC, 0xFFFFFF);
}


static void setup_image_view_re_wireframe(core *th, debugger_interface *dbgr)
{
    debugger_view *dview;
    th->dbg.image_views.re_wireframe = dbgr->make_view(dview_image);
    dview = &th->dbg.image_views.re_wireframe.get();
    image_view *iv = &dview->image;

    iv->width = 256;
    iv->height = 192;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (ivec2){ 0, 0 };
    iv->viewport.p[1] = (ivec2){ 256, 192 };

    iv->update_func.ptr = th;
    iv->update_func.func = &render_image_view_re_wireframe;

    snprintf(iv->label, sizeof(iv->label), "RE Wireframe");

    debugger_widgets_add_textbox(dview->options, "# poly:0", 1);

}

static void setup_image_view_dispcap(core *th, debugger_interface *dbgr) {
    debugger_view *dview;
    th->dbg.image_views.dispcap = dbgr->make_view(dview_image);
    dview = &th->dbg.image_views.dispcap.get();
    image_view *iv = &dview->image;

    iv->width = 256;
    iv->height = 192;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (ivec2) {0, 0};
    iv->viewport.p[1] = (ivec2) {256, 192};

    iv->update_func.ptr = th;
    iv->update_func.func = &render_image_view_dispcap;
    snprintf(iv->label, sizeof(iv->label), "DISPCAP view");

    debugger_widget &rg = debugger_widgets_add_radiogroup(dview->options, "Eng", 1, 0, 0);
    rg.radiogroup.add_button("EngA", 0, 1);
    rg.radiogroup.add_button("EngB", 1, 1);
    debugger_widgets_add_textbox(dview->options, "Default!", 0);
}

static void setup_image_view_ppu_layers(core *th, debugger_interface *dbgr)
{
    debugger_view *dview;
    th->dbg.image_views.ppu_layers = dbgr->make_view(dview_image);
    dview = &th->dbg.image_views.ppu_layers.get();
    image_view *iv = &dview->image;

    iv->width = 256;
    iv->height = 192;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (ivec2){ 0, 0 };
    iv->viewport.p[1] = (ivec2){ 256, 192 };

    iv->update_func.ptr = th;
    iv->update_func.func = &render_image_view_ppu_layers;
    snprintf(iv->label, sizeof(iv->label), "PPU Layer View");

    debugger_widget *rg = &debugger_widgets_add_radiogroup(dview->options, "Eng", 1, 0, 0);
    rg->radiogroup.add_button("EngA", 0, 1);
    rg->radiogroup.add_button("EngB", 1, 1);

    rg = &debugger_widgets_add_radiogroup(dview->options, "Layer", 1, 0, 0);
    rg->radiogroup.add_button("BG0", 0, 1);
    rg->radiogroup.add_button("BG1", 1, 1);
    rg->radiogroup.add_button("BG2", 2, 1);
    rg->radiogroup.add_button("BG3", 3, 1);
    rg->radiogroup.add_button("OBJ", 4, 1);

    rg = &debugger_widgets_add_radiogroup(dview->options, "View", 1, 0, 0);
    rg->radiogroup.add_button("RGB", 0, 1);
    rg->radiogroup.add_button("Has", 1, 1);
    rg->radiogroup.add_button("Priority", 2, 1);
    rg->radiogroup.add_button("Mode", 3, 1);
    rg->radiogroup.add_button("Sp.Trnslcnt.", 4, 0);
    rg->radiogroup.add_button("BPP", 5, 1);

    debugger_widgets_add_textbox(dview->options, "Layer Info", 0);
}

static void setup_image_view_ppu_info(core *th, debugger_interface *dbgr)
{
    debugger_view *dview;
    th->dbg.image_views.ppu_info = dbgr->make_view(dview_image);
    dview = &th->dbg.image_views.ppu_info.get();
    image_view *iv = &dview->image;

    iv->width = 10;
    iv->height = 10;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (ivec2){ 0, 0 };
    iv->viewport.p[1] = (ivec2){ 10, 10 };

    iv->update_func.ptr = th;
    iv->update_func.func = &render_image_view_sys_info;

    snprintf(iv->label, sizeof(iv->label), "Sys Info View");

    debugger_widgets_add_textbox(dview->options, "blah!", 1);
}

static void setup_image_view_re_attr(core *th, debugger_interface *dbgr)
{
    debugger_view *dview;
    th->dbg.image_views.re_attr = dbgr->make_view(dview_image);
    dview = &th->dbg.image_views.re_attr.get();
    image_view *iv = &dview->image;

    iv->width = 256;
    iv->height = 192;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (ivec2){ 0, 0 };
    iv->viewport.p[1] = (ivec2){ 256, 192 };

    iv->update_func.ptr = th;
    iv->update_func.func = &render_image_view_re_attr;

    snprintf(iv->label, sizeof(iv->label), "RE Attr View");

    debugger_widget *rg = &debugger_widgets_add_radiogroup(dview->options, "Attr. to show", 1, 0, 1);
    rg->radiogroup.add_button("Texture Format", 0, 1);
    rg->radiogroup.add_button("Vertex Submit Mode", 1, 1);
    rg->radiogroup.add_button("Pixel Shading Mode", 2, 0);
    debugger_widgets_add_color_key(dview->options, "Key", 1);
}


static void setup_image_view_re_output(core *th, debugger_interface *dbgr)
{
    debugger_view *dview;
    th->dbg.image_views.re_wireframe = dbgr->make_view(dview_image);
    dview = &th->dbg.image_views.re_wireframe.get();
    image_view *iv = &dview->image;

    iv->width = 256;
    iv->height = 192;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (ivec2){ 0, 0 };
    iv->viewport.p[1] = (ivec2){ 256, 192 };

    iv->update_func.ptr = th;
    iv->update_func.func = &render_image_view_re_output;
    snprintf(iv->label, sizeof(iv->label), "RE Raw Output");
}


void core::setup_debugger_interface(debugger_interface &intf) {
    dbg.interface = &intf;
    auto *dbgr = dbg.interface;

    dbgr->supported_by_core = true;
    dbgr->smallest_step = 1;
    dbgr->views.reserve(30);

    //setup_cpu_trace(dbgr, th);

    setup_dbglog(dbgr, this);
    setup_image_view_re_output(this, dbgr);
    setup_image_view_re_wireframe(this, dbgr);
    setup_image_view_re_attr(this, dbgr);
    setup_image_view_palettes(this, dbgr);
    setup_image_view_ppu_info(this, dbgr);
    setup_image_view_ppu_layers(this, dbgr);
    setup_image_view_dispcap(this, dbgr);
}
}