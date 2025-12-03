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
#include "mac.h"

namespace mac {

struct core : jsm_system {
    variants kind{};
    M68k::core cpu{false};
    clock clock{};
    display display{this};
    via via{this};
    iwm iwm{this};

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

    struct {
        u8 param_mem[20]{};
        u8 old_clock_bit{};
        u8 cmd{};
        u32 seconds{};

        u64 cycle_counter{};

        u8 test_register{};
        u8 write_protect_register{};
        struct {
            u8 shift{}, progress{}, kind{};
        } tx{};
    } rtc{};

    DBG_START
    DBG_EVENT_VIEW
    DBG_CPU_REG_START1
        *D[8], *A[8],
        *PC, *USP, *SSP, *SR,
        *supervisor, *trace,
        *IMASK, *CSR, *IR, *IRC
    DBG_CPU_REG_END1
    DBG_END
};


}