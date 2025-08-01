#ifndef _GB_H
#define _GB_H

#include "helpers/int.h"
#include "helpers/sys_interface.h"
#include "helpers/debugger/debuggerdefs.h"

#include "component/audio/gb_apu/gb_apu.h"

#include "gb_enums.h"
#include "gb_clock.h"
#include "gb_bus.h"
#include "gb_ppu.h"
#include "gb_cpu.h"
#include "cart.h"

void GB_new(struct jsm_system* system, enum GB_variants variant);
void GB_delete(struct jsm_system* system);

struct GB_inputs {
    u32 a;
    u32 b;
    u32 start;
    u32 select;
    u32 up;
    u32 down;
    u32 left;
    u32 right;
};

struct DBGGBROW {
    struct {
        u32 SCX, SCY, wx, wy, bg_tile_map_base, window_tile_map_base, window_enable, bg_window_tile_data_base;
    } io;
    u16 sprite_pixels[160];
};

struct GB {
    struct GB_bus bus;
    struct GB_clock clock;
    struct GB_CPU cpu;
    struct GB_PPU ppu;
    struct GB_APU apu;
    enum GB_variants variant;

    struct cvec* IOs;

    u32 described_inputs;

    struct GB_cart cart;
    struct GB_inputs controller_in;
    i32 cycles_left;

    struct buf BIOS;

    struct {
        double master_cycles_per_audio_sample;
        double next_sample_cycle;
        struct audiobuf *buf;
    } audio;

    DBG_START
    DBG_CPU_REG_START1 *A, *X, *Y, *P, *S, *PC DBG_CPU_REG_END1
    DBG_EVENT_VIEW

    DBG_IMAGE_VIEWS_START
    MDBG_IMAGE_VIEW(nametables)
    MDBG_IMAGE_VIEW(sprites)
    MDBG_IMAGE_VIEW(tiles)
    DBG_IMAGE_VIEWS_END

    DBG_WAVEFORM_START1
        DBG_WAVEFORM_MAIN
        DBG_WAVEFORM_CHANS(4)
    DBG_WAVEFORM_END1


    DBG_END

    struct {
        struct DBGGBROW rows[144];
        struct DBGGBROW *row;
    } dbg_data;

};

#endif
