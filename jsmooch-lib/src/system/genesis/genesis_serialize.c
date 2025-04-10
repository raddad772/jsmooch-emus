//
// Created by . on 11/29/24.
//

#include <stdlib.h>
#include "genesis_serialize.h"
#include "genesis_bus.h"
#include "helpers/sys_present.h"
#include "helpers/debug.h"
#include "helpers/serialize/serialize.h"

static void serialize_console(struct genesis *this, struct serialized_state *state)
{
    serialized_state_new_section(state, "console", genesisSS_console, 1);
#define S(x) Sadd(state, &(this-> x), sizeof(this-> x))
    S(RAM);
    S(ARAM);
    S(io.z80.reset_line);
    S(io.z80.reset_line_count);
    S(io.z80.bank_address_register);
    S(io.z80.bus_request);
    S(io.z80.bus_ack);

    S(io.m68k.open_bus_data);
    S(io.m68k.VDP_FIFO_stall);
    S(io.m68k.VDP_prefetch_stall);
    S(io.m68k.stuck);

    S(io.SRAM_enabled);

    S(opts.vdp.enable_A);
    S(opts.vdp.enable_B);
    S(opts.vdp.enable_sprites);
    S(opts.vdp.ex_trace);
    S(opts.JP);
#undef S
}

static void serialize_z80(struct genesis *this, struct serialized_state *state)
{
    serialized_state_new_section(state, "z80", genesisSS_z80, 1);
    Z80_serialize(&this->z80, state);
}

static void serialize_m68k(struct genesis *this, struct serialized_state *state)
{
    serialized_state_new_section(state, "m68k", genesisSS_m68k, 1);
    M68k_serialize(&this->m68k, state);
}

static void serialize_debug(struct genesis *this, struct serialized_state *state)
{
    serialized_state_new_section(state, "debug", genesisSS_debug, 1);
}

static void serialize_clock(struct genesis *this, struct serialized_state *state) {
    serialized_state_new_section(state, "clock", genesisSS_clock, 1);
#define S(x) Sadd(state, &this->clock. x, sizeof(this->clock. x))
    S(master_cycle_count);
    S(master_frame);
    S(frames_since_reset);

    S(kind);

    S(mem_break);

    S(vdp.hcount);
    S(vdp.vcount);
    S(vdp.hblank_active);
    S(vdp.vblank_active);

    S(vdp.hblank_fast);
    S(vdp.field);
    S(vdp.clock_divisor);
    S(vdp.cycles_til_clock);
    S(vdp.bottom_rendered_line);

    S(psg.cycles_til_clock);
    S(psg.clock_divisor);

    S(m68k.clock_divisor);
    S(m68k.cycles_til_clock);

    S(z80.clock_divisor);
    S(z80.cycles_til_clock);

    S(timing.cycles_per_second);
    S(timing.cycles_per_frame);
    S(timing.fps);
#undef S
}

static void serialize_vdp(struct genesis *this, struct serialized_state *state) {
    serialized_state_new_section(state, "vdp", genesisSS_vdp, 1);
#define S(x) Sadd(state, &this->vdp. x, sizeof(this->vdp. x))
    S(sprite_line_buf);
    S(cur_pixel);
    S(io);
    S(irq);
    S(command);
    S(dma);
    S(sprite_collision);
    S(sprite_overflow);
    S(cycle);
    S(sc_count);
    S(sc_slot);
    S(sc_array);
    S(sc_skip);

    S(CRAM);
    S(VSRAM);
    S(VRAM);
    S(fifo);
    S(line);
    S(term_out);
    u64 r = this->vdp.term_out_ptr - this->vdp.term_out;
    u32 aa = (u32)r;
    Sadd(state, &aa, sizeof(aa));
    S(fetcher);
    S(ringbuf);
    S(debug);
    S(latch);
    S(window);
#undef S
}

static void serialize_cartridge(struct genesis *this, struct serialized_state *state) {
    serialized_state_new_section(state, "cartridge", genesisSS_cartridge, 1);
#define S(x) Sadd(state, &this->cart. x, sizeof(this->cart. x))
    //S(ROM.size);
    //Sadd(state, &this->cart.ROM.ptr, this->cart.ROM.size);
    S(RAM_persists);
    S(RAM_mask);
    S(RAM_present);
    S(RAM_always_on);
    S(RAM_size);
    S(header);
    S(kind);
    S(SRAM->requested_size);
    if (this->cart.SRAM->requested_size > 0) {
        Sadd(state, this->cart.SRAM->data, this->cart.SRAM->requested_size);
    }
#undef S
}

static void serialize_ym2612(struct genesis *this, struct serialized_state *state)
{
    serialized_state_new_section(state, "ym2612", genesisSS_ym2612, 1);
    ym2612_serialize(&this->ym2612, state);
}

static void serialize_sn76489(struct genesis *this, struct serialized_state *state)
{
    serialized_state_new_section(state, "sn76489", genesisSS_sn76489, 1);
    SN76489_serialize(&this->psg, state);
}


void genesisJ_save_state(struct jsm_system *jsm, struct serialized_state *state)
{
    struct genesis* this = (struct genesis*)jsm->ptr;
    // Basic info
    state->version = 1;
    state->opt.len = 0;

    // Make screenshot
    state->has_screenshot = 1;
    jsimg_allocate(&state->screenshot, 1280, 240);
    jsimg_clear(&state->screenshot);
    genesis_present(cpg(this->vdp.display_ptr), state->screenshot.data.ptr, 320, 240, 1);

    serialize_console(this, state);
    serialize_debug(this, state);
    serialize_clock(this, state);
    serialize_cartridge(this, state);
    serialize_m68k(this, state);
    serialize_z80(this, state);
    serialize_vdp(this, state);
    serialize_sn76489(this, state);
    serialize_ym2612(this, state);
}

static void deserialize_z80(struct genesis* this, struct serialized_state *state)
{
    Z80_deserialize(&this->z80, state);
}

static void deserialize_m68k(struct genesis* this, struct serialized_state *state)
{
    M68k_deserialize(&this->m68k, state);
}

static void deserialize_ym2612(struct genesis* this, struct serialized_state *state)
{
    ym2612_deserialize(&this->ym2612, state);
}

static void deserialize_sn76489(struct genesis* this, struct serialized_state *state)
{
    SN76489_deserialize(&this->psg, state);
}

static void deserialize_console(struct genesis* this, struct serialized_state *state) {
#define L(x) Sload(state, &this-> x, sizeof(this-> x))
    L(RAM);
    L(ARAM);
    L(io.z80.reset_line);
    L(io.z80.reset_line_count);
    L(io.z80.bank_address_register);
    L(io.z80.bus_request);
    L(io.z80.bus_ack);

    L(io.m68k.open_bus_data);
    L(io.m68k.VDP_FIFO_stall);
    L(io.m68k.VDP_prefetch_stall);
    L(io.m68k.stuck);

    L(io.SRAM_enabled);

    L(opts.vdp.enable_A);
    L(opts.vdp.enable_B);
    L(opts.vdp.enable_sprites);
    L(opts.vdp.ex_trace);
    L(opts.JP);
#undef L
}

static void deserialize_debug(struct genesis* this, struct serialized_state *state) {
#define L(x) Sload(state, &this->cart. x, sizeof(this->cart. x))
#undef L
}

static void deserialize_cartridge(struct genesis* this, struct serialized_state *state) {
#define L(x) Sload(state, &this->cart. x, sizeof(this->cart. x))
    //u64 sz;
    //Sload(state, &sz, sizeof(sz));
    //buf_allocate(&this->cart.ROM, sz);
    //Sload(state, &this->cart.ROM.ptr, sz);
    L(RAM_persists);
    L(RAM_mask);
    L(RAM_present);
    L(RAM_always_on);
    L(RAM_size);
    L(header);
    L(kind);
    L(SRAM->requested_size);
    if (this->cart.SRAM->requested_size > 0) {
        Sload(state, this->cart.SRAM->data, this->cart.SRAM->requested_size);
        this->cart.SRAM->dirty = 1;
    }
#undef L
}

static void deserialize_vdp(struct genesis* this, struct serialized_state *state) {
#define L(x) Sload(state, &this->vdp. x, sizeof(this->vdp. x))
    L(sprite_line_buf);
    L(cur_pixel);
    L(io);
    L(irq);
    L(command);
    L(dma);
    L(sprite_collision);
    L(sprite_overflow);
    L(cycle);
    L(sc_count);
    L(sc_slot);
    L(sc_array);
    L(sc_skip);

    L(CRAM);
    L(VSRAM);
    L(VRAM);
    L(fifo);
    L(line);
    L(term_out);
    u32 pp;
    Sload(state, &pp, sizeof(pp));
    assert(pp<sizeof(this->vdp.term_out));
    this->vdp.term_out_ptr = this->vdp.term_out + pp;

    L(fetcher);
    L(ringbuf);
    L(debug);
    L(latch);
    L(window);
#undef L
}

static void deserialize_clock(struct genesis* this, struct serialized_state *state) {
#define L(x) Sload(state, &this->clock. x, sizeof(this->clock. x))
    L(master_cycle_count);
    L(master_frame);
    L(frames_since_reset);

    L(kind);

    L(mem_break);

    L(vdp.hcount);
    L(vdp.vcount);
    L(vdp.hblank_active);
    L(vdp.vblank_active);

    L(vdp.hblank_fast);
    L(vdp.field);
    L(vdp.clock_divisor);
    L(vdp.cycles_til_clock);
    L(vdp.bottom_rendered_line);

    L(psg.cycles_til_clock);
    L(psg.clock_divisor);

    L(m68k.clock_divisor);
    L(m68k.cycles_til_clock);

    L(z80.clock_divisor);
    L(z80.cycles_til_clock);

    L(timing.cycles_per_second);
    L(timing.cycles_per_frame);
    L(timing.fps);
#undef L
}

void genesisJ_load_state(struct jsm_system *jsm, struct serialized_state *state, struct deserialize_ret *ret) {
    state->iter.offset = 0;
    assert(state->version == 1);

    struct genesis *this = (struct genesis *) jsm->ptr;

    for (u32 i = 0; i < cvec_len(&state->sections); i++) {
        struct serialized_state_section *sec = cvec_get(&state->sections, i);
        state->iter.offset = sec->offset;
        switch (sec->kind) {
            case genesisSS_console:
                deserialize_console(this, state);
                break;
            case genesisSS_debug:
                deserialize_debug(this, state);
                break;
            case genesisSS_vdp:
                deserialize_vdp(this, state);
                break;
            case genesisSS_ym2612:
                deserialize_ym2612(this, state);
                break;
            case genesisSS_sn76489:
                deserialize_sn76489(this, state);
                break;
            case genesisSS_z80:
                deserialize_z80(this, state);
                break;
            case genesisSS_m68k:
                deserialize_m68k(this, state);
                break;
            case genesisSS_clock:
                deserialize_clock(this, state);
                break;
            case genesisSS_cartridge:
                deserialize_cartridge(this, state);
                break;
            default: NOGOHERE;
        }
    }
}
