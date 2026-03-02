//
// Created by . on 3/2/26.
//

#pragma once

#include "helpers/physical_io.h"
#include "helpers/buf.h"
#include "helpers/sys_interface.h"
#include "helpers/scheduler.h"
#include "helpers/cvec.h"
#include "helpers/elf_helpers.h"

#include "component/cpu/sh4/sh4_interpreter_opcodes.h"
#include "component/cpu/sh4/sh4_interpreter.h"
#include "dc_mem.h"
#include "spi.h"
#include "gdi.h"
#include "maple.h"
#include "controller.h"
#include "holly.h"
#include "triangle.h"
#include "gdrom.h"

namespace DC {

#define DC_CYCLES_PER_SEC 200000000

#define DC_INT_VBLANK_IN 0x08
#define DC_INT_VBLANK_OUT 0x10
#define DC_INT_GDROM   0x

struct core : jsm_system {
    core();
    SH4::core sh4;
    SH4::memaccess_t sh4mem{};

    void mem_init();
    void connect_controller(int portnum, DC::controller* which);

    u64 G1_read(u32 addr, u8 sz, bool* success);
    void G1_write(u32 addr, u64 val, u8 sz, bool* success);
    void G2_write(u32 addr, u64 val, u8 sz, bool* success);
    u64 G2_read(u32 addr, u8 sz, bool* success);
    void G2_write_ADST(u64 val);
    void G2_write_E1ST(u64 val);
    void G2_write_E2ST(u64 val);
    void G2_write_DDST(u64 val);
    void write_C2DST(u32 val);
    void write_SDST(u32 val);
    u64 aica_read(u32 addr, u8 sz, bool* success);
    void aica_write(u32 addr, u64 val, u8 sz, bool* success);
    u32 RTC_read();
    u64 read_area0(u32 addr, u8 sz, bool* success);
    void write_area0(u32 addr, u64 val, u8 sz, bool* success);
    u64 read_area1(u32 addr, u8 sz, bool* success);
    void write_area1(u32 addr, u64 val, u8 sz, bool* success);
    u64 read_area3(u32 addr, u8 sz, bool* success);
    void write_area3(u32 addr, u64 val, u8 sz, bool* success);
    u64 read_area4(u32 addr, u8 sz, bool* success);
    void write_area4(u32 addr, u64 val, u8 sz, bool* success);
    u64 read_flash(u32 addr, bool *success, u8 sz);
    u32 fetch_ins(u32 addr);
    void new_frame(bool copy_buf);
    void copy_fb(u32* where);
    void schedule_frame();
    void CPU_state_after_boot_rom();
    void old_ROM_load(multi_file_set &mfs);
    void RAM_state_after_boot_rom(read_file_buf *IPBIN);
    void update_dma_triggers(u32 addr, u64 val);

    u64 trace_cycles{};
    u64 waitstates{};

    controller c1{};
    controller c2{};

    elf_symbol_list32 elf_symbols{};

    u8 RAM[16 * 1024 * 1024]{};
    u8 VRAM[HOLLY_VRAM_SIZE]{};

    bool described_inputs{};

    buf BIOS{};
    buf ROM{};

    scheduler_t scheduler;

    struct {
        buf buf{};
    } flash{};

    u64 mainbus_read(u32 addr, u8 sz, bool is_ins_fetch);
    void mainbus_write(u32 addr, u64 val, u8 sz);

    struct {
        u32 broadcast{}; // 0 -> NTSC{}, 1 -> PAL{}, 2 -> PAL/M{}, 3 -> PAL/N{}, 4 -> default
        u32 language{}; // 0 -> JP{}, 1 -> EN{}, 2 -> DE{}, 3 -> FR{}, 4 -> SP{}, 5 -> IT{}, 6 -> default
        u32 region{}; // 0 -> JP{}, 1 -> USA{}, 2 -> EU{}, 3 -> default
    } settings{};

    struct { // io
#include "generated/io_decls.h"
    } io{};

    struct {
#include "generated/g2_decls.h"
    } g2{};

    struct {
        u32 ARMRST{};
        u8 mem[0x8000]{};
        u8 wave_mem[2*1024*1024]{};
    } aica{};

    struct {
        i32 frame_cycle{};
        i64 frame_start_cycle{};
        u32 cycles_per_frame{};
        u32 cycles_per_line{};
        u32 in_vblank{};

        struct {
            u32 vblank_in_start{};
            u32 vblank_in_end{};
            u32 vblank_out_start{};
            u32 vblank_out_end{};

            u32 vblank_in_yet{};
            u32 vblank_out_yet{};

        } interrupt{};
    } clock{};

    HOLLY::core holly;

    struct {
#include "generated/g1_decls.h"
    } g1{};

    GDROM::DRIVE gdrom;
    struct {
        u32 SB_FFST{};
        u32 SB_FFST_rc{};
    } sb{};

    MAPLE::core maple;

    struct {
        void *rptr[0x40]{};
        void *wptr[0x40]{};
        u64 (*read[0x40])(void*, u32, u8 sz, bool*){};
        void (*write[0x40])(void*, u32, u64, u8 sz, bool*){};
    } mem{};

public:
    void play() final;
    void pause() final;
    void stop() final;
    void get_framevars(framevars& out) final;
    void reset() final;
    void killall();
    u32 finish_frame() final;
    u32 finish_scanline() final;
    u32 step_master(u32 howmany) final;
    void load_BIOS(multi_file_set& mfs) final;
    void enable_tracing();
    void disable_tracing();
    void describe_io() final;
    //void save_state(serialized_state &state) final;
    //void load_state(serialized_state &state, deserialize_ret &ret) final;
    void set_audiobuf(audiobuf *ab) final;
    void setup_debugger_interface(debugger_interface &intf) final;
    void sideload(multi_file_set& mfs) final;
};

}