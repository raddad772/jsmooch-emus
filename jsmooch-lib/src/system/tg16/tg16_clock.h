//
// Created by . on 6/18/25.
//

namespace TG16 {
#include "helpers/int.h"

struct clock {
    clock();
    void reset();
    u64 master_cycles{};
    u64 unused{};
    u64 master_frames{};

    struct {
        u64 scanline_start{};
    } vdc{};

    struct {
        u64 cpu{}, vce{}, timer{};
    } next{};

    struct {
        struct {
            u64 cycles{};
        } scanline{};

        struct {
            u64 cycles{};
            u32 lines{};
        } frame{};

        struct {
            u64 cycles{};
            u32 frames{};
        } second{};
    } timing{};
};

}

