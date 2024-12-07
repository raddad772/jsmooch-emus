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

static void serialize_console(struct GB *this, struct serialized_state *state) {
    serialized_state_new_section(state, "console", GBSS_console, 1);
#define S(x) Sadd(state, &(this-> x), sizeof(this-> x))
    S(dbg_data);
#undef S
#define S(x) Sadd(state, &(this->bus. x), sizeof(this->bus. x))
    S(generic_mapper.WRAM);
    S(generic_mapper.HRAM);
    S(generic_mapper.VRAM);
    S(generic_mapper.VRAM_bank_offset);
    S(generic_mapper.WRAM_bank_offset);
    S(generic_mapper.BIOS_big);
    S(VRAM_bank);
    S(WRAM_bank);
#undef S
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

static void serialize_ppu(struct GB* this, struct serialized_state *state) {
    serialized_state_new_section(state, "PPU", GBSS_ppu, 1);
#define S(x) Sadd(state, &(this->ppu.slice_fetcher. x), sizeof(this->ppu.slice_fetcher. x))

#undef S


#define S(x) Sadd(state, &(this->ppu. x), sizeof(this->ppu. x))

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
    //serialize_apu(this, state);
    //serialize_cartridge(this, state);
}

void GBJ_load_state(struct jsm_system *jsm, struct serialized_state *state, struct deserialize_ret *ret) {
    state->iter.offset = 0;
    assert(state->version == 1);

    struct GB *this = (struct GB *) jsm->ptr;

}