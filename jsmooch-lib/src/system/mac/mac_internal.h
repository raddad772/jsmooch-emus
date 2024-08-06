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
#include "component/floppy/mac_floppy.h"

#include "mac_iwm.h"
#include "mac.h"

struct mac_clock {
    u64 master_cycles;
    u64 master_frame;

    struct {
        u32 hpos, vpos;
    } crt;

    struct {
        u64 cycles_per_frame;
    } timing;
};

enum iwm_modes {
        IWMM_IDLE,
        IWMM_MOTOR_STOP_DELAY,
        IWMM_ACTIVE,
        IWMM_READ,
        IWMM_WRITE
};

struct mac {
    enum mac_variants kind;
    struct M68k cpu;
    struct mac_clock clock;
    struct mac_via {
        struct {
            u8 IRA, ORA; // Input and Output Register A
            u8 IRB, ORB; // Input and Output Register B
            u8 dirA, dirB; // Direction for A and B. 1 = output, 0 = input
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
        struct JSMAC_DRIVE {
            struct mac_floppy* disc;

            u32 RPM;
            u32 pwm_val;

            struct {
                u32 current_data;
                u32 track_num; // reset to 0 at power on

                struct {
                    u32 status;
                    u64 start;
                    u64 end;
                } stepping;
            } head;

            u64 last_RPM_change_time;
            float last_RPM_change_pos;

            u32 io_index;
            u32 motor_on;
            u32 disk_switched;

            u32 head_step_direction;
            struct JSM_DISC_DRIVE *device;

            u32 connected;

            u64 input_clock_cnt;

            u32 pwm_len, pwm_pos;
        } drive[2];

        struct cvec my_disks;
        struct cvec* IOs;

        enum iwm_modes active;
        u64 motor_stop_timer;

        enum iwm_modes rw;
        enum iwm_modes rw_state;

        u8 old_drive_select;
        u32 ctrl;
        i32 selected_drive;
        struct JSMAC_DRIVE *cur_drive;

        struct {
            u8 CA0, CA1, CA2, LSTRB, ENABLE;
            u8 Q6, Q7;
            u8 SELECT;
            u8 motor_on;

            u8 phases;
        } lines;
        struct {
            u8 DIRTN, CSTIN, STEP;

            u8 drive_select;
            u8 data;
            u8 read_shift;
            u8 write_handshake;
            u8 write_shift;
            union {
                struct {
                    u8 lower5: 5; // these are identical to mode I think
                    u8 active: 1;// "active" is bit 5
                    u8 unused2: 2;
                };
                u8 u;
            } status;

            union {
                struct {
                    u8 latched : 1;
                    u8 async: 1;
                    u8 no_timer: 1;
                    u8 fast: 1;
                    u8 mhz8: 1;
                    u8 test: 1;
                    u8 mz_reset: 1;
                    u8 reserved: 1;
                };
                u8 u;
            } mode;
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

u16 mac_mainbus_read(struct mac*, u32 addr, u32 UDS, u32 LDS, u16 old, u32 has_effect);

void mac_iwm_init(struct mac*);
void mac_iwm_delete(struct mac*);
void mac_iwm_reset(struct mac*);

void mac_step_bus(struct mac*);

void mac_mainbus_write(struct mac*, u32 addr, u32 UDS, u32 LDS, u16 val);
u16 mac_mainbus_read(struct mac*, u32 addr, u32 UDS, u32 LDS, u16 old, u32 has_effect);

u16 mac_mainbus_read_via(struct mac*, u32 addr, u16 mask, u16 old, u32 has_effect);
void mac_mainbus_write_via(struct mac*, u32 addr, u16 mask, u16 val);
u16 mac_mainbus_read_iwm(struct mac*, u32 addr, u16 mask, u16 old, u32 has_effect);
void mac_mainbus_write_iwm(struct mac*, u32 addr, u16 mask, u16 val);
u16 mac_mainbus_read_scc(struct mac*, u32 addr, u16 mask, u16 old, u32 has_effect);
void mac_mainbus_write_scc(struct mac*, u32 addr, u16 mask, u16 val);
void mac_reset_via(struct mac*);
void mac_clock_init(struct mac* this);


#endif //JSMOOCH_EMUS_MAC_INTERNAL_H
