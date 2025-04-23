//
// Created by . on 2/11/25.
//

#ifndef JSMOOCH_EMUS_SNES_BUS_H
#define JSMOOCH_EMUS_SNES_BUS_H

#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/physical_io.h"
#include "helpers/cvec.h"
#include "component/cpu/wdc65816/wdc65816.h"
#include "component/cpu/spc700/spc700.h"
#include "helpers/scheduler.h"
#include "helpers/sys_interface.h"


#include "snes.h"
#include "snes_clock.h"
#include "snes_mem.h"
#include "snes_cart.h"
#include "snes_ppu.h"
#include "r5a22.h"
#include "snes_apu.h"

struct snesched_item;
struct SNES;
typedef void (*snesched_callback)(struct SNES *, struct snesched_item *);

#define NUM_SNESCHED 12

// >> 2 = 1,2,3

struct SNES {
    struct R5A22 r5a22;
    struct SNES_APU apu;
    struct scheduler_t scheduler;
    struct SNES_clock clock;
    struct SNES_cart cart;
    struct SNES_PPU ppu;

    struct SNES_mem mem;


    i32 block_cycles_to_run;


    struct {
        struct cvec* IOs;
        u32 described_inputs;
    } jsm;

    DBG_START
        DBG_CPU_REG_START(wdc65816)
                    *C, *D, *X, *Y, *PBR, *PC, *S, *DBR, *E, *P
        DBG_CPU_REG_END(wdc65816)

        DBG_CPU_REG_START(spc700)
                    *A, *X, *Y, *SP, *PC
        DBG_CPU_REG_END(spc700)


        DBG_EVENT_VIEW

        DBG_IMAGE_VIEWS_START
        DBG_IMAGE_VIEWS_END

        DBG_WAVEFORM_START(apu)
            DBG_WAVEFORM_MAIN
            DBG_WAVEFORM_CHANS(8)
        DBG_WAVEFORM_END(psg)

    DBG_END
    struct {
        double master_cycles_per_audio_sample, master_cycles_per_min_sample, master_cycles_per_max_sample;
        double next_sample_cycle_max, next_sample_cycle_min, next_sample_cycle;
        double next_debug_cycle;
        struct audiobuf *buf;
        u64 cycles;
    } audio;

    struct {
    } opts;
};

#endif //JSMOOCH_EMUS_SNES_BUS_H
