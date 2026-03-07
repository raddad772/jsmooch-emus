//
// Created by . on 2/26/25.
//

#pragma once

#include "helpers/physical_io.h"
#include "helpers/scheduler.h"
#include "helpers/sram.h"

#include "ps1_sio.h"

namespace PS1::SIO {

struct memcard {
    // PIO pointer
    explicit memcard(PS1::core *parent);
    void schedule_ack(u64 clock_cycle, u64 time, u32 level);
    u8 exchange_byte(u8 byte, u64 clock_cycle);;
    physical_io_device *pio{};

    core *bus;

    device interface{};

    u32 protocol_step{};
    u32 selected{};
    u32 cmd{};
    u8 buttons[2]{};
    u64 sch_id{};
    u32 still_sched{};
};

}
