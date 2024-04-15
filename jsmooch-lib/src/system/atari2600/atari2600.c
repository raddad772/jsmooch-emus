//
// Created by Dave on 4/14/24.
//

#include "assert.h"
#include "stdlib.h"
#include <stdio.h>
#include "helpers/sys_interface.h"

#include "component/cpu/m6502/m6502.h"
#include "component/cpu/m6502/m6502_opcodes.h"

#include "atari2600.h"
#include "cart.h"

#define JTHIS struct atari2600* this = (struct atari2600*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct atari2600* this

static void atari2600_inputs_init(struct atari2600_inputs* this) {
    *this = (struct atari2600_inputs) {
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
void atari2600J_get_framevars(JSM, struct framevars* out);
void atari2600J_reset(JSM);
void atari2600J_map_inputs(JSM, u32* bufptr, u32 bufsize);
void atari2600J_get_description(JSM, struct machine_description* d);
void atari2600J_killall(JSM);
u32 atari2600J_finish_frame(JSM);
u32 atari2600J_finish_scanline(JSM);
u32 atari2600J_step_master(JSM, u32 howmany);
void atari2600J_load_BIOS(JSM, struct multi_file_set* mfs);
void atari2600J_load_ROM(JSM, struct multi_file_set* mfs);
void atari2600J_enable_tracing(JSM);
void atari2600J_disable_tracing(JSM);

static void atari2600_mmap(struct atari2600* this) {
    u32 addr;
    for (addr = 0; addr < 0x2000; addr++) {
        u32 a7, a9, a12;
        a7 = (addr >> 7) & 1;
        a9 = (addr >> 9) & 1;
        a12 = (addr >> 12) & 1;
        if (a12) { // ROM
            this->mem_map[addr] = 0x1000 | (addr & 0xFFF);
        }
        else if (a9 & a7) { // RIOT
            this->mem_map[addr] = (addr & 0x1F) + 0x280;
        }
        else if (a7) { // RIOT RAM
            this->mem_map[addr] = (addr & 0x7F) + 0x80;
        }
        else if (a9 == 0) { // TIA
            this->mem_map[addr] = addr & 0x3F;
        }
        else {
            printf("\nMISSED ADDR %04x", addr);
        }
    }
}


void atari2600_new(JSM, struct JSM_IOmap *iomap)
{
    struct atari2600* this = (struct atari2600*)malloc(sizeof(struct atari2600));

    atari2600_mmap(this);
    M6502_init(&this->cpu, M6502_decoded_opcodes);
    M6532_init(&this->riot);
    TIA_init(&this->tia);
    atari2600_cart_init(&this->cart);

    this->tia.last_used_buffer = 0;
    this->tia.cur_output_num = 0;
    this->tia.cur_output = (u16 *)iomap->out_buffers[0];
    this->tia.out_buffer[0] = (u16 *)iomap->out_buffers[0];
    this->tia.out_buffer[1] = (u16 *)iomap->out_buffers[1];


    this->clock.timing.cpu_divisor = 3;
    this->clock.timing.tia_divisor = 1;
    this->clock.frames_since_restart = 0;
    this->clock.timing.vblank_in_lines = 40; // 40 NTSC, 48 PAL
    this->clock.timing.display_line_start = 192; // 192 NTSC, 22? PAL
    this->clock.timing.vblank_out_start = 192 + 40;
    this->clock.timing.vblank_out_lines = 30; // 30 NTSC, 36 PAL

    this->cycles_left = 0;
    this->display_enabled = 1;
    atari2600_inputs_init(&this->controller1_in);
    atari2600_inputs_init(&this->controller2_in);

    jsm->ptr = (void*)this;
    jsm->which = SYS_ATARI2600;

    jsm->get_description = &atari2600J_get_description;
    jsm->finish_frame = &atari2600J_finish_frame;
    jsm->finish_scanline = &atari2600J_finish_scanline;
    jsm->step_master = &atari2600J_step_master;
    jsm->reset = &atari2600J_reset;
    jsm->load_ROM = &atari2600J_load_ROM;
    jsm->load_BIOS = &atari2600J_load_BIOS;
    jsm->killall = &atari2600J_killall;
    jsm->map_inputs = &atari2600J_map_inputs;
    jsm->get_framevars = &atari2600J_get_framevars;
    jsm->enable_tracing = &atari2600J_enable_tracing;
    jsm->disable_tracing = &atari2600J_disable_tracing;
    jsm->play = &atari2600J_play;
    jsm->pause = &atari2600J_pause;
    jsm->stop = &atari2600J_stop;

    atari2600J_reset(jsm);
}

void atari2600_delete(JSM)
{
    JTHIS;
    atari2600_cart_delete(&this->cart);

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

void atari2600J_get_framevars(JSM, struct framevars* out)
{
    JTHIS;
    out->master_frame = this->clock.master_frame;
    out->x = this->clock.line_cycle;
    out->scanline = this->clock.sy;
}

void atari2600J_reset(JSM)
{
    JTHIS;
    //atari2600_clock_reset(&this->clock);
    M6502_reset(&this->cpu);
    TIA_reset(&this);
    M6532_reset(&this);

    this->clock.master_frame = 0;
    this->clock.master_clock = 0;
    this->clock.line_cycle = 0;
    this->clock.sy = 0;

}

void atari2600J_map_inputs(JSM, u32* bufptr, u32 bufsize)
{
    JTHIS;

    // TODO
    /*this->controller1_in.up = bufptr[0];
    this->controller1_in.down = bufptr[1];
    this->controller1_in.left = bufptr[2];
    this->controller1_in.right = bufptr[3];
    this->controller1_in.a = bufptr[4];
    this->controller1_in.b = bufptr[5];
    this->controller1_in.start = bufptr[6];
    this->controller1_in.select = bufptr[7];
    // Controller 2 not support yet
    r2A03_update_inputs(&this->cpu, &this->controller1_in, &this->controller2_in);*/
}

void atari2600J_get_description(JSM, struct machine_description* d)
{
    JTHIS;
    sprintf(d->name, "Atari 2600 VCS");
    d->fps = 60;
    d->timing = frame;
    d->display_standard = MD_NTSC;
    d->x_resolution = 160;
    d->y_resolution = 240;
    d->xrh = 4;
    d->xrw = 5;

    d->overscan.top = 8;
    d->overscan.bottom = 8;
    d->overscan.left = 8;
    d->overscan.right = 8;

    d->out_size = (160 * 240 * 4);

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
    sprintf(k->name, "fire");

    d->keymap_size = 5;
}

void atari2600J_killall(JSM)
{

}

u32 atari2600J_finish_frame(JSM)
{
    JTHIS;
    u32 current_frame = this->clock.master_frame;
    while (this->clock.master_frame == current_frame) {
        atari2600J_finish_scanline(jsm);
        if (dbg.do_break) break;
    }
    return 0; // TODO this->tia.last_used_buffer;
}

void CPU_run_cycle(struct atari2600* this)
{
    if (this->cpu.pins.RDY) return; // CPU is halted until next scanline
    this->cpu.pins.D = this->CPU_bus.D;

    M6502_cycle(&this->cpu);

    this->CPU_bus.Addr.u = this->cpu.pins.Addr & 0x1FFF;
    this->CPU_bus.RW = this->cpu.pins.RW;
    this->CPU_bus.D = this->cpu.pins.D;

    if (this->CPU_bus.Addr.a12) // cart
        atari2600_cart_bus_cycle(&this->cart, this->CPU_bus.Addr.u, &this->CPU_bus.D, this->CPU_bus.RW);
    else if ((this->CPU_bus.Addr.a9 && this->CPU_bus.Addr.a7) || this->CPU_bus.Addr.a7) // RIOT, RIOT RAM
        M6532_bus_cycle(&this->riot, this->CPU_bus.Addr.u, &this->CPU_bus.D, this->CPU_bus.RW);
    else if (this->CPU_bus.Addr.a9 == 0) { // TIA
        TIA_bus_cycle(this, this->CPU_bus.Addr.u, &this->CPU_bus.D, this->CPU_bus.RW);
    }
    else {
        printf("\nMISSED ADDR %04x", this->CPU_bus.Addr.u);
    }

    /*if (this->tracing) { TODO
        //dbg.traces.add(TRACERS.M6502, this->clock->trace_cycles, trace_format_write('MOS', MOS_COLOR, this->clock->trace_cycles, this->cpu.pins.Addr, this->cpu.pins.D));
    }*/

}

u32 atari2600J_finish_scanline(JSM)
{
    JTHIS;
    u32 start_y = this->clock.sy;
    while (this->clock.sy == start_y) {
        atari2600J_step_master(jsm, 1);
        if (dbg.do_break) break;
    }
    return 0;
}

u32 atari2600J_step_master(JSM, u32 howmany)
{
    JTHIS;
    this->cycles_left += (i32)howmany;
    while (this->cycles_left > 0) {
        if ((this->clock.master_clock % 3) == 0)
            CPU_run_cycle(this);
        TIA_run_cycle(&this->tia);

        this->clock.master_clock++;
        this->cycles_left--;
        if (dbg.do_break) break;
    }
    return 0;
}

void atari2600J_load_BIOS(JSM, struct multi_file_set* mfs)
{
    printf("\nAtari 2600 doesn't have a BIOS...?");
}

void atari2600J_load_ROM(JSM, struct multi_file_set* mfs)
{
    JTHIS;
    struct buf* b = &mfs->files[0].buf;
    atari2600_cart_load_ROM_from_RAM(&this->cart, b->ptr, b->size, mfs->files[0].name);
    atari2600J_reset(jsm);
}

