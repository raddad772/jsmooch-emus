//
// Created by . on 7/23/24.
//

#ifndef JSMOOCH_EMUS_GENESIS_CONTROLLERPORT_H
#define JSMOOCH_EMUS_GENESIS_CONTROLLERPORT_H

#include "genesis.h"

struct genesis_controller_port {
    void* controller_ptr;
    enum genesis_controller_kinds controller_kind;
    u16 data_lines;
    u8 control;
    u8 data_latch;
};


void genesis_controllerport_connect(struct genesis_controller_port *, enum genesis_controller_kinds kind, void *ptr);
void genesis_controllerport_delete(struct genesis_controller_port *);
u16 genesis_controllerport_read_data(struct genesis_controller_port*);
void genesis_controllerport_write_data(struct genesis_controller_port*, u16 val);
u16 genesis_controllerport_read_control(struct genesis_controller_port*);
void genesis_controllerport_write_control(struct genesis_controller_port*, u16 val);


#endif //JSMOOCH_EMUS_GENESIS_CONTROLLERPORT_H
