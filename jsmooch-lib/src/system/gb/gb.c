#include "assert.h"
#include "stdlib.h"
#include <stdio.h>

#include "helpers/sys_interface.h"
#include "helpers/physical_io.h"
#include "helpers/debugger/debugger.h"

#include "gb.h"
#include "gb_cpu.h"
#include "gb_ppu.h"
#include "gb_clock.h"
#include "gb_bus.h"
#include "gb_enums.h"
#include "gb_debugger.h"

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
static void GBIO_load_cart(JSM, struct multi_file_set *mfs, struct buf* sram);

void GB_new(JSM, enum GB_variants variant)
{
	struct GB* this = (struct GB*)malloc(sizeof(struct GB));
    this->described_inputs = 0;
    GB_clock_init(&this->clock);
	GB_bus_init(&this->bus, &this->clock);
	GB_CPU_init(&this->cpu, variant, &this->clock, &this->bus);
	GB_PPU_init(&this->ppu, variant, &this->clock, &this->bus);
	GB_cart_init(&this->cart, variant, &this->clock, &this->bus);
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
	jsm->killall = &GBJ_killall;
	jsm->get_framevars = &GBJ_get_framevars;
    jsm->describe_io = &GBJ_describe_io;

    jsm->enable_tracing = &GBJ_enable_tracing;
    jsm->disable_tracing = &GBJ_disable_tracing;

	jsm->play = &GBJ_play;
	jsm->pause = &GBJ_pause;
	jsm->stop = &GBJ_stop;

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

    sprintf(d->controller.name, "%s", "GameBoy");
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
    sprintf(b->name, "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    /*b = cvec_push_back(&chassis->chassis.digital_buttons);
    b->common_id = DBCID_ch_reset;
    sprintf(b->name, "Reset");
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

u32 GBJ_step_master(JSM, u32 howmany) {
	JTHIS;
	this->cycles_left += (i32)howmany;
	u32 cpu_step = this->clock.timing.cpu_divisor;
	while (this->cycles_left > 0) {
		this->clock.cycles_left_this_frame--;
		if (this->clock.cycles_left_this_frame <= 0)
			this->clock.cycles_left_this_frame += GB_CYCLES_PER_FRAME;
		if ((this->clock.master_clock & 3) == 0) {
			GB_CPU_cycle(&this->cpu);
			this->clock.cpu_frame_cycle++;
			this->clock.cpu_master_clock += cpu_step;
		}
		this->clock.master_clock++;
		GB_PPU_run_cycles(&this->ppu, 1);
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

static void GBIO_load_cart(JSM, struct multi_file_set *mfs, struct buf* sram)
{
    JTHIS;
    struct buf* b = &mfs->files[0].buf;
    GB_cart_load_ROM_from_RAM(&this->cart, b->ptr, b->size);
    GBJ_reset(jsm);
}

static void GBIO_unload_cart(JSM){

}

void GBJ_load_BIOS(JSM, struct multi_file_set* mfs) {
	JTHIS;
    buf_copy(&this->BIOS, &mfs->files[0].buf);
}