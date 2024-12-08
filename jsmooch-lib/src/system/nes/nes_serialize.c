//
// Created by . on 12/7/24.
//

#include <stdlib.h>
#include "nes.h"
#include "nes_serialize.h"
#include "helpers/sys_present.h"
#include "helpers/debug.h"
#include "helpers/serialize/serialize.h"
#include "component/audio/nes_apu/nes_apu.h"
#include "component/cpu/m6502/m6502.h"

static void serialize_console(struct NES *this, struct serialized_state *state) {
    serialized_state_new_section(state, "console", NESSS_console, 1);
#define S(x) Sadd(state, &(this-> x), sizeof(this-> x))
    S(cycles_left);
    S(display_enabled);
    S(dbg_data);
#undef S
}

static void serialize_clock(struct NES *this, struct serialized_state *state) {
    serialized_state_new_section(state, "clock", NESSS_clock, 1);
#define S(x) Sadd(state, &(this->clock. x), sizeof(this->clock. x))
    S(master_clock);
    S(master_frame);
    S(frames_since_restart);
    S(cpu_master_clock);
    S(apu_master_clock);
    S(ppu_master_clock);
    S(trace_cycles);
    S(nmi);
    S(cpu_frame_cycle);
    S(ppu_frame_cycle);

    S(timing);
    S(ppu_y);
    S(frame_odd);
#undef S
}

static void serialize_apu(struct NES *this, struct serialized_state *state) {
    serialized_state_new_section(state, "APU", NESSS_apu, 1);
    NES_APU_serialize(&this->apu, state);
}

static void serialize_cpu(struct NES *this, struct serialized_state *state) {
    serialized_state_new_section(state, "CPU", NESSS_cpu, 1);
#define S(x) Sadd(state, &(this->cpu. x), sizeof(this->cpu. x))
    S(open_bus);
    S(irq);
    S(io);
    M6502_serialize(&this->cpu.cpu, state);
#undef S
}

static void serialize_ppu(struct NES *this, struct serialized_state *state) {
    serialized_state_new_section(state, "PPU", NESSS_ppu, 1);
#define S(x) Sadd(state, &(this->ppu. x), sizeof(this->ppu. x))

    // render_cycle...//visible, postrender, prerender
    u32 v = 0;
    if (this->ppu.render_cycle == &PPU_scanline_visible) v = 1;
    if (this->ppu.render_cycle == &PPU_scanline_postrender) v = 2;
    Sadd(state, &v, sizeof(v));


    S(line_cycle);
    S(OAM);
    S(secondary_OAM);
    S(secondary_OAM_index);
    S(secondary_OAM_sprite_index);
    S(secondary_OAM_sprite_total);
    S(secondary_OAM_lock);
    S(OAM_transfer_latch);
    S(OAM_eval_index);
    S(OAM_eval_done);
    S(sprite0_on_next_line);
    S(sprite0_on_this_line);
    S(w2006_buffer);
    S(CGRAM);
    S(bg_fetches[0]);
    S(bg_fetches[1]);
    S(bg_fetches[2]);
    S(bg_fetches[3]);
    S(bg_fetches[4]);
    S(bg_shifter);
    S(bg_palette_shifter);
    S(bg_tile_fetch_addr);
    S(bg_tile_fetch_buffer);
    for (u32 i = 0; i < 8; i++) {
        S(sprite_pattern_shifters[i]);
        S(sprite_attribute_latches[i]);
        S(sprite_x_counters[i]);
        S(sprite_y_lines[i]);
    }
    S(last_sprite_addr);
    S(io);
    S(dbg.v);
    S(dbg.t);
    S(dbg.x);
    S(dbg.w);
    S(status);
    S(latch);
    S(rendering_enabled);
    S(new_rendering_enabled);
#undef S
}

static void serialize_cart(struct NES *this, struct serialized_state *state) {
    serialized_state_new_section(state, "cart", NESSS_cartridge, 1);
#define S(x) Sadd(state, &(this->bus. x), sizeof(this->bus. x))
    S(ppu_mirror_mode);
    //void NES_bus_PPU_mirror_set(struct NES_bus *bus)

    Sadd(state, this->bus.CIRAM.ptr, this->bus.CIRAM.sz);
    Sadd(state, this->bus.CPU_RAM.ptr, this->bus.CPU_RAM.sz);
    u32 sz = this->bus.SRAM->actual_size;
    Sadd(state, &sz, sizeof(sz));
    if (sz > 0) {
        Sadd(state, this->bus.SRAM->data, this->bus.SRAM->actual_size);
    }
    Sadd(state, this->bus.CHR_RAM.ptr, this->bus.CHR_RAM.sz);
    this->bus.serialize(&this->bus, state);
#undef S
}

void NESJ_save_state(struct jsm_system *jsm, struct serialized_state *state) {
    struct NES *this = (struct NES *) jsm->ptr;
    // Basic info
    state->version = 1;
    state->opt.len = 0;

    // Make screenshot
    state->has_screenshot = 1;
    jsimg_allocate(&state->screenshot, 256, 240);
    jsimg_clear(&state->screenshot);
    NES_present(cpg(this->ppu.display_ptr), state->screenshot.data.ptr, 0, 0, 256, 240);

    serialize_console(this, state);
    serialize_clock(this, state);
    serialize_apu(this, state);
    serialize_cpu(this, state);
    serialize_ppu(this, state);
    serialize_cart(this, state);
}