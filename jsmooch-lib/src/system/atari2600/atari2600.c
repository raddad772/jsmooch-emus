//
// Created by Dave on 4/14/24.
//

#include "assert.h"
#include "stdlib.h"
#include <stdio.h>
#include "helpers/sys_interface.h"
#include "fail"

#include "component/cpu/m6502/m6502.h"
#include "component/cpu/m6502/m6502_opcodes.h"

#include "atari2600_bus.h"
#include "atari2600.h"
#include "cart.h"

#define JTHIS struct atari2600* this = (atari2600*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct atari2600* this

static void atari2600_inputs_init(atari2600_inputs* this) {
    *this = (atari2600_inputs) {
        .fire = 0,
        .up = 0,
        .down = 0,
        .left = 0,
        .right = 0
    };
}

void atari2600J_play(JSM);
void atari2600J_pause(JSM);
void atari2600J_stop(JSM);
void atari2600J_get_framevars(JSM, framevars* out);
void atari2600J_reset(JSM);
void atari2600J_killall(JSM);
u32 atari2600J_finish_frame(JSM);
u32 atari2600J_finish_scanline(JSM);
u32 atari2600J_step_master(JSM, u32 howmany);
void atari2600J_load_BIOS(JSM, multi_file_set* mfs);
void atari2600J_load_ROM(JSM, multi_file_set* mfs);
void atari2600J_enable_tracing(JSM);
void atari2600J_disable_tracing(JSM);
void atari2600J_describe_io(JSM, cvec* IOs);
static void atari2600IO_load_cart(JSM, multi_file_set *mfs, physical_io_device *pio);
static void atari2600IO_unload_cart(JSM);
static void atari2600J_setup_debugger_interface(JSM, debugger_interface *intf);

void atari2600_new(JSM)
{
    struct atari2600* this = (atari2600*)malloc(sizeof(atari2600));

    M6502_init(&this->cpu, M6502_decoded_opcodes);
    M6532_init(&this->riot);
    TIA_init(&this->tia);
    atari2600_cart_init(&this->cart);
    snprintf(jsm->label, sizeof(jsm->label), "Atari 2600");

    this->case_switches.reset = 0;
    this->case_switches.select = 0;
    this->case_switches.p0_difficulty = 0;
    this->case_switches.p1_difficulty = 0;
    this->case_switches.color = 1;

    this->tia.frames_since_restart = 0;
    this->tia.timing.vblank_in_lines = 40; // 40 NTSC, 48 PAL
    this->tia.timing.display_line_start = 40;
    this->tia.timing.vblank_out_start = 192 + 40; // // 192 NTSC, 22? PAL
    this->tia.timing.vblank_out_lines = 30; // 30 NTSC, 36 PAL

    this->described_inputs = 0;

    this->cycles_left = 0;
    this->display_enabled = 1;
    atari2600_inputs_init(&this->controller1_in);
    atari2600_inputs_init(&this->controller2_in);

    jsm->ptr = (void*)this;
    jsm->kind = jsm::systems::ATARI2600;

    jsm->finish_frame = &atari2600J_finish_frame;
    jsm->finish_scanline = &atari2600J_finish_scanline;
    jsm->step_master = &atari2600J_step_master;
    jsm->reset = &atari2600J_reset;
    jsm->load_BIOS = &atari2600J_load_BIOS;
    jsm->get_framevars = &atari2600J_get_framevars;
    jsm->play = &atari2600J_play;
    jsm->pause = &atari2600J_pause;
    jsm->stop = &atari2600J_stop;
    jsm->describe_io = &atari2600J_describe_io;
    jsm->sideload = NULL;
    jsm->setup_debugger_interface = &atari2600J_setup_debugger_interface;

    atari2600J_reset(jsm);
}

void atari2600_delete(JSM)
{
    JTHIS;
    atari2600_cart_delete(&this->cart);

    free(jsm->ptr);
    jsm->ptr = NULL;

    jsm_clearfuncs(jsm);
}

static void atari2600J_setup_debugger_interface(JSM, debugger_interface *intf)
{
    intf->supported_by_core = 0;
    printf("\nWARNING: debugger interface not supported on core: mac");
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

static void setup_controller(atari2600* this, u32 num, const char*name, u32 connected)
{
    struct physical_io_device *d = cvec_push_back(this->IOs);
    physical_io_device_init(d, HID_CONTROLLER, 0, 0, 1, 1);

    snprintf(d->controller.name, sizeof(d->controller.name), "%s", name);
    d->id = num;
    d->kind = HID_CONTROLLER;
    d->connected = connected;

    struct JSM_CONTROLLER* cnt = &d->controller;

    // up down left right a b start select. in that order
    new_button(cnt, "up", DBCID_co_up);
    new_button(cnt, "down", DBCID_co_down);
    new_button(cnt, "left", DBCID_co_left);
    new_button(cnt, "right", DBCID_co_right);
    new_button(cnt, "fire", DBCID_co_fire1);
}

static void setup_crt(JSM_DISPLAY *d)
{
    d->standard = JSS_NTSC;
    d->enabled = 1;

    d->fps = 60.1;
    d->fps_override_hint = 60;

    d->pixelometry.cols.left_hblank = 1;
    d->pixelometry.cols.right_hblank = 85;
    d->pixelometry.cols.visible = 240;
    d->pixelometry.cols.max_visible = 240;
    d->pixelometry.offset.x = 1;

    d->pixelometry.rows.top_vblank = 1;
    d->pixelometry.rows.visible = 240;
    d->pixelometry.rows.max_visible = 240;
    d->pixelometry.rows.bottom_vblank = 21;
    d->pixelometry.offset.y = 1;

    d->geometry.physical_aspect_ratio.width = 4;
    d->geometry.physical_aspect_ratio.height = 3;

    d->pixelometry.overscan.left = d->pixelometry.overscan.right = d->pixelometry.overscan.top = d->pixelometry.overscan.bottom = 8;
}


void atari2600J_describe_io(JSM, cvec *IOs)
{
    JTHIS;
    if (this->described_inputs) return;
    this->described_inputs = 1;

    this->IOs = IOs;

    // controllers
    setup_controller(this, 0, "Player 1", 1);
    setup_controller(this, 1, "Player 2", 0);

    // Chassis buttons
    struct physical_io_device* chassis = cvec_push_back(IOs);
    physical_io_device_init(chassis, HID_CHASSIS, 1, 1, 1, 1);
    struct HID_digital_button* b;
    b = cvec_push_back(&chassis->chassis.digital_buttons);
    snprintf(b->name, sizeof(b->name), "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    b = cvec_push_back(&chassis->chassis.digital_buttons);
    b->common_id = DBCID_ch_reset;
    snprintf(b->name, sizeof(b->name), "Reset");
    b->state = 0;

    b = cvec_push_back(&chassis->chassis.digital_buttons);
    b->common_id = DBCID_ch_diff_left;
    snprintf(b->name, sizeof(b->name), "Left Difficulty");
    b->state = 0;

    b = cvec_push_back(&chassis->chassis.digital_buttons);
    b->common_id = DBCID_ch_diff_right;
    snprintf(b->name, sizeof(b->name), "Right Difficulty");
    b->state = 0;

    b = cvec_push_back(&chassis->chassis.digital_buttons);
    b->common_id = DBCID_ch_game_select;
    snprintf(b->name, sizeof(b->name), "Game Select");
    b->state = 0;

    // cartridge port
    struct physical_io_device *d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_CART_PORT, 1, 1, 1, 0);
    d->cartridge_port.load_cart = &atari2600IO_load_cart;
    d->cartridge_port.unload_cart = &atari2600IO_unload_cart;

    // screen
    d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_DISPLAY, 1, 1, 0, 1);
    setup_crt(&d->display);
    d->display.output[0] = malloc(256 * 224 * 2);
    d->display.output[1] = malloc(256 * 224 * 2);
    d->display.output_debug_metadata[0] = NULL;
    d->display.output_debug_metadata[1] = NULL;
    this->tia.display_ptr = make_cvec_ptr(IOs, cvec_len(IOs)-1);
    this->tia.cur_output = (u8 *)d->display.output[0];
    d->display.last_written = 1;
    //d->display.last_displayed = 1;

    this->tia.display = &((physical_io_device *)cpg(this->tia.display_ptr))->display;
}

void atari2600J_enable_tracing(JSM)
{
    // TODO
    assert(1==0);
}

void atari2600J_disable_tracing(JSM)
{
    // TODO
    assert(1==0);
}

void atari2600J_play(JSM)
{
}

void atari2600J_pause(JSM)
{
}

void atari2600J_stop(JSM)
{
}

void atari2600J_get_framevars(JSM, framevars* out)
{
    JTHIS;
    out->master_frame = this->tia.master_frame;
    out->x = this->tia.hcounter;
    out->scanline = this->tia.vcounter;
}

static void CPU_reset(atari2600* this)
{
    M6502_reset(&this->cpu);
}

void atari2600J_reset(JSM)
{
    JTHIS;
    //atari2600_clock_reset(&this->clock);
    atari2600_cart_reset(&this->cart);
    CPU_reset(this);
    TIA_reset(&this->tia);
    M6532_reset(&this->riot);

    this->tia.master_frame = 0;
    this->master_clock = 0;
}

void atari2600_map_inputs(JSM)
{
    JTHIS;
    struct physical_io_device* p = (physical_io_device*)cvec_get(this->IOs, 0);
    struct cvec* bl = &p->controller.digital_buttons;
    struct HID_digital_button* b;

#define B_GET(button, num) { b = cvec_get(bl, num); this->controller1_in. button = b->state; }
    B_GET(up, 0);
    B_GET(down, 1);
    B_GET(left, 2);
    B_GET(right, 3);
    B_GET(fire, 4);
#undef B_GET

    // TODO
    this->controller2_in = (atari2600_inputs) {
        .down = 0,
        .up = 0,
        .left = 0,
        .right = 0,
        .fire = 0
    };

    p = (physical_io_device*)cvec_get(this->IOs, 2);
    bl = &p->chassis.digital_buttons;

#define B_GET(button, num) { b = cvec_get(bl, num); this->case_switches. button = b->state; }
    B_GET(reset, 1);
    B_GET(p0_difficulty, 2);
    B_GET(p1_difficulty, 3);
    B_GET(select, 4);
#undef B_GET

    // Now refresh input stuff! YAY!
    this->tia.io.INPT[0] = this->tia.io.INPT[1] = this->tia.io.INPT[2] = this->tia.io.INPT[3] = this->tia.io.INPT[4] = this->tia.io.INPT[5] = 0;
    this->riot.io.SWCHA = 0;
    this->riot.io.SWCHB &= 0b11001011; // Combat stores 3 bits of data in here

    // Do switches
    this->riot.io.SWCHB |=
            (this->case_switches.reset ^ 1) |
            ((this->case_switches.select ^ 1) << 1) |
            (this->case_switches.color << 3) |
            (this->case_switches.p0_difficulty << 6) |
            (this->case_switches.p1_difficulty << 7);

    // P0 controller
    this->riot.io.SWCHA |=
            (this->controller1_in.up << 4) |
            (this->controller1_in.down << 5) |
            (this->controller1_in.left << 6) |
            (this->controller1_in.right << 7);

    this->tia.io.INPT[4] |= (this->controller1_in.fire << 7);

    //P1 controller
    this->riot.io.SWCHA |=
            (this->controller2_in.up) |
            (this->controller2_in.down << 1) |
            (this->controller2_in.left << 2) |
            (this->controller2_in.right << 3);
    this->tia.io.INPT[5] |= (this->controller2_in.fire << 7);
}

void atari2600J_killall(JSM)
{

}

u32 atari2600J_finish_frame(JSM)
{
    JTHIS;
    atari2600_map_inputs(jsm);
    u32 current_frame = this->tia.master_frame;
    while (this->tia.master_frame == current_frame) {
        atari2600J_finish_scanline(jsm);
        if (dbg.do_break) break;
    }
    return 0; // TODO this->tia.last_used_buffer;
}


u32 atari2600J_finish_scanline(JSM)
{
    JTHIS;
    atari2600_map_inputs(jsm);
    u32 start_y = this->tia.vcounter;
    u32 cycles = 0;
    while (this->tia.vcounter == start_y) {
        cycles++;
        atari2600J_step_master(jsm, 1);
        if (dbg.do_break) break;
    }

    return 0;
}

u32 atari2600J_step_master(JSM, u32 howmany)
{
    JTHIS;
    this->cycles_left += (i32)howmany;
    if (howmany > 1) atari2600_map_inputs(jsm);
    while (this->cycles_left > 0) {
        if ((this->master_clock % 3) == 0)
            atari2600_CPU_run_cycle(this);
        TIA_run_cycle(&this->tia);
        M6532_cycle(&this->riot);

        this->master_clock++;
        this->cycles_left--;
        if (dbg.do_break) break;
    }
    return 0;
}

void atari2600J_load_BIOS(JSM, multi_file_set* mfs)
{
    printf("\nAtari 2600 doesn't have a BIOS...?");
}

static void atari2600IO_load_cart(JSM, multi_file_set *mfs, physical_io_device *pio) {
    JTHIS;
    struct buf* b = &mfs->files[0].buf;
    printf("\nLoad ROM");
    atari2600_cart_load_ROM_from_RAM(&this->cart, b->ptr, b->size, mfs->files[0].name);
    atari2600J_reset(jsm);
}

static void atari2600IO_unload_cart(JSM)
{

}
