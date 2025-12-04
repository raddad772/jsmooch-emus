#pragma once

#include "helpers/int.h"
#include "helpers/physical_io.h"
#include "component/floppy/mac_floppy.h"

namespace mac {
struct core;

struct FDD {
    explicit FDD(core *bus_in) : bus(bus_in) {}
    core *bus;
    bool floppy_inserted();
    void write_motor_on(bool onoff);
    void setup_track();
    void do_step();
    u32 read_reg(u8 which);
    void write_reg(u8 which, u8 val);
    floppy::mac::DISC *disc{};

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

    cvec_ptr<physical_io_device> ptr;

    u32 motor_on{};
    u32 disk_switched{};

    u32 head_step_direction{};
    physical_io_device *device{};

    bool connected{};

    u64 input_clock_cnt{};

    u32 pwm_len{65000}, pwm_pos{};
};

}