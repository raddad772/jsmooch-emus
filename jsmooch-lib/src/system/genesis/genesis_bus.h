//
// Created by . on 6/1/24.
//

#ifndef JSMOOCH_EMUS_GENESIS_BUS_H
#define JSMOOCH_EMUS_GENESIS_BUS_H

#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/physical_io.h"
#include "helpers/cvec.h"
#include "component/cpu/z80/z80.h"
#include "component/cpu/m68000/m68000.h"
#include "component/audio/sn76489/sn76489.h"
#include "component/audio/ym2612/ym2612.h"

#include "genesis_clock.h"
#include "genesis_cart.h"

struct genesis {
    struct Z80 z80;
    struct M68k m68k;
    struct genesis_clock clock;
    struct genesis_cart cart;

    struct ym2612 ym2612;
    struct SN76489 psg;

    struct genesis_vdp {
        u16 *cur_output;
        struct physical_io_device *display;
        struct {
            u32 interlace_field;
            u32 vblank;
            u32 h32, h40;
            u32 fast_h40;

            i32 z80_irq_clocks;
        } io;

        u32 cycle;

        struct {
            u32 latch;
            u32 ready;
            u16 target, address, increment;
        } command;

        u16 VRAM[32768];
    } vdp;

    u16 RAM[32768]; // RAM is stored little-endian for some reason
    u8 ARAM[8192]; // Z80 RAM!

    struct {
        struct cvec* IOs;
        u32 described_inputs;
        i64 cycles_left;
    } jsm;

    struct {
        struct {
            u32 reset_line;
            u32 reset_line_count;
            u32 bank_address_register;
            u32 bus_request;
            u32 bus_ack;
        } z80;

        struct {
            u32 open_bus_data;
        } m68k;
    } io;
};

void genesis_cycle_m68k(struct genesis* this);
void genesis_cycle_z80(struct genesis* this);
void gen_test_dbg_break(struct genesis* this);
u16 genesis_mainbus_read(struct genesis* this, u32 addr, u32 UDS, u32 LDS, u16 old, u32 has_effect);
void genesis_z80_interrupt(struct genesis* this, u32 level);
void genesis_m68k_vblank(struct genesis* this, u32 level);
#endif //JSMOOCH_EMUS_GENESIS_BUS_H
