
#include <cstdio>
#include <cassert>
#include <cstdlib>

#include "genesis_bus.h"
#include "component/controller/genesis3/genesis3.h"
#include "component/controller/genesis6/genesis6.h"

namespace genesis {
void controller_port::connect(controller_kinds in_kind, void *ptr)
{
    controller_kind = in_kind;
    controller_ptr = ptr;
    data_lines = 0x7f;
    control = 0;
    //data_latch = 0x7f;
}

void controller_port::refresh()
{
    u16 data, mask;
    mask = 0x7F;
    if (!controller_ptr) {
        data = 0x7F;
    }
    else {
        switch(controller_kind) {
            case controller_3button:
                data = static_cast<c3button *>(controller_ptr)->read_data();
                break;
            case controller_6button:
                data = static_cast<c6button *>(controller_ptr)->read_data();;
                break;
            default:
                printf("\nYOU FORGOT THISSSSS89");
                data = 0x3F;
                break;
        }
    }
    u8 output_mask = 0x80 | control;
    u32 old_lines = data_lines;

    /*data_lines = (data & mask) | (data_lines & ~mask);
    data_lines = (data_latch & output_mask) | (data_lines & ~output_mask);*/
    data_lines = data;

    if (controller_ptr && (old_lines != data_lines)) {
        switch(controller_kind) {
            case controller_3button:
                //genesis3_write_data(controller_ptr, data_lines);
                break;
            case controller_6button:
                //genesis6_write_data(controller_ptr, data_lines);
                break;
            default:
                printf("\nYOU FORGOT THISSSSS90");
        }
    }
}

u16 controller_port::read_data()
{
    if (controller_ptr != nullptr) {
        switch(controller_kind) {
            case controller_3button:
                static_cast<c3button *>(controller_ptr)->latch();
                break;
            case controller_6button:
                static_cast<c6button *>(controller_ptr)->latch();
                break;
            default:
                printf("FORGOT THIS55");
                break;
        }
    }
    refresh();

    return data_lines;
}

void controller_port::write_data(u16 val)
{
    //data_latch = val;
    //refresh(this);
    if (controller_ptr) {
        switch(controller_kind) {
            case controller_none:
                return;
            case controller_3button:
                static_cast<c3button *>(controller_ptr)->write_data(val);
                return;
            case controller_6button:
                static_cast<c6button *>(controller_ptr)->write_data(val);
                return;
        }
    }
}

u16 controller_port::read_control() const
{
    return control;
}

void controller_port::write_control(u16 val)
{
    control = val & 0xFF;
    refresh();
}

}