//
// Created by . on 3/2/26.
//

#include "dc_bus.h"
#include "holly.h"
namespace DC {
#define MTHIS auto *th = static_cast<core *>(ptr)

static u64 DCread(void *ptr, u32 addr, u8 sz, bool is_ins_fetch) {
    MTHIS;
    return th->mainbus_read(addr, sz, is_ins_fetch);
}

static void DCwrite(void *ptr, u32 addr, u64 val, u8 sz) {
    MTHIS;
    th->mainbus_write(addr, val, sz);
}


static u32 DCfetch_ins(void *ptr, u32 addr) {
    MTHIS;
    return th->fetch_ins(addr);
}

static u64 DCread_noins(void *ptr, u32 addr, u8 sz)
{
    return DCread(ptr, addr, sz, false);
}

static void run_block(void *ptr, u64 num_cycles, u64 clock, u32 jitter) {
    auto *th = static_cast<core *>(ptr);
    th->sh4.run_cycles(num_cycles);
}

core::core() :
        scheduler(&trace_cycles),
        sh4(&scheduler),
        holly(this),
        gdrom(this),
        maple(this)
{
    has.load_BIOS = true;
    has.max_loaded_files = 0;
    has.max_loaded_folders = 0;
    has.save_state = false;
    has.set_audiobuf = false;

    fflush(stdout);
    scheduler.max_block_size = 150;
    scheduler.run.func = &run_block;
    scheduler.run.ptr = this;
    snprintf(label, sizeof(label), "Sega Dreamcast");

    dbg.dcptr = this;

    struct jsm_debug_read_trace rt;
    rt.ptr = (void *)this;
    sh4.setup_tracing(&rt, &trace_cycles);
    trace_cycles = 0;
    described_inputs = false;
    sh4.mptr = (void *)this;
    sh4.read = &DCread_noins;
    sh4.write = &DCwrite;
    sh4.fetch_ins = &DCfetch_ins;
    sh4.give_memaccess(&sh4mem);
    mem_init();

    for (u32 i = 0; i < 4; i++) {
        maple.ports[i].num = i;
    }
    connect_controller(0, &c1);

    //gdrom.reset();

    clock.frame_cycle = 0;
    clock.cycles_per_frame = DC_CYCLES_PER_SEC / 60;

    sb.SB_FFST = sb.SB_FFST_rc = 0;

    //holly.reset();
    holly.master_frame = -1;

    //new_frame(false);

    //gdrom.reset();

    settings.broadcast = 4; // NTSC
    settings.region = 0; //1; // EN
    settings.language = 6; //1; // EN
}

}