//
// Created by Dave on 2/11/2024.
//

#include "assert.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#include "gdi.h"
#include "gdrom.h"
#include "helpers/scheduler.h"
#include "helpers/sys_interface.h"
#include "dreamcast.h"
#include "dc_mem.h"
#include "holly.h"
#include "controller.h"

#define IP_BIN

#define JTHIS struct DC* this = (struct DC*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct NES* this

#define DC_CYCLES_PER_FRAME (DC_CYCLES_PER_SEC / 60)

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
void DCJ_load_BIOS(JSM, struct multi_file_set* mfs);
void DCJ_load_ROM(JSM, struct multi_file_set* mfs);
void DCJ_enable_tracing(JSM);
void DCJ_disable_tracing(JSM);
static void DC_schedule_frame(struct DC* this);
static void new_frame(struct DC* this);

void DC_new(JSM, struct JSM_IOmap *iomap)
{
    fflush(stdout);
    do_sh4_decode();
    struct DC* this = (struct DC*)malloc(sizeof(struct DC));
    scheduler_init(&this->scheduler);

    dbg.dcptr = this;

    SH4_init(&this->sh4, &this->scheduler);
    this->sh4.mptr = (void *)this;
    this->sh4.read = &DCread_noins;
    this->sh4.write = &DCwrite;
    this->sh4.fetch_ins = &DCfetch_ins;
    SH4_give_memaccess(&this->sh4, &this->sh4mem);
    DC_mem_init(this);

    for (u32 i = 0; i < 4; i++) {
        MAPLE_port_init(&this->maple.ports[i]);
    }
    DC_controller_init(&this->c1);
    DC_controller_connect(this, 0, &this->c1);

    this->gdrom.gdi.num_tracks = 0;

    GDROM_init(this);
    GDROM_reset(this);

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

    new_frame(this);

    buf_init(&this->BIOS);
    buf_init(&this->ROM);
    buf_init(&this->flash.buf);
    GDROM_reset(this);

    this->settings.broadcast = 0; // NTSC
    this->settings.region = 1; // EN
    this->settings.language = 1; // EN

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
    jsm->enable_tracing = &DCJ_enable_tracing;
    jsm->disable_tracing = &DCJ_disable_tracing;
    jsm->play = &DCJ_play;
    jsm->pause = &DCJ_pause;
    jsm->stop = &DCJ_stop;
}

void DC_delete(struct jsm_system* jsm)
{
    JTHIS;

    if (this->gdrom.gdi.num_tracks > 0)
        GDI_delete(&this->gdrom.gdi);

    buf_delete(&this->BIOS);
    buf_delete(&this->ROM);
    buf_delete(&this->flash.buf);
    scheduler_delete(&this->scheduler);

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
    jsm->enable_tracing = NULL;
    jsm->disable_tracing = NULL;
}

void DC_copy_fb(struct DC* this, u32* where) {
    u32* ptr = (u32*)this->VRAM;
    ptr += this->holly.FB_R_SOF1.field;
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

void DCJ_enable_tracing(JSM)
{
    // TODO
    assert(1==0);
}

void DCJ_disable_tracing(JSM)
{
    // TODO
    assert(1==0);
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
    this->clock.frame_start_cycle = this->sh4.trace_cycles;
    this->holly.master_frame++;
    this->clock.interrupt.vblank_in_yet = this->clock.interrupt.vblank_out_yet = 0;
    DC_recalc_frame_timing(this);
    DC_copy_fb(this, this->holly.cur_output);
    this->holly.last_used_buffer = 0;
    this->holly.master_frame++;
    DC_schedule_frame(this);
}

enum DC_frame_events {
    evt_EMPTY=0,
    evt_FRAME_START,
    evt_VBLANK_IN,
    evt_VBLANK_OUT,
    evt_FRAME_END,
};

struct DCF_event {
    u32 cycle;
    enum DC_frame_events kind;
};

static void DC_schedule_frame(struct DC* this)
{
    // events
    // frame start @0.
    // vblank_in_start @?
    // vblank_out_start @?
    // frame end @200mil
    scheduler_clear(&this->scheduler);
    scheduler_add(&this->scheduler, 0, evt_FRAME_START);
    if (this->clock.interrupt.vblank_in_start < this->clock.interrupt.vblank_out_start) {
        scheduler_add(&this->scheduler, this->clock.interrupt.vblank_in_start, evt_VBLANK_IN);
        scheduler_add(&this->scheduler, this->clock.interrupt.vblank_out_start, evt_VBLANK_OUT);
    }
    else {
        scheduler_add(&this->scheduler, this->clock.interrupt.vblank_out_start, evt_VBLANK_OUT);
        scheduler_add(&this->scheduler, this->clock.interrupt.vblank_in_start, evt_VBLANK_IN);
    }
    scheduler_add(&this->scheduler, DC_CYCLES_PER_FRAME, evt_FRAME_END);
}

u32 DCJ_finish_frame(JSM)
{
    JTHIS;
    if (scheduler_at_end(&this->scheduler)) {
        DC_schedule_frame(this);
    }
    DCJ_step_master(jsm, DC_CYCLES_PER_FRAME - this->scheduler.timecode);
    return 0;
}

u32 DCJ_finish_scanline(JSM)
{
    JTHIS;
    return 0;
}

void DC_scheduler_pprint(struct DC* this)
{
    for (u32 i = 0; i < this->scheduler.array.used_len; i++) {
        struct scheduler_event* evt = &this->scheduler.array.events[i];
        printf("\nSCHED EVENT CYC:%8llu ", evt->timecode);
        switch(evt->event) {
            case evt_EMPTY:
                printf("EMPTY");
                break;
            case evt_FRAME_END:
                printf("FRAME_END");
                break;
            case evt_FRAME_START:
                printf("FRAME_START");
                break;
            case evt_VBLANK_IN:
                printf("VBLANK_IN");
                break;
            case evt_VBLANK_OUT:
                printf("VBLANK_OUT");
                break;
        }
    }
}

u32 DCJ_step_master(JSM, u32 howmany)
{
    JTHIS;
    i64 steps_done = 0;

    struct scheduler_event* next_evt = scheduler_next_event(&this->scheduler);
    i64 til_next_event = scheduler_til_next_event(&this->scheduler);
    if (next_evt == NULL) {
        printf("\nOOPS!6");
        DC_schedule_frame(this);
        til_next_event = scheduler_til_next_event(&this->scheduler);
    }
    u32 quit = 0;

    while(!quit) {
        while(til_next_event < 1) {
            // handle an event if any exist
            next_evt = scheduler_next_event(&this->scheduler);
            if (next_evt == NULL) {
                printf("\nHOW DID THIS HAPPEN5;");
                break;
            }
            u32 new_scheduler = 0;

            switch ((enum DC_frame_events) next_evt->event) {
                case evt_FRAME_START:
                    this->clock.frame_cycle = 0;
                    this->clock.in_vblank = 1;
                    break;
                case evt_VBLANK_IN:
                    holly_vblank_in(this);
                    break;
                case evt_VBLANK_OUT:
                    holly_vblank_out(this);
                    break;
                case evt_EMPTY:
                    break;
                case evt_FRAME_END:
                    //printf("\nEVENT: FRAME END %llu", this->sh4.trace_cycles);
                    new_scheduler = 1;
                    new_frame(this);
                    break;
                default:
                    printf("\nUNKNOWN FRAME EVENT %d", this->clock.frame_cycle);
                    break;
            }
            if (quit) {
                break;
            }
            if (!new_scheduler) scheduler_advance_event(&this->scheduler);
            til_next_event = scheduler_til_next_event(&this->scheduler);
            if (dbg.do_break) break;
        }
        if (quit || dbg.do_break) {
            break;
        }
        if ((til_next_event+steps_done) > howmany) {
            til_next_event = howmany - steps_done;
            if (til_next_event < 1) {
                break; // ran outta cycles before an event
            }
        }
        i64 old_cycles = (i64)this->sh4.trace_cycles;
        SH4_run_cycles(&this->sh4, til_next_event);
        i64 ran_cycles = (i64)this->sh4.trace_cycles - old_cycles;
        this->clock.frame_cycle += ran_cycles;
        scheduler_ran(&this->scheduler, ran_cycles);
        steps_done += ran_cycles;
        til_next_event = scheduler_til_next_event(&this->scheduler);
        if (dbg.do_break) break;
    }
    return steps_done;
}

void DCJ_load_BIOS(JSM, struct multi_file_set* mfs)
{
    JTHIS;
    // We expect dc_boot.bin and dc_flash.bin
    u32 found = 0;
    for (u32 i = 0; i < mfs->num_files; i++) {
        struct read_file_buf* rfb = &mfs->files[i];
        if (!strcmp(rfb->name, "dc_boot.bin")) {
            buf_copy(&this->BIOS, &rfb->buf);
            found++;
        }
        else if (!strcmp(rfb->name, "dc_flash.bin")) {
            buf_copy(&this->flash.buf, &rfb->buf);
            found++;
        }
        else {
            printf("\n UNKNOWN FILE? %s", rfb->name);
        }
    }
    if (found != 2) {
        printf("\nHmmm what?!?!?! DC BIOS LOAD FAILURE!?!?!");
        fflush(stdout);
    }
}

void DCJ_load_ROM(JSM, struct multi_file_set* mfs)
{
    JTHIS;
/*
    struct GDI_image foo;
    GDI_init(&foo);
    const char *homeDir = getenv("HOME");

    if (!homeDir) {
        struct passwd* pwd = getpwuid(getuid());
        if (pwd)
            homeDir = pwd->pw_dir;
    }

    char PATH[500];
    sprintf(PATH, "%s/Documents/emu/rom/dreamcast/crazy_taxi", homeDir);
    printf("\nHEY! %s", PATH);
    GDI_load(PATH, "crazytaxi.gdi", &foo);

    GDI_delete(&foo);

 */
    GDI_load(mfs->files[0].path, mfs->files[0].name, &this->gdrom.gdi);
}

void DCJ_old_load_ROM(JSM, struct multi_file_set* mfs)
{
    JTHIS;
    struct buf* b = &mfs->files[0].buf;
    buf_copy(&this->ROM, b);

    u32 offset = 0x8000;

    for (u32 i = 0; i < this->ROM.size; i++) {
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
    this->sh4.write(this->sh4.mptr, 0x8C000010, 0x00090009, DC32); // nop nop
    this->sh4.write(this->sh4.mptr, 0x8C000014, 0x0009002B, DC32); // rte nop
    // RTS
    this->sh4.write(this->sh4.mptr, 0x8C000018, 0x00090009, DC32);
    this->sh4.write(this->sh4.mptr, 0x8C00001C, 0x0009000B, DC32);

    // Write some values to HOLLY...
    this->sh4.write(this->sh4.mptr, 0x005F8048, 6, DC32);          // FB_W_CTRL
    this->sh4.write(this->sh4.mptr, 0x005F8060, 0x00600000, DC32); // FB_W_SOF1
    this->sh4.write(this->sh4.mptr, 0x005F8064, 0x00600000, DC32); // FB_W_SOF2
    this->sh4.write(this->sh4.mptr, 0x005F8044, 0x0080000D, DC32); // FB_R_CTRL
    this->sh4.write(this->sh4.mptr, 0x005F8050, 0x00200000, DC32); // FB_R_SOF1
    this->sh4.write(this->sh4.mptr, 0x005F8054, 0x00200000, DC32); // FB_R_SOF2

    DC_schedule_frame(this);
}
