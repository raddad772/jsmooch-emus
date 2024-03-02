//
// Created by Dave on 2/7/2024.
//

#include "assert.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"

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
void SMSGGJ_map_inputs(JSM, u32* bufptr, u32 bufsize);
void SMSGGJ_get_description(JSM, struct machine_description* d);
void SMSGGJ_killall(JSM);
u32 SMSGGJ_finish_frame(JSM);
u32 SMSGGJ_finish_scanline(JSM);
u32 SMSGGJ_step_master(JSM, u32 howmany);
void SMSGGJ_load_BIOS(JSM, struct multi_file_set* mfs);
void SMSGGJ_load_ROM(JSM, struct multi_file_set* mfs);
void SMSGGJ_enable_tracing(JSM);
void SMSGGJ_disable_tracing(JSM);

u32 SMSGG_CPU_read_trace(void *ptr, u32 addr)
{
    struct SMSGG* this = (struct SMSGG*)ptr;
    return SMSGG_bus_read(this, addr, 0, 0);
}

void SMSGG_new(struct jsm_system* jsm, struct JSM_IOmap *iomap, enum jsm_systems variant, enum jsm_regions region) {
    struct SMSGG* this = (struct SMSGG*)malloc(sizeof(struct SMSGG));
    this->tracing = 0;
    SMSGG_clock_init(&this->clock, variant, region);
    SMSGG_VDP_init(&this->vdp, this, variant);
    SMSGG_mapper_sega_init(&this->mapper, variant);
    Z80_init(&this->cpu, 0);
    smspad_inputs_init(&this->controller1_in);
    smspad_inputs_init(&this->controller2_in);

    // setup tracing reads
    struct jsm_debug_read_trace a;
    a.ptr = (void *)this;
    a.read_trace = &SMSGG_CPU_read_trace;
    Z80_setup_tracing(&this->cpu, &a);

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
            break;
        case SYS_SMS2:
            this->cpu_in = &SMSGG_bus_cpu_in_sms1;
            this->cpu_out = &SMSGG_bus_cpu_out_sms1;
            break;
        case SYS_GG:
            this->cpu_in = &SMSGG_bus_cpu_in_gg;
            this->cpu_out = &SMSGG_bus_cpu_out_sms1;
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

    this->vdp.last_used_buffer = 0;
    this->vdp.cur_output_num = 0;
    this->vdp.cur_output = (u16 *)iomap->out_buffers[0];
    this->vdp.out_buffer[0] = (u16 *)iomap->out_buffers[0];
    this->vdp.out_buffer[1] = (u16 *)iomap->out_buffers[1];

    jsm->ptr = (void*)this;

    jsm->get_description = &SMSGGJ_get_description;
    jsm->finish_frame = &SMSGGJ_finish_frame;
    jsm->finish_scanline = &SMSGGJ_finish_scanline;
    jsm->step_master = &SMSGGJ_step_master;
    jsm->reset = &SMSGGJ_reset;
    jsm->load_ROM = &SMSGGJ_load_ROM;
    jsm->load_BIOS = &SMSGGJ_load_BIOS;
    jsm->killall = &SMSGGJ_killall;
    jsm->map_inputs = &SMSGGJ_map_inputs;
    jsm->get_framevars = &SMSGGJ_get_framevars;
    jsm->play = &SMSGGJ_play;
    jsm->pause = &SMSGGJ_pause;
    jsm->stop = &SMSGGJ_stop;
    jsm->enable_tracing = &SMSGGJ_enable_tracing;
    jsm->disable_tracing = &SMSGGJ_disable_tracing;

    SMSGGJ_reset(jsm);
}

void SMSGG_delete(struct jsm_system* jsm)
{
    JTHIS;
    SMSGG_mapper_sega_delete(&this->mapper);

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

void SMSGGJ_get_description(JSM, struct machine_description* d)
{
    JTHIS;
    if (this->variant == SYS_SMS1) sprintf(d->name, "Sega Master System v1");
    if (this->variant == SYS_SMS2) sprintf(d->name, "Sega Master System v2");
    if (this->variant == SYS_GG) sprintf(d->name, "Sega GameGear");

    d->fps = 60;
    d->timing = frame;
    d->display_standard = MD_NTSC;
    if (this->variant == SYS_GG) {
        d->x_resolution = 160;
        d->y_resolution = 144;
        d->xrw = 3;
        d->xrh = 4;
    }
    else {
        d->x_resolution = 256;
        d->y_resolution = 240;
        d->xrw = 4;
        d->xrh = 3;
    }

    d->overscan.top = 0;
    d->overscan.bottom = 0;
    d->overscan.left = 0;
    d->overscan.right = 0;

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
    sprintf(k->name, "b1");

    k = &d->keymap[5];
    k->buf_pos = 5;
    sprintf(k->name, "b2");

    k = &d->keymap[6];
    k->buf_pos = 6;
    sprintf(k->name, "start");

    d->keymap_size = 7;
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

void SMSGGJ_map_inputs(JSM, u32* bufptr, u32 bufsize)
{
    JTHIS;
    if (this->variant == SYS_GG) {
        this->controller1_in.up = bufptr[0];
        this->controller1_in.down = bufptr[1];
        this->controller1_in.left = bufptr[2];
        this->controller1_in.right = bufptr[3];
        this->controller1_in.b1 = bufptr[4];
        this->controller1_in.b2 = bufptr[5];
        this->controller1_in.start = bufptr[6];
    } else {
        this->controller1_in.up = bufptr[0];
        this->controller1_in.down = bufptr[1];
        this->controller1_in.left = bufptr[2];
        this->controller1_in.right = bufptr[3];
        this->controller1_in.b1 = bufptr[4];
        this->controller1_in.b2 = bufptr[5];
        this->controller2_in.up = bufptr[6];
        this->controller2_in.down = bufptr[7];
        this->controller2_in.left = bufptr[8];
        this->controller2_in.right = bufptr[9];
        this->controller2_in.b1 = bufptr[10];
        this->controller2_in.b2 = bufptr[11];
        this->controller1_in.start = bufptr[12];
    }
    update_inputs(this, &this->controller1_in, &this->controller2_in);
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
        if (dbg.do_break) return this->vdp.last_used_buffer;
    }
    return this->vdp.last_used_buffer;
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
                dbg_printf("\n\e[0;32mSMS(%06llu)r   %04x   $%02x         TCU:%d\e[0;37m", this->cpu.trace_cycles, this->cpu.pins.Addr, this->cpu.pins.D, this->cpu.regs.TCU);
                //dbg.traces.add(D_RESOURCE_TYPES.Z80, this->cpu.trace_cycles, trace_format_read('Z80', Z80_COLOR, this->cpu.trace_cycles, this->cpu.pins.Addr, this->cpu.pins.D, null, this->cpu.regs.TCU));
            }
#endif
        } else if (this->cpu.pins.IO) { // read IO port
            this->cpu.pins.D = this->cpu_in(this, this->cpu.pins.Addr, this->cpu.pins.D, 1);
#ifndef LYCODER
            if (dbg.trace_on)
                dbg_printf("\nSMS(%06llu)in  %04x   $%02x         TCU:%d", this->cpu.trace_cycles, this->cpu.pins.Addr, this->cpu.pins.D, this->cpu.regs.TCU);
#endif
        }
    }
    if (this->cpu.pins.WR) {
        if (this->cpu.pins.MRQ) { // write RAM
            if (dbg.trace_on && (this->cpu.last_trace_cycle != this->cpu.trace_cycles)) {
                //dbg.traces.add(D_RESOURCE_TYPES.Z80, this->cpu.trace_cycles, trace_format_write('Z80', Z80_COLOR, this->cpu.trace_cycles, this->cpu.pins.Addr, this->cpu.pins.D));
                this->cpu.last_trace_cycle = this->cpu.trace_cycles;
#ifndef LYCODER
                dbg_printf("\n\e[0;34mSMS(%06llu)wr  %04x   $%02x         TCU:%d\e[0;37m", this->cpu.trace_cycles, this->cpu.pins.Addr, this->cpu.pins.D, this->cpu.regs.TCU);
#endif
            }
            SMSGG_bus_write(this, this->cpu.pins.Addr, this->cpu.pins.D);
        } else if (this->cpu.pins.IO) {
            this->cpu_out(this, this->cpu.pins.Addr, this->cpu.pins.D);
#ifndef LYCODER
            if (dbg.trace_on)
                dbg_printf("\n\e[0;34mSMS(%06llu)out %04x   $%02x         TCU:%d\e[0;37m", this->cpu.trace_cycles, this->cpu.pins.Addr, this->cpu.pins.D, this->cpu.regs.TCU);
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
    if (this->variant != SYS_GG) {
        if (this->controller1_in.start)
            SMSGG_notify_NMI(this, 1);
        else
            SMSGG_notify_NMI(this, 0);
    }
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
        if (this->cpu.trace_cycles > 8000000) break;
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

void SMSGGJ_load_ROM(JSM, struct multi_file_set* mfs) {
    JTHIS;
    struct buf* b = &mfs->files[0].buf;
    SMSGG_mapper_load_ROM_from_RAM(&this->mapper, b);
    SMSGGJ_reset(jsm);
}
