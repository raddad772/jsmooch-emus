//
// Created by . on 7/23/24.
//

#pragma once
#include "genesis.h"

namespace genesis {
struct controller_port {
    void* controller_ptr{};
    controller_kinds controller_kind{};
    u16 data_lines{};
    u8 control{};

    void connect(controller_kinds in_kind, void *ptr);
    u16 read_data();
    void write_data(u16 val);
    u16 read_control() const;
    void write_control(u16 val);

private:
    void refresh();
};
}


