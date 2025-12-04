//
// Created by . on 8/5/24.
//

#pragma once

#include "component/floppy/mac_floppy.h"
#include "helpers/int.h"
#include "mac_fdd.h"
#include "helpers/cvec.h"

namespace mac {
enum iwm_modes {
    M_IDLE,
    M_MOTOR_STOP_DELAY,
    M_ACTIVE,
    M_READ,
    M_WRITE
};


struct iwm {
    void reset();
    void drive_select(u8 towhich);
    void update_phases();
    u32 get_drive_reg();
    void set_drive_reg();
    void clock();
    u16 control(u8 addr, u8 val, u32 is_write);
    u16 read(u8 addr);
    void write(u8 addr, u8 val);
    explicit iwm(core* parent);
    [[nodiscard]] bool floppy_write_protected() const;
    [[nodiscard]] bool floppy_inserted() const;
    void write_data(u8 val);
    void write_mode(u8 val);
    core *bus;
    FDD drive[2];
    std::vector <floppy::mac::DISC> my_disks{};

    iwm_modes active{M_IDLE};
    u64 motor_stop_timer{};

    iwm_modes rw{ M_IDLE};
    iwm_modes rw_state{};

    u8 old_drive_select{};
    u32 ctrl{};
    i32 selected_drive{-1};
    FDD *cur_drive{};

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