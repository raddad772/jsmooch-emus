//
// Created by . on 7/24/24.
//

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "mac.h"
#include "mac_internal.h"

#include "helpers/int.h"
#include "helpers/physical_io.h"
#include "helpers/sys_interface.h"

#include "component/cpu/m68000/m68000.h"

#define JTHIS struct mac* this = (struct mac*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct mac* this


static void macJ_play(JSM);
static void macJ_pause(JSM);
static void macJ_stop(JSM);
static void macJ_get_framevars(JSM, struct framevars* out);
static void macJ_reset(JSM);
static void macJ_killall(JSM);
static u32 macJ_finish_frame(JSM);
static u32 macJ_finish_scanline(JSM);
static u32 macJ_step_master(JSM, u32 howmany);
static void macJ_load_BIOS(JSM, struct multi_file_set* mfs);
static void macJ_enable_tracing(JSM);
static void macJ_disable_tracing(JSM);
static void macJ_describe_io(JSM, struct cvec* IOs);

u32 read_trace_m68k(void *ptr, u32 addr, u32 UDS, u32 LDS) {
    struct mac* this = (struct mac*)ptr;
    return mac_mainbus_read(this, addr, UDS, LDS, this->io.cpu.last_read_data, 0);
}


void mac_new(struct jsm_system* jsm, enum mac_variants variant)
{
    struct mac* this = (struct mac*)calloc(1, sizeof(struct mac));
    this->kind = variant;
    M68k_init(&this->cpu, 1);

    struct jsm_debug_read_trace dt;
    dt.read_trace_m68k = &read_trace_m68k;
    dt.ptr = (void*)this;
    M68k_setup_tracing(&this->cpu, &dt, &this->clock.master_cycle_count);

    this->jsm.described_inputs = 0;
    this->jsm.IOs = NULL;
    this->jsm.cycles_left = 0;

    jsm->ptr = (void*)this;

    jsm->finish_frame = &macJ_finish_frame;
    jsm->finish_scanline = &macJ_finish_scanline;
    jsm->step_master = &macJ_step_master;
    jsm->reset = &macJ_reset;
    jsm->load_BIOS = &macJ_load_BIOS;
    jsm->killall = &macJ_killall;
    jsm->get_framevars = &macJ_get_framevars;
    jsm->enable_tracing = &macJ_enable_tracing;
    jsm->disable_tracing = &macJ_disable_tracing;
    jsm->play = &macJ_play;
    jsm->pause = &macJ_pause;
    jsm->stop = &macJ_stop;
    jsm->describe_io = &macJ_describe_io;
    jsm->sideload = NULL;

}

void mac_delete(struct jsm_system* system)
{

}
