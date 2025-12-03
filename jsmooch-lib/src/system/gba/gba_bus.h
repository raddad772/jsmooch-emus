//
// Created by . on 12/4/24.
//

#pragma once
//#define GBA_STATS

#include "gba_clock.h"
#include "gba_ppu.h"
#include "gba_dma.h"
#include "gba_apu.h"
#include "gba_controller.h"
#include "cart/gba_cart.h"
#include "gba_timers.h"
#include "helpers/audiobuf.h"

#include "component/cpu/arm7tdmi/arm7tdmi.h"

namespace GBA {
typedef u32 (*rdfunc)(core *, u32 addr, u32 sz, u32 access, bool has_effect);
typedef void (*wrfunc)(core *, u32 addr, u32 sz, u32 access, u32 val);

struct core {
    core();
    ARM7TDMI::core cpu;
    clock clock{};
    cart::core cart;
    PPU::core ppu;
    controller controller{};
    APU apu{};
    scheduler_t scheduler;

    [[nodiscard]] u64 clock_current() const { return clock.master_cycle_count + waitstates.current_transaction; }
    void eval_irqs();
    void process_button_IRQ();

    struct { // Only bits 27-24 are needed to distinguish valid endpoints
        rdfunc read[16]{};
        wrfunc write[16]{};
    } mem{};

    struct {
        u64 current_transaction{};
        struct {
            u32 sram{};
            u32 ws0_n, ws0_s{};
            u32 ws1_n, ws1_s{};
            u32 ws2_n, ws2_s{};
            u32 empty_bit{};
            u32 phi_term{};
        } io{};
        u32 sram{};
        u32 timing16[2][16]{}; // 0=nonsequential, 1=sequential
        u32 timing32[2][16]{}; // 0=nonsequential, 1=sequential
    } waitstates{};

    char WRAM_slow[256 * 1024]{};
    char WRAM_fast[32 * 1024]{};

    struct {
        u32 has{};
        char data[16384]{};
    } BIOS{};

    struct {
        struct {
            u32 open_bus_data{};
        } cpu{};
        struct {
            u32 buttons{};
            u32 enable, condition{};
        } button_irq{};
        u32 IE, IF, IME{};
        u32 halted{};

        // Unsupported-yet stub stuff

        struct {
            u32 general_purpose_data{};
            u32 control{};
            u32 send{};
            u32 multi[4]{};
        } SIO{};

        u8 POSTFLG{};

        i32 bios_open_bus{};
        u32 dma_open_bus{};
    } io{};

    struct {
        struct cvec* IOs{};
        u32 described_inputs{};
        i64 cycles_left{};
    } jsm{};

#ifdef GBA_STATS
    struct {
        u64 arm_cycles{};
        u64 timer0_cycles{};
    } timing{};
#endif

    i32 scanline_cycles_to_execute{};

    DMA dma;


    struct {
        double master_cycles_per_audio_sample, master_cycles_per_min_sample, master_cycles_per_max_sample{};
        double next_sample_cycle_max, next_sample_cycle_min, next_sample_cycle{};
        audiobuf *buf{};
        debug_waveform *waveforms[6]{};
        debug_waveform *main_waveform{};
    } audio{};

    TIMER timer[4];

    struct {
        struct GBA_DBG_line {
            u8 window_coverage[240]{}; // 240 4-bit values, bit 1 = on, bit 0 = off
            struct GBA_DBG_line_bg {
                u32 hscroll, vscroll{};
                i32 hpos, vpos{};
                i32 x_lerp, y_lerp{};
                i32 pa, pb, pc, pd{};
                u32 reset_x, reset_y{};
                u32 htiles, vtiles{};
                u32 display_overflow{};
                u32 screen_base_block, character_base_block{};
                u32 priority{};
                 bool bpp8{};
            } bg[4]{};
            u32 bg_mode{};
        } line[160]{};
        struct GBA_DBG_tilemap_line_bg {
            u8 lines[1024 * 128]{}; // 4, 128*8 1-bit "was rendered or not" values
        } bg_scrolls[4]{};
        struct {
            u32 enable{};
            char str[257]{};
        } mgba{};
    } dbg_info{};

    DBG_START
        DBG_EVENT_VIEW

        DBG_CPU_REG_START1
            *R[16]
        DBG_CPU_REG_END1

        DBG_IMAGE_VIEWS_START
            MDBG_IMAGE_VIEW(window0)
            MDBG_IMAGE_VIEW(window1)
            MDBG_IMAGE_VIEW(window2)
            MDBG_IMAGE_VIEW(window3)
            MDBG_IMAGE_VIEW(bg0)
            MDBG_IMAGE_VIEW(bg1)
            MDBG_IMAGE_VIEW(bg2)
            MDBG_IMAGE_VIEW(bg3)
            MDBG_IMAGE_VIEW(bg0map)
            MDBG_IMAGE_VIEW(bg1map)
            MDBG_IMAGE_VIEW(bg2map)
            MDBG_IMAGE_VIEW(bg3map)
            MDBG_IMAGE_VIEW(sprites)
            MDBG_IMAGE_VIEW(palettes)
            MDBG_IMAGE_VIEW(tiles)
            MDBG_IMAGE_VIEW(sys_info)
        DBG_IMAGE_VIEWS_END
        DBG_WAVEFORM_START1
            DBG_WAVEFORM_MAIN
            DBG_WAVEFORM_CHANS(6)
        DBG_WAVEFORM_END1
        DBG_LOG_VIEW
    DBG_END

};

}

