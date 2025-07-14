
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "genesis_bus.h"
#include "component/controller/genesis3/genesis3.h"
#include "component/controller/genesis6/genesis6.h"

void genesis_controllerport_connect(struct genesis_controller_port *this, enum genesis_controller_kinds kind, void *ptr)
{
    this->controller_kind = kind;
    this->controller_ptr = ptr;
    this->data_lines = 0x7f;
    this->control = 0;
    //this->data_latch = 0x7f;
}

void genesis_controllerport_delete(struct genesis_controller_port *this)
{
    this->controller_kind = genesis_controller_none;
    this->controller_ptr = NULL;
}

static void refresh(struct genesis_controller_port* this)
{
    u16 data, mask;
    mask = 0x7F;
    if (!this->controller_ptr) {
        data = 0x7F;
    }
    else {
        switch(this->controller_kind) {
            case genesis_controller_3button:
                data = genesis3_read_data(this->controller_ptr);
                break;
            case genesis_controller_6button:
                data = genesis6_read_data(this->controller_ptr);
                break;
            default:
                printf("\nYOU FORGOT THISSSSS89");
                data = 0x3F;
                break;
        }
    }
    u8 output_mask = 0x80 | this->control;
    u32 old_lines = this->data_lines;

    /*this->data_lines = (data & mask) | (this->data_lines & ~mask);
    this->data_lines = (this->data_latch & output_mask) | (this->data_lines & ~output_mask);*/
    this->data_lines = data;

    if (this->controller_ptr && (old_lines != this->data_lines)) {
        switch(this->controller_kind) {
            case genesis_controller_3button:
                //genesis3_write_data(this->controller_ptr, this->data_lines);
                break;
            case genesis_controller_6button:
                //genesis6_write_data(this->controller_ptr, this->data_lines);
                break;
            default:
                printf("\nYOU FORGOT THISSSSS90");
        }
    }
}

u16 genesis_controllerport_read_data(struct genesis_controller_port* this)
{
    if (this->controller_ptr != NULL) {
        switch(this->controller_kind) {
            case genesis_controller_3button:
                genesis_controller_3button_latch(this->controller_ptr);
                break;
            case genesis_controller_6button:
                genesis_controller_6button_latch(this->controller_ptr);
                break;
            default:
                printf("FORGOT THIS55");
                break;
        }
    }
    refresh(this);

    return this->data_lines;
    //return 0;
}

void genesis_controllerport_write_data(struct genesis_controller_port* this, u16 val)
{
    //this->data_latch = val;
    //refresh(this);
    if (this->controller_ptr) {
        switch(this->controller_kind) {
            case genesis_controller_none:
                return;
            case genesis_controller_3button:
                genesis3_write_data(this->controller_ptr, val);
                return;
            case genesis_controller_6button:
                genesis6_write_data(this->controller_ptr, val);
                return;
        }
    }
    else {
        return;
    }
}

u16 genesis_controllerport_read_control(struct genesis_controller_port* this)
{
    return this->control;
}

void genesis_controllerport_write_control(struct genesis_controller_port* this, u16 val)
{
    this->control = val & 0xFF;
    refresh(this);
}

