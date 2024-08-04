//
// Created by Dave on 2/4/2024.
//
#include "assert.h"
#include "stdlib.h"
#include <stdio.h>
#include "helpers/physical_io.h"
#include "helpers/sys_interface.h"

#include "nes.h"
#include "nes_cart.h"
#include "nes_ppu.h"
#include "nes_cpu.h"

#define JTHIS struct NES* this = (struct NES*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct NES* this

static void NESJ_play(JSM);
static void NESJ_pause(JSM);
static void NESJ_stop(JSM);
static void NESJ_get_framevars(JSM, struct framevars* out);
static void NESJ_reset(JSM);
static void NESJ_killall(JSM);
static u32 NESJ_finish_frame(JSM);
static u32 NESJ_finish_scanline(JSM);
static u32 NESJ_step_master(JSM, u32 howmany);
static void NESJ_load_BIOS(JSM, struct multi_file_set* mfs);
static void NESJ_enable_tracing(JSM);
static void NESJ_disable_tracing(JSM);
static void NESJ_describe_io(JSM, struct cvec* IOs);

void NES_new(JSM)
{
    struct NES* this = (struct NES*)malloc(sizeof(struct NES));
    NES_clock_init(&this->clock);
    //NES_bus_init(&this, &this->clock);
    r2A03_init(&this->cpu, this);
    NES_PPU_init(&this->ppu, this);
    NES_cart_init(&this->cart, this);
    NES_mapper_init(&this->bus, this);
    snprintf(jsm->label, sizeof(jsm->label), "Nintendo Entertainment System");

    this->described_inputs = 0;

    this->cycles_left = 0;
    this->display_enabled = 1;
    this->IOs = NULL;
    jsm->ptr = (void*)this;

    jsm->finish_frame = &NESJ_finish_frame;
    jsm->finish_scanline = &NESJ_finish_scanline;
    jsm->step_master = &NESJ_step_master;
    jsm->reset = &NESJ_reset;
    jsm->load_BIOS = &NESJ_load_BIOS;
    jsm->killall = &NESJ_killall;
    jsm->get_framevars = &NESJ_get_framevars;
    jsm->enable_tracing = &NESJ_enable_tracing;
    jsm->disable_tracing = &NESJ_disable_tracing;
    jsm->play = &NESJ_play;
    jsm->pause = &NESJ_pause;
    jsm->stop = &NESJ_stop;
    jsm->describe_io = &NESJ_describe_io;
    jsm->sideload = NULL;
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

    while (cvec_len(this->IOs) > 0) {
        struct physical_io_device* pio = cvec_pop_back(this->IOs);
        if (pio->kind == HID_CART_PORT) {
            if (pio->device.cartridge_port.unload_cart) pio->device.cartridge_port.unload_cart(jsm);
        }
        physical_io_device_delete(pio);
    }

    free(jsm->ptr);
    jsm->ptr = NULL;

    jsm->finish_frame = NULL;
    jsm->finish_scanline = NULL;
    jsm->step_master = NULL;
    jsm->reset = NULL;
    jsm->load_BIOS = NULL;
    jsm->killall = NULL;
    jsm->get_framevars = NULL;
    jsm->play = NULL;
    jsm->pause = NULL;
    jsm->stop = NULL;
    jsm->enable_tracing = NULL;
    jsm->disable_tracing = NULL;
    jsm->describe_io = NULL;
}

static void NESIO_load_cart(JSM, struct multi_file_set *mfs, struct buf* sram)
{
    JTHIS;
    struct buf* b = &mfs->files[0].buf;
    NES_cart_load_ROM_from_RAM(&this->cart, b->ptr, b->size);
    NES_mapper_set_which(&this->bus, this->cart.header.mapper_number);
    this->bus.set_cart(this, &this->cart);
    NESJ_reset(jsm);
}

static void NESIO_unload_cart(JSM)
{
}

void NESJ_describe_io(JSM, struct cvec *IOs)
{
    JTHIS;
    if (this->described_inputs) return;
    this->described_inputs = 1;

    this->IOs = IOs;

    // controllers
    struct physical_io_device *c1 = cvec_push_back(this->IOs);
    struct physical_io_device *c2 = cvec_push_back(this->IOs);
    NES_joypad_setup_pio(c1, 0, "Player 1", 1);
    NES_joypad_setup_pio(c2, 1, "Player 2", 0);

    // power and reset buttons
    struct physical_io_device* chassis = cvec_push_back(IOs);
    physical_io_device_init(chassis, HID_CHASSIS, 1, 1, 1, 1);
    struct HID_digital_button* b;
    b = cvec_push_back(&chassis->device.chassis.digital_buttons);
    sprintf(b->name, "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    b = cvec_push_back(&chassis->device.chassis.digital_buttons);
    b->common_id = DBCID_ch_reset;
    sprintf(b->name, "Reset");
    b->state = 0;

    // cartridge port
    struct physical_io_device *d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_CART_PORT, 1, 1, 1, 0);
    d->device.cartridge_port.load_cart = &NESIO_load_cart;
    d->device.cartridge_port.unload_cart = &NESIO_unload_cart;

    // screen
    d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_DISPLAY, 1, 1, 0, 1);
    d->device.display.fps = 60;
    d->device.display.output[0] = malloc(256*224*2);
    d->device.display.output[1] = malloc(256*224*2);
    this->ppu.display = d;
    this->ppu.cur_output = (u16 *)d->device.display.output[0];
    d->device.display.last_written = 1;
    d->device.display.last_displayed = 1;

    this->cpu.joypad1.devices = IOs;
    this->cpu.joypad1.device_index = NES_INPUTS_PLAYER1;
    this->cpu.joypad2.devices = IOs;
    this->cpu.joypad2.device_index = NES_INPUTS_PLAYER2;
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
    return this->ppu.display->device.display.last_written;
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

