//
// Created by . on 4/19/24.
//

#include "physical_io.h"

void physical_io_device::move_from(physical_io_device&& other) noexcept {
    kind = other.kind;
    connected = other.connected;
    enabled = other.enabled;
    sys_ptr = other.sys_ptr;
    id = other.id;
    input = other.input;
    output = other.output;

    // Move the correct union member
    switch (kind) {
        case HID_CONTROLLER: new(&controller) JSM_CONTROLLER(std::move(other.controller)); break;
        case HID_KEYBOARD: new(&keyboard) JSM_KEYBOARD(std::move(other.keyboard)); break;
        case HID_DISPLAY: new(&display) JSM_DISPLAY(std::move(other.display)); break;
        case HID_MOUSE: new(&mouse) JSM_MOUSE(std::move(other.mouse)); break;
        case HID_CHASSIS: new(&chassis) JSM_CHASSIS(std::move(other.chassis)); break;
        case HID_AUDIO_CHANNEL: new(&audio_channel) JSM_AUDIO_CHANNEL(std::move(other.audio_channel)); break;
        case HID_CART_PORT: new(&cartridge_port) JSM_CARTRIDGE_PORT(std::move(other.cartridge_port)); break;
        case HID_DISC_DRIVE: new(&disc_drive) JSM_DISC_DRIVE(std::move(other.disc_drive)); break;
        case HID_AUDIO_CASSETTE: new(&audio_cassette) JSM_AUDIO_CASSETTE(std::move(other.audio_cassette)); break;
        case HID_TOUCHSCREEN: new(&touchscreen) JSM_TOUCHSCREEN(std::move(other.touchscreen)); break;
        default: break;
    }

    // Reset source
    other.kind = IO_CLASSES{};
}

void physical_io_device::init(IO_CLASSES inkind, u32 inenabled, u32 inconnected, u32 ininput, u32 inoutput)
{
    kind = inkind;
    enabled = inenabled;
    connected = inconnected;
    sys_ptr = nullptr;
    input = ininput;
    output = inoutput;
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

void physical_io_device::destroy_active_member() noexcept
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

physical_io_device::~physical_io_device()
{
    destroy_active_member();
}

void pio_new_button(JSM_CONTROLLER* cnt, const char* name, enum JKEYS common_id)
{
    HID_digital_button &b = cnt->digital_buttons.emplace_back();
    snprintf(b.name, sizeof(b.name), "%s", name);
    b.state = 0;
    b.id = 0;
    b.kind = DBK_BUTTON;
    b.common_id = common_id;
}

