//
// Created by . on 7/24/24.
//

#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <cstring>

#include "mac.h"
#include "mac_bus.h"
#include "mac_debugger.h"

#include "helpers/int.h"
#include "helpers/physical_io.h"
#include "helpers/sys_interface.h"

#include "component/cpu/m68000/m68000.h"

jsm_system *mac_new(mac::variants variant)
{
    auto* th = new mac::core(variant);
    return th;
}

void mac_delete(jsm_system* jsm)
{
    //auto *th = static_cast<core *>();
    //via6522_delete(&th->via);
    /*
    while (cvec_len(->jsm.IOs) > 0) {
        physical_io_device* pio = cvec_pop_back(->jsm.IOs);
        if (pio->kind == HID_DISC_DRIVE) {
            if (pio->disc_drive.remove_disc) pio->disc_drive.remove_disc();
        }
        physical_io_device_delete(pio);
    }*/
}


namespace mac {
u32 read_trace_m68k(void *ptr, u32 addr, u32 UDS, u32 LDS) {
    auto *th = static_cast<core *>(ptr);
    return th->mainbus_read(addr, UDS, LDS, th->io.cpu.last_read_data, false);
}

static constexpr JKEYS mac_keyboard_keymap[77] = {
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

void core::setup_keyboard()
{
    physical_io_device *d = &IOs.emplace_back();
    d->init(HID_KEYBOARD, 0, 0, 1, 1);

    d->id = 0;
    d->kind = HID_KEYBOARD;
    d->connected = 1;

    JSM_KEYBOARD* kbd = &d->keyboard;
    kbd->num_keys = 77;

    for (u32 i = 0; i < kbd->num_keys; i++) {
        kbd->key_defs[i] = mac_keyboard_keymap[i];
    }
}

void macJ_IO_insert_disk(jsm_system *jsm, physical_io_device& pio, multi_file_set& mfs)
{
    printf("\nINSERT DISC");
    auto *th = static_cast<core *>(jsm);
    floppy::mac::DISC &mflpy = th->iwm.my_disks.emplace_back();
    mflpy.load(mfs.files[0].name, mfs.files[0].buf);
    th->iwm.drive[0].disc = &mflpy;
}

void core::setup_crt(JSM_DISPLAY &d)
{
    d.kind = jsm::CRT;
    d.enabled = true;

    d.fps = 60.185;
    d.fps_override_hint = 60;

    // 704x370 total pixels,
    // 512x342 visible
    d.pixelometry.cols.left_hblank = 0;
    d.pixelometry.cols.visible = 512;
    d.pixelometry.cols.max_visible = 512;
    d.pixelometry.cols.right_hblank = 192;
    d.pixelometry.offset.x = 0;

    d.pixelometry.rows.top_vblank = 0;
    d.pixelometry.rows.visible = 342;
    d.pixelometry.rows.max_visible = 342;
    d.pixelometry.rows.bottom_vblank = 28;
    d.pixelometry.offset.y = 0;

    d.geometry.physical_aspect_ratio.width = 512; // sqquare
    d.geometry.physical_aspect_ratio.height = 342; // pixels

    d.pixelometry.overscan.left = d.pixelometry.overscan.right = d.pixelometry.overscan.top = d.pixelometry.overscan.bottom = 8;
}

void core::set_audiobuf(audiobuf *ab) {
    printf("\nERROR NO MAC AUDIO SUPPORT");
}

void core::describe_io()
{
    if (jsm.described_inputs) return;
    jsm.described_inputs = true;

    IOs.reserve(15);

    // chassis - power and reset buttons - 0
    physical_io_device& chassis = IOs.emplace_back();
    chassis.init(HID_CHASSIS, 1, 1, 1, 1);
    HID_digital_button* b;
    b = &chassis.chassis.digital_buttons.emplace_back();
    snprintf(b->name, sizeof(b->name), "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    // keyboard - 1
    setup_keyboard();
    keyboard.make(IOs, IOs.size() - 1);

    // disc drive - 2
    physical_io_device *d = &IOs.emplace_back();
    d->init(HID_DISC_DRIVE, 1, 1, 1, 0);
    d->disc_drive.insert_disc = &macJ_IO_insert_disk;
    d->disc_drive.remove_disc = nullptr;
    d->disc_drive.close_drive = nullptr;
    d->disc_drive.open_drive = nullptr;
    iwm.drive[0].device = nullptr;
    iwm.drive[0].ptr.make(IOs, 2);
    iwm.drive[1].device = nullptr;
    iwm.drive[0].connected = true;
    iwm.drive[1].connected = false;

    // screen - 3
    d = &IOs.emplace_back();
    d->init(HID_DISPLAY, 1, 1, 0, 1);
    setup_crt(d->display);
    d->display.output[0] = malloc(512 * 342);
    d->display.output[1] = malloc(512 * 342);
    d->display.output_debug_metadata[0] = nullptr;
    d->display.output_debug_metadata[1] = nullptr;
    display.crt_ptr.make(IOs, IOs.size()-1);
    display.cur_output = static_cast<u8 *>(d->display.output[0]);
    d->display.last_written = 1;
    //d->display.last_displayed = 1;

    display.crt = &display.crt_ptr.get().display;
}

void core::play()
{
}

void core::pause()
{
}

void core::stop()
{
}

void core::get_framevars(framevars &out)
{
    out.master_frame = clock.master_frame;
    out.x = clock.crt.hpos;
    out.scanline = clock.crt.vpos;
    out.master_cycle = clock.master_cycles;
}

void core::load_BIOS(multi_file_set &mfs)
{
    buf* b = &mfs.files[0].buf;
    u16 *src_ptr = static_cast<u16 *>(b->ptr);
    u16 *dst_ptr = &ROM[0];
    for (u32 i = 0; i < 32768; i++) {
        *dst_ptr = bswap_16(*src_ptr);
        src_ptr++;
        dst_ptr++;
    }
    reset();
}

void core::reset()
{
    cpu.reset();
    display.reset();
    via.reset();
    iwm.reset();
    io.irq.iswitch = io.irq.scc = io.irq.via = false;
    io.eclock = 0;
    io.ROM_overlay = true;
    // TODO: more
}

u32 core::finish_frame()
{
    u32 current_frame = clock.master_frame;
    u32 scanlines = 0;
    while(current_frame == clock.master_frame) {
        scanlines++;
        finish_scanline();
        if (::dbg.do_break) break;
    }
    //printf("\nScanlines: %d", scanlines);
    return display.crt->last_written;
}

void macJ_killall()
{
}

u32 core::finish_scanline()
{
    u32 current_y = clock.crt.vpos;

    while(current_y == clock.crt.vpos) {
        // TODO: add here...
        step_bus();
        if (::dbg.do_break) break;
    }
    return 0;
}

u32 core::step_master(u32 howmany)
{
    howmany >>= 1;
    for (u32 i = 0; i < howmany; i++) {
        step_bus();
        if (::dbg.do_break) break;
    }
    return 0;
}

void core::enable_tracing()
{
    // TODO
    assert(1==0);
}

void core::disable_tracing()
{
    // TODO
    assert(1==0);
}
}
