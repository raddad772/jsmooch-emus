//
// Created by Dave on 2/11/2024.
//

#include "stdio.h"
#include "malloc.h"
#include "string.h"

#include "helpers/sys_interface.h"
#include "dreamcast.h"
#include "dc_mem.h"
#include "holly.h"

#define JTHIS struct DC* this = (struct DC*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct NES* this

void DCJ_play(JSM);
void DCJ_pause(JSM);
void DCJ_stop(JSM);
void DCJ_get_framevars(JSM, struct framevars* out);
void DCJ_reset(JSM);
void DCJ_map_inputs(JSM, u32* bufptr, u32 bufsize);
void DCJ_get_description(JSM, struct machine_description* d);
void DCJ_killall(JSM);
u32 DCJ_finish_frame(JSM);
u32 DCJ_finish_scanline(JSM);
u32 DCJ_step_master(JSM, u32 howmany);
void DCJ_load_BIOS(JSM, char* buf, u32 bufsize);
void DCJ_load_ROM(JSM, char name[200], char* buf, u32 bufsize);

void DC_recalc_interrupts(struct DC* this)
{
    u32 level2 = (this->io.SB_IML2NRM & this->io.SB_ISTNRM) & 0x3FFFFF;
    u32 level4 = (this->io.SB_IML4NRM & this->io.SB_ISTNRM) & 0x3FFFFF;
    u32 level6 = (this->io.SB_IML6NRM & this->io.SB_ISTNRM) & 0x3FFFFF;
    u32 highest_level = 0;
    if (level2) highest_level = 2;
    if (level4) highest_level = 4;
    if (level6) highest_level = 6;
    SH4_set_interrupt(&this->sh4, highest_level);
}


void DC_raise_interrupt(struct DC* this, u32 imask)
{
    this->io.SB_ISTNRM |= imask;
    DC_recalc_interrupts(this);
}

void DC_new(JSM, struct JSM_IOmap *iomap)
{
    fflush(stdout);
    do_sh4_decode();
    struct DC* this = (struct DC*)malloc(sizeof(struct DC));

    SH4_init(&this->sh4);
    this->sh4.mptr = (void *)this;
    this->sh4.read8 = &DCread8;
    this->sh4.read16 = &DCread16;
    this->sh4.read32 = &DCread32;
    this->sh4.write8 = &DCwrite8;
    this->sh4.write16 = &DCwrite16;
    this->sh4.write32 = &DCwrite32;
    DC_mem_init(this);

    this->clock.frame_cycle = 0;
    this->clock.cycles_per_frame = DC_CYCLES_PER_SEC / 60;

    /*NES_clock_init(&this->clock);
    //NES_bus_init(&this, &this->clock);
    r2A03_init(&this->cpu, this);
    NES_PPU_init(&this->ppu, this);
    NES_cart_init(&this->cart, this);
    NES_mapper_init(&this->bus, this);

    */
    holly_reset(this);
    this->holly.last_used_buffer = 0;
    this->holly.cur_output_num = 0;
    this->holly.cur_output = (u32 *)iomap->out_buffers[0];
    this->holly.out_buffer[0] = (u32 *)iomap->out_buffers[0];
    this->holly.out_buffer[1] = (u32 *)iomap->out_buffers[1];
    this->holly.master_frame = 0;

    /*
    this->cycles_left = 0;
    this->display_enabled = 1;
    nespad_inputs_init(&this->controller1_in);
    nespad_inputs_init(&this->controller2_in);*/

    buf_init(&this->BIOS);
    buf_init(&this->ROM);

    DC_mem_init(this);

    jsm->ptr = (void*)this;
    jsm->which = SYS_DREAMCAST;

    jsm->get_description = &DCJ_get_description;
    jsm->finish_frame = &DCJ_finish_frame;
    jsm->finish_scanline = &DCJ_finish_scanline;
    jsm->step_master = &DCJ_step_master;
    jsm->reset = &DCJ_reset;
    jsm->load_ROM = &DCJ_load_ROM;
    jsm->load_BIOS = &DCJ_load_BIOS;
    jsm->killall = &DCJ_killall;
    jsm->map_inputs = &DCJ_map_inputs;
    jsm->get_framevars = &DCJ_get_framevars;
    jsm->play = &DCJ_play;
    jsm->pause = &DCJ_pause;
    jsm->stop = &DCJ_stop;
}

void DC_delete(struct jsm_system* jsm)
{
    JTHIS;

    buf_delete(&this->BIOS);
    buf_delete(&this->ROM);

    free(jsm->ptr);
    jsm->ptr = NULL;

    jsm->get_description = NULL;
    jsm->finish_frame = NULL;
    jsm->finish_scanline = NULL;
    jsm->step_master = NULL;
    jsm->reset = NULL;
    jsm->load_ROM = NULL;
    jsm->load_BIOS = NULL;
    jsm->killall = NULL;
    jsm->map_inputs = NULL;
    jsm->get_framevars = NULL;
    jsm->play = NULL;
    jsm->pause = NULL;
    jsm->stop = NULL;
}

void DC_copy_fb(struct DC* this, u32* where) {
    u32* ptr = (u32*)this->VRAM;
    ptr += (this->holly.FB_R_SOF1 >> 2);
    //ptr += 0x00020000;

    //printf("\nRENDER USING PTR %08llx", ptr - ((u32*)this->VRAM));

    u32* out = where;
    u8* rgb;
    for (u32 y = 0; y <= this->holly.FB_R_SIZE.fb_y_size; y++) {
        out = (where + (y * 640));
        for (u32 x = 0; x <= this->holly.FB_R_SIZE.fb_x_size; x++) {
            rgb = (u8*) ptr;
            u32 r = rgb[0];
            u32 g = rgb[1];
            u32 b = rgb[2];
            *out = (b << 16) | (g << 8) | (r) | 0xFF000000;
            ptr++;
            out++;
        }
        ptr += (this->holly.FB_R_SIZE.fb_modulus) - 1;
    }
}

void DCJ_play(JSM)
{
    JTHIS;
    // FOr now we use this to copy framebuffer
    this->holly.cur_output = this->holly.out_buffer[0];
}

void DCJ_pause(JSM)
{

}

void DCJ_stop(JSM)
{
    JTHIS;
    DC_copy_fb(this, this->holly.cur_output);
    this->holly.last_used_buffer = 0;
    this->holly.master_frame++;
}

void DCJ_get_framevars(JSM, struct framevars* out)
{
    JTHIS;
    out->master_cycle = this->sh4.trace_cycles;
    out->master_frame = this->holly.master_frame;
    out->last_used_buffer = this->holly.last_used_buffer;
}

void DCJ_reset(JSM)
{
    JTHIS;
    SH4_reset(&this->sh4);
}

void DCJ_map_inputs(JSM, u32* bufptr, u32 bufsize)
{

}

void DCJ_get_description(JSM, struct machine_description* d)
{

}

void DCJ_killall(JSM)
{

}

static void new_frame(struct DC* this)
{
    this->clock.frame_cycle = 0;
    this->clock.interrupt.vblank_in_yet = this->clock.interrupt.vblank_out_yet = 0;
    DC_recalc_frame_timing(this);
}

enum DC_frame_events {
    VBLANK_IN,
    VBLANK_OUT,
    FRAME_END
};

struct DCF_event {
    u32 cycle;
    enum DC_frame_events kind;
};

u32 DCJ_finish_frame(JSM)
{
    JTHIS;
    //DCJ_step_master(jsm, 3000000);

    new_frame(this);
    this->clock.frame_cycle = 0;

    struct DCF_event events[3];
    u32 vbi, vbo;
    if (this->clock.interrupt.vblank_in_start < this->clock.interrupt.vblank_out_start)
        // #0 = vblank in
        vbi = 0;
    else
        vbi = 1;
    vbo = vbi ^ 1;
    events[vbi].kind = VBLANK_IN;
    events[vbi].cycle = this->clock.interrupt.vblank_in_start;
    events[vbo].kind = VBLANK_OUT;
    events[vbo].cycle = this->clock.interrupt.vblank_out_start;
    events[vbo].kind = FRAME_END;
    events[vbo].cycle = this->clock.cycles_per_frame;
    this->clock.in_vblank = 1;
    i32 current_event = -1;

    while(this->clock.frame_cycle < this->clock.cycles_per_frame) {
        current_event++;
        u32 cycles_to_do = events[current_event].cycle - this->clock.frame_cycle;
        SH4_run_cycles(&this->sh4, cycles_to_do);
        this->clock.frame_cycle += cycles_to_do;
        u32 out = 0;
        switch(events[current_event].kind) {
            case VBLANK_IN: // start vblank
                holly_vblank_in(this);
                break;
            case VBLANK_OUT: // end vblank
                holly_vblank_out(this);
                break;
            case FRAME_END:
                out = 1;
                break;
        }
        if (out) break;
    }

    DC_copy_fb(this, this->holly.cur_output);
    this->holly.last_used_buffer = 0;
    this->holly.master_frame++;
    return 0;
}

u32 DCJ_finish_scanline(JSM)
{
    JTHIS;
    return 0;
}

u32 DCJ_step_master(JSM, u32 howmany)
{
    JTHIS;
    SH4_run_cycles(&this->sh4, howmany);
    return 0;
}

void DCJ_load_BIOS(JSM, char* buf, u32 bufsize)
{
    JTHIS;
    // The IP.BIN is loaded by the ROM to address 8C008000
    buf_allocate(&this->BIOS, bufsize);
    memcpy(this->BIOS.ptr, buf, bufsize);
}

void DCJ_load_ROM(JSM, char name[200], char* buf, u32 bufsize)
{
    JTHIS;
    printf("\nROM LOAD SIZE %d", bufsize);
    buf_allocate(&this->ROM, bufsize);
    memcpy(this->ROM.ptr, buf, bufsize);

    //u32 offset = 0x100000;
    u32 offset = 0x8000;

    for (u32 i = 0; i < bufsize; i++) {
        this->RAM[offset+i] = ((u8 *)this->ROM.ptr)[i];
    }

    //this->sh4.regs.PC = 0xAC010000; // for like a demo
    this->sh4.regs.PC = 0xAC008300; // for IP.BIN
    for (u32 i = 0; i < 15; i++) {
        this->sh4.regs.R[i] = 0;
        if (i < 8) this->sh4.regs.R_[i] = 0;
    }
    this->sh4.regs.R[15] = 0x8C00D400;

    // Thanks Senryoku!
    // RTE - Some interrupts jump there instead of having their own RTE, I have NO idea why.
    this->sh4.write32(this->sh4.mptr, 0x8C000010, 0x00090009); // nop nop
    this->sh4.write32(this->sh4.mptr, 0x8C000014, 0x0009002B); // rte nop
    // RTS
    this->sh4.write32(this->sh4.mptr, 0x8C000018, 0x00090009);
    this->sh4.write32(this->sh4.mptr, 0x8C00001C, 0x0009000B);

    // Write some values to HOLLY...
    this->sh4.write32(this->sh4.mptr, 0x005F8048, 6);          // FB_W_CTRL
    this->sh4.write32(this->sh4.mptr, 0x005F8060, 0x00600000); // FB_W_SOF1
    this->sh4.write32(this->sh4.mptr, 0x005F8064, 0x00600000); // FB_W_SOF2
    this->sh4.write32(this->sh4.mptr, 0x005F8044, 0x0080000D); // FB_R_CTRL
    this->sh4.write32(this->sh4.mptr, 0x005F8050, 0x00200000); // FB_R_SOF1
    this->sh4.write32(this->sh4.mptr, 0x005F8054, 0x00200000); // FB_R_SOF2
}
