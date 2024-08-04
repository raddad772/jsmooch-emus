//
// Created by . on 4/19/24.
//

#ifndef JSMOOCH_EMUS_PHYSICAL_IO_H
#define JSMOOCH_EMUS_PHYSICAL_IO_H

#include "helpers/int.h"
#include "helpers/cvec.h"
#include "helpers/buf.h"

enum JKEYS {
    JK_0,
    JK_1,
    JK_2,
    JK_3,
    JK_4,
    JK_5,
    JK_6,
    JK_7,
    JK_8,
    JK_9,
    JK_Q,
    JK_W,
    JK_E,
    JK_R,
    JK_T,
    JK_P,
    JK_O,
    JK_I,
    JK_U,
    JK_Y,
    JK_A,
    JK_S,
    JK_D,
    JK_F,
    JK_G,
    JK_L,
    JK_K,
    JK_J,
    JK_H,
    JK_Z,
    JK_X,
    JK_C,
    JK_V,
    JK_M,
    JK_N,
    JK_B,
    JK_ENTER,
    JK_UP,
    JK_DOWN,
    JK_LEFT,
    JK_RIGHT,
    JK_TAB,
    JK_TILDE,
    JK_BACKSPACE,
    JK_CAPS,
    JK_SPACE,
    JK_SHIFT,
    JK_RSHIFT,
    JK_OPTION, // MAC option key
    JK_CMD,    // MAC cmd key
    JK_MINUS,
    JK_EQUALS,
    JK_SQUARE_BRACKET_LEFT,
    JK_SQUARE_BRACKET_RIGHT,
    JK_COMMA,
    JK_SEMICOLON,
    JK_APOSTROPHE,
    JK_SLASH_FW,
    JK_SLASH_BW,
    JK_DOT,
    JK_NUM0,
    JK_NUM1,
    JK_NUM2,
    JK_NUM3,
    JK_NUM4,
    JK_NUM5,
    JK_NUM6,
    JK_NUM7,
    JK_NUM8,
    JK_NUM9,
    JK_NUM_ENTER,
    JK_NUM_DOT,
    JK_NUM_PLUS,
    JK_NUM_MINUS,
    JK_NUM_DIVIDE,
    JK_NUM_STAR,
    JK_NUM_LOCK,
    JK_NUM_CLEAR,
    JK_NONE
};


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

        struct JSM_KEYBOARD {
            enum JKEYS key_defs[100];
            u32 key_states[100];
            u32 num_keys;
        } keyboard;

        struct JSM_DISPLAY{
            float fps;
            void *output[2];
            u32 last_written;
            u32 last_displayed;
        } display;

        struct JSM_MOUSE{
            struct HID_digital_button *left_button;
            struct HID_digital_button *right_button;
            i32 new_x, new_y;
        } mouse;

        struct JSM_CHASSIS{
            struct cvec indicators;
            struct cvec digital_buttons;
        } chassis;

        struct JSM_AUDIO_CHANNEL{
            u32 sample_rate;
            void *samples[2];
            u32 last_written;
        } audio_channel;

        struct JSM_CARTRIDGE_PORT{
            void (*load_cart)(struct jsm_system *ptr, struct multi_file_set* mfs, struct buf* sram);
            void (*unload_cart)(struct jsm_system *ptr);
        } cartridge_port;

        struct JSM_DISC_DRIVE{
            void (*insert_disc)(struct jsm_system *ptr, struct multi_file_set* mfs);
            void (*remove_disc)(struct jsm_system *ptr);
            void (*close_drive)(struct jsm_system *ptr);
            void (*open_drive)(struct jsm_system *ptr);
        } disc_drive;

        struct JSM_AUDIO_CASSETTE{
            void (*insert_tape)(struct jsm_system *ptr, struct multi_file_set* mfs, struct buf* sram);
            void (*remove_tape)(struct jsm_system *ptr);
            void (*rewind)(struct jsm_system *ptr);
            void (*play)(struct jsm_system *ptr);
            void (*stop)(struct jsm_system *ptr);
        } audio_cassette;
    } device;
};

void physical_io_device_init(struct physical_io_device*, enum IO_CLASSES kind, u32 enabled, u32 connected, u32 input, u32 output);
void physical_io_device_delete(struct physical_io_device*);
void pio_new_button(struct JSM_CONTROLLER* cnt, const char* name, enum HID_digital_button_common_id common_id);

#endif //JSMOOCH_EMUS_PHYSICAL_IO_H
