//
// Created by . on 8/5/24.
//

#pragma once

#include "component/floppy/mac_floppy.h"
#include "helpers/int.h"
#include "mac_fdd.h"
#include "mac.h"
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
    iwm(core *parent, variants variant_in);
    u32 num_drives{};
    variants variant;
    void reset();
    void clock();
    void update_pwm(u8 val);

    u16 do_read(u32 addr, u16 mask, u16 old, bool has_effect);
    void do_write(u32 addr, u16 mask, u16 val);
    void write_HEADSEL(u8 what);

    [[nodiscard]] bool floppy_write_protected() const;
    [[nodiscard]] bool floppy_inserted() const;
        core *bus;
    FDD drive[2];
    std::vector <floppy::mac::DISC> my_disks{};
    FDD *selected_drive = &drive[0];

    struct {
        u8 HEADSEL{}, CA0{}, CA1{}, CA2{}, LSTRB{}, ENABLE{}, SELECT{}, Q6{}, Q7{};

    } lines{};

    struct {
        i32 pos{};
        i32 buffer{-1};
    } write{};

    struct {
        u8 data{}, shifter{};
        union {
            struct {
                u8 mode : 5;
                u8 enable : 1;
                u8 mz : 1;
                u8 sense : 1;
            };
            u8 u{};
        } status{};

        union {
            struct {
                u8 latch : 1; //  latch mode (1) always on mac
                u8 sync : 1; //
                u8 onesec : 1;
                u8 fast : 1; // 1 = fast mode
                u8 speed : 1; // 0 = 7MHz, 1 = 8MHz
                u8 test : 1; // test
                u8 mzreset : 1; // MZ-reset
                u8 reserved: 1; // reserved
            };
            u8 u{};
        } mode{};
    }regs{};

private:
    void access(u32 addr);
    u8 get_drive_reg() const;

};

}