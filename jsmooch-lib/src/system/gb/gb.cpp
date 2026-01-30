#include <cassert>
#include <cstdlib>
#include <cstdio>

#include "helpers/sys_interface.h"
#include "helpers/physical_io.h"

#include "component/audio/gb_apu/gb_apu.h"

#include "gb.h"
#include "gb_bus.h"
#include "gb_cpu.h"
#include "gb_ppu.h"
#include "gb_clock.h"
#include "gb_bus.h"
#include "gb_enums.h"
#include "gb_debugger.h"
#include "gb_serialize.h"

#define GB_QUICK_BOOT true

jsm_system *GB_new(GB::variants variant) {
	return new GB::core(variant);
}

void GB_delete(jsm_system* sys) {
	auto *th = dynamic_cast<GB::core *>(sys);
	for (auto &pio : th->IOs) {
		if (pio.kind == HID_CART_PORT) {
			if (pio.cartridge_port.unload_cart) pio.cartridge_port.unload_cart(sys);
		}
	}
}


namespace GB {

#define MASTER_CYCLES_PER_FRAME clock.cycles_per_frame
void core::setup_debug_waveform(debug_waveform *dw)
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
        audio.master_cycles_per_audio_sample = (MASTER_CYCLES_PER_FRAME / static_cast<float>(ab->samples_len));
        audio.next_sample_cycle = 0;
        auto *wf = &dbg.waveforms.main.get();
        apu.ext_enable = wf->ch_output_enabled;
    }
    setup_debug_waveform(&dbg.waveforms.main.get());
    for (u32 i = 0; i < 4; i++) {
        setup_debug_waveform(&dbg.waveforms.chan[i].get());
        auto *wf = &dbg.waveforms.chan[i].get();
        apu.channels[i].ext_enable = wf->ch_output_enabled;
    }
}

u32 core::read_IO(u32 addr, u32 val) {
    //u32 out = 0xFF;
    //if (addr == 0xFF44) printf("\nFF44 STARTING VALUE %02x", out);
    u32 out = cpu.read_IO(addr, 0xFF);
    //if (addr == 0xFF44) printf("\nFF44 OUT %02x", out);
    out &= ppu.read_IO(addr, out);
    //if (addr == 0xFF44) printf("\nFF44 OUT2 %02x", out);
    return out;
}

void core::write_IO(u32 addr, u32 val) {
    cpu.write_IO(addr, val);
    ppu.write_IO(addr, val);
}

static void setup_lcd(JSM_DISPLAY *d)
{
    d->kind = jsm::LCD;
    d->enabled = true;

    d->fps=59.7;
    d->fps_override_hint = 60;

    // 456x154 total, 160x144 visible

    d->pixelometry.cols.left_hblank = 80; // for OAM search
    d->pixelometry.cols.visible = 160;
    d->pixelometry.cols.max_visible = 160;
    d->pixelometry.cols.right_hblank = 216;
    d->pixelometry.offset.x = 80;

    d->pixelometry.rows.top_vblank = 0;
    d->pixelometry.rows.visible = 144;
    d->pixelometry.rows.max_visible = 144;
    d->pixelometry.rows.bottom_vblank = 10;
    d->pixelometry.offset.y = 0;

    d->geometry.physical_aspect_ratio.width = 160;
    d->geometry.physical_aspect_ratio.height = 144;

    d->pixelometry.overscan.left = d->pixelometry.overscan.right = d->pixelometry.overscan.top = d->pixelometry.overscan.bottom = 0;
}


void core::setup_audio()
{
    physical_io_device *pio = &IOs.emplace_back();
    pio->kind = HID_AUDIO_CHANNEL;
    JSM_AUDIO_CHANNEL *chan = &pio->audio_channel;
    chan->sample_rate = (MASTER_CYCLES_PER_FRAME * 60) / 3;
    chan->low_pass_filter = 16000;
}

static void GBIO_load_cart(jsm_system *jsm, multi_file_set &mfs, physical_io_device &pio);
static void GBIO_unload_cart(jsm_system *sm);

void core::describe_io()
{
    // TODO guard against more than one init
    if (described_inputs) return;
    described_inputs = true;
	IOs.reserve(15);

    // controllers
    physical_io_device *d = &IOs.emplace_back();
    d->init(HID_CONTROLLER, true, true, true, false);
    snprintf(d->controller.name, sizeof(d->controller.name), "%s", "GameBoy");
    d->id = 0;
    d->kind = HID_CONTROLLER;
    d->connected = true;
    d->enabled = true;

    JSM_CONTROLLER* cnt = &d->controller;

    // up down left right a b start select. in that order
    cpu.device_ptr.make(IOs, IOs.size()-1);
    pio_new_button(cnt, "up", DBCID_co_up);
    pio_new_button(cnt, "down", DBCID_co_down);
    pio_new_button(cnt, "left", DBCID_co_left);
    pio_new_button(cnt, "right", DBCID_co_right);
    pio_new_button(cnt, "a", DBCID_co_fire1);
    pio_new_button(cnt, "b", DBCID_co_fire2);
    pio_new_button(cnt, "start", DBCID_co_start);
    pio_new_button(cnt, "select", DBCID_co_select);

    // power and reset buttons
    physical_io_device* chassis = &IOs.emplace_back();
    chassis->init(HID_CHASSIS, true, true, true, true);
    HID_digital_button* b;
    b = &chassis->chassis.digital_buttons.emplace_back();
    snprintf(b->name, sizeof(b->name), "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    // cartridge port
    d = &IOs.emplace_back();
    d->init(HID_CART_PORT, true, true, true, true);
    d->cartridge_port.load_cart = &GBIO_load_cart;
    d->cartridge_port.unload_cart = &GBIO_unload_cart;

    // screen
    d = &IOs.emplace_back();
    d->init(HID_DISPLAY, 1, 1, 0, 1);
    setup_lcd(&d->display);
    d->display.output[0] = malloc(160 * 144 * 2);
    d->display.output[1] = malloc(160 * 144 * 2);
    d->display.output_debug_metadata[0] = malloc(160 * 144 * 2);
    d->display.output_debug_metadata[1] = malloc(160 * 144 * 2);
    ppu.display_ptr.make(IOs, IOs.size()-1);
    ppu.cur_output = static_cast<u16 *>(d->display.output[0]);
    ppu.cur_output_debug_metadata = static_cast<u16 *>(d->display.output_debug_metadata[0]);
    d->display.last_written = 1;
    //d->display.last_displayed = 1;

    setup_audio();

    ppu.display = &ppu.display_ptr.get().display;
}

void core::killall() {
	// Do nothing
}

u32 core::finish_frame() {
	step_master(clock.cycles_left_this_frame);
	return ppu.last_used_buffer;
}

u32 core::finish_scanline() {
	printf("STEP SCANLINE NOT SUPPORT GB AS YET");
	return ppu.last_used_buffer ^ 1;
}

void core::sample_audio()
{
    if (audio.buf && (clock.master_clock >= (u64)audio.next_sample_cycle)) {
        audio.next_sample_cycle += audio.master_cycles_per_audio_sample;
        float *sptr = ((float *)audio.buf->ptr) + (audio.buf->upos);
        //assert(audio.buf->upos < audio.buf->samples_len);
        if (audio.buf->upos >= audio.buf->samples_len) {
            audio.buf->upos++;
        }
        else {
            *sptr = apu.mix_sample(false);
            audio.buf->upos++;
        }
    }

    debug_waveform *dw = &dbg.waveforms.main.get();
    if (clock.master_clock >= dw->user.next_sample_cycle) {
        if (dw->user.buf_pos < dw->samples_requested) {
            dw->user.next_sample_cycle += dw->user.cycle_stride;
            static_cast<float *>(dw->buf.ptr)[dw->user.buf_pos] = apu.mix_sample(true);
            dw->user.buf_pos++;
        }
    }

    dw = &dbg.waveforms.chan[0].get();
    if (clock.master_clock >= dw->user.next_sample_cycle) {
        for (int j = 0; j < 4; j++) {
            dw = &dbg.waveforms.chan[j].get();
            if (dw->user.buf_pos < dw->samples_requested) {
                dw->user.next_sample_cycle += dw->user.cycle_stride;
                float sv = apu.channels[j].sample();
                static_cast<float *>(dw->buf.ptr)[dw->user.buf_pos] = sv;
                dw->user.buf_pos++;
                assert(dw->user.buf_pos < 410);
            }
        }
    }
}

u32 core::step_master(u32 howmany) {
	cycles_left += static_cast<i32>(howmany);
	u32 cpu_step = clock.timing.cpu_divisor;
	while (cycles_left > 0) {
		clock.cycles_left_this_frame--;
		if (clock.cycles_left_this_frame <= 0)
			clock.cycles_left_this_frame += clock.cycles_per_frame;
        if ((clock.turbo && ((clock.master_clock & 1) == 0)) || ((clock.master_clock & 3) == 0)) {
            cpu.cycle();
            clock.cpu_frame_cycle++;
            clock.cpu_master_clock += cpu_step;
        }
        if ((clock.master_clock & 3) == 0)
            apu.cycle();
		clock.master_clock++;
		ppu.run_cycles(1);
        sample_audio();
		clock.ppu_master_clock += 1;
		cycles_left--;
	}
	return ppu.last_used_buffer ^ 1;

}

u32 core::DMA_read(u32 addr)
{
	return CPU_read(addr, 0, 1);
}

void core::IRQ_vblank_up()
{
	cpu.cpu.regs.IF |= 1;
}

void core::IRQ_vblank_down()
{
	// Do nothin!
}

void core::reset()
{
    clock.reset();
    cpu.reset();
    ppu.reset();
    generic_mapper_reset();
    if (mapper) mapper->reset(mapper);
	if (GB_QUICK_BOOT) {
		ppu.quick_boot();
		cpu.quick_boot();
	}
}

void core::get_framevars(framevars& out)
{
	out.master_frame = clock.master_frame;
	out.x = clock.lx;
	out.scanline = clock.ly;
    out.last_used_buffer = ppu.last_used_buffer;
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

void core::play()
{

}

void core::pause()
{

}

void core::stop()
{

}

static void GBIO_load_cart(jsm_system *sm, multi_file_set &mfs, physical_io_device &pio) {
	auto *th = dynamic_cast<GB::core *>(sm);
    buf* b = &mfs.files[0].buf;
    th->cart.load_ROM_from_RAM(b->ptr, b->size, pio);
    th->reset();
}

static void GBIO_unload_cart(jsm_system *sm) {

}

void core::load_BIOS(multi_file_set& mfs) {
    BIOS.copy(&mfs.files[0].buf);
}
}