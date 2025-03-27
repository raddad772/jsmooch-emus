//
// Created by . on 12/4/24.
//

#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "helpers/color.h"
#include "nds.h"
#include "nds_bus.h"
#include "nds_debugger.h"

#define JTHIS struct NDS* this = (struct NDS*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct NDS* this

#define PAL_BOX_SIZE 10
#define PAL_BOX_SIZE_W_BORDER 11

static void render_image_view_palette(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width) {
    struct NDS *this = (struct NDS *) ptr;
    if (this->clock.master_frame == 0) return;
    struct image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = iv->img_buf[iv->draw_which_buf].ptr;
    memset(outbuf, 0, out_width * (((16 * PAL_BOX_SIZE_W_BORDER) * 5) + 2));
    u32 which = ((struct debugger_widget *)cvec_get(&dview->options, 0))->radiogroup.value;

    // We get a 32x32 playground, ish.
    // That's 1000 colors, but extended palettes have 4000.

    if (which == 0) {

        for (u32 eng_num = 0; eng_num < 2; eng_num++) {
            struct NDSENG2D *eng = &this->ppu.eng2d[eng_num];
            u32 upper_row = 0;
            u32 offset = 0;
            u32 y_offset = 0;
            u32 x_offset = eng_num * ((16 * PAL_BOX_SIZE_W_BORDER) + 1);
            // iv->width = ((16 * PAL_BOX_SIZE_W_BORDER * 2) + 2);

            for (u32 lohi = 0; lohi < 0x200; lohi += 0x100) { // lohi = bg or OBJ palettes
                u16 *curpal = lohi == 0 ? (u16 *) eng->mem.bg_palette : (u16 *) eng->mem.obj_palette;
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

static void setup_image_view_palettes(struct NDS* this, struct debugger_interface *dbgr)
{
    struct debugger_view *dview;
    this->dbg.image_views.palettes = debugger_view_new(dbgr, dview_image);
    dview = cpg(this->dbg.image_views.palettes);
    struct image_view *iv = &dview->image;

    iv->width = (64 * PAL_BOX_SIZE_W_BORDER) + 10;
    iv->height = (64 * PAL_BOX_SIZE_W_BORDER) + 10;

    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (struct ivec2){ 0, 0 };
    iv->viewport.p[1] = (struct ivec2){ iv->width, iv->height };

    iv->update_func.ptr = this;
    iv->update_func.func = &render_image_view_palette;
    snprintf(iv->label, sizeof(iv->label), "Palettes Viewer");

    struct debugger_widget *rg = debugger_widgets_add_radiogroup(&dview->options, "Which", 1, 0, 0);
    debugger_widget_radiogroup_add_button(rg, "Regular", 0, 1);
    debugger_widget_radiogroup_add_button(rg, "EngA Ext.BG", 1, 1);
    debugger_widget_radiogroup_add_button(rg, "EngA Ext.OBJ", 2, 1);
    debugger_widget_radiogroup_add_button(rg, "EngB Ext.BG", 3, 1);
    debugger_widget_radiogroup_add_button(rg, "EngB Ext.OBJ", 4, 1);

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

static void render_image_view_re_wireframe(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width) {
    struct NDS *this = (struct NDS *) ptr;
    if (this->clock.master_frame == 0) return;

    struct image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = iv->img_buf[iv->draw_which_buf].ptr;
    memset(outbuf, 0, out_width * 4 * 192);
    struct NDS_GE_BUFFERS *b = &this->ge.buffers[this->ge.ge_has_buffer ^ 1];

    struct debugger_widget_textbox *tb = &((struct debugger_widget *)cvec_get(&dview->options, 0))->textbox;
    debugger_widgets_textbox_clear(tb);
    debugger_widgets_textbox_sprintf(tb, "#poly:%d", b->polygon_index);

    for (u32 i = 0; i < b->polygon_index; i++) {
        struct NDS_RE_POLY *p = &b->polygon[i];
        if (p->attr.mode > 2) continue;
        struct NDS_GE_VTX_list_node *v0, *v1;
        v1 = p->vertex_list.first;
        for (u32 vn = 1; vn < p->vertex_list.len+1; vn++) {
            v0 = v1;
            v1 = v1->next;
            if (!v1) v1 = p->vertex_list.first;

            struct NDS_GE_VTX_list_node *n0, *n1;
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

static void render_image_view_re_output(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width) {
    struct NDS *this = (struct NDS *) ptr;
    if (this->clock.master_frame == 0) return;

    struct image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = iv->img_buf[iv->draw_which_buf].ptr;
    memset(outbuf, 0, out_width * 4 * 192);
    struct NDS_GE_BUFFERS *b = &this->ge.buffers[this->ge.ge_has_buffer ^ 1];

    for (u32 y = 0; y < 192; y++) {
        struct NDS_RE_LINEBUFFER *lbuf = &this->re.out.linebuffer[y];
        u32 *out_line = outbuf + (y * out_width);
        for (u32 x = 0; x < 256; x++) {
            out_line[x] = gba_to_screen(C18to15(lbuf->rgb[x]) & 0x7FFF);
        }
    }
}

static void set_info_A(char *mapstr, u32 mst, u32 ofs, u32 *mapaddr_start, u32 *mapaddr_end)
{
    switch(mst) {
        case 0:
            sprintf(mapstr, "arm9");
            *mapaddr_start = 0x06800000;
            break;
        case 1:
            sprintf(mapstr, "arm9+engA BG VRAM");
            *mapaddr_start = 0x20000 * ofs;
            break;
        case 2:
            sprintf(mapstr, "arm9+engA OBJ VRAM");
            *mapaddr_start = 0x20000 * (ofs & 1);
            break;
        case 3:
            sprintf(mapstr, "3d texture");
            *mapaddr_start = 0x20000 * ofs;
            break;
    }
    *mapaddr_end = *mapaddr_start + 0x1FFFF;
}

static void set_info_B(char *mapstr, u32 mst, u32 ofs, u32 *mapaddr_start, u32 *mapaddr_end) {
    switch (mst) {
        case 0:
            sprintf(mapstr, "arm9");
            *mapaddr_start = 0x06820000;
            break;
        case 1:
            sprintf(mapstr, "arm9+engA BG VRAM");
            *mapaddr_start = 0x20000 * ofs;
            break;
        case 2:
            sprintf(mapstr, "arm9+engA OBJ VRAM");
            *mapaddr_start = 0x20000 * (ofs & 1);
            break;
        case 3:
            sprintf(mapstr, "3d texture");
            *mapaddr_start = 0x20000 * ofs;
            break;
    }
    *mapaddr_end = *mapaddr_start + 0x1FFFF;
}

static void set_info_C(char *mapstr, u32 mst, u32 ofs, u32 *mapaddr_start, u32 *mapaddr_end) {
    switch (mst) {
        case 0:
            sprintf(mapstr, "arm9");
            *mapaddr_start = 0x06840000;
            break;
        case 1:
            sprintf(mapstr, "arm9+engA BG VRAM");
            *mapaddr_start = 0x20000 * ofs;
            break;
        case 2:
            sprintf(mapstr, "arm7");
            *mapaddr_start = 0x06000000 + (0x20000 * (ofs & 1));
            break;
        case 3:
            sprintf(mapstr, "3d texture");
            *mapaddr_start = 0x20000 * ofs;
            break;
        case 4:
            sprintf(mapstr, "arm9+engB BG VRAM");
            *mapaddr_start = 0;
            break;
    }
    *mapaddr_end = *mapaddr_start + 0x1FFFF;
}

static void set_info_D(char *mapstr, u32 mst, u32 ofs, u32 *mapaddr_start, u32 *mapaddr_end) {
    switch (mst) {
        case 0:
            sprintf(mapstr, "arm9");
            *mapaddr_start = 0x06860000;
            break;
        case 1:
            sprintf(mapstr, "arm9+engA BG VRAM");
            *mapaddr_start = 0x20000 * ofs;
            break;
        case 2:
            sprintf(mapstr, "arm7");
            *mapaddr_start = 0x06000000 + (0x20000 * (ofs & 1));
            break;
        case 3:
            sprintf(mapstr, "3d texture");
            *mapaddr_start = 0x20000 * ofs;
            break;
        case 4:
            sprintf(mapstr, "arm9+engB OBJ VRAM");
            *mapaddr_start = 0;
            break;
    }
    *mapaddr_end = *mapaddr_start + 0x1FFFF;
}

static void set_info_E(char *mapstr, u32 mst, u32 ofs, u32 *mapaddr_start, u32 *mapaddr_end) {
    switch (mst) {
        case 0:
            sprintf(mapstr, "arm9");
            *mapaddr_start = 0x06880000;
            break;
        case 1:
            sprintf(mapstr, "arm9+engA BG VRAM");
            *mapaddr_start = 0;
            break;
        case 2:
            sprintf(mapstr, "arm9+engA OBJ VRAM");
            *mapaddr_start = 0;
            break;
        case 3:
            sprintf(mapstr, "3d palette 0-3");
            *mapaddr_start = 0;
            break;
        case 4:
            sprintf(mapstr, "engA BG extended palette 0-3");
            *mapaddr_start = 0;
            *mapaddr_end = 0x7FFF;
            return;
    }
    *mapaddr_end = *mapaddr_start + 0xFFFF;
}

static void set_info_F(char *mapstr, u32 mst, u32 ofs, u32 *mapaddr_start, u32 *mapaddr_end) {
    switch (mst) {
        case 0:
            sprintf(mapstr, "arm9");
            *mapaddr_start = 0x06890000;
            break;
        case 1:
            sprintf(mapstr, "arm9+engA BG VRAM");
            *mapaddr_start =  (0x4000*(ofs & 1)) + (0x10000 * ((ofs >> 1) & 1));
            break;
        case 2:
            sprintf(mapstr, "arm9+engA OBJ VRAM");
            *mapaddr_start = (0x4000*(ofs & 1)) + (0x10000 * ((ofs >> 1) & 1));
            break;
        case 3:
            sprintf(mapstr, "3d palette %d", (ofs & 1) + ((ofs & 2) << 1));
            *mapaddr_start = 0x4000 * ((ofs & 1) + ((ofs & 2) << 1));
            break;
        case 4: {
            char *ptr = mapstr;
            ptr += sprintf(ptr, "engA BG extended palette ");
            if ((ofs & 1) == 0) {
                ptr += sprintf(ptr, "slot 0-1");
                *mapaddr_start = 0;
            }
            else {
                ptr += sprintf(ptr, "slot 2-3");
                *mapaddr_start = 0x4000;
            }
            break; }
        case 5:
            sprintf(mapstr, "engA OBJ extended palette");
            *mapaddr_start = 0;
            *mapaddr_end = 0x1FFF;
            return;
    }
    *mapaddr_end = *mapaddr_start + 0x3FFF;
}

static void set_info_G(char *mapstr, u32 mst, u32 ofs, u32 *mapaddr_start, u32 *mapaddr_end) {
    switch (mst) {
        case 0:
            sprintf(mapstr, "arm9");
            *mapaddr_start = 0x06894000;
            break;
        case 1:
            sprintf(mapstr, "arm9+engA BG VRAM");
            *mapaddr_start =  (0x4000 * (ofs & 1)) + (0x10000 * ((ofs >> 1) & 1));
            break;
        case 2:
            sprintf(mapstr, "arm9+engA OBJ VRAM");
            *mapaddr_start = (0x4000 * (ofs & 1)) + (0x10000 * ((ofs >> 1) & 1));
            break;
        case 3:
            sprintf(mapstr, "3d palette %d", ((ofs & 1) + ((ofs & 2) << 1)));
            *mapaddr_start = 0x4000 * ((ofs & 1) + ((ofs & 2) << 1));
            break;
        case 4: {
            char *ptr = mapstr;
            ptr += sprintf(ptr, "engA BG extended palette ");
            if ((ofs & 1) == 0) {
                ptr += sprintf(ptr, "slot 0-1");
                *mapaddr_start = 0;
            }
            else {
                ptr += sprintf(ptr, "slot 2-3");
                *mapaddr_start = 0x4000;
            }
            break; }
        case 5:
            sprintf(mapstr, "engA OBJ extended palette");
            *mapaddr_start = 0;
            *mapaddr_end = 0x1FFF;
            return;
    }
    *mapaddr_end = *mapaddr_start + 0x3FFF;
}

static void set_info_H(char *mapstr, u32 mst, u32 ofs, u32 *mapaddr_start, u32 *mapaddr_end) {
    switch (mst) {
        case 0:
            sprintf(mapstr, "arm9");
            *mapaddr_start = 0x06898000;
            break;
        case 1:
            sprintf(mapstr, "arm9+engB BG VRAM");
            *mapaddr_start =  0;
            break;
        case 2:
            sprintf(mapstr, "engB BG extended palette 0-3");
            *mapaddr_start = 0;
            break;
    }
    *mapaddr_end = *mapaddr_start + 0x7FFF;
}

static void set_info_I(char *mapstr, u32 mst, u32 ofs, u32 *mapaddr_start, u32 *mapaddr_end) {
    switch (mst) {
        case 0:
            sprintf(mapstr, "arm9");
            *mapaddr_start = 0x068A0000;
            break;
        case 1:
            sprintf(mapstr, "arm9+engB BG VRAM");
            *mapaddr_start =  0x8000;
            break;
        case 2:
            sprintf(mapstr, "arm9+engB OBJ VRAM");
            *mapaddr_start =  0;
            break;
        case 3:
            sprintf(mapstr, "engB OBJ extended palette");
            *mapaddr_start = 0;
            *mapaddr_end = 0x1FFF;
            return;
    }
    *mapaddr_end = *mapaddr_start + 0x3FFF;
}

static void classify_bg_kind(struct NDS *this, struct NDSENG2D *eng, u32 bgnum, struct jsm_string *js)
{
    struct NDS_PPU_bg *bg = &eng->bg[bgnum];

#define cl(...) { jsm_string_sprintf(js, __VA_ARGS__); return; }
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

static void print_layer_info(struct NDS *this, struct NDSENG2D *eng, u32 bgnum, struct debugger_widget_textbox *tb)
{
    if (bgnum < 4) {
        struct NDS_PPU_bg *bg = &eng->bg[bgnum];
        debugger_widgets_textbox_sprintf(tb, "\n BG%d:", bgnum);
        if (bg->enable) debugger_widgets_textbox_sprintf(tb, "on ");
        else debugger_widgets_textbox_sprintf(tb, "off");
        debugger_widgets_textbox_sprintf(tb, "  kind:");
        classify_bg_kind(this, eng, bgnum, &tb->contents);
//mosaic:%d  screen_size:%d  8bpp:%d\", bgnum, bg->enable,bg->mosaic_enable, bg->screen_size, bg->bpp8);
        debugger_widgets_textbox_sprintf(tb, "\n   hscroll:%d  vscroll:%d  ", bg->hscroll, bg->vscroll);
        if (bgnum < 2) {
            debugger_widgets_textbox_sprintf(tb, "ext_palette:%d", bg->ext_pal_slot);
        }
        else {
            debugger_widgets_textbox_sprintf(tb, "disp_overflow:%d", bg->display_overflow);
        }
    }
    else {
        debugger_widgets_textbox_sprintf(tb, "\n OBJ  on:%d  mosaic:%d", eng->obj.enable, eng->mosaic.obj.enable);
        debugger_widgets_textbox_sprintf(tb, "\n  ext_palettes:%d  tile_1d:%d  bitmap_1d:%d", eng->io.obj.extended_palettes, eng->io.obj.tile.map_1d, eng->io.obj.bitmap.map_1d);
    }
}

static void render_image_view_sys_info(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width) {
    struct NDS *this = (struct NDS *) ptr;
    //memset(ptr, 0, out_width * 4 * 10);
    struct debugger_widget_textbox *tb = &((struct debugger_widget *)cvec_get(&dview->options, 0))->textbox;
    debugger_widgets_textbox_clear(tb);
    for (u32 ppun = 0; ppun < 2; ppun++) {
        struct NDSENG2D *eng = &this->ppu.eng2d[ppun];
        debugger_widgets_textbox_sprintf(tb, "eng%c  display_mode:%d  bg_mode:%d  bg_ext_pal:%d", ppun == 0 ? 'A' : 'B', eng->io.display_mode, eng->io.bg_mode, eng->io.bg.extended_palettes);
        debugger_widgets_textbox_sprintf(tb, "\n");
        if (ppun ^ this->ppu.io.display_swap) debugger_widgets_textbox_sprintf(tb, "top   ");
        else debugger_widgets_textbox_sprintf(tb, "bottom");
        if (ppun == 0) debugger_widgets_textbox_sprintf(tb, "  do_3d:%d", eng->io.bg.do_3d);

        for (u32 bgnum = 0; bgnum < 5; bgnum++) {
            print_layer_info(this, eng, bgnum, tb);
        }


        debugger_widgets_textbox_sprintf(tb, "\n\n");
    }

    // Now do VRAM mappings
    char mapstr[50];
    u32 mapaddr_start, mapaddr_end;
    for (u32 bnum = 0; bnum < 9; bnum++) {
        u32 mst = this->mem.vram.io.bank[bnum].mst;
        u32 ofs = this->mem.vram.io.bank[bnum].ofs;

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
        debugger_widgets_textbox_sprintf(tb, "\nVRAM %c  MST:%d  OFS:%d  mapping:%s  start:%x  end:%x", 'A' + bnum, mst, ofs, mapstr, mapaddr_start, mapaddr_end);
    }

}

static void render_image_view_ppu_layers(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width)
{
    struct NDS *this = (struct NDS *) ptr;
    if (this->clock.master_frame == 0) return;
    struct debugger_widget_radiogroup *engrg = &((struct debugger_widget *)cvec_get(&dview->options, 0))->radiogroup;
    struct debugger_widget_radiogroup *layernum = &((struct debugger_widget *)cvec_get(&dview->options, 1))->radiogroup;
    struct debugger_widget_radiogroup *attrkind = &((struct debugger_widget *)cvec_get(&dview->options, 2))->radiogroup;
    struct debugger_widget_textbox *tb = &((struct debugger_widget *)cvec_get(&dview->options, 3))->textbox;

    struct image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = iv->img_buf[iv->draw_which_buf].ptr;
    memset(outbuf, 0, out_width * 4 * 192);

    debugger_widgets_textbox_clear(tb);
    struct NDSENG2D *eng = &this->ppu.eng2d[engrg->value];
    print_layer_info(this, eng, layernum->value, tb);

    for (u32 y = 0; y < 192; y++) {
        //struct NDS_RE_LINEBUFFER *lbuf = &this->re.out.linebuffer[y];
        u32 *out_line = outbuf + (y * out_width);
        struct NDS_PX *px_line;
        if (layernum->value < 4) {
            px_line = this->dbg_info.eng[engrg->value].line[y].bg[layernum->value].buf;
        }
        else {
            px_line = this->dbg_info.eng[engrg->value].line[y].sprite_buf;
        }
        for (u32 x = 0; x < 256; x++) {
            if (attrkind->value == 0) {
                if (px_line[x].has) {
                    u32 c = px_line[x].color;
                    out_line[x] = gba_to_screen(c);
                }
                else {
                    out_line[x] = 0xFF000000;
                }
            }
            else {
                out_line[x] = 0xFF000000 | (px_line[x].has * 0xFFFFFF);
            }
        }
    }
}


static void render_image_view_re_attr(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width) {
    struct NDS *this = (struct NDS *) ptr;
    if (this->clock.master_frame == 0) return;
    struct debugger_widget_radiogroup *attr_kind = &((struct debugger_widget *)cvec_get(&dview->options, 0))->radiogroup;

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

    struct debugger_widget_colorkey *colorkey = &((struct debugger_widget *)cvec_get(&dview->options, 1))->colorkey;

    switch(attr_kind->value) {
        case 0: // Texture format
            debugger_widgets_colorkey_set_title(colorkey, "Texture Format");
            debugger_widgets_colorkey_add_item(colorkey, "No texture", tm_colors[0]);
            debugger_widgets_colorkey_add_item(colorkey, "A3I5", tm_colors[1]);
            debugger_widgets_colorkey_add_item(colorkey, "2bpp", tm_colors[2]);
            debugger_widgets_colorkey_add_item(colorkey, "4bpp", tm_colors[3]);
            debugger_widgets_colorkey_add_item(colorkey, "8bpp", tm_colors[4]);
            debugger_widgets_colorkey_add_item(colorkey, "Compressed", tm_colors[5]);
            debugger_widgets_colorkey_add_item(colorkey, "A5I3", tm_colors[6]);
            debugger_widgets_colorkey_add_item(colorkey, "Direct color", tm_colors[7]);
            break;
        case 1: // Vertex submit mode
            debugger_widgets_colorkey_set_title(colorkey, "Vertex submission mode");
            debugger_widgets_colorkey_add_item(colorkey, "Triangle", tm_colors[1]);
            debugger_widgets_colorkey_add_item(colorkey, "Quad", tm_colors[2]);
            debugger_widgets_colorkey_add_item(colorkey, "Triangle strip", tm_colors[3]);
            debugger_widgets_colorkey_add_item(colorkey, "Quad strip", tm_colors[4]);
            break;
        case 2: // Pixel shading mode
            debugger_widgets_colorkey_set_title(colorkey, "Pixel shading mode");
            debugger_widgets_colorkey_add_item(colorkey, "Untextured", tm_colors[1]);
            debugger_widgets_colorkey_add_item(colorkey, "Modulation", tm_colors[2]);
            debugger_widgets_colorkey_add_item(colorkey, "Decal", tm_colors[3]);
            debugger_widgets_colorkey_add_item(colorkey, "Toon", tm_colors[4]);
            debugger_widgets_colorkey_add_item(colorkey, "Highlight", tm_colors[5]);
            debugger_widgets_colorkey_add_item(colorkey, "Shadow", tm_colors[6]);
            break;
    }

    struct image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = iv->img_buf[iv->draw_which_buf].ptr;
    memset(outbuf, 0, out_width * 4 * 192);

    for (u32 y = 0; y < 192; y++) {
        struct NDS_RE_LINEBUFFER *lbuf = &this->re.out.linebuffer[y];
        u32 *out_line = outbuf + (y * out_width);
        for (u32 x = 0; x < 256; x++) {
            switch(attr_kind->value) {
                case 0: {
                    union NDS_GE_TEX_PARAM tp = lbuf->tex_param[x];
                    out_line[x] = tm_colors[tp.format];
                    break; }
                case 1: {
                    union NDS_RE_EXTRA_ATTR ea = lbuf->extra_attr[x];
                    if (ea.has_px)
                        out_line[x] = tm_colors[ea.vertex_mode];
                    break; }
                case 2: {
                    union NDS_RE_EXTRA_ATTR ea = lbuf->extra_attr[x];
                    if (ea.has_px)
                        out_line[x] = tm_colors[ea.shading_mode];
                    break; }
                default:
                    printf("\n!?");
            }
        }
    }
}


static void setup_cpu_trace(struct debugger_interface *dbgr, struct NDS *this)
{
    struct cvec_ptr p = debugger_view_new(dbgr, dview_trace);
    struct debugger_view *dview = cpg(p);
    struct trace_view *tv = &dview->trace;
    snprintf(tv->name, sizeof(tv->name), "Trace");
    trace_view_add_col(tv, "Arch", 5);
    trace_view_add_col(tv, "Cycle#", 10);
    trace_view_add_col(tv, "Addr", 8);
    trace_view_add_col(tv, "Opcode", 8);
    trace_view_add_col(tv, "Disassembly", 45);
    trace_view_add_col(tv, "Context", 32);
    tv->autoscroll = 1;
    tv->display_end_top = 0;

    this->arm7.dbg.tvptr = tv;
    this->arm9.dbg.tvptr = tv;
}

static void setup_dbglog(struct debugger_interface *dbgr, struct NDS *this)
{
    struct cvec_ptr p = debugger_view_new(dbgr, dview_dbglog);
    struct debugger_view *dview = cpg(p);
    struct dbglog_view *dv = &dview->dbglog;
    this->dbg.dvptr = dv;
    snprintf(dv->name, sizeof(dv->name), "Trace");
    dv->has_extra = 1;

    struct dbglog_category_node *root = dbglog_category_get_root(dv);
    struct dbglog_category_node *arm7 = dbglog_category_add_node(dv, root, "ARM7TDMI", NULL, 0, 0);
    dbglog_category_add_node(dv, arm7, "Instruction Trace", "ARM7", NDS_CAT_ARM7_INSTRUCTION, 0x80FF80);
    this->arm7.trace.dbglog.view = dv;
    this->arm7.trace.dbglog.id = NDS_CAT_ARM7_INSTRUCTION;
    dbglog_category_add_node(dv, arm7, "Halt", "ARM7.H", NDS_CAT_ARM7_HALT, 0xA0AF80);

    struct dbglog_category_node *arm9 = dbglog_category_add_node(dv, root, "ARM946ES", NULL, 0, 0);
    dbglog_category_add_node(dv, arm9, "Instruction Trace", "ARM9", NDS_CAT_ARM9_INSTRUCTION, 0x8080FF);
    this->arm9.dbg.dvptr = dv;
    this->arm9.dbg.dv_id = NDS_CAT_ARM9_INSTRUCTION;

    struct dbglog_category_node *cart = dbglog_category_add_node(dv, root, "Cart", NULL, 0, 0);
    dbglog_category_add_node(dv, cart, "Read Start", "Cart.read", NDS_CAT_CART_READ_START, 0xFFFFFF);
    dbglog_category_add_node(dv, cart, "Read Complete", "Cart.read.", NDS_CAT_CART_READ_COMPLETE, 0xFFFFFF);

    struct dbglog_category_node *dma = dbglog_category_add_node(dv, root, "DMA", NULL, 0, 0);
    dbglog_category_add_node(dv, dma, "DMA Start", "dma.start", NDS_CAT_DMA_START, 0xFFFFFF);

    struct dbglog_category_node *ppu = dbglog_category_add_node(dv, root, "PPU", NULL, 0, 0);
    dbglog_category_add_node(dv, ppu, "Register Writes", "PPU.regw", NDS_CAT_PPU_REG_WRITE, 0xFFFFFF);
    dbglog_category_add_node(dv, ppu, "BG mode changes", "PPU.BG+", NDS_CAT_PPU_BG_MODE, 0xFFFFFF);
    dbglog_category_add_node(dv, ppu, "Misc.", "PPU.misc", NDS_CAT_PPU_MISC, 0xFFFFFF);
}


static void setup_image_view_re_wireframe(struct NDS* this, struct debugger_interface *dbgr)
{
    struct debugger_view *dview;
    this->dbg.image_views.re_wireframe = debugger_view_new(dbgr, dview_image);
    dview = cpg(this->dbg.image_views.re_wireframe);
    struct image_view *iv = &dview->image;

    iv->width = 256;
    iv->height = 192;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (struct ivec2){ 0, 0 };
    iv->viewport.p[1] = (struct ivec2){ 256, 192 };

    iv->update_func.ptr = this;
    iv->update_func.func = &render_image_view_re_wireframe;

    snprintf(iv->label, sizeof(iv->label), "RE Wireframe");

    debugger_widgets_add_textbox(&dview->options, "# poly:0", 1);

}

static void setup_image_view_ppu_layers(struct NDS *this, struct debugger_interface *dbgr)
{
    struct debugger_view *dview;
    this->dbg.image_views.ppu_layers = debugger_view_new(dbgr, dview_image);
    dview = cpg(this->dbg.image_views.ppu_layers);
    struct image_view *iv = &dview->image;

    iv->width = 256;
    iv->height = 192;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (struct ivec2){ 0, 0 };
    iv->viewport.p[1] = (struct ivec2){ 256, 192 };

    iv->update_func.ptr = this;
    iv->update_func.func = &render_image_view_ppu_layers;
    snprintf(iv->label, sizeof(iv->label), "PPU Layer View");

    struct debugger_widget *rg = debugger_widgets_add_radiogroup(&dview->options, "Eng", 1, 0, 0);
    debugger_widget_radiogroup_add_button(rg, "EngA", 0, 1);
    debugger_widget_radiogroup_add_button(rg, "EngB", 1, 1);

    rg = debugger_widgets_add_radiogroup(&dview->options, "Layer", 1, 0, 0);
    debugger_widget_radiogroup_add_button(rg, "BG0", 0, 1);
    debugger_widget_radiogroup_add_button(rg, "BG1", 1, 1);
    debugger_widget_radiogroup_add_button(rg, "BG2", 2, 1);
    debugger_widget_radiogroup_add_button(rg, "BG3", 3, 1);
    debugger_widget_radiogroup_add_button(rg, "OBJ", 4, 1);

    rg = debugger_widgets_add_radiogroup(&dview->options, "View", 1, 0, 0);
    debugger_widget_radiogroup_add_button(rg, "RGB", 0, 1);
    debugger_widget_radiogroup_add_button(rg, "Has", 1, 1);

    debugger_widgets_add_textbox(&dview->options, "Layer Info", 0);
}
static void setup_image_view_ppu_info(struct NDS *this, struct debugger_interface *dbgr)
{
    struct debugger_view *dview;
    this->dbg.image_views.ppu_info = debugger_view_new(dbgr, dview_image);
    dview = cpg(this->dbg.image_views.ppu_info);
    struct image_view *iv = &dview->image;

    iv->width = 10;
    iv->height = 10;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (struct ivec2){ 0, 0 };
    iv->viewport.p[1] = (struct ivec2){ 10, 10 };

    iv->update_func.ptr = this;
    iv->update_func.func = &render_image_view_sys_info;

    snprintf(iv->label, sizeof(iv->label), "Sys Info View");

    debugger_widgets_add_textbox(&dview->options, "blah!", 1);
}

static void setup_image_view_re_attr(struct NDS* this, struct debugger_interface *dbgr)
{
    struct debugger_view *dview;
    this->dbg.image_views.re_attr = debugger_view_new(dbgr, dview_image);
    dview = cpg(this->dbg.image_views.re_attr);
    struct image_view *iv = &dview->image;

    iv->width = 256;
    iv->height = 192;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (struct ivec2){ 0, 0 };
    iv->viewport.p[1] = (struct ivec2){ 256, 192 };

    iv->update_func.ptr = this;
    iv->update_func.func = &render_image_view_re_attr;

    snprintf(iv->label, sizeof(iv->label), "RE Attr View");

    struct debugger_widget *rg = debugger_widgets_add_radiogroup(&dview->options, "Attr. to show", 1, 0, 1);
    debugger_widget_radiogroup_add_button(rg, "Texture Format", 0, 1);
    debugger_widget_radiogroup_add_button(rg, "Vertex Submit Mode", 1, 1);
    debugger_widget_radiogroup_add_button(rg, "Pixel Shading Mode", 2, 0);
    struct debugger_widget *key = debugger_widgets_add_color_key(&dview->options, "Key", 1);
}


static void setup_image_view_re_output(struct NDS* this, struct debugger_interface *dbgr)
{
    struct debugger_view *dview;
    this->dbg.image_views.re_wireframe = debugger_view_new(dbgr, dview_image);
    dview = cpg(this->dbg.image_views.re_wireframe);
    struct image_view *iv = &dview->image;

    iv->width = 256;
    iv->height = 192;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (struct ivec2){ 0, 0 };
    iv->viewport.p[1] = (struct ivec2){ 256, 192 };

    iv->update_func.ptr = this;
    iv->update_func.func = &render_image_view_re_output;
    snprintf(iv->label, sizeof(iv->label), "RE Raw Output");
}


void NDSJ_setup_debugger_interface(JSM, struct debugger_interface *dbgr) {
    JTHIS;
    this->dbg.interface = dbgr;

    dbgr->supported_by_core = 0;
    dbgr->smallest_step = 1;
    cvec_lock_reallocs(&dbgr->views);

    setup_image_view_palettes(this, dbgr);
    //setup_cpu_trace(dbgr, this);

    setup_dbglog(dbgr, this);
    setup_image_view_re_output(this, dbgr);
    setup_image_view_re_wireframe(this, dbgr);
    setup_image_view_re_attr(this, dbgr);
    setup_image_view_ppu_info(this, dbgr);
    setup_image_view_ppu_layers(this, dbgr);
}
