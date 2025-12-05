#pragma once

#include "helpers/int.h"
#include "helpers/physical_io.h"

namespace mac {
struct display {
    explicit display(struct core* parent);
    static void scanline_visible(core *bus);
    static void scanline_vblank(core* th);
    void update_irqs();
    void step2();
    void reset();

    u32 pwm_base_addr;

    core *bus;
    bool in_hblank();
    void new_frame();
    void do_sound_and_pwm();
    JSM_DISPLAY* crt;
    cvec_ptr<physical_io_device> crt_ptr{};
    u8 *cur_output{};

    u32 ram_base_address{};
    u32 read_addr{};
    u16 output_shifter{};

    u32 IRQ_signal{};
    u32 IRQ_out{};

    void (*scanline_func)(core*);

private:
    void calc_display_addr();
    void new_scanline();
};
}
