//
// Created by . on 12/4/24.
//
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "galaksija.h"
#include "galaksija_bus.h"
#include "galaksija_debugger.h"

#include "helpers/physical_io.h"
#include "helpers/multisize_memaccess.cpp"


// 192x320 incl. hblank
// 128x208 not inc;.
#define MASTER_CYCLES_PER_FRAME 122880
#define MASTER_CYCLES_PER_LINE 384
#define MASTER_CYCLES_PER_SECOND 6144000
#define CRT_WIDTH 384

jsm_system *galaksija_new()
{
    return new galaksija::core();
}

void galaksija_delete(jsm_system *system) {
    delete system;
}


namespace galaksija {
u32 read_trace_cpu(void *ptr, u32 addr) {
    auto *th = static_cast<core *>(ptr);
    return th->mainbus_read(addr, th->io.open_bus, false);
}

void core::load_state(serialized_state &state, deserialize_ret &ret)
{}

void core::save_state(serialized_state &state) {

}


core::core() :
    z80(false)
    {
    has.load_BIOS = true;
    has.max_loaded_files = 0;
    has.max_loaded_folders = 0;
    has.save_state = false;
    has.set_audiobuf = false;
    snprintf(label, sizeof(label), "Galaksija");
    jsm_debug_read_trace dt;
    dt.read_trace = &read_trace_cpu;
    dt.ptr = this;
    z80.setup_tracing(&dt, &clock.master_cycle_count);
}

u32 core::timer_reload_ticks(u32 reload)
{
    // So it overflows at 0x100
    if (reload == 0xFFFF) return 0x10000;
    return 0x10000 - reload;
}

static void setup_debug_waveform(debug_waveform *dw)
{
    if (dw->samples_requested == 0) return;
    dw->samples_rendered = dw->samples_requested;
    dw->user.cycle_stride = (static_cast<float>(MASTER_CYCLES_PER_FRAME) / static_cast<float>(dw->samples_requested));
    dw->user.buf_pos = 0;
}

void core::set_audiobuf(audiobuf *ab)
{
    
    audio.buf = ab;
    if (audio.master_cycles_per_audio_sample == 0) {
        audio.master_cycles_per_audio_sample = ((float)(MASTER_CYCLES_PER_FRAME / (float)ab->samples_len));
        audio.next_sample_cycle = 0;
        //debug_waveform *wf = &dbg.waveforms.main.get();
    }

    // PSG
    /*setup_debug_waveform(cvec_get(dbg.waveforms.main.vec, dbg.waveforms.main.index));
    for (u32 i = 0; i < 6; i++) {
        setup_debug_waveform(cvec_get(dbg.waveforms.chan[i].vec, dbg.waveforms.chan[i].index));
        debug_waveform *wf = (debug_waveform *)cvec_get(dbg.waveforms.chan[i].vec, dbg.waveforms.chan[i].index);
        if (i < 4)
            apu.channels[i].ext_enable = wf->ch_output_enabled;
        else
            apu.fifo[i - 4].ext_enable = wf->ch_output_enabled;
    }
    */
}

u32 core::finish_frame()
{
    

    u64 current_frame = clock.master_frame;
    while (clock.master_frame == current_frame) {
        core::finish_scanline();
        if (::dbg.do_break) break;
    }
    return crt.display->last_written;
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

void core::get_framevars(framevars& out)
{
    
    out.master_frame = clock.master_frame;
    out.x = crt.x;
    out.scanline = crt.y;
    out.master_cycle = clock.master_cycle_count;
}

void core::reset()
{
    
    z80.reset();
    crt.x = 0;
    crt.y = 0;

    printf("\ngalaksija reset!");
}

void core::sample_audio(u32 num_cycles)
{
    return;
    /*for (u64 i = 0; i < num_cycles; i++) {
        galaksija_APU_cycle();
        u64 mc = clock.master_cycle_count + i;
        if (audio.buf && (mc >= (u64) audio.next_sample_cycle)) {
            audio.next_sample_cycle += audio.master_cycles_per_audio_sample;
            if (audio.buf->upos < audio.buf->samples_len) {
                ((float *)(audio.buf->ptr))[audio.buf->upos] = galaksija_APU_mix_sample(0);
            }
            audio.buf->upos++;
        }

        debug_waveform *dw = cpg(dbg.waveforms.main);
        if (mc >= (u64)dw->user.next_sample_cycle) {
            if (dw->user.buf_pos < dw->samples_requested) {
                dw->user.next_sample_cycle += dw->user.cycle_stride;
                ((float *) dw->buf.ptr)[dw->user.buf_pos] = galaksija_APU_mix_sample(1);
                dw->user.buf_pos++;
            }
        }

        dw = cpg(dbg.waveforms.chan[0]);
        if (mc >= (u64)dw->user.next_sample_cycle) {
            for (int j = 0; j < 6; j++) {
                dw = cpg(dbg.waveforms.chan[j]);
                if (dw->user.buf_pos < dw->samples_requested) {
                    dw->user.next_sample_cycle += dw->user.cycle_stride;
                    float sv = galaksija_APU_sample_channel(j);
                    ((float *) dw->buf.ptr)[dw->user.buf_pos] = sv;
                    dw->user.buf_pos++;
                    assert(dw->user.buf_pos < 410);
                }
            }
        }
    }
    */
}

u32 core::finish_scanline()
{
    
    u32 cycles_left = MASTER_CYCLES_PER_LINE - crt.x;
    core::step_master(cycles_left);
    return 0;
}

u32 core::step_master(u32 howmany)
{
    
    for (u32 i = 0; i < howmany; i++) {
        cycle();
        if (::dbg.do_break) break;
    }
    return 0;
}

void core::load_BIOS(multi_file_set& mfs)
{
    
    // File 0, chargen
    // File 1, ROM A
    // File 2, if exist, ROMB
    ROMB_present = mfs.files.size() > 2;
    memcpy(ROM_char, mfs.files[0].buf.ptr, 2048);
    memcpy(ROMA, mfs.files[1].buf.ptr, 4096);
    if (mfs.files.size() > 2) memcpy(ROMB, mfs.files[2].buf.ptr, 4096);
}

static void setup_crt(JSM_DISPLAY *d)
{
    d->kind = jsm::CRT;
    d->enabled = true;

    d->fps = 50;
    d->fps_override_hint = 50;
    // 240x160, but 308x228 with v and h blanks

    d->pixelometry.cols.left_hblank = 0;
    d->pixelometry.cols.visible = CRT_WIDTH;
    d->pixelometry.cols.max_visible = CRT_WIDTH;
    d->pixelometry.cols.right_hblank = 0;
    d->pixelometry.offset.x = 0;

    d->pixelometry.rows.top_vblank = 0;
    d->pixelometry.rows.visible = 320;
    d->pixelometry.rows.max_visible = 320;
    d->pixelometry.rows.bottom_vblank = 0;
    d->pixelometry.offset.y = 0;

    d->geometry.physical_aspect_ratio.width = 1;
    d->geometry.physical_aspect_ratio.height = 1;

    d->pixelometry.overscan.left = d->pixelometry.overscan.right = 0;
    d->pixelometry.overscan.top = d->pixelometry.overscan.bottom = 0;
}

void core::setup_audio()
{
    physical_io_device *pio = &IOs.emplace_back();
    pio->kind = HID_AUDIO_CHANNEL;
    JSM_AUDIO_CHANNEL *chan = &pio->audio_channel;
    chan->sample_rate = MASTER_CYCLES_PER_SECOND;
    chan->low_pass_filter = 24000;
}

#define NUMKEYS 53
static constexpr u32 galaksija_keyboard_keymap[NUMKEYS] = {
      JK_A, JK_B, JK_C, JK_D, JK_E, JK_F, JK_G,
        JK_H, JK_I, JK_J, JK_K, JK_L, JK_M, JK_N, JK_O,
        JK_P, JK_Q, JK_R, JK_S, JK_T, JK_U, JK_V, JK_W,
        JK_X, JK_Y, JK_Z, JK_UP, JK_DOWN, JK_LEFT, JK_RIGHT, JK_SPACE,
        JK_0, JK_1, JK_2, JK_3, JK_4, JK_5, JK_6, JK_7,
        JK_8, JK_9, JK_SEMICOLON, //41
        JK_COMMA, // 42
        JK_EQUALS, // 43
        JK_DOT, // 44
        JK_SLASH_FW, // 45
        JK_ENTER, // 46
        JK_BACKSPACE, // 47
        JK_SHIFT, // 48
        JK_F1, // 49
        JK_F2, // 50
        JK_QUOTE, // 51
        JK_ESC // 52
};

void core::setup_keyboard()
{
    physical_io_device *d = &IOs.emplace_back();
    d->init(HID_KEYBOARD, true, true, true, false);

    d->id = 0;
    d->kind = HID_KEYBOARD;
    d->connected = true;
    d->enabled = true;

    JSM_KEYBOARD* kbd = &d->keyboard;
    kbd->num_keys = NUMKEYS;

    for (u32 i = 0; i < NUMKEYS; i++) {
        kbd->key_defs[i] = static_cast<JKEYS>(galaksija_keyboard_keymap[i]);
    }
}


void core::describe_io()
{
    if (described_inputs) return;
    IOs.reserve(15);
    described_inputs = true;

    // controllers
    setup_keyboard();

    // power and reset buttons
    physical_io_device* chassis = &IOs.emplace_back();
    chassis->init(HID_CHASSIS, true, true, true, true);
    HID_digital_button* b;

    b = &chassis->chassis.digital_buttons.emplace_back();
    snprintf(b->name, sizeof(b->name), "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    // cartridge port
    /*d->init(HID_CART_PORT, 1, 1, 1, 0);
    d->cartridge_port.load_cart = &galaksijaIO_load_cart;
    d->cartridge_port.unload_cart = &galaksijaIO_unload_cart;*/

    // screen
    physical_io_device *d = &IOs.emplace_back();
    d->init(HID_DISPLAY, true, true, false, true);
    d->display.output[0] = malloc(384 * 320);
    d->display.output[1] = malloc(384 * 320);
    d->display.output_debug_metadata[0] = nullptr;
    d->display.output_debug_metadata[1] = nullptr;
    setup_crt(&d->display);
    crt.display_ptr.make(IOs, IOs.size()-1);
    d->display.last_written = 1;
    //d->display.last_displayed = 1;
    crt.cur_output = static_cast<u8 *>(d->display.output[0]);

    setup_audio();

    crt.display = &crt.display_ptr.get().display;
}
}