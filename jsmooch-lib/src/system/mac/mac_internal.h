//
// Created by . on 7/24/24.
//

#ifndef JSMOOCH_EMUS_MAC_INTERNAL_H
#define JSMOOCH_EMUS_MAC_INTERNAL_H

#include "helpers/int.h"
#include "helpers/sys_interface.h"
#include "helpers/debug.h"
#include "helpers/physical_io.h"

#include "component/misc/via6522/via6522.h"
#include "component/cpu/m68000/m68000.h"

#include "mac.h"

struct mac_clock {
    u64 master_cycles;
    u64 master_frame;

    struct {
        u32 hpos, vpos;
    } crt;
};

struct mac {
    enum mac_variants kind;
    struct M68k cpu;
    struct mac_clock clock;
    struct mac_via {
        struct {
            u8 regA, regB;
            u8 dirA, dirB;
            u16 T1C; // timer 1 count
            u16 T1L; // timer 1 latch
            u16 T2C; // timer 2 count
            u16 T2L; // write-only timer 2 latch
            u8 ACR, PCR; // Auxilliary and Peripheral control registers
            u8 IER, IFR; // Interrupt enable and flag registers
            u8 SR; // Keyboard shfit register

            u8 num_shifts;
        } regs;

        struct {
            u32 t1_active, t2_active;
        } state;
    } via;

    struct {
        struct {
            u8 aData, aCtl, bData, bCtl;
        } regs;
    } scc;

    struct {
        struct {
            u32 on;
        } io;
    } sound;

    struct {
        struct {
            u8 CA0, CA1, CA2, LSTRB, ENABLE;
            u8 Q6, Q7;
            u8 SELECT;
        } lines;
        struct {
            u8 DIRTN, CSTIN, STEP;
        } regs;
    } iwm;

    u32 described_inputs;
    struct cvec *IOs;

    u16 *ROM;
    u16 *RAM;
    u32 ram_contended; // by display
    u32 ROM_size, ROM_mask;
    u32 RAM_size, RAM_mask;

    struct {
        struct cvec *IOs;
        u32 disc_drive_index;
    } disc_drive;

    struct {
        struct cvec *IOs;
        u32 keyboard_index;
    } keyboard;

    struct {
        struct physical_io_device* display;
        u8 *cur_output;

        u32 ram_base_address;
        u32 read_addr;
        u16 output_shifter;

        u32 IRQ_signal;
        u32 IRQ_out;

        void (*scanline_func)(struct mac*);
    } display;

    struct {
        struct cvec* IOs;
        u32 described_inputs;
        i64 cycles_left;
    } jsm;

    struct {
        u32 ROM_overlay;
        u32 eclock;

        struct {
            u16 last_read_data;
        } cpu;

        struct {
            u32 via, scc, iswitch;
        } irq;
    } io;

    struct {
        u8 param_mem[20];
        u8 old_clock_bit;
        u8 cmd;
        u32 seconds;

        u64 cycle_counter;

        u8 test_register;
        u8 write_protect_register;
        struct {
            u8 shift, progress, kind;
        } tx;
    } rtc;
};

u16 mac_mainbus_read(struct mac* this, u32 addr, u32 UDS, u32 LDS, u16 old, u32 has_effect);

void mac_step_bus(struct mac* this);

void mac_mainbus_write(struct mac* this, u32 addr, u32 UDS, u32 LDS, u16 val);
u16 mac_mainbus_read(struct mac* this, u32 addr, u32 UDS, u32 LDS, u16 old, u32 has_effect);

u16 mac_mainbus_read_via(struct mac* this, u32 addr, u16 mask, u16 old, u32 has_effect);
void mac_mainbus_write_via(struct mac* this, u32 addr, u16 mask, u16 val);
u16 mac_mainbus_read_iwm(struct mac* this, u32 addr, u16 mask, u16 old, u32 has_effect);
void mac_mainbus_write_iwm(struct mac* this, u32 addr, u16 mask, u16 val);
u16 mac_mainbus_read_scc(struct mac* this, u32 addr, u16 mask, u16 old, u32 has_effect);
void mac_mainbus_write_scc(struct mac* this, u32 addr, u16 mask, u16 val);
void mac_reset_via(struct mac* this);

#endif //JSMOOCH_EMUS_MAC_INTERNAL_H
