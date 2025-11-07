//
// Created by . on 7/13/25.
//

#ifndef JSMOOCH_EMUS_TG16_CONTROLLERPORT_H
#define JSMOOCH_EMUS_TG16_CONTROLLERPORT_H

#include "helpers_c/int.h"

enum TG16_controller_kinds {
    TG16CK_none,
    TG16CK_2button
};

struct TG16_controllerport {
    void* controller_ptr;
    enum TG16_controller_kinds controller_kind;

};

#endif //JSMOOCH_EMUS_TG16_CONTROLLERPORT_H

void TG16_controllerport_connect(struct TG16_controllerport *, enum TG16_controller_kinds kind, void *ptr);
void TG16_controllerport_delete(struct TG16_controllerport *);
u8 TG16_controllerport_read_data(struct TG16_controllerport*);
void TG16_controllerport_write_data(struct TG16_controllerport*, u8 val);
