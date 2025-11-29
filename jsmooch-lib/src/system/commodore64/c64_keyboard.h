//
// Created by . on 11/29/25.
//

#include "helpers/int.h"
#include "helpers/physical_io.h"

namespace C64 {

struct keyboard {
    u8 read_cols(u8 val);

    u8 scan_col(u32 num);

    void setup(std::vector<physical_io_device> &IOs);

    u8 last_val{}, last_out{};
    bool new_data{};

    cvec_ptr<physical_io_device> kbd_ptr;
    // CIA1
};
}