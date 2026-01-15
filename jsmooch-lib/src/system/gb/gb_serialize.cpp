//
// Created by . on 12/7/24.
//

#include <cstdlib>
#include <cassert>
#include "gb_serialize.h"
#include "gb.h"
#include "gb_bus.h"
#include "helpers/sys_present.h"
#include "helpers/debug.h"
#include "helpers/serialize/serialize.h"
#include "component/cpu/sm83/sm83.h"
#include "mappers/mapper.h"

namespace GB {
void core::serialize_console(serialized_state &state) {
    state.new_section("console", SS_console, 1);
#define S(x) Sadd(state, &x, sizeof(x))
    S(generic_mapper.WRAM);
    S(generic_mapper.HRAM);
    S(generic_mapper.VRAM);
    S(generic_mapper.VRAM_bank_offset);
    S(generic_mapper.WRAM_bank_offset);
    S(VRAM_bank);
    S(WRAM_bank);
#undef S
#define S(x) Sadd(state, &( x), sizeof( x))
    S(dbg_data);
#undef S
}

void core::serialize_cartridge(serialized_state &state) {
    state.new_section("cartridge", SS_cartridge, 1);

    // First, serialize SRAM
    u32 sz = cart.SRAM->actual_size;
    Sadd(state, &sz, sizeof(sz));
    if (sz > 0) {
        Sadd(state, cart.SRAM->data, cart.SRAM->actual_size);
    }

    // Then, serialize mapper...
    cart.mapper->serialize(cart.mapper, state);
}


void core::serialize_clock(serialized_state &state) {
    state.new_section("clock", SS_clock, 1);
#define S(x) Sadd(state, &(clock. x), sizeof(clock. x))
    S(frames_since_restart);
    S(master_frame);
    S(cycles_left_this_frame);
    S(trace_cycles);
    S(master_clock);
    S(ppu_master_clock);
    S(cpu_master_clock);
    S(cgb_enable);
    S(turbo);
    S(ly);
    S(lx);
    S(wly);
    S(cpu_frame_cycle);
    S(CPU_can_VRAM);
    S(old_OAM_can);
    S(CPU_can_OAM);
    S(bootROM_enabled);
    S(SYSCLK);
    S(timing.ppu_divisor);
    S(timing.cpu_divisor);
    S(timing.apu_divisor);
#undef S
}

void core::serialize_cpu(serialized_state &state)
{
    state.new_section("sm83", SS_sm83, 1);
    cpu.cpu.serialize(state);
#define S(x) Sadd(state, &(cpu. x), sizeof(cpu. x))
    S(FFregs);
    S(io.JOYP.action_select);
    S(io.JOYP.direction_select);
    S(io.speed_switch_prepare);
    S(io.speed_switch_cnt);
    S(dma);
    S(hdma);
#undef S
#define S(x) Sadd(state, &(cpu.timer. x), sizeof(cpu.timer. x))
    S(TIMA);
    S(TMA);
    S(TAC);
    S(last_bit);
    S(TIMA_reload_cycle);
    S(SYSCLK);
    S(cycles_til_TIMA_IRQ);
#undef S
}

void core::serialize_sp_obj_pointer(GB::PPU::sprite *fo, serialized_state &state)
{
    u32 v = 80;
    if (fo == nullptr)
        v = 100;
    // Can be up to 10 of struct GB_PPU_sprite OBJ[10];
    for (u32 i = 0; i < 10; i++) {
        if (fo == &ppu.OBJ[i]) {
            v = i;
            break;
        }
    }
    assert(v!=80);
    Sadd(state, &v, sizeof(v));
}

void core::deserialize_sp_obj_pointer(GB::PPU::sprite **fo, serialized_state &state)
{
    u32 v;
    Sload(state, &v, sizeof(v));
    if (v == 100) {
        *fo = nullptr;
        return;
    }
    assert(v<10);
    *fo = &ppu.OBJ[v];
}


void GB::PPU::FIFO::serialize(GB::core *gb, serialized_state &state)
{
#define S(x) Sadd(state, &( x), sizeof( x))
    S(variant);
    S(compat_mode);
    S(max);
    S(head);
    S(tail);
    S(num_items);
    for (u32 i = 0; i < 10; i++) {
        S(items[i].pixel);
        S(items[i].palette);
        S(items[i].cgb_priority);
        S(items[i].sprite_priority);
        gb->serialize_sp_obj_pointer(items[i].sprite_obj, state);
    }
#undef S
}

void GB::PPU::FIFO::deserialize(GB::core *gb, serialized_state &state)
{
#define L(x) Sload(state, &( x), sizeof( x))
    L(variant);
    L(compat_mode);
    L(max);
    L(head);
    L(tail);
    L(num_items);
    for (u32 i = 0; i < 10; i++) {
        L(items[i].pixel);
        L(items[i].palette);
        L(items[i].cgb_priority);
        L(items[i].sprite_priority);
        gb->deserialize_sp_obj_pointer(&items[i].sprite_obj, state);
    }
#undef L
}

void core::serialize_apu(serialized_state &state) {
    state.new_section("APU", SS_apu, 1);
#define S(x) Sadd(state, &(apu. x), sizeof(apu. x))
    S(channels);
    S(io);
    S(clocks);
#undef S
}

void core::serialize_ppu(serialized_state &state) {
    state.new_section("PPU", SS_ppu, 1);
#define S(x) Sadd(state, &(ppu.slice_fetcher. x), sizeof(ppu.slice_fetcher. x))
    S(fetch_cycle);
    S(fetch_addr);

    serialize_sp_obj_pointer(ppu.slice_fetcher.fetch_obj, state);

    S(fetch_bp0);
    S(fetch_bp1);
    S(spfetch_bp0);
    S(spfetch_bp1);
    S(fetch_cgb_attr);

    ppu.slice_fetcher.bg_FIFO.serialize(this, state);
    ppu.slice_fetcher.obj_FIFO.serialize(this, state);

    S(bg_request_x);
    S(sp_request);
    ppu.slice_fetcher.sprites_queue.serialize(this, state);
    S(out_px.had_pixel);
    S(out_px.color);
    S(out_px.bg_or_sp);
    S(out_px.palette);
#undef S

#define S(x) Sadd(state, &(ppu. x), sizeof(ppu. x))
    S(line_cycle);
    S(cycles_til_vblank);
    S(enabled);
    S(bg_palette);
    S(sp_palette);
    S(bg_CRAM);
    S(obj_CRAM);
    S(OAM);
    S(OBJ);
    S(io);
    S(sprites);
    S(is_window_line);
    S(window_triggered_on_line);
#undef S
}

void core::save_state(serialized_state &state)
{
    // Basic info
    state.version = 1;
    state.opt.len = 0;

    // Make screenshot
    state.has_screenshot = 1;
    state.screenshot.allocate(160, 144);
    state.screenshot.clear();
    switch(variant) {
        case DMG:
            DMG_present(ppu.display_ptr.get(), state.screenshot.data.ptr, 0, 0, 160, 144, false);
            break;
        case GBC:
            GBC_present(ppu.display_ptr.get(), state.screenshot.data.ptr, 0, 0, 160, 144, false);
            break;
        case SGB:
            assert(1==2);
            break;
    }

    serialize_console(state);
    serialize_clock(state);
    serialize_cpu(state);
    serialize_ppu(state);
    serialize_apu(state);
    serialize_cartridge(state);
}


void core::deserialize_console(serialized_state &state) {
#define L(x) Sload(state, &x, sizeof(x))
    L(generic_mapper.WRAM);
    L(generic_mapper.HRAM);
    L(generic_mapper.VRAM);
    L(generic_mapper.VRAM_bank_offset);
    L(generic_mapper.WRAM_bank_offset);
    L(VRAM_bank);
    L(WRAM_bank);
#undef L
#define L(x) Sload(state, & x, sizeof( x))
    L(dbg_data);
#undef L
}

void core::deserialize_ppu(serialized_state &state) {
#define L(x) Sload(state, &ppu.slice_fetcher. x, sizeof(ppu.slice_fetcher. x))

    L(fetch_cycle);
    L(fetch_addr);

    deserialize_sp_obj_pointer(&ppu.slice_fetcher.fetch_obj, state);

    L(fetch_bp0);
    L(fetch_bp1);
    L(spfetch_bp0);
    L(spfetch_bp1);
    L(fetch_cgb_attr);

    ppu.slice_fetcher.bg_FIFO.deserialize(this, state);
    ppu.slice_fetcher.obj_FIFO.deserialize(this, state);

    L(bg_request_x);
    L(sp_request);
    ppu.slice_fetcher.sprites_queue.deserialize(this, state);
    L(out_px.had_pixel);
    L(out_px.color);
    L(out_px.bg_or_sp);
    L(out_px.palette);
#undef L


#define L(x) Sload(state, &ppu. x, sizeof(ppu. x))
    L(line_cycle);
    L(cycles_til_vblank);
    L(enabled);
    L(bg_palette);
    L(sp_palette);
    L(bg_CRAM);
    L(obj_CRAM);
    L(OAM);
    L(OBJ);
    L(io);
    L(sprites);
    L(is_window_line);
    L(window_triggered_on_line);
#undef L
}

void core::deserialize_apu(serialized_state &state) {
#define L(x) Sload(state, &apu. x, sizeof(apu. x))
    L(channels);
    L(io);
    L(clocks);
#undef L
}

void core::deserialize_cartridge(serialized_state &state) {
    u32 sz;
    Sload(state, &sz, sizeof(sz));
    if (sz > 0) {
        Sload(state, cart.SRAM->data, sz);
        cart.SRAM->dirty = true;
    }

    cart.mapper->deserialize(cart.mapper, state);
}


void core::deserialize_clock(serialized_state &state) {
#define L(x) Sload(state, &clock. x, sizeof(clock. x))
    L(frames_since_restart);
    L(master_frame);
    L(cycles_left_this_frame);
    L(trace_cycles);
    L(master_clock);
    L(ppu_master_clock);
    L(cpu_master_clock);
    L(cgb_enable);
    L(turbo);
    L(ly);
    L(lx);
    L(wly);
    L(cpu_frame_cycle);
    L(CPU_can_VRAM);
    L(old_OAM_can);
    L(CPU_can_OAM);
    L(bootROM_enabled);
    L(SYSCLK);
    L(timing.ppu_divisor);
    L(timing.cpu_divisor);
    L(timing.apu_divisor);
#undef L
}

void core::deserialize_cpu(serialized_state &state) {
    cpu.cpu.deserialize(state);
#define L(x) Sload(state, &cpu. x, sizeof(cpu. x))
    L(FFregs);
    L(io.JOYP.action_select);
    L(io.JOYP.direction_select);
    L(io.speed_switch_prepare);
    L(io.speed_switch_cnt);
    L(dma);
    L(hdma);
#undef L

#define L(x) Sload(state, &cpu.timer. x, sizeof(cpu.timer. x))
    L(TIMA);
    L(TMA);
    L(TAC);
    L(last_bit);
    L(TIMA_reload_cycle);
    L(SYSCLK);
    L(cycles_til_TIMA_IRQ);
#undef L
}

void core::load_state(serialized_state &state, deserialize_ret &ret) {
    state.iter.offset = 0;
    assert(state.version == 1);

    for (u32 i = 0; i < state.sections.size(); i++) {
        serialized_state_section &sec = state.sections.at(i);
        state.iter.offset = sec.offset;
        switch (sec.kind) {
            case SS_console:
                deserialize_console(state);
                break;
            case SS_ppu:
                deserialize_ppu(state);
                break;
            case SS_apu:
                deserialize_apu(state);
                break;
            case SS_clock:
                deserialize_clock(state);
                break;
            case SS_sm83:
                deserialize_cpu(state);
                break;
            case SS_cartridge:
                deserialize_cartridge(state);
                break;
            default: NOGOHERE;
        }
    }
}

}