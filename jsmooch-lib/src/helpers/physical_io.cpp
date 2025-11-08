//
// Created by . on 4/19/24.
//

#include "physical_io.h"


void physical_io_device::init(IO_CLASSES kind, u32 enabled, u32 connected, u32 input, u32 output)
{
    kind = kind;
    enabled = enabled;
    connected = connected;
    sys_ptr = nullptr;
    input = input;
    output = output;
    id = 0;
    switch(kind) {
        case HID_TOUCHSCREEN:
            new (&touchscreen)JSM_TOUCHSCREEN();
            break;
        case HID_CONTROLLER:
            new (&controller)JSM_CONTROLLER();
            break;
        case HID_DISPLAY:
            new (&display)JSM_DISPLAY();
            break;
        case HID_KEYBOARD:
            new (&keyboard)JSM_KEYBOARD();
            break;
        case HID_MOUSE:
            new (&mouse)JSM_MOUSE();
            break;
        case HID_CHASSIS:
            new(&chassis)JSM_CHASSIS();
            break;
        case HID_AUDIO_CHANNEL:
            new(&audio_channel)JSM_AUDIO_CHANNEL();
            break;
        case HID_CART_PORT:
            new(&cartridge_port)JSM_CARTRIDGE_PORT();
            break;
        case HID_DISC_DRIVE:
            new(&disc_drive)JSM_DISC_DRIVE();
            break;
        case HID_AUDIO_CASSETTE:
            new(&audio_cassette)JSM_AUDIO_CASSETTE();
            break;
        case HID_NONE:
            break;
    }
}

physical_io_device::~physical_io_device()
{
    switch(kind) {
        case HID_TOUCHSCREEN:
            touchscreen.~JSM_TOUCHSCREEN();
            break;
        case HID_CONTROLLER:
            controller.~JSM_CONTROLLER();
            break;
        case HID_DISPLAY:
            display.~JSM_DISPLAY();
            break;
        case HID_KEYBOARD:
            keyboard.~JSM_KEYBOARD();
            break;
        case HID_MOUSE:
            mouse.~JSM_MOUSE();
            break;
        case HID_CHASSIS:
            chassis.~JSM_CHASSIS();
            break;
        case HID_AUDIO_CHANNEL:
            audio_cassette.~JSM_AUDIO_CASSETTE();
            //if (audio_channel.samples[0] != NULL) free(audio_channel.samples[0]);
            break;
        case HID_CART_PORT:
            cartridge_port.~JSM_CARTRIDGE_PORT();
            break;
        case HID_DISC_DRIVE:
            disc_drive.~JSM_DISC_DRIVE();
            break;
        case HID_AUDIO_CASSETTE:
            audio_cassette.~JSM_AUDIO_CASSETTE();
            break;
        case HID_NONE:
            break;
    }
}

void pio_new_button(struct JSM_CONTROLLER* cnt, const char* name, enum JKEYS common_id)
{
    HID_digital_button &b = cnt->digital_buttons.emplace_back();
    snprintf(b.name, sizeof(b.name), "%s", name);
    b.state = 0;
    b.id = 0;
    b.kind = DBK_BUTTON;
    b.common_id = common_id;
}

