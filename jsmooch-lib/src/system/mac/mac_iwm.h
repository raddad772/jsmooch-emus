//
// Created by . on 8/5/24.
//

#pragma once

#include "component/floppy/mac_floppy.h"
#include "helpers/int.h"

namespace mac {
enum iwm_modes {
    IWMM_IDLE,
    IWMM_MOTOR_STOP_DELAY,
    IWMM_ACTIVE,
    IWMM_READ,
    IWMM_WRITE
};

struct iwm {
    explicit iwm(struct core* parent);
    core *bus;

    struct DRIVE {
        mac_floppy* disc{};

        double RPM{};
        u32 pwm_val{};

        struct {
            u32 current_data{};
            u32 track_num{}; // reset to 0 at power on

            struct {
                u32 status{};
                u64 start{};
                u64 end{};
            } stepping{};
        } head{};

        u64 last_RPM_change_time{};
        float last_RPM_change_pos{};

        u32 io_index{};
        u32 motor_on{};
        u32 disk_switched{};

        u32 head_step_direction{};
        JSM_DISC_DRIVE *device{};

        u32 connected{};

        u64 input_clock_cnt{};

        u32 pwm_len{65000}, pwm_pos{};
    } drive[2]{};

    std::vector<mac_floppy> my_disks{};
    cvec* IOs{};

    iwm_modes active{IWMM_IDLE};
    u64 motor_stop_timer{};

    iwm_modes rw{IWMM_IDLE};
    iwm_modes rw_state{};

    u8 old_drive_select{};
    u32 ctrl{};
    i32 selected_drive{-1};
    DRIVE *cur_drive{};

    struct {
        u8 CA0{}, CA1{}, CA2{}, LSTRB{}, ENABLE{};
        u8 Q6{}, Q7{};
        u8 SELECT{};
        u8 enable{};

        u8 phases{};
    } lines{};
    struct {
        u8 DIRTN{}, CSTIN{}, STEP{};

        u8 drive_select{};
        u8 data{};
        u8 read_shift{};
        u8 write_handshake{0xBD};
        u8 write_shift{};
        union {
            struct {
                u8 lower5: 5; // these are identical to mode I think
                u8 active: 1;// "active" is bit 5
                u8 unused2: 2;
            };
            u8 u{};
        } status{};

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
            u8 u{};
        } mode{};
    } regs{};
};

}