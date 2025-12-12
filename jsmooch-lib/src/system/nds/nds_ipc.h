//
// Created by . on 1/21/25.
//

#pragma once

#include "helpers/int.h"
namespace NDS {

struct IPC_FIFO {
    [[nodiscard]] bool is_empty() const;
    [[nodiscard]] bool is_not_empty() const;
    void empty();
    [[nodiscard]] bool is_full() const;
    bool push(u32 val);
    [[nodiscard]] u32 peek_last() const;
    u32 pop();
    u32 data[16]{};
    u32 head{}, tail{};
    u32 last{};
    u32 len{};

    u32 enable{};
};


}
