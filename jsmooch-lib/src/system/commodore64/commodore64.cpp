//
// Created by . on 11/25/25.
//
#include <cassert>
#include "commodore64.h"
#include "c64_bus.h"

#define TAG_SCANLINE 1
#define TAG_FRAME 2

#define JSM jsm_system* jsm

u32 read_trace_m6502(void *ptr, u32 addr) {
    return static_cast<C64::core *>(ptr)->mem.read_main_bus(addr, 0, 0);
}

static void run_block(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<C64::core *>(ptr);
    th->run_cpu();
}

jsm_system *Commodore64_new(jsm::systems in_kind)
{
    auto* th = new C64::core(jsm::regions::USA);

    th->scheduler.max_block_size = 8;

    snprintf(th->label, sizeof(th->label), "Cosmac VIP %dk", in_kind == jsm::systems::COSMAC_VIP_2k ? 2 : 4);
    th->scheduler.max_block_size = 20;
    th->scheduler.run.func = &run_block;
    th->scheduler.run.ptr = th;

    jsm_debug_read_trace dt;
    dt.read_trace = &read_trace_m6502;
    dt.ptr = static_cast<void *>(th);
    th->cpu.setup_tracing(&dt, &th->master_clock);
    return th;
}

void C64_delete(JSM) {
    auto *th = dynamic_cast<C64::core *>(jsm);

    for (physical_io_device &pio : th->IOs) {
    }
    th->IOs.clear();

    delete th;
}



void C64::core::load_BIOS(multi_file_set& mfs) {
    if (mfs.files.size() == 1) { // 1 file. first 8KB BIOS, second 8KB RAM
        assert(mfs.files[0].buf.size == 0x4000);
        mem.load_BASIC(mfs.files[0].buf, 0);
        mem.load_KERNAL(mfs.files[0].buf, 0x2000);
    }
    else {
        if (mfs.files.size() >= 2) {
            assert(mfs.files[0].buf.size == 0x2000);
            assert(mfs.files[1].buf.size == 0x2000);
            mem.load_BASIC(mfs.files[0].buf, 0);
            mem.load_KERNAL(mfs.files[1].buf, 0);
        }
        if (mfs.files.size() >= 3) {
            assert(mfs.files[2].buf.size == 0x1000);
            mem.load_CHARGEN(mfs.files[2].buf, 0);
        }
    }
}
