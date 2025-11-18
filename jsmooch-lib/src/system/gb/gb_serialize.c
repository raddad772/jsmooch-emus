//
// Created by . on 12/7/24.
//

#include <stdlib.h>
#include "gb_serialize.h"
#include "gb.h"
#include "gb_bus.h"
#include "helpers/sys_present.h"
#include "helpers/debug.h"
#include "helpers/serialize/serialize.h"
#include "component/cpu/sm83/sm83.h"
#include "mappers/mapper.h"

static void serialize_console(struct GB *this, struct serialized_state *state) {
    serialized_state_new_section(state, "console", GBSS_console, 1);
#define S(x) Sadd(state, &(this->bus. x), sizeof(this->bus. x))
    S(generic_mapper.WRAM);
    S(generic_mapper.HRAM);
    S(generic_mapper.VRAM);
    S(generic_mapper.VRAM_bank_offset);
    S(generic_mapper.WRAM_bank_offset);
    S(VRAM_bank);
    S(WRAM_bank);
#undef S
#define S(x) Sadd(state, &(this-> x), sizeof(this-> x))
    S(dbg_data);
#undef S
}

static void serialize_cartridge(struct GB *this, struct serialized_state *state) {
    serialized_state_new_section(state, "cartridge", GBSS_cartridge, 1);

    // First, serialize SRAM
    u32 sz = this->cart.SRAM->actual_size;
    Sadd(state, &sz, sizeof(sz));
    if (sz > 0) {
        Sadd(state, this->cart.SRAM->data, this->cart.SRAM->actual_size);
    }

    // Then, serialize mapper...
    this->cart.mapper->serialize(this->cart.mapper, state);
}


static void serialize_clock(struct GB *this, struct serialized_state *state) {
    serialized_state_new_section(state, "clock", GBSS_clock, 1);
#define S(x) Sadd(state, &(this->clock. x), sizeof(this->clock. x))
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

static void serialize_cpu(struct GB* this, struct serialized_state *state)
{
    serialized_state_new_section(state, "sm83", GBSS_sm83, 1);
    SM83_serialize(&this->cpu.cpu, state);
#define S(x) Sadd(state, &(this->cpu. x), sizeof(this->cpu. x))
    S(FFregs);
    S(io.JOYP.action_select);
    S(io.JOYP.direction_select);
    S(io.speed_switch_prepare);
    S(io.speed_switch_cnt);
    S(dma);
    S(hdma);
#undef S
#define S(x) Sadd(state, &(this->cpu.timer. x), sizeof(this->cpu.timer. x))
    S(TIMA);
    S(TMA);
    S(TAC);
    S(last_bit);
    S(TIMA_reload_cycle);
    S(SYSCLK);
    S(cycles_til_TIMA_IRQ);
#undef S
}

static void serialize_sp_obj_pointer(struct GB_PPU_sprite *fo, struct GB* this, struct serialized_state *state)
{
    u32 v = 80;
    if (fo == NULL)
        v = 100;
    // Can be up to 10 of struct GB_PPU_sprite OBJ[10];
    for (u32 i = 0; i < 10; i++) {
        if (fo == &this->ppu.OBJ[i]) {
            v = i;
            break;
        }
    }
    assert(v!=80);
    Sadd(state, &v, sizeof(v));
}

static void deserialize_sp_obj_pointer(struct GB_PPU_sprite **fo, struct GB* this, struct serialized_state *state)
{
    u32 v;
    Sload(state, &v, sizeof(v));
    if (v == 100) {
        *fo = NULL;
        return;
    }
    assert(v<10);
    *fo = &this->ppu.OBJ[v];
}


static void serialize_fifo(struct GB_FIFO *this, struct GB *gb, struct serialized_state *state)
{
#define S(x) Sadd(state, &(this-> x), sizeof(this-> x))
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
        serialize_sp_obj_pointer(this->items[i].sprite_obj, gb, state);
    }
#undef S
}

static void deserialize_fifo(struct GB_FIFO *this, struct GB *gb, struct serialized_state *state)
{
#define L(x) Sload(state, &(this-> x), sizeof(this-> x))
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
        deserialize_sp_obj_pointer(&this->items[i].sprite_obj, gb, state);
    }
#undef L
}

static void serialize_apu(struct GB* this, struct serialized_state *state) {
    serialized_state_new_section(state, "APU", GBSS_apu, 1);
#define S(x) Sadd(state, &(this->apu. x), sizeof(this->apu. x))
    S(channels);
    S(io);
    S(clocks);
#undef S
}

static void serialize_ppu(struct GB* this, struct serialized_state *state) {
    serialized_state_new_section(state, "PPU", GBSS_ppu, 1);
#define S(x) Sadd(state, &(this->ppu.slice_fetcher. x), sizeof(this->ppu.slice_fetcher. x))
    S(fetch_cycle);
    S(fetch_addr);

    serialize_sp_obj_pointer(this->ppu.slice_fetcher.fetch_obj, this, state);

    S(fetch_bp0);
    S(fetch_bp1);
    S(spfetch_bp0);
    S(spfetch_bp1);
    S(fetch_cgb_attr);

    serialize_fifo(&this->ppu.slice_fetcher.bg_FIFO, this, state);
    serialize_fifo(&this->ppu.slice_fetcher.obj_FIFO, this, state);

    S(bg_request_x);
    S(sp_request);
    serialize_fifo(&this->ppu.slice_fetcher.sprites_queue, this, state);
    S(out_px.had_pixel);
    S(out_px.color);
    S(out_px.bg_or_sp);
    S(out_px.palette);
#undef S

#define S(x) Sadd(state, &(this->ppu. x), sizeof(this->ppu. x))
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

void GBJ_save_state(struct jsm_system *jsm, struct serialized_state *state)
{
    struct GB* this = (struct GB*)jsm->ptr;
    // Basic info
    state->version = 1;
    state->opt.len = 0;

    // Make screenshot
    state->has_screenshot = 1;
    jsimg_allocate(&state->screenshot, 160, 144);
    jsimg_clear(&state->screenshot);
    switch(this->variant) {
        case DMG:
            DMG_present(cpg(this->ppu.display_ptr), state->screenshot.data.ptr, 0, 0, 160, 144, 0);
            break;
        case GBC:
            GBC_present(cpg(this->ppu.display_ptr), state->screenshot.data.ptr, 0, 0, 160, 144, 0);
            break;
        case SGB:
            assert(1==2);
            break;
    }

    serialize_console(this, state);
    serialize_clock(this, state);
    serialize_cpu(this, state);
    serialize_ppu(this, state);
    serialize_apu(this, state);
    serialize_cartridge(this, state);
}


static void deserialize_console(struct GB* this, struct serialized_state *state) {
#define L(x) Sload(state, &this->bus. x, sizeof(this->bus. x))
    L(generic_mapper.WRAM);
    L(generic_mapper.HRAM);
    L(generic_mapper.VRAM);
    L(generic_mapper.VRAM_bank_offset);
    L(generic_mapper.WRAM_bank_offset);
    L(VRAM_bank);
    L(WRAM_bank);
#undef L
#define L(x) Sload(state, &this-> x, sizeof(this-> x))
    L(dbg_data);
#undef L
}

static void deserialize_ppu(struct GB* this, struct serialized_state *state) {
#define L(x) Sload(state, &this->ppu.slice_fetcher. x, sizeof(this->ppu.slice_fetcher. x))

    L(fetch_cycle);
    L(fetch_addr);

    deserialize_sp_obj_pointer(&this->ppu.slice_fetcher.fetch_obj, this, state);

    L(fetch_bp0);
    L(fetch_bp1);
    L(spfetch_bp0);
    L(spfetch_bp1);
    L(fetch_cgb_attr);

    deserialize_fifo(&this->ppu.slice_fetcher.bg_FIFO, this, state);
    deserialize_fifo(&this->ppu.slice_fetcher.obj_FIFO, this, state);

    L(bg_request_x);
    L(sp_request);
    deserialize_fifo(&this->ppu.slice_fetcher.sprites_queue, this, state);
    L(out_px.had_pixel);
    L(out_px.color);
    L(out_px.bg_or_sp);
    L(out_px.palette);
#undef L


#define L(x) Sload(state, &this->ppu. x, sizeof(this->ppu. x))
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

static void deserialize_apu(struct GB* this, struct serialized_state *state) {
#define L(x) Sload(state, &this->apu. x, sizeof(this->apu. x))
    L(channels);
    L(io);
    L(clocks);
#undef L
}

static void deserialize_cartridge(struct GB* this, struct serialized_state *state) {
    u32 sz;
    Sload(state, &sz, sizeof(sz));
    if (sz > 0) {
        Sload(state, this->cart.SRAM->data, sz);
        this->cart.SRAM->dirty = 1;
    }

    this->cart.mapper->deserialize(this->cart.mapper, state);
}


static void deserialize_clock(struct GB* this, struct serialized_state *state) {
#define L(x) Sload(state, &this->clock. x, sizeof(this->clock. x))
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

static void deserialize_cpu(struct GB* this, struct serialized_state *state) {
    SM83_deserialize(&this->cpu.cpu, state);
#define L(x) Sload(state, &this->cpu. x, sizeof(this->cpu. x))
    L(FFregs);
    L(io.JOYP.action_select);
    L(io.JOYP.direction_select);
    L(io.speed_switch_prepare);
    L(io.speed_switch_cnt);
    L(dma);
    L(hdma);
#undef L

#define L(x) Sload(state, &this->cpu.timer. x, sizeof(this->cpu.timer. x))
    L(TIMA);
    L(TMA);
    L(TAC);
    L(last_bit);
    L(TIMA_reload_cycle);
    L(SYSCLK);
    L(cycles_til_TIMA_IRQ);
#undef L
}

void GBJ_load_state(struct jsm_system *jsm, struct serialized_state *state, struct deserialize_ret *ret) {
    state->iter.offset = 0;
    assert(state->version == 1);

    struct GB *this = (struct GB *) jsm->ptr;

    for (u32 i = 0; i < cvec_len(&state->sections); i++) {
        struct serialized_state_section *sec = cvec_get(&state->sections, i);
        state->iter.offset = sec->offset;
        switch (sec->kind) {
            case GBSS_console:
                deserialize_console(this, state);
                break;
            case GBSS_ppu:
                deserialize_ppu(this, state);
                break;
            case GBSS_apu:
                deserialize_apu(this, state);
                break;
            case GBSS_clock:
                deserialize_clock(this, state);
                break;
            case GBSS_sm83:
                deserialize_cpu(this, state);
                break;
            case GBSS_cartridge:
                deserialize_cartridge(this, state);
                break;
            default: NOGOHERE;
        }
    }
}

