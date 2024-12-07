#include "assert.h"
#include "stdlib.h"
#include <stdio.h>

#include "helpers/sys_interface.h"
#include "helpers/physical_io.h"
#include "helpers/debugger/debugger.h"

#include "component/audio/gb_apu/gb_apu.h"

#include "gb.h"
#include "gb_cpu.h"
#include "gb_ppu.h"
#include "gb_clock.h"
#include "gb_bus.h"
#include "gb_enums.h"
#include "gb_debugger.h"
#include "gb_serialize.h"

#define JTHIS struct GB* this = (struct GB*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct GB* this

#define GB_QUICK_BOOT true

u32 GB_bus_DMA_read(struct GB_bus* this, u32 addr);
void GB_bus_IRQ_vblank_up(struct GB_bus* this);
void GB_bus_IRQ_vblank_down(struct GB_bus* this);
void GBJ_play(JSM);
void GBJ_pause(JSM);
void GBJ_stop(JSM);
void GBJ_get_framevars(JSM, struct framevars* out);
void GBJ_reset(JSM);
void GBJ_killall(JSM);
u32 GBJ_finish_frame(JSM);
u32 GBJ_finish_scanline(JSM);
u32 GBJ_step_master(JSM, u32 howmany);
void GBJ_load_BIOS(JSM, struct multi_file_set* mfs);
void GB_write_IO(struct GB_bus* bus, u32 addr, u32 val);
u32 GB_read_IO(struct GB_bus* bus, u32 addr, u32 val);
void GBJ_enable_tracing(JSM);
void GBJ_disable_tracing(JSM);
void GBJ_describe_io(JSM, struct cvec* IOs);
static void GBIO_unload_cart(JSM);
static void GBIO_load_cart(JSM, struct multi_file_set *mfs, struct physical_io_device *pio);

#define MASTER_CYCLES_PER_FRAME GB_CYCLES_PER_FRAME
static void setup_debug_waveform(struct debug_waveform *dw)
{
    if (dw->samples_requested == 0) return;
    dw->samples_rendered = dw->samples_requested;
    dw->user.cycle_stride = ((float)MASTER_CYCLES_PER_FRAME / (float)dw->samples_requested);
    dw->user.buf_pos = 0;
}

static void GBJ_set_audiobuf(struct jsm_system* jsm, struct audiobuf *ab)
{
    JTHIS;
    this->audio.buf = ab;
    if (this->audio.master_cycles_per_audio_sample == 0) {
        this->audio.master_cycles_per_audio_sample = (MASTER_CYCLES_PER_FRAME / (float)ab->samples_len);
        this->audio.next_sample_cycle = 0;
        struct debug_waveform *wf = (struct debug_waveform *)cpg(this->dbg.waveforms.main);
        this->apu.ext_enable = wf->ch_output_enabled;
    }
    setup_debug_waveform(cpg(this->dbg.waveforms.main));
    for (u32 i = 0; i < 4; i++) {
        setup_debug_waveform(cpg(this->dbg.waveforms.chan[i]));
        struct debug_waveform *wf = (struct debug_waveform *)cpg(this->dbg.waveforms.chan[i]);
        this->apu.channels[i].ext_enable = wf->ch_output_enabled;
    }
}

void GB_new(JSM, enum GB_variants variant)
{
	struct GB* this = (struct GB*)malloc(sizeof(struct GB));
    this->described_inputs = 0;
    GB_clock_init(&this->clock);
	GB_bus_init(&this->bus, &this->clock);
	GB_CPU_init(&this->cpu, variant, &this->clock, &this->bus);
	GB_PPU_init(&this->ppu, variant, &this->clock, &this->bus);
	GB_cart_init(&this->cart, variant, &this->clock, &this->bus);
    GB_APU_init(&this->apu);
    this->bus.apu = &this->apu;
    buf_init(&this->BIOS);

	this->variant = variant;
    switch(variant) {
        case DMG:
            snprintf(jsm->label, sizeof(jsm->label), "Nintendo GameBoy");
            break;
        case GBC:
            snprintf(jsm->label, sizeof(jsm->label), "Nintendo GameBoy Color");
            break;
        case SGB:
            snprintf(jsm->label, sizeof(jsm->label), "Nintendo Super GameBoy");
            break;
    }
	this->cycles_left = 0;

	this->bus.DMA_read = &GB_bus_DMA_read;
	this->bus.IRQ_vblank_down = &GB_bus_IRQ_vblank_down;
	this->bus.IRQ_vblank_up = &GB_bus_IRQ_vblank_up;
    this->bus.gb = this;

    this->bus.read_IO = &GB_read_IO;
    this->bus.write_IO = &GB_write_IO;

	jsm->ptr = (void*)this;

	jsm->finish_frame = &GBJ_finish_frame;
	jsm->finish_scanline = &GBJ_finish_scanline;
	jsm->step_master = &GBJ_step_master;
	jsm->reset = &GBJ_reset;
	jsm->load_BIOS = &GBJ_load_BIOS;
	jsm->get_framevars = &GBJ_get_framevars;
    jsm->describe_io = &GBJ_describe_io;
    jsm->save_state = &GBJ_save_state;
    jsm->load_state = &GBJ_load_state;

	jsm->play = &GBJ_play;
	jsm->pause = &GBJ_pause;
	jsm->stop = &GBJ_stop;

    jsm->set_audiobuf = &GBJ_set_audiobuf;

    jsm->sideload = NULL;
    jsm->setup_debugger_interface = &GBJ_setup_debugger_interface;
}

u32 GB_read_IO(struct GB_bus* bus, u32 addr, u32 val) {
    //u32 out = 0xFF;
    //if (addr == 0xFF44) printf("\nFF44 STARTING VALUE %02x", out);
    u32 out = GB_CPU_bus_read_IO(bus, addr, 0xFF);
    //if (addr == 0xFF44) printf("\nFF44 OUT %02x", out);
    out &= GB_PPU_bus_read_IO(bus, addr, out);
    //if (addr == 0xFF44) printf("\nFF44 OUT2 %02x", out);
    return out;
}

void GB_write_IO(struct GB_bus* bus, u32 addr, u32 val) {
    GB_CPU_bus_write_IO(bus, addr, val);
    GB_PPU_bus_write_IO(bus, addr, val);
}


void GB_delete(JSM)
{
	JTHIS;
	//GB_CPU_delete(&this->cpu);
	//GB_PPU_delete(&this->ppu);
	GB_cart_delete(&this->cart);
	GB_bus_delete(&this->bus);
    buf_delete(&this->BIOS);
	//GB_clock_delete(&this->clock);

	free(jsm->ptr);
    jsm_clearfuncs(jsm);
}

static void new_button(struct JSM_CONTROLLER* cnt, const char* name, enum JKEYS common_id)
{
    struct HID_digital_button *b = cvec_push_back(&cnt->digital_buttons);
    snprintf(b->name, 40, "%s", name);
    b->state = 0;
    b->id = 0;
    b->kind = DBK_BUTTON;
    b->common_id = common_id;
}

static void setup_lcd(struct JSM_DISPLAY *d)
{
    d->standard = JSS_LCD;
    d->enabled = 1;

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


static void setup_audio(struct cvec* IOs)
{
    struct physical_io_device *pio = cvec_push_back(IOs);
    pio->kind = HID_AUDIO_CHANNEL;
    struct JSM_AUDIO_CHANNEL *chan = &pio->audio_channel;
    chan->sample_rate = (MASTER_CYCLES_PER_FRAME * 60) / 3;
    chan->low_pass_filter = 16000;
}

void GBJ_describe_io(JSM, struct cvec *IOs)
{
    // TODO guard against more than one init
    JTHIS;
    if (this->described_inputs) return;
    this->described_inputs = 1;

    this->IOs = IOs;

    // controllers
    struct physical_io_device *d = cvec_push_back(this->IOs);
    physical_io_device_init(d, HID_CONTROLLER, 0, 0, 1, 1);

    snprintf(d->controller.name, sizeof(d->controller.name), "%s", "GameBoy");
    d->id = 0;
    d->kind = HID_CONTROLLER;
    d->connected = 1;
    d->enabled = 1;

    struct JSM_CONTROLLER* cnt = &d->controller;

    // up down left right a b start select. in that order
    this->cpu.device_ptr = make_cvec_ptr(IOs, cvec_len(IOs)-1);
    pio_new_button(cnt, "up", DBCID_co_up);
    pio_new_button(cnt, "down", DBCID_co_down);
    pio_new_button(cnt, "left", DBCID_co_left);
    pio_new_button(cnt, "right", DBCID_co_right);
    pio_new_button(cnt, "a", DBCID_co_fire1);
    pio_new_button(cnt, "b", DBCID_co_fire2);
    pio_new_button(cnt, "start", DBCID_co_start);
    pio_new_button(cnt, "select", DBCID_co_select);

    // power and reset buttons
    struct physical_io_device* chassis = cvec_push_back(IOs);
    physical_io_device_init(chassis, HID_CHASSIS, 1, 1, 1, 1);
    struct HID_digital_button* b;
    b = cvec_push_back(&chassis->chassis.digital_buttons);
    snprintf(b->name, sizeof(b->name), "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    /*b = cvec_push_back(&chassis->chassis.digital_buttons);
    b->common_id = DBCID_ch_reset;
    snprintf(b->name, sizeof(b->name), "Reset");
    b->state = 0;*/

    // cartridge port
    d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_CART_PORT, 1, 1, 1, 0);
    d->cartridge_port.load_cart = &GBIO_load_cart;
    d->cartridge_port.unload_cart = &GBIO_unload_cart;

    // screen
    d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_DISPLAY, 1, 1, 0, 1);
    setup_lcd(&d->display);
    d->display.output[0] = malloc(160 * 144 * 2);
    d->display.output[1] = malloc(160 * 144 * 2);
    d->display.output_debug_metadata[0] = malloc(160 * 144 * 2);
    d->display.output_debug_metadata[1] = malloc(160 * 144 * 2);
    this->ppu.display_ptr = make_cvec_ptr(IOs, cvec_len(IOs)-1);
    this->ppu.cur_output = (u16 *)d->display.output[0];
    this->ppu.cur_output_debug_metadata = (u16 *)d->display.output_debug_metadata[0];
    d->display.last_written = 1;
    d->display.last_displayed = 1;

    setup_audio(IOs);

    this->ppu.display = &((struct physical_io_device *)cpg(this->ppu.display_ptr))->display;
}

void GBJ_killall(JSM) {
	// Do nothing
}

u32 GBJ_finish_frame(JSM) {
	JTHIS;
	i32 cycles_left = this->clock.cycles_left_this_frame;
	GBJ_step_master(jsm, cycles_left);
	return this->ppu.last_used_buffer;
}

u32 GBJ_finish_scanline(JSM) {
	JTHIS;
	printf("STEP SCANLINE NOT SUPPORT GB AS YET");
	return this->ppu.last_used_buffer ^ 1;
}

static void sample_audio(struct GB* this)
{
    if (this->audio.buf && (this->clock.master_clock >= (u64)this->audio.next_sample_cycle)) {
        this->audio.next_sample_cycle += this->audio.master_cycles_per_audio_sample;
        float *sptr = ((float *)this->audio.buf->ptr) + (this->audio.buf->upos);
        //assert(this->audio.buf->upos < this->audio.buf->samples_len);
        if (this->audio.buf->upos >= this->audio.buf->samples_len) {
            this->audio.buf->upos++;
        }
        else {
            *sptr = GB_APU_mix_sample(&this->apu, 0);
            this->audio.buf->upos++;
        }
    }

    struct debug_waveform *dw = cpg(this->dbg.waveforms.main);
    if (this->clock.master_clock >= dw->user.next_sample_cycle) {
        if (dw->user.buf_pos < dw->samples_requested) {
            dw->user.next_sample_cycle += dw->user.cycle_stride;
            ((float *) dw->buf.ptr)[dw->user.buf_pos] = GB_APU_mix_sample(&this->apu, 1);
            dw->user.buf_pos++;
        }
    }

    dw = cpg(this->dbg.waveforms.chan[0]);
    if (this->clock.master_clock >= dw->user.next_sample_cycle) {
        for (int j = 0; j < 4; j++) {
            dw = cpg(this->dbg.waveforms.chan[j]);
            if (dw->user.buf_pos < dw->samples_requested) {
                dw->user.next_sample_cycle += dw->user.cycle_stride;
                float sv = GB_APU_sample_channel(&this->apu, j);
                ((float *) dw->buf.ptr)[dw->user.buf_pos] = sv;
                dw->user.buf_pos++;
                assert(dw->user.buf_pos < 410);
            }
        }
    }
}

u32 GBJ_step_master(JSM, u32 howmany) {
	JTHIS;
	this->cycles_left += (i32)howmany;
	u32 cpu_step = this->clock.timing.cpu_divisor;
	while (this->cycles_left > 0) {
		this->clock.cycles_left_this_frame--;
		if (this->clock.cycles_left_this_frame <= 0)
			this->clock.cycles_left_this_frame += GB_CYCLES_PER_FRAME;
        if ((this->clock.turbo && ((this->clock.master_clock & 1) == 0)) || ((this->clock.master_clock & 3) == 0)) {
            GB_CPU_cycle(&this->cpu);
            this->clock.cpu_frame_cycle++;
            this->clock.cpu_master_clock += cpu_step;
        }
        if ((this->clock.master_clock & 3) == 0)
            GB_APU_cycle(&this->apu);
		this->clock.master_clock++;
		GB_PPU_run_cycles(&this->ppu, 1);
        sample_audio(this);
		this->clock.ppu_master_clock += 1;
		this->cycles_left--;
	}
	return this->ppu.last_used_buffer ^ 1;

}

u32 GB_bus_DMA_read(struct GB_bus* this, u32 addr)
{
	return GB_bus_CPU_read(this, addr, 0, 1);

	if (addr >= 0xA000) {
		printf("IMPLEMENT OAM >0xA000!");
	}
	else {
		return GB_bus_CPU_read(this, addr, 0, 1);
	}
}

void GB_bus_IRQ_vblank_up(struct GB_bus* this)
{
	this->cpu->cpu.regs.IF |= 1;
}

void GB_bus_IRQ_vblank_down(struct GB_bus* this)
{
	// Do nothin!
}

void GBJ_reset(JSM)
{
	JTHIS;
	GB_clock_reset(&this->clock);
	GB_CPU_reset(&this->cpu);
	GB_PPU_reset(&this->ppu);
    GB_bus_reset(&this->bus);
	//GB_cart_reset(&this->cart);
	if (GB_QUICK_BOOT) {
		GB_PPU_quick_boot(&this->ppu);
		GB_CPU_quick_boot(&this->cpu);
	}
}

void GBJ_get_framevars(JSM, struct framevars* out)
{
	JTHIS;
	out->master_frame = this->clock.master_frame;
	out->x = this->clock.lx;
	out->scanline = this->clock.ly;
    out->last_used_buffer = this->ppu.last_used_buffer;
}

void GBJ_enable_tracing(JSM)
{
    // TODO
    assert(1==0);
}

void GBJ_disable_tracing(JSM)
{
    // TODO
    assert(1==0);
}

void GBJ_play(JSM)
{

}

void GBJ_pause(JSM)
{

}

void GBJ_stop(JSM)
{

}

static void GBIO_load_cart(JSM, struct multi_file_set *mfs, struct physical_io_device *pio) {
    JTHIS;
    struct buf* b = &mfs->files[0].buf;
    GB_cart_load_ROM_from_RAM(&this->cart, b->ptr, b->size, pio);
    GBJ_reset(jsm);
}

static void GBIO_unload_cart(JSM){

}

void GBJ_load_BIOS(JSM, struct multi_file_set* mfs) {
	JTHIS;
    buf_copy(&this->BIOS, &mfs->files[0].buf);
}