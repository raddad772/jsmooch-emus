#pragma once

#include "helpers/int.h"
#include "helpers/physical_io.h"
#include "component/floppy/mac_floppy.h"

namespace mac {
struct core;

struct FDD {
    explicit FDD(core *bus_in, u32 num_in);
    void calculate_ticks_per_flux();
    bool floppy_inserted() const;
    void write_motor_on(bool onoff);
    void setup_track();
    void do_step();
    u8 read_reg(u8 which);
    void write_reg(u8 which);
    floppy::mac::DISC *disc{};
    bool clock();
    void set_pwm_dutycycle(i64 to);

    core *bus;
    u32 num;

    u32 RPM{};
    u32 pwm_val{};

    struct HEAD {
        u32 track_num{}; // reset to 0 at power on
        u8 flux{}; // currently-read flux value
        struct {
            bool is_stepping{};
            u64 start{};
            u64 end{};
        } stepping{};
    } head{};
    u8 step_direction{};
    bool mfm{};
    long double next_flux_tick{};
    long double ticks_per_flux{};
    u64 track_pos;

    u64 last_RPM_change_time{};
    float last_RPM_change_pos{};

    cvec_ptr<physical_io_device> ptr{};

    bool motor_on{};
    bool disk_switched{};

    physical_io_device *device{};

    bool connected{};

    u64 input_clock_cnt{};

    struct {
        i64 avg_sum{};
        i64 avg_count{};
        i64 dutycycle{};
    } pwm;
};

}