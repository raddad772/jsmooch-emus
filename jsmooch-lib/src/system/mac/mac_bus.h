//
// Created by . on 7/24/24.
//

#pragma once

#include "helpers/int.h"
#include "helpers/sys_interface.h"
#include "helpers/debug.h"
#include "helpers/physical_io.h"
#include "helpers/debugger/debugger.h"

#include "component/misc/via6522/via6522.h"
#include "component/cpu/m68000/m68000.h"
#include "component/floppy/mac_floppy.h"

#include "mac_iwm.h"
#include "mac_clock.h"
#include "mac_display.h"
#include "mac_via.h"
#include "mac_rtc.h"
#include "mac.h"

namespace mac {

struct core : jsm_system {
    explicit core(variants variant);
    ~core();
    void set_sound_output(u32 set_to);
    void step_CPU();
    void set_cpu_irq();
    void step_eclock();
    void step_bus();
    u16 mainbus_read_iwm(u32 addr, u16 mask, u16 old, bool has_effect);
    void mainbus_write_iwm(u32 addr, u16 mask, u16 val);
    u16 mainbus_read_scc(u32 addr, u16 mask, u16 old, bool has_effect);
    void mainbus_write_scc(u32 addr, u16 mask, u16 val);
    void write_RAM(u32 addr, u16 mask, u16 val);
    u16 read_ROM(u32 addr, u16 mask, u16 old);
    u16 read_RAM(u32 addr, u16 mask, u16 old) const;
    void mainbus_write(u32 addr, u32 UDS, u32 LDS, u16 val);
    u16 mainbus_read(u32 addr, u32 UDS, u32 LDS, u16 old, bool has_effect);

    variants kind{};
    M68k::core cpu{false};
    clock clock{};
    display display{this};
    via via{this};
    iwm iwm{this};
    RTC rtc{this};

    struct {
        struct {
            u8 aData{}, aCtl{}, bData{}, bCtl{};
        } regs{};
    } scc{};

    struct {
        i64 cycles_left;
        bool described_inputs;
    } jsm{};

    struct {
        struct {
            u32 on{};
        } io{};
    } sound{};

    u16 *ROM{};
    u16 *RAM{};
    bool ram_contended{}; // by display
    u32 ROM_size{}, ROM_mask{};
    u32 RAM_size{}, RAM_mask{};

    cvec_ptr<physical_io_device> disc_drive;
    cvec_ptr<physical_io_device> keyboard;

    struct {
        bool ROM_overlay{};
        u32 eclock{};

        struct {
            u16 last_read_data{};
        } cpu{};

        struct {
            bool via{}, scc{}, iswitch{};
        } irq{};
    } io{};

    DBG_START
    DBG_EVENT_VIEW
    DBG_CPU_REG_START1
        *D[8], *A[8],
        *PC, *USP, *SSP, *SR,
        *supervisor, *trace,
        *IMASK, *CSR, *IR, *IRC
    DBG_CPU_REG_END1
    DBG_END

    void setup_crt(JSM_DISPLAY &d);
    void setup_audio(std::vector<physical_io_device> &inIOs);
    void setup_keyboard();
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
    void save_state(serialized_state &state) final;
    void load_state(serialized_state &state, deserialize_ret &ret) final;
    void set_audiobuf(audiobuf *ab) final;
    void setup_debugger_interface(debugger_interface &intf) final;
    //void sideload(multi_file_set& mfs) final;
};


}