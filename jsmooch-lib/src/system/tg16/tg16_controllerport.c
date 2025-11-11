//
// Created by . on 7/13/25.
//

#include "tg16_controllerport.h"
#include "component/controller/tg16b2/tg16b2.h"

void TG16_controllerport_connect(TG16_controllerport *this, enum TG16_controller_kinds kind, void *ptr)
{
    this->controller_kind = kind;
    this->controller_ptr = ptr;
}

void TG16_controllerport_delete(TG16_controllerport *this)
{
    this->controller_kind = TG16CK_none;
    this->controller_ptr = NULL;
}

u8 TG16_controllerport_read_data(TG16_controllerport *this)
{
    if (!this->controller_ptr) return 0x0F;
    switch(this->controller_kind) {
        case TG16CK_none:
            return 0x0F;
        case TG16CK_2button:
            return TG16_2button_read_data(this->controller_ptr);
        default:
            printf("\nNOT IMPL BAD TG16 CT");
            return 0x0F;
    }
}

void TG16_controllerport_write_data(TG16_controllerport *this, u8 val)
{
    if (!this->controller_ptr) return;
    switch(this->controller_kind) {
        case TG16CK_none:
            return;
        case TG16CK_2button:
            TG16_2button_write_data(this->controller_ptr, val);
            return;
        default:
            printf("\nNOT IMPL BAD TG16 CT");
            return;
    }
}
