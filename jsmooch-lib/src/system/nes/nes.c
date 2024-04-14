//
// Created by Dave on 2/4/2024.
//
#include "assert.h"
#include "stdlib.h"
#include <stdio.h>
#include "helpers/sys_interface.h"

#include "nes.h"
#include "nes_cart.h"
#include "nes_ppu.h"
#include "nes_cpu.h"

#define JTHIS struct NES* this = (struct NES*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct NES* this

void NESJ_play(JSM);
void NESJ_pause(JSM);
void NESJ_stop(JSM);
void NESJ_get_framevars(JSM, struct framevars* out);
void NESJ_reset(JSM);
void NESJ_map_inputs(JSM, u32* bufptr, u32 bufsize);
void NESJ_get_description(JSM, struct machine_description* d);
void NESJ_killall(JSM);
u32 NESJ_finish_frame(JSM);
u32 NESJ_finish_scanline(JSM);
u32 NESJ_step_master(JSM, u32 howmany);
void NESJ_load_BIOS(JSM, struct multi_file_set* mfs);
void NESJ_load_ROM(JSM, struct multi_file_set* mfs);
void NESJ_enable_tracing(JSM);
void NESJ_disable_tracing(JSM);

void NES_new(JSM, struct JSM_IOmap *iomap)
{
    struct NES* this = (struct NES*)malloc(sizeof(struct NES));
    NES_clock_init(&this->clock);
    //NES_bus_init(&this, &this->clock);
    r2A03_init(&this->cpu, this);
    NES_PPU_init(&this->ppu, this);
    NES_cart_init(&this->cart, this);
    NES_mapper_init(&this->bus, this);

    this->ppu.last_used_buffer = 0;
    this->ppu.cur_output_num = 0;
    this->ppu.cur_output = (u16 *)iomap->out_buffers[0];
    this->ppu.out_buffer[0] = (u16 *)iomap->out_buffers[0];
    this->ppu.out_buffer[1] = (u16 *)iomap->out_buffers[1];

    this->cycles_left = 0;
    this->display_enabled = 1;
    nespad_inputs_init(&this->controller1_in);
    nespad_inputs_init(&this->controller2_in);

    jsm->ptr = (void*)this;

    jsm->get_description = &NESJ_get_description;
    jsm->finish_frame = &NESJ_finish_frame;
    jsm->finish_scanline = &NESJ_finish_scanline;
    jsm->step_master = &NESJ_step_master;
    jsm->reset = &NESJ_reset;
    jsm->load_ROM = &NESJ_load_ROM;
    jsm->load_BIOS = &NESJ_load_BIOS;
    jsm->killall = &NESJ_killall;
    jsm->map_inputs = &NESJ_map_inputs;
    jsm->get_framevars = &NESJ_get_framevars;
    jsm->enable_tracing = &NESJ_enable_tracing;
    jsm->disable_tracing = &NESJ_disable_tracing;
    jsm->play = &NESJ_play;
    jsm->pause = &NESJ_pause;
    jsm->stop = &NESJ_stop;
}

void NES_delete(JSM)
{
    JTHIS;
    //NES_CPU_delete(&this->cpu);
    //NES_PPU_delete(&this->ppu);
    /*GB_cart_delete(&this->cart);
    GB_bus_delete(&this->bus);*/
    //GB_clock_delete(&this->clock);

    NES_mapper_delete(&this->bus);
    NES_cart_delete(&this->cart);

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

void NESJ_enable_tracing(JSM)
{
    // TODO
    assert(1==0);
}

void NESJ_disable_tracing(JSM)
{
    // TODO
    assert(1==0);
}

void NESJ_play(JSM)
{
}

void NESJ_pause(JSM)
{
}

void NESJ_stop(JSM)
{
}

void NESJ_get_framevars(JSM, struct framevars* out)
{
    JTHIS;
    out->master_frame = this->clock.master_frame;
    out->x = this->ppu.line_cycle;
    out->scanline = this->clock.ppu_y;
}

void NESJ_reset(JSM)
{
    JTHIS;
    NES_clock_reset(&this->clock);
    r2A03_reset(&this->cpu);
    NES_PPU_reset(&this->ppu);
}

void NESJ_map_inputs(JSM, u32* bufptr, u32 bufsize)
{
    JTHIS;
    this->controller1_in.up = bufptr[0];
    this->controller1_in.down = bufptr[1];
    this->controller1_in.left = bufptr[2];
    this->controller1_in.right = bufptr[3];
    this->controller1_in.a = bufptr[4];
    this->controller1_in.b = bufptr[5];
    this->controller1_in.start = bufptr[6];
    this->controller1_in.select = bufptr[7];
    // Controller 2 not support yet
    r2A03_update_inputs(&this->cpu, &this->controller1_in, &this->controller2_in);
}

void NESJ_get_description(JSM, struct machine_description* d)
{
    JTHIS;
    sprintf(d->name, "Nintendo Entertainment System");
    d->fps = 60;
    d->timing = frame;
    d->display_standard = MD_NTSC;
    d->x_resolution = 256;
    d->y_resolution = 240;
    d->xrh = 4;
    d->xrw = 3;

    d->overscan.top = 8;
    d->overscan.bottom = 8;
    d->overscan.left = 8;
    d->overscan.right = 8;

    d->out_size = (256 * 240 * 4);

    struct input_map_keypoint* k;

    k = &d->keymap[0];
    k->buf_pos = 0;
    sprintf(k->name, "up");

    k = &d->keymap[1];
    k->buf_pos = 1;
    sprintf(k->name, "down");

    k = &d->keymap[2];
    k->buf_pos = 2;
    sprintf(k->name, "left");

    k = &d->keymap[3];
    k->buf_pos = 3;
    sprintf(k->name, "right");

    k = &d->keymap[4];
    k->buf_pos = 4;
    sprintf(k->name, "a");

    k = &d->keymap[5];
    k->buf_pos = 5;
    sprintf(k->name, "b");

    k = &d->keymap[6];
    k->buf_pos = 6;
    sprintf(k->name, "start");

    k = &d->keymap[7];
    k->buf_pos = 7;
    sprintf(k->name, "select");

    d->keymap_size = 8;
}

void NESJ_killall(JSM)
{

}

u32 NESJ_finish_frame(JSM)
{
    JTHIS;
    u32 current_frame = this->clock.master_frame;
    while (this->clock.master_frame == current_frame) {
        NESJ_finish_scanline(jsm);
        if (dbg.do_break) break;
    }
    return this->ppu.last_used_buffer;
}

u32 NESJ_finish_scanline(JSM)
{
    JTHIS;
    i32 cpu_step = (i32)this->clock.timing.cpu_divisor;
    i64 ppu_step = (i64)this->clock.timing.ppu_divisor;
    i32 done = 0;
    i32 start_y = this->clock.ppu_y;
    while (this->clock.ppu_y == start_y) {
        this->clock.master_clock += cpu_step;
        //this->apu.cycle(this->clock.master_clock);
        r2A03_run_cycle(&this->cpu);
        this->bus.cycle(this);
        this->clock.cpu_frame_cycle++;
        this->clock.cpu_master_clock += cpu_step;
        i64 ppu_left = (i64)this->clock.master_clock - (i64)this->clock.ppu_master_clock;
        done = 0;
        while (ppu_left >= ppu_step) {
            ppu_left -= ppu_step;
            done++;
        }
        NES_PPU_cycle(&this->ppu, done);
        this->cycles_left -= cpu_step;
        if (dbg.do_break) break;
    }
    return 0;
}

u32 NESJ_step_master(JSM, u32 howmany)
{
    JTHIS;
    this->cycles_left += howmany;
    i64 cpu_step = (i64)this->clock.timing.cpu_divisor;
    i64 ppu_step = (i64)this->clock.timing.ppu_divisor;
    u32 done = 0;
    while (this->cycles_left >= cpu_step) {
        //this->apu.cycle(this->clock.master_clock);
        this->clock.master_clock += cpu_step;
        r2A03_run_cycle(&this->cpu);
        this->bus.cycle(this);
        this->clock.cpu_frame_cycle++;
        this->clock.cpu_master_clock += cpu_step;
        i64 ppu_left = (i64)this->clock.master_clock - (i64)this->clock.ppu_master_clock;
        done = 0;
        while (ppu_left >= ppu_step) {
            ppu_left -= ppu_step;
            done++;
        }
        NES_PPU_cycle(&this->ppu, done);
        this->cycles_left -= cpu_step;
        if (dbg.do_break) break;
    }
    return 0;
}

void NESJ_load_BIOS(JSM, struct multi_file_set* mfs)
{
    printf("\nNES doesn't have a BIOS...?");
}

void NESJ_load_ROM(JSM, struct multi_file_set* mfs)
{
    JTHIS;
    struct buf* b = &mfs->files[0].buf;
    NES_cart_load_ROM_from_RAM(&this->cart, b->ptr, b->size);
    NES_mapper_set_which(&this->bus, this->cart.header.mapper_number);
    this->bus.set_cart(this, &this->cart);
    NESJ_reset(jsm);
}
