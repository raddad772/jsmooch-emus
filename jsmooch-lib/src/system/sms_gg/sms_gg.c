//
// Created by Dave on 2/7/2024.
//

#include "assert.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"

#include "helpers/debugger/debugger.h"

#include "sms_gg.h"
#include "sms_gg_clock.h"
#include "sms_gg_io.h"
#include "sms_gg_mapper_sega.h"
#include "sms_gg_vdp.h"

#define JTHIS struct SMSGG* this = (struct SMSGG*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct SMSGG* this

void SMSGGJ_play(JSM) {}
void SMSGGJ_pause(JSM) {}
void SMSGGJ_stop(JSM) {}
void SMSGGJ_get_framevars(JSM, struct framevars* out);
void SMSGGJ_reset(JSM);
void SMSGGJ_killall(JSM);
u32 SMSGGJ_finish_frame(JSM);
u32 SMSGGJ_finish_scanline(JSM);
u32 SMSGGJ_step_master(JSM, u32 howmany);
void SMSGGJ_load_BIOS(JSM, struct multi_file_set* mfs);
void SMSGGJ_enable_tracing(JSM);
void SMSGGJ_disable_tracing(JSM);
void SMSGGJ_describe_io(JSM, struct cvec *IOs);
static void SMSGGIO_unload_cart(JSM);
static void SMSGGIO_load_cart(JSM, struct multi_file_set *mfs, struct buf* sram);
static void SMSGGJ_setup_debugger_interface(JSM, struct debugger_interface *intf);

u32 SMSGG_CPU_read_trace(void *ptr, u32 addr)
{
    struct SMSGG* this = (struct SMSGG*)ptr;
    return SMSGG_bus_read(this, addr, 0, 0);
}

void SMSGG_new(struct jsm_system* jsm, enum jsm_systems variant, enum jsm_regions region) {
    struct SMSGG* this = (struct SMSGG*)malloc(sizeof(struct SMSGG));
    this->tracing = 0;
    SMSGG_clock_init(&this->clock, variant, region);
    SMSGG_VDP_init(&this->vdp, this, variant);
    SMSGG_mapper_sega_init(&this->mapper, variant);
    Z80_init(&this->cpu, 0);

    // setup tracing reads
    struct jsm_debug_read_trace a;
    a.ptr = (void *)this;
    a.read_trace = &SMSGG_CPU_read_trace;
    Z80_setup_tracing(&this->cpu, &a, &this->clock.master_cycles);

    // bus init
    SMSGG_gamepad_init(&this->io.controllerA, variant, 1);
    SMSGG_controller_port_init(&this->io.portA, variant, 1);
    SMSGG_controller_port_init(&this->io.portB, variant, 2);
    this->io.portA.attached_device = (void *)&this->io.controllerA;

    this->io.disable = 0;
    this->io.gg_start = 0;

    switch(variant) {
        case SYS_SMS1:
            this->cpu_in = &SMSGG_bus_cpu_in_sms1;
            this->cpu_out = &SMSGG_bus_cpu_out_sms1;
            snprintf(jsm->label, sizeof(jsm->label), "Sega Master System");
            break;
        case SYS_SMS2:
            this->cpu_in = &SMSGG_bus_cpu_in_sms1;
            this->cpu_out = &SMSGG_bus_cpu_out_sms1;
            snprintf(jsm->label, sizeof(jsm->label), "Sega Master System II");
            break;
        case SYS_GG:
            this->cpu_in = &SMSGG_bus_cpu_in_gg;
            this->cpu_out = &SMSGG_bus_cpu_out_sms1;
            snprintf(jsm->label, sizeof(jsm->label), "Sega Game Gear");
            break;
        default:
            assert(1!=0);
            return;
    }

    // bus init done
    this->display_enabled = 1;
    this->last_frame = 0;


    SMSGG_VDP_reset(&this->vdp);
    SN76489_reset(&this->sn76489);

    this->described_inputs = 0;

    jsm->ptr = (void*)this;

    jsm->finish_frame = &SMSGGJ_finish_frame;
    jsm->finish_scanline = &SMSGGJ_finish_scanline;
    jsm->step_master = &SMSGGJ_step_master;
    jsm->reset = &SMSGGJ_reset;
    jsm->load_BIOS = &SMSGGJ_load_BIOS;
    jsm->killall = &SMSGGJ_killall;
    jsm->get_framevars = &SMSGGJ_get_framevars;
    jsm->play = &SMSGGJ_play;
    jsm->pause = &SMSGGJ_pause;
    jsm->stop = &SMSGGJ_stop;
    jsm->enable_tracing = &SMSGGJ_enable_tracing;
    jsm->disable_tracing = &SMSGGJ_disable_tracing;
    jsm->describe_io = &SMSGGJ_describe_io;
    jsm->sideload = NULL;
    jsm->setup_debugger_interface = &SMSGGJ_setup_debugger_interface;

    SMSGGJ_reset(jsm);
}

static void SMSGGJ_setup_debugger_interface(JSM, struct debugger_interface *intf)
{
    intf->supported_by_core = 0;
    printf("\nWARNING: debugger interface not supported on core: sms/gg");
}

static void new_button(struct JSM_CONTROLLER* cnt, const char* name, enum JKEYS common_id)
{
    struct HID_digital_button *b = cvec_push_back(&cnt->digital_buttons);
    sprintf(b->name, "%s", name);
    b->state = 0;
    b->id = 0;
    b->kind = DBK_BUTTON;
    b->common_id = common_id;
}


static void setup_controller(struct SMSGG* this, u32 num, const char*name, u32 connected, u32 pause_button)
{
    struct physical_io_device *d = cvec_push_back(this->IOs);
    physical_io_device_init(d, HID_CONTROLLER, 0, 0, 1, 1);

    sprintf(d->controller.name, "%s", name);
    d->id = num;
    d->kind = HID_CONTROLLER;
    d->connected = connected;

    struct JSM_CONTROLLER* cnt = &d->controller;

    // up down left right a b start select. in that order
    new_button(cnt, "up", DBCID_co_up);
    new_button(cnt, "down", DBCID_co_down);
    new_button(cnt, "left", DBCID_co_left);
    new_button(cnt, "right", DBCID_co_right);
    new_button(cnt, "1", DBCID_co_fire1);
    new_button(cnt, "2", DBCID_co_fire2);
    if (pause_button)
        new_button(cnt, "pause", DBCID_co_start);
}


static void setup_crt_sms(struct JSM_DISPLAY *d)
{
    d->standard = JSS_NTSC;
    d->enabled = 1;

    d->fps = 59.922743;
    d->fps_override_hint = 60;

    d->pixelometry.cols.left_hblank = 12;
    d->pixelometry.cols.visible = 256;
    d->pixelometry.cols.right_hblank = 74;
    d->pixelometry.offset.x = 12;

    d->pixelometry.rows.top_vblank = 0;
    d->pixelometry.rows.visible = 192;
    d->pixelometry.rows.bottom_vblank = 70;
    d->pixelometry.offset.y = 0;

    d->geometry.physical_aspect_ratio.width = 4;
    d->geometry.physical_aspect_ratio.height = 3;

    d->pixelometry.overscan.left = d->pixelometry.overscan.right = 8;
    d->pixelometry.overscan.top = d->pixelometry.overscan.bottom = 0;
}

void setup_lcd_gg(struct JSM_DISPLAY *d)
{
    d->standard = JSS_LCD;
    d->enabled = 1;

    d->fps = 59.922751;
    d->fps_override_hint = 60;

    d->pixelometry.cols.left_hblank = 12;
    d->pixelometry.cols.visible = 256;
    d->pixelometry.cols.right_hblank = 74;
    d->pixelometry.offset.x = 12;

    d->pixelometry.rows.top_vblank = 0;
    d->pixelometry.rows.visible = 192;
    d->pixelometry.rows.bottom_vblank = 70;
    d->pixelometry.offset.y = 0;

    d->geometry.physical_aspect_ratio.width = 4; // 69.4mm
    d->geometry.physical_aspect_ratio.height = 3; // 53.24mm

    // displays only the inner 160x144 of 256x192   -96x-48   so -48x-24
    d->pixelometry.overscan.left = d->pixelometry.overscan.right = 48;
    d->pixelometry.overscan.top = d->pixelometry.overscan.bottom = 24;
}

void SMSGGJ_describe_io(JSM, struct cvec *IOs)
{
    JTHIS;
    if (this->described_inputs) return;
    this->described_inputs = 1;

    this->IOs = IOs;

    // controllers
    u32 idx = 0;
    setup_controller(this, 0, "Player A", 1, true);
    if (this->variant != SYS_GG) {
        setup_controller(this, 1, "Player B", 0, false);
    }

    // power, reset, and pause buttons
    struct physical_io_device* chassis = cvec_push_back(IOs);
    physical_io_device_init(chassis, HID_CHASSIS, 1, 1, 1, 1);
    struct HID_digital_button* b;
    b = cvec_push_back(&chassis->chassis.digital_buttons);
    sprintf(b->name, "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    if (this->variant != SYS_GG) {
        b = cvec_push_back(&chassis->chassis.digital_buttons);
        b->common_id = DBCID_ch_reset;
        sprintf(b->name, "Reset");
        b->state = 0;
    }

    this->io.pause_button = NULL;

    if (this->variant != SYS_GG) {
        b = cvec_push_back(&chassis->chassis.digital_buttons);
        b->common_id = DBCID_ch_pause;
        sprintf(b->name, "Pause");
        b->state = 0;

        this->io.pause_button = b;
    }

    // cartridge port
    struct physical_io_device *d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_CART_PORT, 1, 1, 1, 0);
    d->cartridge_port.load_cart = &SMSGGIO_load_cart;
    d->cartridge_port.unload_cart = &SMSGGIO_unload_cart;

    // screen
    d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_DISPLAY, 1, 1, 0, 1);
    d->display.output[0] = malloc(256 * 224 * 2);
    d->display.output[1] = malloc(256 * 224 * 2);
    this->vdp.display_ptr = make_cvec_ptr(IOs, cvec_len(IOs)-1);
    if (this->variant == SYS_GG)
        setup_lcd_gg(&d->display);
    else
        setup_crt_sms(&d->display);

    this->vdp.display = NULL;
    this->vdp.cur_output = (u16 *)d->display.output[0];
    d->display.last_written = 1;
    d->display.last_displayed = 1;

    this->io.controllerA.devices = IOs;
    this->io.controllerA.device_index = 0;
    this->io.controllerB.devices = IOs;
    this->io.controllerB.device_index = 1;

    this->vdp.display = &((struct physical_io_device *)cpg(this->vdp.display_ptr))->display;
}

void SMSGG_delete(struct jsm_system* jsm)
{
    JTHIS;
    SMSGG_mapper_sega_delete(&this->mapper);

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
    jsm->setup_debugger_interface = NULL;
}

void SMSGGJ_get_framevars(JSM, struct framevars* out)
{
    JTHIS;
    out->master_frame = this->clock.frames_since_restart;
    out->x = this->clock.hpos;
    out->scanline = this->clock.vpos;
}

void SMSGGJ_reset(JSM)
{
    JTHIS;
    Z80_reset(&this->cpu);
    SMSGG_VDP_reset(&this->vdp);
    SMSGG_mapper_sega_reset(&this->mapper);
    SN76489_reset(&this->sn76489);
    this->last_frame = 0;
}

static void update_inputs(struct SMSGG* this, struct smspad_inputs *inp1, struct smspad_inputs *inp2)
{
    SMSGG_gamepad_buffer_input(&this->io.controllerA, inp1);
    //SMSGG_gamepad_buffer_input(&this->io.controllerB, inp2);
}

void SMSGGJ_killall(JSM)
{
}

u32 SMSGGJ_finish_frame(JSM)
{
    JTHIS;
    u32 current_frame = this->clock.frames_since_restart;
    u32 scanlines_done = 0;
    this->clock.cpu_frame_cycle = 0;
    while(current_frame == this->clock.frames_since_restart) {
        scanlines_done++;
        SMSGGJ_finish_scanline(jsm);
        if (dbg.do_break) return this->vdp.display->last_written;
    }
    return this->vdp.display->last_written;
}

u32 SMSGGJ_finish_scanline(JSM)
{
    JTHIS;
    u32 cycles_left = 684 - (this->clock.hpos * 2);
    SMSGGJ_step_master(jsm, cycles_left);
    return 0;
}

static void cpu_cycle(struct SMSGG* this)
{
    if (this->cpu.pins.RD) {
        if (this->cpu.pins.MRQ) {// read ROM/RAM
            this->cpu.pins.D = SMSGG_bus_read(this, this->cpu.pins.Addr, this->cpu.pins.D, 1);
#ifndef LYCODER
            if (dbg.trace_on) {
                // Z80(    25)r   0006   $18     TCU:1
                dbg_printf(DBGC_Z80 "\nSMS(%06llu)r   %04x   $%02x         TCU:%d" DBGC_RST, *this->cpu.trace.cycles, this->cpu.pins.Addr, this->cpu.pins.D, this->cpu.regs.TCU);
                //dbg.traces.add(D_RESOURCE_TYPES.Z80, this->cpu.trace_cycles, trace_format_read('Z80', Z80_COLOR, this->cpu.trace_cycles, this->cpu.pins.Addr, this->cpu.pins.D, null, this->cpu.regs.TCU));
            }
#endif
        } else if (this->cpu.pins.IO) { // read IO port
            this->cpu.pins.D = this->cpu_in(this, this->cpu.pins.Addr, this->cpu.pins.D, 1);
#ifndef LYCODER
            if (dbg.trace_on)
                dbg_printf(DBGC_Z80 "\nSMS(%06llu)in  %04x   $%02x         TCU:%d" DBGC_RST, *this->cpu.trace.cycles, this->cpu.pins.Addr, this->cpu.pins.D, this->cpu.regs.TCU);
#endif
        }
    }
    if (this->cpu.pins.WR) {
        if (this->cpu.pins.MRQ) { // write RAM
            if (dbg.trace_on && (this->cpu.trace.last_cycle != *this->cpu.trace.cycles)) {
                //dbg.traces.add(D_RESOURCE_TYPES.Z80, this->cpu.trace_cycles, trace_format_write('Z80', Z80_COLOR, this->cpu.trace_cycles, this->cpu.pins.Addr, this->cpu.pins.D));
                this->cpu.trace.last_cycle = *this->cpu.trace.cycles;
#ifndef LYCODER
                dbg_printf(DBGC_Z80 "\nSMS(%06llu)wr  %04x   $%02x         TCU:%d" DBGC_RST, *this->cpu.trace.cycles, this->cpu.pins.Addr, this->cpu.pins.D, this->cpu.regs.TCU);
#endif
            }
            SMSGG_bus_write(this, this->cpu.pins.Addr, this->cpu.pins.D);
        } else if (this->cpu.pins.IO) {
            this->cpu_out(this, this->cpu.pins.Addr, this->cpu.pins.D);
#ifndef LYCODER
            if (dbg.trace_on)
                dbg_printf(DBGC_Z80 "\nSMS(%06llu)out %04x   $%02x         TCU:%d" DBGC_RST, *this->cpu.trace.cycles, this->cpu.pins.Addr, this->cpu.pins.D, this->cpu.regs.TCU);
#endif
        }
    }
    Z80_cycle(&this->cpu);
    this->clock.cpu_frame_cycle += this->clock.cpu_divisor;
}

static void SMSGG_notify_NMI(struct SMSGG* this, u32 level)
{
    Z80_notify_NMI(&this->cpu, level);
}

void SMSGG_bus_notify_IRQ(struct SMSGG* this, u32 level)
{
    Z80_notify_IRQ(&this->cpu, level);
}

static void poll_pause(struct SMSGG* this)
{
    if (this->variant != SYS_GG)
        SMSGG_notify_NMI(this, this->io.pause_button->state);
}

u32 SMSGGJ_step_master(JSM, u32 howmany) {
    JTHIS;
#ifdef LYCODER
    do {
#else
        for (u32 i = 0; i < howmany; i++) {
#endif
        if (this->clock.frames_since_restart != this->last_frame) {
            this->last_frame = this->clock.frames_since_restart;
            poll_pause(this);
        }
        this->clock.cpu_master_clock++;
        this->clock.vdp_master_clock++;
        this->clock.apu_master_clock += 1;
        if (this->clock.cpu_master_clock > this->clock.cpu_divisor) {
            cpu_cycle(this);
            this->clock.cpu_master_clock -= this->clock.cpu_divisor;
        }
        if (this->clock.vdp_master_clock > this->clock.vdp_divisor) {
            SMSGG_VDP_cycle(&this->vdp);
            this->clock.vdp_master_clock -= this->clock.vdp_divisor;
        }
        if (this->clock.apu_master_clock > this->clock.apu_divisor) {
            SN76489_cycle(&this->sn76489);
            this->clock.apu_master_clock -= this->clock.apu_divisor;
        }
        if (this->clock.frames_since_restart != this->last_frame) {
            this->last_frame = this->clock.frames_since_restart;
            poll_pause(this);
        }
#ifdef LYCODER
        if (*this->cpu.trace.cycles > 8000000) break;
#endif
        if (dbg.do_break) break;
        this->clock.master_cycles++;
#ifdef LYCODER
    } while (true);
#else
    }
#endif
    return 0;
}

void SMSGGJ_enable_tracing(JSM)
{
// TODO
assert(1==0);
}

void SMSGGJ_disable_tracing(JSM) {
// TODO
    assert(1 == 0);
}

void SMSGGJ_load_BIOS(JSM, struct multi_file_set* mfs) {
    JTHIS;
    SMSGG_mapper_load_BIOS_from_RAM(&this->mapper, &mfs->files[0].buf);
}

static void SMSGGIO_unload_cart(JSM)
{

}

static void SMSGGIO_load_cart(JSM, struct multi_file_set *mfs, struct buf* sram) {
    JTHIS;
    struct buf *b = &mfs->files[0].buf;
    SMSGG_mapper_load_ROM_from_RAM(&this->mapper, b);
    SMSGGJ_reset(jsm);
}
