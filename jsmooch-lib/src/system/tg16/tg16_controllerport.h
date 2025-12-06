//
// Created by . on 7/13/25.
//

#include "helpers/int.h"

namespace TG16 {

enum controller_kinds {
    CK_none,
    CK_2button
};

struct controllerport {
    void connect(controller_kinds kind, void *ptr);
    u8 read_data();
    void write_data(u8 val);

    void* controller_ptr{};
    controller_kinds controller_kind=CK_none;
};

}
