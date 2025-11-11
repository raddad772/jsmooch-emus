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
};


void genesis_controllerport_connect(genesis_controller_port *, enum genesis_controller_kinds kind, void *ptr);
void genesis_controllerport_delete(genesis_controller_port *);
u16 genesis_controllerport_read_data(genesis_controller_port*);
void genesis_controllerport_write_data(genesis_controller_port*, u16 val);
u16 genesis_controllerport_read_control(genesis_controller_port*);
void genesis_controllerport_write_control(genesis_controller_port*, u16 val);


#endif //JSMOOCH_EMUS_GENESIS_CONTROLLERPORT_H
