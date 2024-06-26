//
// Created by . on 6/1/24.
//
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "component/controller/genesis3/genesis3.h"
#include "genesis.h"
#include "genesis_bus.h"
#include "genesis_cart.h"

#define JTHIS struct genesis* this = (struct genesis*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct genesis* this

static void genesisJ_play(JSM);
static void genesisJ_pause(JSM);
static void genesisJ_stop(JSM);
static void genesisJ_get_framevars(JSM, struct framevars* out);
static void genesisJ_reset(JSM);
static void genesisJ_killall(JSM);
static u32 genesisJ_finish_frame(JSM);
static u32 genesisJ_finish_scanline(JSM);
static u32 genesisJ_step_master(JSM, u32 howmany);
static void genesisJ_load_BIOS(JSM, struct multi_file_set* mfs);
static void genesisJ_enable_tracing(JSM);
static void genesisJ_disable_tracing(JSM);
static void genesisJ_describe_io(JSM, struct cvec* IOs);

void genesis_new(JSM)
{
    struct genesis* this = (struct genesis*)malloc(sizeof(struct genesis));
    Z80_init(&this->z80, 0);
    M68k_init(&this->m68k, 1);
    genesis_clock_init(&this->clock);
    genesis_cart_init(&this->cart);

    this->jsm.described_inputs = 0;
    this->jsm.IOs = NULL;
    this->jsm.cycles_left = 0;

    jsm->ptr = (void*)this;

    jsm->finish_frame = &genesisJ_finish_frame;
    jsm->finish_scanline = &genesisJ_finish_scanline;
    jsm->step_master = &genesisJ_step_master;
    jsm->reset = &genesisJ_reset;
    jsm->load_BIOS = &genesisJ_load_BIOS;
    jsm->killall = &genesisJ_killall;
    jsm->get_framevars = &genesisJ_get_framevars;
    jsm->enable_tracing = &genesisJ_enable_tracing;
    jsm->disable_tracing = &genesisJ_disable_tracing;
    jsm->play = &genesisJ_play;
    jsm->pause = &genesisJ_pause;
    jsm->stop = &genesisJ_stop;
    jsm->describe_io = &genesisJ_describe_io;
    jsm->sideload = NULL;
}

void genesis_delete(JSM) {
    JTHIS;

    M68k_delete(&this->m68k);

    while (cvec_len(this->jsm.IOs) > 0) {
        struct physical_io_device* pio = cvec_pop_back(this->jsm.IOs);
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

static void genesisIO_load_cart(JSM, struct multi_file_set *mfs, struct buf* sram)
{
    JTHIS;
    struct buf* b = &mfs->files[0].buf;
    genesis_cart_load_ROM_from_RAM(&this->cart, b->ptr, b->size);
    //genesis_mapper_set_which(&this->bus, this->cart.header.mapper_number);
    //this->bus.set_cart(this, &this->cart);
    genesisJ_reset(jsm);
}

static void genesisIO_unload_cart(JSM)
{
}

void genesisJ_describe_io(JSM, struct cvec *IOs)
{
    JTHIS;
    if (this->jsm.described_inputs) return;
    this->jsm.described_inputs = 1;

    this->jsm.IOs = IOs;

    // controllers
    struct physical_io_device *c1 = cvec_push_back(this->jsm.IOs);
    struct physical_io_device *c2 = cvec_push_back(this->jsm.IOs);
    genesis3_setup_pio(c1, 0, "Player 1", 1);
    genesis3_setup_pio(c2, 1, "Player 2", 0);

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
    d->device.cartridge_port.load_cart = &genesisIO_load_cart;
    d->device.cartridge_port.unload_cart = &genesisIO_unload_cart;

    // screen
    d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_DISPLAY, 1, 1, 0, 1);
    d->device.display.fps = 60;
    d->device.display.output[0] = malloc(320*448*2);
    d->device.display.output[1] = malloc(328*448*2);
    this->vdp.display = d;
    this->vdp.cur_output = (u16 *)d->device.display.output[0];
    d->device.display.last_written = 1;
    d->device.display.last_displayed = 1;
}

void genesisJ_enable_tracing(JSM)
{
    // TODO
    assert(1==0);
}

void genesisJ_disable_tracing(JSM)
{
    // TODO
    assert(1==0);
}

void genesisJ_play(JSM)
{
}

void genesisJ_pause(JSM)
{
}

void genesisJ_stop(JSM)
{
}

void genesisJ_get_framevars(JSM, struct framevars* out)
{
    JTHIS;
    //out->master_frame = this->clock.master_frame;
    //out->x = this->ppu.line_cycle;
    //out->scanline = this->clock.ppu_y;
}

void genesisJ_reset(JSM)
{
    JTHIS;
    Z80_reset(&this->z80);
    M68k_reset(&this->m68k);
    genesis_clock_reset(&this->clock);
}


void genesisJ_killall(JSM)
{

}

u32 genesisJ_finish_frame(JSM)
{
    JTHIS;
    u32 current_frame = this->clock.master_frame;
    while (this->clock.master_frame == current_frame) {
        genesisJ_finish_scanline(jsm);
        if (dbg.do_break) break;
    }
    return this->vdp.display->device.display.last_written;
}

u32 genesisJ_finish_scanline(JSM)
{
    JTHIS;
    /*i32 cpu_step = (i32)this->clock.timing.cpu_divisor;
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
        genesis_PPU_cycle(&this->ppu, done);
        this->cycles_left -= cpu_step;
        if (dbg.do_break) break;
    }*/
    return 0;
}

#define MIN(a,b) ((a) < (b) ? (a) : (b))

u32 genesisJ_step_master(JSM, u32 howmany)
{
    JTHIS;
    this->jsm.cycles_left += howmany;
    while (this->jsm.cycles_left >= 0) {
        i32 biggest_step = MIN(MIN(this->clock.vdp.cycles_til_clock, this->clock.m68k.cycles_til_clock), this->clock.z80.cycles_til_clock);
        this->jsm.cycles_left -= biggest_step;
        this->clock.m68k.cycles_til_clock -= biggest_step;
        this->clock.z80.cycles_til_clock -= biggest_step;
        this->clock.vdp.cycles_til_clock -= biggest_step;
        if (this->clock.m68k.cycles_til_clock <= 0) {
            this->clock.m68k.cycles_til_clock += this->clock.m68k.clock_divisor;
            genesis_cycle_m68k(this);
        }
        if (this->clock.z80.cycles_til_clock <= 0) {
            this->clock.z80.cycles_til_clock += this->clock.z80.clock_divisor;
            genesis_cycle_z80(this);
        }
        if (this->clock.vdp.cycles_til_clock <= 0) {
            this->clock.vdp.cycles_til_clock += this->clock.vdp.clock_divisor;
            genesis_cycle_vdp(this);
        }

        if (dbg.do_break) break;
    }
    return 0;
}

void genesisJ_load_BIOS(JSM, struct multi_file_set* mfs)
{
    printf("\nSega Genesis doesn't have a BIOS...?");
}

