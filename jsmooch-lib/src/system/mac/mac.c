//
// Created by . on 7/24/24.
//

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "mac.h"
#include "mac_internal.h"
#include "mac_display.h"

#include "helpers/int.h"
#include "helpers/physical_io.h"
#include "helpers/sys_interface.h"

#include "component/cpu/m68000/m68000.h"

#define JTHIS struct mac* this = (struct mac*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct mac* this


static void macJ_play(JSM);
static void macJ_pause(JSM);
static void macJ_stop(JSM);
static void macJ_get_framevars(JSM, struct framevars* out);
static void macJ_reset(JSM);
static void macJ_killall(JSM);
static u32 macJ_finish_frame(JSM);
static u32 macJ_finish_scanline(JSM);
static u32 macJ_step_master(JSM, u32 howmany);
static void macJ_load_BIOS(JSM, struct multi_file_set* mfs);
static void macJ_enable_tracing(JSM);
static void macJ_disable_tracing(JSM);
static void macJ_describe_io(JSM, struct cvec* IOs);


static u32 read_trace_m68k(void *ptr, u32 addr, u32 UDS, u32 LDS) {
    struct mac* this = (struct mac*)ptr;
    return mac_mainbus_read(this, addr, UDS, LDS, this->io.cpu.last_read_data, 0);
}


void mac_new(struct jsm_system* jsm, enum mac_variants variant)
{
    struct mac* this = (struct mac*)calloc(1, sizeof(struct mac));
    this->kind = variant;
    switch(variant) {
        case mac128k:
            this->RAM_size = 128 * 1024;
            this->ROM_size = 64 * 1024;
            break;
        case mac512k:
            this->RAM_size = 512 * 1024;
            this->ROM_size = 64 * 1024;
            break;
        case macplus_1mb:
            this->RAM_size = 1 * 1024 * 1024;
            this->ROM_size = 128 * 1024;
            break;
        default:
            assert(1==2);
    }
    this->RAM = calloc(1, this->RAM_size);
    this->RAM_mask = (this->RAM_size >> 1) - 1; // in words
    this->ROM = calloc(1, this->ROM_size);
    this->ROM_mask = (this->ROM_size >> 1) - 1;
    this->kind = variant;
    M68k_init(&this->cpu, 1);
    this->cpu.trace.hpos = &this->clock.crt.hpos;
    //via6522_init(&this->via, &this->clock.master_cycles);

    struct jsm_debug_read_trace dt;
    dt.read_trace_m68k = &read_trace_m68k;
    dt.ptr = (void*)this;
    M68k_setup_tracing(&this->cpu, &dt, &this->clock.master_cycles);

    this->jsm.described_inputs = 0;
    this->jsm.IOs = NULL;
    this->jsm.cycles_left = 0;

    jsm->ptr = (void*)this;

    jsm->finish_frame = &macJ_finish_frame;
    jsm->finish_scanline = &macJ_finish_scanline;
    jsm->step_master = &macJ_step_master;
    jsm->reset = &macJ_reset;
    jsm->load_BIOS = &macJ_load_BIOS;
    jsm->killall = &macJ_killall;
    jsm->get_framevars = &macJ_get_framevars;
    jsm->enable_tracing = &macJ_enable_tracing;
    jsm->disable_tracing = &macJ_disable_tracing;
    jsm->play = &macJ_play;
    jsm->pause = &macJ_pause;
    jsm->stop = &macJ_stop;
    jsm->describe_io = &macJ_describe_io;
    jsm->sideload = NULL;

}

void mac_delete(struct jsm_system* jsm)
{
    JTHIS;

    M68k_delete(&this->cpu);
    //via6522_delete(&this->via);
    /*
    while (cvec_len(this->jsm.IOs) > 0) {
        struct physical_io_device* pio = cvec_pop_back(this->jsm.IOs);
        if (pio->kind == HID_DISC_DRIVE) {
            if (pio->device.disc_drive.remove_disc) pio->device.disc_drive.remove_disc(jsm);
        }
        physical_io_device_delete(pio);
    }*/

    if (this->RAM) free(this->RAM);
    this->RAM = NULL;

    if (this->ROM) free(this->ROM);
    this->ROM = NULL;

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

static u32 mac_keyboard_keymap[77] = {
        JK_1, JK_2, JK_3, JK_4, JK_5,
        JK_6, JK_7, JK_8, JK_9, JK_0,
        JK_A, JK_B, JK_C, JK_D, JK_E,
        JK_F, JK_G, JK_H, JK_I, JK_J,
        JK_K, JK_L, JK_M, JK_N, JK_O,
        JK_P, JK_Q, JK_R, JK_S, JK_T,
        JK_U, JK_V, JK_W, JK_X, JK_Y,
        JK_Z, JK_ENTER, JK_CAPS, JK_SHIFT,
        JK_TAB, JK_TILDE, JK_MINUS, JK_EQUALS,
        JK_BACKSPACE, JK_OPTION, JK_CMD, JK_RSHIFT,
        JK_DOT, JK_SLASH_FW, JK_APOSTROPHE,
        JK_UP, JK_DOWN, JK_LEFT, JK_RIGHT,
        JK_SQUARE_BRACKET_LEFT, JK_SQUARE_BRACKET_RIGHT,
        JK_COMMA, JK_SEMICOLON,
        JK_NUM1, JK_NUM2, JK_NUM3, JK_NUM4,
        JK_NUM5, JK_NUM6, JK_NUM7, JK_NUM8,
        JK_NUM9, JK_NUM0,
        JK_NUM_ENTER, JK_NUM_DOT, JK_NUM_PLUS, JK_NUM_MINUS,
        JK_NUM_DIVIDE, JK_NUM_STAR, JK_NUM_LOCK, JK_NUM_CLEAR
};



static void setup_keyboard(struct mac* this)
{
    struct physical_io_device *d = cvec_push_back(this->IOs);
    physical_io_device_init(d, HID_KEYBOARD, 0, 0, 1, 1);

    d->id = 0;
    d->kind = HID_KEYBOARD;
    d->connected = 1;

    struct JSM_KEYBOARD* kbd = &d->device.keyboard;
    memset(kbd, 0, sizeof(struct JSM_KEYBOARD));
    kbd->num_keys = 77;

    for (u32 i = 0; i < kbd->num_keys; i++) {
        kbd->key_defs[i] = mac_keyboard_keymap[i];
    }
}

void macJ_describe_io(JSM, struct cvec *IOs)
{
    JTHIS;
    if (this->described_inputs) return;
    this->described_inputs = 1;

    this->IOs = IOs;

    // chassis - power and reset buttons
    struct physical_io_device* chassis = cvec_push_back(IOs);
    physical_io_device_init(chassis, HID_CHASSIS, 1, 1, 1, 1);
    struct HID_digital_button* b;
    b = cvec_push_back(&chassis->device.chassis.digital_buttons);
    sprintf(b->name, "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    // keyboard
    setup_keyboard(this);
    this->keyboard.IOs = IOs;
    this->keyboard.keyboard_index = 1;

    // disc drive - 2
    struct physical_io_device *d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_DISC_DRIVE, 1, 1, 1, 0);
    d->device.disc_drive.insert_disc = NULL;
    d->device.disc_drive.remove_disc = NULL;
    d->device.disc_drive.close_drive = NULL;
    d->device.disc_drive.open_drive = NULL;
    this->disc_drive.IOs = IOs;
    this->disc_drive.disc_drive_index = 2;

    // screen - 3
    d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_DISPLAY, 1, 1, 0, 1);
    d->device.display.fps = 60;
    d->device.display.output[0] = malloc(512*342);
    d->device.display.output[1] = malloc(512*342);
    this->display.display = d;
    this->display.cur_output = (u8 *)d->device.display.output[0];
    d->device.display.last_written = 1;
    d->device.display.last_displayed = 1;
}

void macJ_play(JSM)
{
}

void macJ_pause(JSM)
{
}

void macJ_stop(JSM)
{
}

void macJ_get_framevars(JSM, struct framevars* out)
{
    JTHIS;
    out->master_frame = this->clock.master_frame;
    out->x = this->clock.crt.hpos;
    out->scanline = this->clock.crt.vpos;
    out->master_cycle = this->clock.master_cycles;
}

void macJ_load_BIOS(JSM, struct multi_file_set* mfs)
{
    JTHIS;
    struct buf* b = &mfs->files[0].buf;
    u16 *src_ptr = (u16 *)b->ptr;
    u16 *dst_ptr = &this->ROM[0];
    for (u32 i = 0; i < 32768; i++) {
        *dst_ptr = bswap_16(*src_ptr);
        src_ptr++;
        dst_ptr++;
    }
    macJ_reset(jsm);
}

void macJ_reset(JSM)
{
    JTHIS;
    M68k_reset(&this->cpu);
    mac_display_reset(this);
    mac_reset_via(this);
    this->io.irq.iswitch = this->io.irq.scc = this->io.irq.via = 0;
    this->io.eclock = 0;
    this->io.ROM_overlay = 1;
    // TODO: more
}

u32 macJ_finish_frame(JSM)
{
    JTHIS;
    u32 current_frame = this->clock.master_frame;
    u32 scanlines = 0;
    while(current_frame == this->clock.master_frame) {
        scanlines++;
        macJ_finish_scanline(jsm);
        if (dbg.do_break) break;
    }
    //printf("\nScanlines: %d", scanlines);
    return this->display.display->device.display.last_written;
}

void macJ_killall(JSM)
{
}

u32 macJ_finish_scanline(JSM)
{
    JTHIS;
    u32 current_y = this->clock.crt.vpos;

    while(current_y == this->clock.crt.vpos) {
        // TODO: add here...
        mac_step_bus(this);
        if (dbg.do_break) break;
    }
    return 0;
}

u32 macJ_step_master(JSM, u32 howmany)
{
    JTHIS;
    howmany >>= 1;
    for (u32 i = 0; i < howmany; i++) {
        mac_step_bus(this);
        if (dbg.do_break) break;
    }
    return 0;
}

void macJ_enable_tracing(JSM)
{
    // TODO
    assert(1==0);
}

void macJ_disable_tracing(JSM)
{
    // TODO
    assert(1==0);
}

