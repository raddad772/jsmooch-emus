//
// Created by . on 4/19/24.
//

#ifndef JSMOOCH_EMUS_PHYSICAL_IO_H
#define JSMOOCH_EMUS_PHYSICAL_IO_H

#include "helpers/int.h"
#include "helpers/cvec.h"
#include "helpers/buf.h"

enum IO_CLASSES {
    HID_NONE,
    HID_DISPLAY,
    HID_CONTROLLER,
    HID_KEYBOARD,
    HID_MOUSE,
    HID_CHASSIS,
    HID_AUDIO_CHANNEL,
    HID_CART_PORT,
    HID_DISC_DRIVE,
    HID_AUDIO_CASSETTE
};

enum DIGITAL_BUTTON_KIND {
    DBK_BUTTON,
    DBK_SWITCH
};

enum HID_digital_button_common_id {
    DBCID_unknown,
    DBCID_co_up,
    DBCID_co_down,
    DBCID_co_left,
    DBCID_co_right,
    DBCID_co_fire1,
    DBCID_co_fire2,
    DBCID_co_fire3,
    DBCID_co_start,
    DBCID_co_select,
    DBCID_ch_power,
    DBCID_ch_pause,
    DBCID_ch_reset,
    DBCID_ch_diff_left,
    DBCID_ch_diff_right,
    DBCID_ch_game_select
};

struct HID_digital_button {
    char name[40];
    enum DIGITAL_BUTTON_KIND kind;
    u32 state;

    u32 id;

    enum HID_digital_button_common_id common_id;
};

struct HID_analog_axis {
    char name[40];
    u32 value;
    u32 min, max;
    u32 id;
};

struct jsm_system;

struct physical_io_device {
    enum IO_CLASSES kind;

    u32 connected;
    u32 enabled;
    void *sys_ptr;
    u32 id;

    u32 input;
    u32 output;

    union physical_io_device_union {
        struct JSM_CONTROLLER {
            char name[50];
            struct cvec analog_axes;
            struct cvec digital_buttons;
        } controller;

        struct {

        } keyboard;

        struct {
            float fps;
            void *output[2];
            u32 last_written;
            u32 last_displayed;
        } display;

        struct {

        } mouse;

        struct {
            struct cvec indicators;
            struct cvec digital_buttons;
        } chassis;

        struct {
            u32 sample_rate;
            void *samples[2];
            u32 last_written;
        } audio_channel;

        struct {
            void (*load_cart)(struct jsm_system *ptr, struct multi_file_set* mfs, struct buf* sram);
            void (*unload_cart)(struct jsm_system *ptr);
        } cartridge_port;

        struct {
            void (*insert_disc)(struct jsm_system *ptr, struct multi_file_set* mfs);
            void (*remove_disc)(struct jsm_system *ptr);
            void (*close_drive)(struct jsm_system *ptr);
            void (*open_drive)(struct jsm_system *ptr);
        } disc_drive;

        struct {

        } audio_cassette;
    } device;
};

void physical_io_device_init(struct physical_io_device* this, enum IO_CLASSES kind, u32 enabled, u32 connected, u32 input, u32 output);
void physical_io_device_delete(struct physical_io_device* this);

#endif //JSMOOCH_EMUS_PHYSICAL_IO_H
