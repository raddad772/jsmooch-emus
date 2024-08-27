//
// Created by . on 8/8/24.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "component/cpu/m6502/m6502.h"
#include "component/cpu/m6502/m6502_disassembler.h"
#include "nes_debugger.h"
#include "nes.h"

#define JTHIS struct NES* this = (struct NES*)jsm->ptr
#define JSM struct jsm_system* jsm

static void get_dissasembly(void *nesptr, struct debugger_interface *dbgr, struct disassembly_view *dview, struct disassembly_entry *entry)
{
    struct NES* this = (struct NES*)nesptr;
    M6502_disassemble_entry(&this->cpu.cpu, entry);
}


static struct disassembly_vars get_disassembly_vars(void *nesptr, struct debugger_interface *dbgr, struct disassembly_view *dv)
{
    struct NES* this = (struct NES*)nesptr;
    struct disassembly_vars dvar;
    dvar.address_of_executing_instruction = this->cpu.cpu.PCO;
    dvar.current_clock_cycle = this->clock.trace_cycles;
    return dvar;
}

static int render_p(struct cpu_reg_context*ctx, void *outbuf, size_t outbuf_sz)
{
    return snprintf(outbuf, outbuf_sz, "%c%c-%c%c%c%c%c",
                    ctx->int32_data & 0x80 ? 'N' : 'n',
                    ctx->int32_data & 0x40 ? 'V' : 'v',
                    ctx->int32_data & 0x10 ? 'B' : 'b',
                    ctx->int32_data & 8 ? 'D' : 'd',
                    ctx->int32_data & 4 ? 'I' : 'i',
                    ctx->int32_data & 2 ? 'Z' : 'z',
                    ctx->int32_data & 1 ? 'C' : 'c'
    );
}

static void fill_disassembly_view(void *nesptr, struct debugger_interface *dbgr, struct disassembly_view *dview)
{
    struct NES* this = (struct NES*)nesptr;

    this->dbg.dasm.A->int8_data = this->cpu.cpu.regs.A;
    this->dbg.dasm.X->int8_data = this->cpu.cpu.regs.X;
    this->dbg.dasm.Y->int8_data = this->cpu.cpu.regs.Y;
    this->dbg.dasm.PC->int16_data = this->cpu.cpu.regs.PC;
    this->dbg.dasm.S->int8_data = this->cpu.cpu.regs.S;
    this->dbg.dasm.P->int8_data = M6502_regs_P_getbyte(&this->cpu.cpu.regs.P);
}

static void create_and_bind_registers(struct NES* this, struct disassembly_view *dv)
{
    u32 tkindex = 0;
    struct cpu_reg_context *rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "A");
    rg->kind = RK_int8;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "X");
    rg->kind = RK_int8;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "Y");
    rg->kind = RK_int8;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "PC");
    rg->kind = RK_int16;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "S");
    rg->kind = RK_int8;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "P");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = &render_p;

#define BIND(dn, index) this->dbg.dasm. dn = cvec_get(&dv->cpu.regs, index)
    BIND(A, 0);
    BIND(X, 1);
    BIND(Y, 2);
    BIND(PC, 3);
    BIND(S, 4);
    BIND(P, 5);
#undef BIND
}

static void setup_disassembly_view(struct NES* this, struct debugger_interface *dbgr)
{
    struct cvec_ptr p = debugger_view_new(dbgr, dview_disassembly);
    struct debugger_view *dview = cpg(p);
    struct disassembly_view *dv = &dview->disassembly;
    dv->addr_column_size = 4;
    dv->has_context = 0;
    jsm_string_sprintf(&dv->processor_name, "m6502");

    create_and_bind_registers(this, dv);
    dv->fill_view.ptr = (void *)this;
    dv->fill_view.func = &fill_disassembly_view;

    dv->get_disassembly.ptr = (void *)this;
    dv->get_disassembly.func = &get_dissasembly;

    dv->get_disassembly_vars.ptr = (void *)this;
    dv->get_disassembly_vars.func = &get_disassembly_vars;
}

static void render_image_view_nametables(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width)
{
    struct NES* this = (struct NES*)ptr;
    if (this->clock.master_frame == 0) return;
    struct DBGNESROW *row = &this->dbg_data.rows[0];
    struct image_view *iv = &dview->image;
    iv->viewport.exists = 1;
    iv->viewport.p[0].x = row->io.x_scroll % 512;
    iv->viewport.p[0].y = row->io.y_scroll % 480;
    iv->viewport.p[1].x = (iv->viewport.p[0].x + 256);
    iv->viewport.p[1].y = (iv->viewport.p[0].x + 240);
    iv->draw_which_buf ^= 1;
    u32 *outbuf = iv->img_buf[iv->draw_which_buf].ptr;

    // Draw 4 nametables 2x2
    for (u32 nt_x = 0; nt_x < 2; nt_x++) {
        for (u32 nt_y = 0; nt_y < 2; nt_y++) {
            u32 ntnum = (nt_y * 2) + nt_x;
            u32 nt_base_addr = (ntnum * 0x400) | 0x2000;
            u32 img_y = nt_y * 240;
            assert(img_y < 480);
            // 30 rows of 32 tiles, 256x240
            for (u32 get_tile_y = 0; get_tile_y < 30; get_tile_y++) {
                for (u32 inner_tile_y = 0; inner_tile_y < 8; inner_tile_y++) {
                    u32 img_x = nt_x * 256;
                    if ((img_y >= iv->viewport.p[0].y) && (img_y < iv->viewport.p[1].y)) {
                        row = &this->dbg_data.rows[img_y - iv->viewport.p[0].y];
                    }
                    u32 bg_pattern_table = row->io.bg_pattern_table;
                    for (u32 get_tile_x = 0; get_tile_x < 32; get_tile_x++) {
                        u32 addr = nt_base_addr + (32 * get_tile_y) + get_tile_x;
                        u32 tilenum = this->bus.PPU_read_noeffect(this, addr);
                        u32 tile_addr = 0x1000 * bg_pattern_table;
                        u32 rownum = inner_tile_y;
                        u32 taddr = tile_addr + (tilenum << 4) + rownum;
                        u32 p0 = this->bus.PPU_read_noeffect(this, taddr);
                        u32 p1 = this->bus.PPU_read_noeffect(this, taddr + 8);

                        // p0 is the lower bit of each pixel
                        // LSB = rightmost pixel, so leftmost is MSB
                        for (u32 i = 0; i < 8; i++) {
                            u32 bclr = ((p0 >> 7) & 1) | ((p1 >> 6) & 2);
                            p0 <<= 1;
                            p1 <<= 1;
                            u32 c = 0xFF000000 | (0x555555 * bclr);
                            if ((img_y >= iv->viewport.p[0].y) && (img_y <= iv->viewport.p[1].y)) {
                                if (img_x == iv->viewport.p[0].x) c = 0xFFFF0000;
                                if (img_x == iv->viewport.p[1].x) c = 0xFF808000;
                            }
                            if (img_y == iv->viewport.p[0].y) c = 0xFF00FFFF;

                            outbuf[(img_y * out_width) + img_x] = c;
                            img_x++;
                            assert(img_x <= 512);
                        } // i
                    } // get_tile_x
                    img_y++;
                    assert(img_y <= 480);
                } // inner_tile_y
            }
        }
    }
#undef get
}

static void setup_image_view_nametables(struct NES* this, struct debugger_interface *dbgr)
{
    this->dbg.image_views.nametables = debugger_view_new(dbgr, dview_image);
    struct debugger_view *dview = cpg(this->dbg.image_views.nametables);
    struct image_view *iv = &dview->image;
    // 512x480

    iv->width = 512;
    iv->height = 480;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (struct ivec2){ 0, 0 };
    iv->viewport.p[1] = (struct ivec2){ 256, 240 };

    iv->update_func.ptr = this;
    iv->update_func.func = &render_image_view_nametables;

    sprintf(iv->label, "Nametable Viewer");
}

static void setup_events_view(struct NES* this, struct debugger_interface *dbgr)
{
    // Setup events view
    this->dbg.events.view = debugger_view_new(dbgr, dview_events);
    struct debugger_view *dview = cpg(this->dbg.events.view);
    struct events_view *ev = &dview->events;

    for (u32 i = 0; i < 2; i++) {
        ev->display[i].width = 341;
        ev->display[i].height = 262;
        ev->display[i].buf = NULL;
        ev->display[i].frame_num = 0;
    }
    ev->associated_display = this->ppu.display_ptr;

    cvec_grow(&ev->events, DBG_NES_EVENT_MAX);
    DEBUG_REGISTER_EVENT_CATEGORY("CPU events", DBG_NES_CATEGORY_CPU);
    DEBUG_REGISTER_EVENT_CATEGORY("PPU events", DBG_NES_CATEGORY_PPU);

    DEBUG_REGISTER_EVENT("IRQ", 0xFF0000, DBG_NES_CATEGORY_CPU, DBG_NES_EVENT_IRQ);
    DEBUG_REGISTER_EVENT("NMI", 0x00FF00, DBG_NES_CATEGORY_CPU, DBG_NES_EVENT_NMI);
    DEBUG_REGISTER_EVENT("OAM DMA", 0x609020, DBG_NES_CATEGORY_CPU, DBG_NES_EVENT_OAM_DMA);

    DEBUG_REGISTER_EVENT("Write $2006", 0xFF0080, DBG_NES_CATEGORY_PPU, DBG_NES_EVENT_W2006);
    DEBUG_REGISTER_EVENT("Write $2007", 0x0080FF, DBG_NES_CATEGORY_PPU, DBG_NES_EVENT_W2007);

    SET_EVENT_VIEW(this->cpu.cpu);
    SET_EVENT_VIEW(this->ppu);

    SET_CPU_CPU_EVENT_ID(DBG_NES_EVENT_IRQ, IRQ);
    SET_CPU_CPU_EVENT_ID(DBG_NES_EVENT_NMI, NMI);

    event_view_begin_frame(this->dbg.events.view);
}

void NESJ_setup_debugger_interface(struct jsm_system *jsm, struct debugger_interface *dbgr)
{
    JTHIS;
    this->dbg.interface = dbgr;

    dbgr->supported_by_core = 1;
    dbgr->smallest_step = 1;

    setup_disassembly_view(this, dbgr);
    setup_events_view(this, dbgr);
    setup_image_view_nametables(this, dbgr);
}

