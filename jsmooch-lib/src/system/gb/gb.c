#include "assert.h"
#include "stdlib.h"
#include <stdio.h>
#include "helpers/sys_interface.h"
#include "gb.h"
#include "gb_cpu.h"
#include "gb_ppu.h"
#include "gb_clock.h"
#include "gb_bus.h"
#include "gb_enums.h"

#define _CRT_SECURE_NO_WARNINGS

#define JTHIS struct GB* this = (struct GB*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct GB* this

#define GB_QUICK_BOOT true

void GB_get_description(struct jsm_system* jsm, struct machine_description* d);
u32 GB_bus_DMA_read(struct GB_bus* this, u32 addr);
void GB_bus_IRQ_vblank_up(struct GB_bus* this);
void GB_bus_IRQ_vblank_down(struct GB_bus* this);
void GBJ_play(JSM);
void GBJ_pause(JSM);
void GBJ_stop(JSM);
void GBJ_get_framevars(JSM, struct framevars* out);
void GBJ_reset(JSM);
void GBJ_map_inputs(JSM, u32* bufptr, u32 bufsize);
void GBJ_get_description(JSM, struct machine_description* d);
void GBJ_killall(JSM);
u32 GBJ_finish_frame(JSM);
u32 GBJ_finish_scanline(JSM);
u32 GBJ_step_master(JSM, u32 howmany);
void GBJ_load_BIOS(JSM, struct multi_file_set* mfs);
void GBJ_load_ROM(JSM, struct multi_file_set* mfs);
void GB_write_IO(struct GB_bus* bus, u32 addr, u32 val);
u32 GB_read_IO(struct GB_bus* bus, u32 addr, u32 val);
void GBJ_enable_tracing(JSM);
void GBJ_disable_tracing(JSM);

void GB_new(JSM, enum GB_variants variant, struct JSM_IOmap *iomap)
{
	struct GB* this = (struct GB*)malloc(sizeof(struct GB));
    GB_clock_init(&this->clock);
	GB_bus_init(&this->bus, &this->clock);
	GB_CPU_init(&this->cpu, variant, &this->clock, &this->bus);
	GB_PPU_init(&this->ppu, variant, &this->clock, &this->bus);
	GB_cart_init(&this->cart, variant, &this->clock, &this->bus);

    this->ppu.last_used_buffer = 0;
    this->ppu.cur_output_num = 0;
    this->ppu.cur_output = (u16 *)iomap->out_buffers[0];
    this->ppu.out_buffer[0] = (u16 *)iomap->out_buffers[0];
    this->ppu.out_buffer[1] = (u16 *)iomap->out_buffers[1];

	this->variant = variant;
	this->cycles_left = 0;

	this->bus.DMA_read = &GB_bus_DMA_read;
	this->bus.IRQ_vblank_down = &GB_bus_IRQ_vblank_down;
	this->bus.IRQ_vblank_up = &GB_bus_IRQ_vblank_up;
	this->BIOS = NULL;
	this->BIOS_size = 0;

    this->bus.read_IO = &GB_read_IO;
    this->bus.write_IO = &GB_write_IO;


	jsm->ptr = (void*)this;

	jsm->get_description = &GBJ_get_description;
	jsm->finish_frame = &GBJ_finish_frame;
	jsm->finish_scanline = &GBJ_finish_scanline;
	jsm->step_master = &GBJ_step_master;
	jsm->reset = &GBJ_reset;
	jsm->load_ROM = &GBJ_load_ROM;
	jsm->load_BIOS = &GBJ_load_BIOS;
	jsm->killall = &GBJ_killall;
	jsm->map_inputs = &GBJ_map_inputs;
	jsm->get_framevars = &GBJ_get_framevars;

    jsm->enable_tracing = &GBJ_enable_tracing;
    jsm->disable_tracing = &GBJ_disable_tracing;

	jsm->play = &GBJ_play;
	jsm->pause = &GBJ_pause;
	jsm->stop = &GBJ_stop;
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
	//GB_clock_delete(&this->clock);

	if (this->BIOS != NULL) {
		free(this->BIOS);
		this->BIOS = NULL;
		this->BIOS_size = 0;
	}

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

void GBJ_get_description(JSM, struct machine_description* d)
{
	JTHIS;
	sprintf(d->name, "GameBoy");
	switch (this->variant) {
	case GBC:
		sprintf(d->name, "GameBoy Color");
		break;
	case SGB:
		sprintf(d->name, "Super GameBoy");
		break;
	}
	d->fps = 60;
	d->timing = frame;
	d->display_standard = MD_LCD;
	d->x_resolution = 160;
	d->y_resolution = 144;
	d->xrh = 160;
	d->xrw = 144;

	d->overscan.top = 0;
	d->overscan.bottom = 0;
	d->overscan.left = 0;
	d->overscan.right = 0;

	d->out_size = (160 * 144 * 4);

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
	sprintf(k->name, "a");

	k = &d->keymap[5];
	k->buf_pos = 5;
	sprintf(k->name, "b");

	k = &d->keymap[6];
	k->buf_pos = 6;
	sprintf(k->name, "start");

	k = &d->keymap[7];
	k->buf_pos = 7;
	sprintf(k->name, "select");

	d->keymap_size = 8;
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

void GBJ_map_inputs(JSM, u32* bufptr, u32 bufsize) {
	JTHIS;
	this->controller_in.up = bufptr[0];
	this->controller_in.down = bufptr[1];
	this->controller_in.left = bufptr[2];
	this->controller_in.right = bufptr[3];
	this->controller_in.a = bufptr[4];
	this->controller_in.b = bufptr[5];
	this->controller_in.start = bufptr[6];
	this->controller_in.select = bufptr[7];
	GB_CPU_update_inputs(&this->cpu, &this->controller_in);
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

void GBJ_load_ROM(JSM, struct multi_file_set* mfs)
{
	JTHIS;
    struct buf* b = &mfs->files[0].buf;
	GB_cart_load_ROM_from_RAM(&this->cart, b->ptr, b->size);
	GBJ_reset(jsm);
}

void GBJ_load_BIOS(JSM, struct multi_file_set* mfs) {
	JTHIS;
    struct buf* b = &mfs->files[0].buf;
	this->BIOS = (u8 *)malloc(b->size);
	for (u32 i = 0; i < b->size; i++) {
		this->BIOS[i] = ((u8*)b->ptr)[i];
	}
    this->BIOS_size = b->size;
}

