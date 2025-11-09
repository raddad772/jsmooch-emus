//
// Created by Dave on 2/4/2024.
//
#include "assert.h"
#include "stdlib.h"
#include <stdio.h>

#include "helpers/physical_io.h"
#include "helpers/sys_interface.h"
#include "helpers/debugger/debugger.h"
#include "component/audio/nes_apu/nes_apu.h"

#include "nes.h"
#include "nes_cart.h"
#include "nes_ppu.h"
#include "nes_cpu.h"
#include "nes_debugger.h"
#include "nes_serialize.h"

#define JTHIS struct NES* this = (struct NES*)jsm->ptr
#define JSM struct jsm_system* jsm

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

static u32 read_trace(void *ptr, u32 addr)
{
    NES* nes= (struct NES*)ptr;
    return nes->bus.CPU_read(addr, 0, 0);
}

#define APU_CYCLES_PER_FRAME  29780

static void setup_debug_waveform(struct debug_waveform &dw)
{
    if (dw.samples_requested == 0) return;
    dw.samples_rendered = dw.samples_requested;
    dw.user.cycle_stride = ((float)APU_CYCLES_PER_FRAME / (float)dw.samples_requested);
    dw.user.buf_pos = 0;
}

void NESJ::set_audiobuf(audiobuf *ab)
{
    nes.audio.buf = ab;
    if (nes.audio.master_cycles_per_audio_sample == 0) {
        nes.audio.master_cycles_per_audio_sample = ((float)APU_CYCLES_PER_FRAME / (float)ab->samples_len);
        printf("\nCYCLES PER FRAME:%d PER SAMPLE:%f", APU_CYCLES_PER_FRAME, nes.audio.master_cycles_per_audio_sample);
        nes.audio.next_sample_cycle = 0;
        struct debug_waveform &wf = nes.dbg.waveforms.main.get();
        nes.apu.ext_enable = wf.ch_output_enabled;
    }
    setup_debug_waveform(nes.dbg.waveforms.main.get());
    for (u32 i = 0; i < 4; i++) {
        struct debug_waveform &wf = nes.dbg.waveforms.chan[i].get();
        setup_debug_waveform(wf);
        nes.apu.channels[i].ext_enable = wf.ch_output_enabled;
    }
    struct debug_waveform &wf = nes.dbg.waveforms.chan[4].get();
    nes.apu.dmc.ext_enable = wf.ch_output_enabled;
}

NES::NES() : cpu(this), ppu(this), bus(this), cart(this) {

}

NES::~NES() {
}

void NES_new(JSM)
{
    NES *nes = new NES();
    nes->apu.master_cycles = &nes->clock.master_clock;

    jsm_debug_read_trace dt;
    dt.read_trace = &read_trace;
    dt.ptr = static_cast<void *>(nes);

    nes->cpu.cpu.setup_tracing(&dt, &nes->clock.master_clock);
    snprintf(jsm->label, sizeof(jsm->label), "Nintendo Entertainment System");

    nes->described_inputs = 0;

    nes->cycles_left = 0;
    nes->display_enabled = 1;
    nes->IOs = nullptr;
}


void NES_delete(JSM)
{
    NESJ *nesj = static_cast<NESJ *>(jsm);
    std::vector<physical_io_device> &ms = *nesj->nes.IOs;
    for (physical_io_device &pio : *nesj->nes.IOs) {
        if (pio.kind == HID_CART_PORT) {
            if (pio.cartridge_port.unload_cart) pio.cartridge_port.unload_cart(jsm);
        }
    }
    ms.clear();
}

static void NESIO_load_cart(JSM, multi_file_set &mfs, physical_io_device &pio) {
    NESJ *nesj = dynamic_cast<NESJ *>(jsm);
    buf* b = &mfs.files[0].buf;
    nesj->nes.cart.load_ROM_from_RAM(static_cast<char *>(b->ptr), b->size);
    nesj->nes.bus.set_which_mapper(nesj->nes.cart.header.mapper_number);
    nesj->nes.bus.set_cart(pio);
}

static void NESIO_unload_cart(JSM)
{
}

static void setup_crt(struct JSM_DISPLAY *d)
{
    d->standard = JSS_NTSC;
    d->enabled = 1;

    d->fps = 60.1;
    d->fps_override_hint = 60;

    d->pixelometry.cols.left_hblank = 1;
    d->pixelometry.cols.right_hblank = 85;
    d->pixelometry.cols.visible = 256;
    d->pixelometry.cols.max_visible = 256;
    d->pixelometry.offset.x = 1;

    d->pixelometry.rows.top_vblank = 1;
    d->pixelometry.rows.visible = 240;
    d->pixelometry.rows.max_visible = 240;
    d->pixelometry.rows.bottom_vblank = 21;
    d->pixelometry.offset.y = 1;

    //d->geometry.physical_aspect_ratio.width = 4;
    //d->geometry.physical_aspect_ratio.height = 3;
    d->geometry.physical_aspect_ratio.width = 5;
    d->geometry.physical_aspect_ratio.height = 4;

    d->pixelometry.overscan.left = d->pixelometry.overscan.right = d->pixelometry.overscan.top = d->pixelometry.overscan.bottom = 8;
}

void NESJ::setup_audio(std::vector<physical_io_device> &inIOs)
{
    physical_io_device &pio = inIOs.emplace_back();
    pio.kind = HID_AUDIO_CHANNEL;
    JSM_AUDIO_CHANNEL *chan = &pio.audio_channel;
    chan->sample_rate = APU_CYCLES_PER_FRAME * 60;
    chan->low_pass_filter = 14000;
}

void NESJ::describe_io(std::vector<physical_io_device> &inIOs)
{
    if (nes.described_inputs) return;
    nes.described_inputs = 1;

    nes.IOs = IOs;
    IOs->reserve(15);

    // controllers
    physical_io_device &c1 = nes.IOs->emplace_back(); //0
    //struct physical_io_device *c2 = cvec_push_back(nes.IOs); //1
    NES_joypad::setup_pio(c1, 0, "Player 1", 1);
    //NES_joypad_setup_pio(c2, 1, "Player 2", 0);

    // power and reset buttons
    physical_io_device* chassis = &IOs->emplace_back(); //2
    chassis->init(HID_CHASSIS, 1, 1, 1, 1);
    HID_digital_button* b;

    b = &chassis->chassis.digital_buttons.emplace_back();
    snprintf(b->name, sizeof(b->name), "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    b = &chassis->chassis.digital_buttons.emplace_back();
    b->common_id = DBCID_ch_reset;
    snprintf(b->name, sizeof(b->name), "Reset");
    b->state = 0;

    // cartridge port
    physical_io_device *d = &IOs->emplace_back(); //4
    d->init(HID_CART_PORT, 1, 1, 1, 0);
    d->cartridge_port.load_cart = &NESIO_load_cart;
    d->cartridge_port.unload_cart = &NESIO_unload_cart;

    // screen
    d = &IOs->emplace_back(); //4
    d->init(HID_DISPLAY, 1, 1, 0, 1); //5
    d->display.output[0] = malloc(256 * 224 * 2);
    d->display.output[1] = malloc(256 * 224 * 2);
    d->display.output_debug_metadata[0] = NULL;
    d->display.output_debug_metadata[1] = NULL;
    nes.ppu.display_ptr = cvec_ptr(*IOs, IOs->size()-1);
    nes.ppu.cur_output = static_cast<u16 *>(d->display.output[0]);
    setup_crt(&d->display);
    d->display.last_written = 1;
    //d->display.last_displayed = 1;

    setup_audio(*IOs);

    nes.cpu.joypad1.devices = IOs;
    nes.cpu.joypad1.device_index = NES_INPUTS_PLAYER1;
    nes.cpu.joypad2.devices = IOs;
    nes.cpu.joypad2.device_index = NES_INPUTS_PLAYER2;

    nes.ppu.display = &nes.ppu.display_ptr.get().display;
}

void NESJ::enable_tracing()
{
    // TODO
    assert(1==0);
}

void NESJ::disable_tracing()
{
    // TODO
    assert(1==0);
}

void NESJ::play()
{
}

void NESJ::pause()
{
}

void NESJ::stop()
{
}

void NESJ::get_framevars(framevars* out)
{
    out->master_frame = nes.clock.master_frame;
    out->x = nes.ppu.line_cycle;
    out->scanline = nes.clock.ppu_y;
}

void NESJ::reset()
{
    nes.clock.reset();
    nes.cpu.reset();
    nes.ppu.reset();
    nes.apu.reset();
    if (nes.bus.reset) nes.bus.reset(&nes.bus);
}


void NESJ::killall()
{

}

void NESJ::sample_audio()
{
    nes.clock.apu_master_clock++;
    if (nes.audio.buf && (nes.clock.apu_master_clock >= (u64)nes.audio.next_sample_cycle)) {
        nes.audio.next_sample_cycle += nes.audio.master_cycles_per_audio_sample;
        float *sptr = ((float *)nes.audio.buf->ptr) + (nes.audio.buf->upos);
        if (nes.audio.buf->upos <= nes.audio.buf->samples_len) {
            *sptr = nes.apu.mix_sample(0);
        }
        nes.audio.buf->upos++;
    }

    struct debug_waveform *dw = &nes.dbg.waveforms.main.get();
    if (nes.clock.master_clock >= dw->user.next_sample_cycle) {
        if (dw->user.buf_pos < dw->samples_requested) {
            dw->user.next_sample_cycle += dw->user.cycle_stride;
            static_cast<float *>(dw->buf.ptr)[dw->user.buf_pos] = nes.apu.mix_sample(1);
            dw->user.buf_pos++;
        }
    }

    dw = &nes.dbg.waveforms.chan[0].get();
    if (nes.clock.master_clock >= dw->user.next_sample_cycle) {
        for (int j = 0; j < 5; j++) {
            dw = &nes.dbg.waveforms.chan[j].get();
            if (dw->user.buf_pos < dw->samples_requested) {
                dw->user.next_sample_cycle += dw->user.cycle_stride;
                float sv = nes.apu.sample_channel(j);
                static_cast<float *>(dw->buf.ptr)[dw->user.buf_pos] = sv;
                dw->user.buf_pos++;
                assert(dw->user.buf_pos < 410);
            }
        }
    }
}


u32 NESJ::finish_frame()
{
    if (nes.bus.fake_PRG_RAM.ptr == NULL)
        nes.bus.fake_PRG_RAM.ptr = static_cast<u8 *>(nes.bus.SRAM->data);
    u64 current_frame = nes.clock.master_frame;
    while (nes.clock.master_frame == current_frame) {
        finish_scanline();
        if (dbg.do_break) break;
    }
    return nes.ppu.display->last_written;
}

u32 NESJ::finish_scanline()
{
    i32 cpu_step = static_cast<i32>(nes.clock.timing.cpu_divisor);
    i64 ppu_step = nes.clock.timing.ppu_divisor;
    i32 done = 0;
    i32 start_y = nes.clock.ppu_y;
    while (nes.clock.ppu_y == start_y) {
        nes.clock.master_clock += cpu_step;
        //nes.apu.cycle(nes.clock.master_clock);
        nes.cpu.run_cycle();
        nes.apu.cycle();
        sample_audio();
        nes.bus.CPU_cycle();
        nes.clock.cpu_frame_cycle++;
        nes.clock.cpu_master_clock += cpu_step;
        i64 ppu_left = static_cast<i64>(nes.clock.master_clock) - static_cast<i64>(nes.clock.ppu_master_clock);
        done = 0;
        while (ppu_left >= ppu_step) {
            ppu_left -= ppu_step;
            done++;
        }
        nes.ppu.cycle(done);
        nes.cycles_left -= cpu_step;
        if (dbg.do_break) break;
    }
    return 0;
}


u32 NESJ::step_master(u32 howmany)
{
    nes.cycles_left += howmany;
    i64 cpu_step = nes.clock.timing.cpu_divisor;
    i64 ppu_step = nes.clock.timing.ppu_divisor;
    u32 done = 0;
    while (nes.cycles_left >= cpu_step) {
        //nes.apu.cycle(nes.clock.master_clock);
        nes.clock.master_clock += cpu_step;
        nes.cpu.run_cycle();
        nes.apu.cycle();
        sample_audio();
        nes.bus.CPU_cycle();
        nes.clock.cpu_frame_cycle++;
        nes.clock.cpu_master_clock += cpu_step;
        i64 ppu_left = static_cast<i64>(nes.clock.master_clock) - static_cast<i64>(nes.clock.ppu_master_clock);
        done = 0;
        while (ppu_left >= ppu_step) {
            ppu_left -= ppu_step;
            done++;
        }
        nes.ppu.cycle(done);
        nes.cycles_left -= cpu_step;
        if (dbg.do_break) break;
    }
    return 0;
}

void NESJ::load_BIOS(multi_file_set* mfs)
{
    printf("\nNES doesn't have a BIOS...?");
}

