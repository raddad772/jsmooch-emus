//
// Created by . on 8/29/24.
//

#include <cstdio>
#include <cassert>
#include <cstring>
#include <cstdlib>

#include "helpers/int.h"
#include "helpers/sys_interface.h"

#include "component/cpu/m6502/m6502.h"
#include "component/cpu/m6502/m6502_opcodes.h"

#include "apple2.h"
#include "apple2_internal.h"
#include "iou.h"

#define JTHIS struct apple2* this = (apple2*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct apple2* this

static void apple2J_play(JSM);
static void apple2J_pause(JSM);
static void apple2J_stop(JSM);
static void apple2J_get_framevars(JSM, framevars* out);
static void apple2J_reset(JSM);
static void apple2J_killall(JSM);
static u32 apple2J_finish_frame(JSM);
static u32 apple2J_finish_scanline(JSM);
static u32 apple2J_step_master(JSM, u32 howmany);
static void apple2J_load_BIOS(JSM, multi_file_set* mfs);
static void apple2J_enable_tracing(JSM);
static void apple2J_disable_tracing(JSM);
static void apple2J_describe_io(JSM, cvec* IOs);

u32 apple2_CPU_read_trace(void *ptr, u32 addr);

static void apple2J_setup_debugger_interface(JSM, debugger_interface *intf);

/*
$D000 - $DFFF (53248 - 57343): Bank-Switched RAM (2 Banks RAM, 1 Bank ROM)
$E000 - $FFFF (57344 - 65535): Bank-Switched RAM (1 Bank RAM, 1 Bank ROM)
 */
void apple2_new(JSM)
{
    struct apple2* this = (apple2*)malloc(sizeof(apple2));
    this->described_inputs = 0;

    simplebuf8_init(&this->mmu.RAM);
    simplebuf8_init(&this->mmu.ROM);
    simplebuf8_init(&this->iou.ROM);

    simplebuf8_allocate(&this->mmu.RAM, 64 * 1024);

    cvec_ptr_init(&this->iou.display_ptr);
    cvec_ptr_init(&this->iou.display_ptr);

    M6502_init(&this->cpu, M6502_decoded_opcodes);

    snprintf(jsm->label, sizeof(jsm->label), "Apple II 48K RAM");

    // setup tracing reads
    struct jsm_debug_read_trace a;
    a.ptr = (void *)this;
    a.read_trace = &apple2_CPU_read_trace;
    M6502_setup_tracing(&this->cpu, &a, &this->clock.master_cycles);
    M6502_reset(&this->cpu);

    jsm->ptr = (void*)this;

    jsm->finish_frame = &apple2J_finish_frame;
    jsm->finish_scanline = &apple2J_finish_scanline;
    jsm->step_master = &apple2J_step_master;
    jsm->reset = &apple2J_reset;
    jsm->load_BIOS = &apple2J_load_BIOS;
    jsm->get_framevars = &apple2J_get_framevars;
    jsm->play = &apple2J_play;
    jsm->pause = &apple2J_pause;
    jsm->stop = &apple2J_stop;
    jsm->describe_io = &apple2J_describe_io;
    jsm->sideload = NULL;
    jsm->setup_debugger_interface = &apple2J_setup_debugger_interface;
}

void apple2_delete(JSM)
{
    JTHIS;
    while (cvec_len(this->IOs) > 0) {
        struct physical_io_device* pio = cvec_pop_back(this->IOs);
        physical_io_device_delete(pio);
    }

    simplebuf8_delete(&this->mmu.RAM);
    simplebuf8_delete(&this->mmu.ROM);

    free(jsm->ptr);
    jsm->ptr = NULL;

    jsm_clearfuncs(jsm);
}

static void apple2J_setup_debugger_interface(JSM, debugger_interface *intf)
{
    intf->supported_by_core = 0;
    printf("\nWARNING: debugger interface not supported on core: apple2");
}

static void new_button(JSM_CONTROLLER* cnt, const char* name, enum JKEYS common_id)
{
    struct HID_digital_button *b = cvec_push_back(&cnt->digital_buttons);
    snprintf(b->name, sizeof(b->name), "%s", name);
    b->state = 0;
    b->id = 0;
    b->kind = DBK_BUTTON;
    b->common_id = common_id;
}


#define MAX_WIDTH 560
#define MAX_HEIGHT 192

static void setup_crt(JSM_DISPLAY *d)
{
    d->standard = JSS_NTSC;
    d->enabled = 1;

    d->fps=59.92;
    d->fps_override_hint = 60;

    d->pixelometry.cols.left_hblank = 0;
    d->pixelometry.cols.visible = MAX_WIDTH;
    d->pixelometry.cols.max_visible = MAX_WIDTH;
    d->pixelometry.cols.right_hblank = 96;
    d->pixelometry.offset.x = 0;

    d->pixelometry.rows.top_vblank = 0;
    d->pixelometry.rows.visible = MAX_HEIGHT;
    d->pixelometry.rows.max_visible = MAX_HEIGHT;
    d->pixelometry.rows.bottom_vblank = 70;
    d->pixelometry.offset.y = 0;

    d->geometry.physical_aspect_ratio.width = 4;
    d->geometry.physical_aspect_ratio.height = 3;

    d->pixelometry.overscan.left = d->pixelometry.overscan.right = d->pixelometry.overscan.top = d->pixelometry.overscan.bottom = 0;
}

void apple2J_describe_io(JSM, cvec *IOs)
{
    JTHIS;
    if (this->described_inputs) return;
    this->described_inputs = 1;

    this->IOs = IOs;

    // controllers
    apple2_setup_keyboard(this);
    this->iou.keyboard_ptr = make_cvec_ptr(IOs, cvec_len(IOs)-1);



    // power and reset buttons
    struct physical_io_device* chassis = cvec_push_back(IOs);
    physical_io_device_init(chassis, HID_CHASSIS, 1, 1, 1, 1);
    struct HID_digital_button* b;
    b = cvec_push_back(&chassis->chassis.digital_buttons);
    snprintf(b->name, sizeof(b->name), "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    // screen
    struct physical_io_device *d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_DISPLAY, 1, 1, 0, 1);
    this->iou.display_ptr = make_cvec_ptr(IOs, cvec_len(IOs)-1);
    d->display.output[0] = malloc(MAX_WIDTH*MAX_HEIGHT);
    d->display.output[1] = malloc(MAX_WIDTH*MAX_HEIGHT);
    d->display.output_debug_metadata[0] = NULL;
    d->display.output_debug_metadata[1] = NULL;
    this->iou.cur_output = (u8 *)d->display.output[0];
    d->display.last_written = 1;
    //d->display.last_displayed = 1;
    setup_crt(&d->display);

    this->iou.display = &((physical_io_device *)cpg(this->iou.display_ptr))->display;
}

void apple2J_enable_tracing(JSM)
{
    // TODO
    assert(1==0);
}

void apple2J_disable_tracing(JSM)
{
    // TODO
    assert(1==0);
}

void apple2J_play(JSM)
{
}

void apple2J_pause(JSM)
{
}

void apple2J_stop(JSM)
{
}

void apple2J_get_framevars(JSM, framevars* out)
{
    JTHIS;
    out->master_frame = this->clock.frames_since_restart;
    out->x = this->clock.crt_x;
    out->scanline = this->clock.crt_y;
}

void apple2J_reset(JSM)
{
    JTHIS;
    apple2_reset(this);
}


void apple2J_killall(JSM)
{

}

u32 apple2J_finish_frame(JSM)
{
    JTHIS;
    u32 current_frame = this->clock.frames_since_restart;
    u32 scanlines = 0;
    while(current_frame == this->clock.frames_since_restart) {
        scanlines++;
        apple2J_finish_scanline(jsm);
        if (dbg.do_break) break;
    }
    return this->iou.display->last_written;
}

u32 apple2J_finish_scanline(JSM)
{
    JTHIS;
    u32 current_y = this->clock.crt_y;

    while(current_y == this->clock.crt_y) {
        apple2_cycle(this);
        if (dbg.do_break) break;
    }
    return 0;
}

u32 apple2J_step_master(JSM, u32 howmany)
{
    JTHIS;
    i32 todo = (howmany >> 1);
    if (todo == 0) todo = 1;
    for (i32 i = 0; i < todo; i++) {
        apple2_cycle(this);
        if (dbg.do_break) break;
    }
    return 0;
}

void apple2J_load_BIOS(JSM, multi_file_set* mfs)
{
    JTHIS;
    struct buf* b = &mfs->files[0].buf;
    simplebuf8_allocate(&this->mmu.ROM, b->size);
    memcpy(this->mmu.ROM.ptr, b->ptr, b->size);

    b = &mfs->files[1].buf;
    simplebuf8_allocate(&this->iou.ROM, b->size);
    memcpy(this->iou.ROM.ptr, b->ptr, b->size);
}

