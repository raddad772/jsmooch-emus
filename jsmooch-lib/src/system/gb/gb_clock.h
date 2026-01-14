#pragma once

#include "helpers/int.h"
#include "gb_enums.h"

namespace GB {

struct clock {
    void reset();
    void setCPU_can_OAM(bool to);
    static constexpr u32 cycles_per_frame = 70224;
    PPU::modes ppu_mode{PPU::OAM_search}; // = GB_PPU_modes.OAM_search
    u32 frames_since_restart{};
    u32 master_frame{};

    i32 cycles_left_this_frame{cycles_per_frame};

    u32 trace_cycles{};

    u32 master_clock{};
    u32 ppu_master_clock{};
    u32 cpu_master_clock{};

    u32 cgb_enable{};
    u32 turbo{};

    u32 ly{};
    u32 lx{};

    u32 wly{};

    u32 cpu_frame_cycle{};
    bool CPU_can_VRAM{true};
    bool old_OAM_can{};
    bool CPU_can_OAM{};
    u32 bootROM_enabled{true};

    u32 SYSCLK{};

    struct {
        u32 ppu_divisor{1};// : u32 = 1
        u32 cpu_divisor{4};// : u32 = 4
        u32 apu_divisor{4}; // : u32 = 4
    } timing{};
};


}