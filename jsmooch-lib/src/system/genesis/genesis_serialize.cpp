//
// Created by . on 11/29/24.
//

#include <cassert>
#include <cstdlib>
#include "genesis_serialize.h"
#include "genesis_bus.h"
#include "helpers/sys_present.h"
#include "helpers/debug.h"
#include "helpers/serialize/serialize.h"
namespace genesis {

void core::serialize_console(serialized_state &state)
{
    state.new_section("console", SS_console, 1);
#define S(x) Sadd(state, &( x), sizeof( x))
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

    S(v_opts.vdp.enable_A);
    S(v_opts.vdp.enable_B);
    S(v_opts.vdp.enable_sprites);
    S(v_opts.vdp.ex_trace);
    S(v_opts.JP);
#undef S
}

void core::serialize_z80(serialized_state &state)
{
    state.new_section("z80", SS_z80, 1);
    z80.serialize(state);
}

void core::serialize_m68k(serialized_state &state)
{
    state.new_section("m68k", SS_m68k, 1);
    m68k.serialize(state);
}

void core::serialize_debug(serialized_state &state)
{
    state.new_section("debug", SS_debug, 1);
}

void core::serialize_clock(serialized_state &state) {
    state.new_section("clock", SS_clock, 1);
#define S(x) Sadd(state, &clock. x, sizeof(clock. x))
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
    S(vdp.bottom_rendered_line);

    S(psg.clock_divisor);

#undef S
}

void core::serialize_vdp(serialized_state &state) {
    state.new_section("vdp", SS_vdp, 1);
#define S(x) Sadd(state, &vdp. x, sizeof(vdp. x))
    S(sprite_line_buf);
    S(cur_pixel);
    S(io);
    S(irq);
    S(command);
    S(dma);
    S(sprite_collision);
    S(sprite_overflow);
    S(cycle);
    S(sc_slot);
    S(sc_array);
    S(sc_skip);

    S(CRAM);
    S(VSRAM);
    S(VRAM);
    S(fifo);
    S(line);
    S(term_out);
    u64 r = vdp.term_out_ptr - vdp.term_out;
    u32 aa = static_cast<u32>(r);
    Sadd(state, &aa, sizeof(aa));
    S(fetcher);
    S(ringbuf);
    S(debug);
    S(latch);
    S(window);
#undef S
}

void core::serialize_cartridge(serialized_state &state) {
    state.new_section("cartridge", SS_cartridge, 1);
#define S(x) Sadd(state, &cart. x, sizeof(cart. x))
    //S(ROM.size);
    //Sadd(state, &cart.ROM.ptr, cart.ROM.size);
    S(RAM_persists);
    S(RAM_mask);
    S(RAM_present);
    S(RAM_always_on);
    S(RAM_size);
    S(header);
    S(kind);
    S(SRAM->requested_size);
    if (cart.SRAM->requested_size > 0) {
        Sadd(state, cart.SRAM->data, cart.SRAM->requested_size);
    }
#undef S
}

void core::serialize_ym2612(serialized_state &state)
{
    state.new_section("ym2612", SS_ym2612, 1);
    ym2612.serialize(state);
}

void core::serialize_sn76489(serialized_state &state)
{
    state.new_section("sn76489", SS_sn76489, 1);
    psg.serialize(state);
}


void core::save_state(serialized_state &state)
{
    // Basic info
    state.version = 1;
    state.opt.len = 0;

    // Make screenshot
    state.has_screenshot = 1;
    state.screenshot.allocate(1280, 240);
    state.screenshot.clear();
    genesis_present(vdp.display_ptr.get(), state.screenshot.data.ptr, 320, 240, 1);

    serialize_console(state);
    serialize_debug(state);
    serialize_clock(state);
    serialize_cartridge(state);
    serialize_m68k(state);
    serialize_z80(state);
    serialize_vdp(state);
    serialize_sn76489(state);
    serialize_ym2612(state);
}

void core::deserialize_z80(serialized_state &state)
{
    z80.deserialize(state);
}

void core::deserialize_m68k(serialized_state &state)
{
    m68k.deserialize(state);
}

void core::deserialize_ym2612(serialized_state &state)
{
    ym2612.deserialize(state);
}

void core::deserialize_sn76489(serialized_state &state)
{
    psg.deserialize(state);
}

void core::deserialize_console(serialized_state &state) {
#define L(x) Sload(state, & x, sizeof( x))
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

    L(v_opts.vdp.enable_A);
    L(v_opts.vdp.enable_B);
    L(v_opts.vdp.enable_sprites);
    L(v_opts.vdp.ex_trace);
    L(v_opts.JP);
#undef L
}

void core::deserialize_debug(serialized_state &state) {
#define L(x) Sload(state, &cart. x, sizeof(cart. x))
#undef L
}

void core::deserialize_cartridge(serialized_state &state) {
#define L(x) Sload(state, &cart. x, sizeof(cart. x))
    //u64 sz;
    //Sload(state, &sz, sizeof(sz));
    //buf_allocate(&cart.ROM, sz);
    //Sload(state, &cart.ROM.ptr, sz);
    L(RAM_persists);
    L(RAM_mask);
    L(RAM_present);
    L(RAM_always_on);
    L(RAM_size);
    L(header);
    L(kind);
    L(SRAM->requested_size);
    if (cart.SRAM->requested_size > 0) {
        Sload(state, cart.SRAM->data, cart.SRAM->requested_size);
        cart.SRAM->dirty = true;
    }
#undef L
}

void core::deserialize_vdp(serialized_state &state) {
#define L(x) Sload(state, &vdp. x, sizeof(vdp. x))
    L(sprite_line_buf);
    L(cur_pixel);
    L(io);
    L(irq);
    L(command);
    L(dma);
    L(sprite_collision);
    L(sprite_overflow);
    L(cycle);
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
    assert(pp<sizeof(vdp.term_out));
    vdp.term_out_ptr = vdp.term_out + pp;

    L(fetcher);
    L(ringbuf);
    L(debug);
    L(latch);
    L(window);
#undef L
}

void core::deserialize_clock(serialized_state &state) {
#define L(x) Sload(state, &clock. x, sizeof(clock. x))
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
    L(vdp.bottom_rendered_line);

    L(psg.clock_divisor);

#undef L
}

void core::load_state(serialized_state &state, deserialize_ret &ret) {
    state.iter.offset = 0;
    assert(state.version == 1);

    for (auto &sec : state.sections) {
        state.iter.offset = sec.offset;
        switch (sec.kind) {
            case SS_console:
                deserialize_console(state);
                break;
            case SS_debug:
                deserialize_debug(state);
                break;
            case SS_vdp:
                deserialize_vdp(state);
                break;
            case SS_ym2612:
                deserialize_ym2612(state);
                break;
            case SS_sn76489:
                deserialize_sn76489(state);
                break;
            case SS_z80:
                deserialize_z80(state);
                break;
            case SS_m68k:
                deserialize_m68k(state);
                break;
            case SS_clock:
                deserialize_clock(state);
                break;
            case SS_cartridge:
                deserialize_cartridge(state);
                break;
            default: NOGOHERE;
        }
    }
}
}