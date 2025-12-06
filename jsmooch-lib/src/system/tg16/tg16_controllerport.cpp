//
// Created by . on 7/13/25.
//

#include "tg16_controllerport.h"
#include "component/controller/tg16b2/tg16b2.h"

namespace TG16 {

void controllerport::connect(controller_kinds kind, void *ptr)
{
    controller_kind = kind;
    controller_ptr = ptr;
}

u8 controllerport::read_data()
{
    if (!controller_ptr) return 0x0F;
    switch(controller_kind) {
        case CK_none:
            return 0x0F;
        case CK_2button:
            return static_cast<controller_2button *>(controller_ptr)->read_data();;
        default:
            printf("\nNOT IMPL BAD TG16 CT");
            return 0x0F;
    }
}

void controllerport::write_data(u8 val)
{
    if (!controller_ptr) return;
    switch(controller_kind) {
        case CK_none:
            return;
        case CK_2button:
            return static_cast<controller_2button *>(controller_ptr)->write_data(val);
            return;
        default:
            printf("\nNOT IMPL BAD TG16 CT");
            return;
    }
}
}