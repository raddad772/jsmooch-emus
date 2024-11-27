//
// Created by . on 4/19/24.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "physical_io.h"


void physical_io_device_init(struct physical_io_device* this, enum IO_CLASSES kind, u32 enabled, u32 connected, u32 input, u32 output)
{
    memset(this, 0, sizeof(struct physical_io_device));
    this->kind = kind;
    this->enabled = enabled;
    this->connected = connected;
    this->sys_ptr = NULL;
    this->input = input;
    this->output = output;
    this->id = 0;
    switch(kind) {
        case HID_CONTROLLER:
            cvec_init(&this->controller.analog_axes, sizeof(struct HID_analog_axis), 2);
            cvec_init(&this->controller.digital_buttons, sizeof(struct HID_digital_button), 12);
            break;
        case HID_DISPLAY:
            break;
        case HID_KEYBOARD:
            break;
        case HID_MOUSE:
            break;
        case HID_CHASSIS:
            cvec_init(&this->chassis.digital_buttons, sizeof(struct HID_digital_button), 2);
            break;
        case HID_AUDIO_CHANNEL:
            break;
        case HID_CART_PORT:
            persistent_store_init(&this->cartridge_port.SRAM);
            break;
        case HID_DISC_DRIVE:
            break;
        case HID_AUDIO_CASSETTE:
            break;
        case HID_NONE:
            break;
    }
}

void physical_io_device_delete(struct physical_io_device* this)
{
    switch(this->kind) {
        case HID_CONTROLLER:
            cvec_delete(&this->controller.analog_axes);
            cvec_delete(&this->controller.digital_buttons);
            break;
        case HID_DISPLAY:
            if (this->display.output[0] != NULL) { free(this->display.output[0]); this->display.output[0] = NULL; }
            if (this->display.output[1] != NULL) { free(this->display.output[1]); this->display.output[1] = NULL; }
            if (this->display.output_debug_metadata[0] != NULL) { free(this->display.output_debug_metadata[0]); this->display.output_debug_metadata[0] = NULL; }
            if (this->display.output_debug_metadata[1] != NULL) { free(this->display.output_debug_metadata[1]); this->display.output_debug_metadata[1] = NULL; }
            break;
        case HID_KEYBOARD:
            break;
        case HID_MOUSE:
            break;
        case HID_CHASSIS:
            cvec_delete(&this->chassis.digital_buttons);
            break;
        case HID_AUDIO_CHANNEL:
            if (this->audio_channel.samples[0] != NULL) free(this->audio_channel.samples[0]);
            break;
        case HID_CART_PORT:
            //TODO: this
            persistent_store_delete(&this->cartridge_port.SRAM);
            //this->cartridge_port.unload_cart(this);
            break;
        case HID_DISC_DRIVE:
            break;
        case HID_AUDIO_CASSETTE:
            break;
        case HID_NONE:
            break;
    }
}

void pio_new_button(struct JSM_CONTROLLER* cnt, const char* name, enum JKEYS common_id)
{
    struct HID_digital_button *b = cvec_push_back(&cnt->digital_buttons);
    snprintf(b->name, sizeof(b->name), "%s", name);
    b->state = 0;
    b->id = 0;
    b->kind = DBK_BUTTON;
    b->common_id = common_id;
}

