//
// Created by . on 2/26/25.
//

#ifndef JSMOOCH_EMUS_PS1_DIGITAL_PAD_H
#define JSMOOCH_EMUS_PS1_DIGITAL_PAD_H

#include "helpers/physical_io.h"
#include "helpers/scheduler.h"

#include "ps1_sio.h"

struct PS1;

struct PS1_SIO_digital_gamepad {
    // PIO pointer
    struct physical_io_device *pio;

    struct PS1 *bus;
    u32 num;

    struct PS1_SIO_device interface;

    u32 protocol_step;
    u32 selected;
    u32 cmd;
    u8 buttons[2];
    u64 sch_id;
    u32 still_sched;
};

void PS1_SIO_digital_gamepad_init(struct PS1_SIO_digital_gamepad *, struct PS1 *bus);
void PS1_SIO_gamepad_setup_pio(struct physical_io_device *d, u32 num, const char*name, u32 connected);

#endif //JSMOOCH_EMUS_PS1_DIGITAL_PAD_H
