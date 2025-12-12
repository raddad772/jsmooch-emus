//
// Created by . on 12/7/24.
//

#include <cstdlib>
#include <cassert>
#include "nes.h"
#include "nes_serialize.h"
#include "helpers/sys_present.h"
#include "helpers/serialize/serialize.h"
#include "component/audio/nes_apu/nes_apu.h"
#include "component/cpu/m6502/m6502.h"

void NES::serialize(serialized_state &state) const {
    state.new_section("console", NESSS_console, 1);
#define S(x) Sadd(state, &(this-> x), sizeof(this-> x))
    S(cycles_left);
    S(display_enabled);
    S(dbg_data);
#undef S
}

void NES_clock::serialize(serialized_state &state) const {
    state.new_section("clock", NESSS_clock, 1);
#define S(x) Sadd(state, &x, sizeof(x))
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

static void serialize_apu(NES &nes, serialized_state &state) {
    state.new_section("APU", NESSS_apu, 1);
    nes.apu.serialize(state);
}

void r2A03::serialize(serialized_state &state) {
    state.new_section("CPU", NESSS_cpu, 1);
#define S(x) Sadd(state, &x, sizeof(x))
    S(open_bus);
    S(irq);
    S(io);
    cpu.serialize(state);
#undef S
}

void NES_PPU::serialize(serialized_state &state) {
    state.new_section("PPU", NESSS_ppu, 1);
#define S(x) Sadd(state, &x, sizeof(x))

    // render_cycle...//visible, postrender, prerender
    u32 v = 0;
    if (render_cycle == &NES_PPU::scanline_visible) v = 1;
    if (render_cycle == &NES_PPU::scanline_postrender) v = 2;
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

void serialize_cart(NES &nes, serialized_state &state) {
    state.new_section("cart", NESSS_cartridge, 1);
#define S(x) Sadd(state, &(nes.bus. x), sizeof(nes.bus. x))
    S(ppu_mirror_mode);
    //void NES_bus_PPU_mirror_set(NES_bus *bus)

    Sadd(state, nes.bus.CIRAM.ptr, nes.bus.CIRAM.sz);
    Sadd(state, nes.bus.CPU_RAM.ptr, nes.bus.CPU_RAM.sz);
    u32 sz = nes.bus.SRAM->actual_size;
    Sadd(state, &sz, sizeof(sz));
    if (sz > 0) {
        Sadd(state, nes.bus.SRAM->data, nes.bus.SRAM->actual_size);
    }
    Sadd(state, nes.bus.CHR_RAM.ptr, nes.bus.CHR_RAM.sz);
    nes.bus.mapper->serialize(state);
#undef S
}

void NESJ::save_state(serialized_state &state) {
    // Basic info
    state.version = 1;
    state.opt.len = 0;

    // Make screenshot
    state.has_screenshot = 1;
    state.screenshot.allocate(256, 240);
    state.screenshot.clear();
    NES_present(nes.ppu.display_ptr.get(), state.screenshot.data.ptr, 0, 0, 256, 240);

    nes.serialize(state);
    nes.clock.serialize(state);
    serialize_apu(nes, state);
    nes.cpu.serialize(state);
    nes.ppu.serialize(state);
    serialize_cart(nes, state);
}

void NES::deserialize(serialized_state &state) {
#define L(x) Sload(state, &this-> x, sizeof(this-> x))
    L(cycles_left);
    L(display_enabled);
    L(dbg_data);
#undef L
}

void NES_clock::deserialize(serialized_state &state) {
#define L(x) Sload(state, &x, sizeof(x))
    L(master_clock);
    L(master_frame);
    L(frames_since_restart);
    L(cpu_master_clock);
    L(apu_master_clock);
    L(ppu_master_clock);
    L(trace_cycles);
    L(nmi);
    L(cpu_frame_cycle);
    L(ppu_frame_cycle);

    L(timing);
    L(ppu_y);
    L(frame_odd);
#undef L
}

void deserialize_apu(NES &nes, serialized_state &state) {
    nes.apu.deserialize(state);
}

void r2A03::deserialize(serialized_state &state) {
#define L(x) Sload(state, &x, sizeof(x))
    L(open_bus);
    L(irq);
    L(io);
    cpu.deserialize(state);
#undef L
}

void NES_PPU::deserialize(serialized_state &state) {
#define L(x) Sload(state, &x, sizeof(x))
    u32 v = 0;
    Sload(state, &v, sizeof(v));
    switch(v) {
        case 0:
            render_cycle = &NES_PPU::scanline_prerender;
            break;
        case 1:
            render_cycle = &NES_PPU::scanline_visible;
            break;
        case 2:
            render_cycle = &NES_PPU::scanline_postrender;
            break;
        default:
            NOGOHERE;
            break;
    }

    L(line_cycle);
    L(OAM);
    L(secondary_OAM);
    L(secondary_OAM_index);
    L(secondary_OAM_sprite_index);
    L(secondary_OAM_sprite_total);
    L(secondary_OAM_lock);
    L(OAM_transfer_latch);
    L(OAM_eval_index);
    L(OAM_eval_done);
    L(sprite0_on_next_line);
    L(sprite0_on_this_line);
    L(w2006_buffer);
    L(CGRAM);
    L(bg_fetches[0]);
    L(bg_fetches[1]);
    L(bg_fetches[2]);
    L(bg_fetches[3]);
    L(bg_fetches[4]);
    L(bg_shifter);
    L(bg_palette_shifter);
    L(bg_tile_fetch_addr);
    L(bg_tile_fetch_buffer);
    for (u32 i = 0; i < 8; i++) {
        L(sprite_pattern_shifters[i]);
        L(sprite_attribute_latches[i]);
        L(sprite_x_counters[i]);
        L(sprite_y_lines[i]);
    }
    L(last_sprite_addr);
    L(io);
    L(dbg.v);
    L(dbg.t);
    L(dbg.x);
    L(dbg.w);
    L(status);
    L(latch);
    L(rendering_enabled);
    L(new_rendering_enabled);
#undef L
}

void deserialize_cart(NES &nes, serialized_state &state) {
#define L(x) Sload(state, &nes.bus. x, sizeof(nes.bus. x))
    L(ppu_mirror_mode);

    Sload(state, nes.bus.CIRAM.ptr, nes.bus.CIRAM.sz);
    Sload(state, nes.bus.CPU_RAM.ptr, nes.bus.CPU_RAM.sz);
    u32 v;
    Sload(state, &v, sizeof(v));
    if (v > 0) {
        Sload(state, nes.bus.SRAM->data, v);
        nes.bus.SRAM->dirty = true;
    }
    Sload(state, nes.bus.CHR_RAM.ptr, nes.bus.CHR_RAM.sz);
    nes.bus.mapper->deserialize(state);
#undef L
}

void NESJ::load_state(serialized_state &state, deserialize_ret &ret) {
    state.iter.offset = 0;
    assert(state.version == 1);

    for (auto & sec : state.sections) {
        state.iter.offset = sec.offset;
        switch (sec.kind) {
            case NESSS_console:
                nes.deserialize(state);
                break;
            case NESSS_clock:
                nes.clock.deserialize(state);
                break;
            case NESSS_apu:
                deserialize_apu(nes, state);
                break;
            case NESSS_cpu:
                nes.cpu.deserialize(state);
                break;
            case NESSS_ppu:
                nes.ppu.deserialize(state);
                break;
            case NESSS_cartridge:
                deserialize_cart(nes, state);
                break;
            default: NOGOHERE;
        }
    }
}
