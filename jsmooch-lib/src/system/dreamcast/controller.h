//
// Created by RadDad772 on 3/13/24.
//

#ifndef JSMOOCH_EMUS_CONTROLLER_H
#define JSMOOCH_EMUS_CONTROLLER_H

#include "helpers/int.h"

struct DC_inputs {
    u32 a, b, x, y, up, down, left, right, start;
    u32 analog_x, analog_y, analog_l, analog_r;
};

enum DCC {
    DCC_rx,
    DCC_tx
};

struct DC_controller {
    struct DC_inputs input_waiting;
    enum DCC state;
    u32 cmd_index;
    u32 cmd[16];
    u32 de_cmd;
    u32 reply_len;

    u32 src_AP;
    u32 dest_AP;
    u32 data_size;
};

struct MAPLE_port;
struct DC;
void DC_controller_write(void *ptr, u32 data);
u32 DC_controller_read(void *ptr, u32* more);
void DC_controller_init(struct DC_controller* this);
void DC_controller_connect(struct DC* console, int portnum, struct DC_controller* which);

#endif //JSMOOCH_EMUS_CONTROLLER_H
