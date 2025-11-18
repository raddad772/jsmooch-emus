//
// Created by . on 5/9/25.
//

#ifndef JSMOOCH_EMUS_SNES_CONTROLLER_PORT_H
#define JSMOOCH_EMUS_SNES_CONTROLLER_PORT_H

#include "helpers/int.h"

enum SNES_controller_kinds {
    SNES_CK_none,
    SNES_CK_standard
};

struct SNES_controller_port {
    void *ptr;
    enum SNES_controller_kinds kind;
};

struct SNES;

void SNES_controllerport_latch(struct SNES *, u32 num, u32 val);
u32 SNES_controllerport_data(struct SNES *, u32 num);
void SNES_controllerport_connect(struct SNES_controller_port *, enum SNES_controller_kinds kind, void *ptr);
void SNES_controllerport_delete(struct SNES_controller_port *);

#endif //JSMOOCH_EMUS_SNES_CONTROLLER_PORT_H
