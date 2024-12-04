//
// Created by . on 4/19/24.
//

#ifndef JSMOOCH_EMUS_PHYSICAL_IO_H
#define JSMOOCH_EMUS_PHYSICAL_IO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "helpers/sram.h"
#include "helpers/int.h"
#include "helpers/cvec.h"
#include "helpers/buf.h"

struct physical_io_device;

enum JKEYS {
    JK_NONE,
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
    JK_ESC,
    JK_CTRL,

    // Digital button common IDs, for defaults
    DBCID_begin,
    DBCID_co_up = DBCID_begin,
    DBCID_co_down,
    DBCID_co_left,
    DBCID_co_right,
    DBCID_co_fire1,
    DBCID_co_fire2,
    DBCID_co_fire3,
    DBCID_co_fire4,
    DBCID_co_fire5,
    DBCID_co_fire6,
    DBCID_co_shoulder_left,
    DBCID_co_shoulder_right,
    DBCID_co_start,
    DBCID_co_select,
    DBCID_ch_power,
    DBCID_ch_pause,
    DBCID_ch_reset,
    DBCID_ch_diff_left,
    DBCID_ch_diff_right,
    DBCID_ch_game_select,
    DBCID_end = DBCID_ch_game_select
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

struct HID_digital_button {
    char name[40];
    enum DIGITAL_BUTTON_KIND kind;
    u32 state;

    u32 id;

    enum JKEYS common_id;
};

struct HID_analog_axis {
    char name[40];
    u32 value;
    u32 min, max;
    u32 id;
};

struct jsm_system;

struct JSM_CONTROLLER {
    char name[50];
    struct cvec analog_axes;
    struct cvec digital_buttons;
};

struct JSM_KEYBOARD {
    enum JKEYS key_defs[100];
    u32 key_states[100];
    u32 num_keys;
};


enum JSM_DISPLAY_STANDARDS {
    JSS_CRT,
    JSS_NTSC,
    JSS_PAL,
    JSS_SECAM,
    JSS_LCD
};


struct JSM_DISPLAY_PIXELOMETRY {
    struct {
        u32 left_hblank, right_hblank;
        u32 visible;

        u32 max_visible;
    } cols;

    struct {
        u32 top_vblank, bottom_vblank;
        u32 visible;

        u32 max_visible;
    } rows;

    struct { // Visible area not seen
        u32 left, right, top, bottom;
        u32 force; // for things like gamegear that draw a whole frame and crop it onto an LCD
    } overscan;

    struct { // Defines how our output is from the top-left of the frame, as defined starting before left_hblank and above top_vblank
        i32 x, y;
    } offset;

    struct {
        u32 width, height;
    } outbuf_info;
};

struct JSM_DISPLAY_GEOMETRY {
    struct {
        u32 width, height;
    } physical_aspect_ratio;
};


struct JSM_DISPLAY {
    enum JSM_DISPLAY_STANDARDS standard;
    u32 enabled;
    double fps;
    u32 fps_override_hint; // Is it OK to go to a close value near this

    void *output[2];
    void *output_debug_metadata[2];
    u32 last_written;
    u32 last_displayed;
    u16 *cur_output;

    // geometry, used mostly for event viewer really
    struct JSM_DISPLAY_PIXELOMETRY pixelometry;
    struct JSM_DISPLAY_GEOMETRY geometry;

    u32 scan_x, scan_y; // Current electron gun X and Y, as defined inside the geometry above.
};

struct JSM_MOUSE {
    struct HID_digital_button *left_button;
    struct HID_digital_button *right_button;
    i32 new_x, new_y;
};

struct JSM_CHASSIS {
    struct cvec indicators;
    struct cvec digital_buttons;
};

struct JSM_AUDIO_CHANNEL {
    u32 sample_rate;
    u32 low_pass_filter;
    void *samples[2];
    u32 last_written;
};

struct JSM_CARTRIDGE_PORT {
    void (*load_cart)(struct jsm_system *ptr, struct multi_file_set* mfs, struct physical_io_device *whichpio);
    void (*unload_cart)(struct jsm_system *ptr);
    struct persistent_store SRAM;
};

struct physical_io_device;

struct JSM_DISC_DRIVE {
    void (*insert_disc)(struct jsm_system *ptr, struct physical_io_device *pio, struct multi_file_set* mfs);
    void (*remove_disc)(struct jsm_system *ptr);
    void (*close_drive)(struct jsm_system *ptr);
    void (*open_drive)(struct jsm_system *ptr);
};

struct JSM_AUDIO_CASSETTE {
    void (*insert_tape)(struct jsm_system *ptr, struct physical_io_device *pio, struct multi_file_set* mfs, struct buf* sram);
    void (*remove_tape)(struct jsm_system *ptr);
    void (*rewind)(struct jsm_system *ptr);
    void (*play)(struct jsm_system *ptr);
    void (*stop)(struct jsm_system *ptr);
};

struct physical_io_device {
    enum IO_CLASSES kind;

    u32 connected;
    u32 enabled;
    void *sys_ptr;
    u32 id;

    u32 input;
    u32 output;

    union {
        struct JSM_CONTROLLER controller;
        struct JSM_KEYBOARD keyboard;
        struct JSM_DISPLAY display;
        struct JSM_MOUSE mouse;
        struct JSM_CHASSIS chassis;
        struct JSM_AUDIO_CHANNEL audio_channel;
        struct JSM_CARTRIDGE_PORT cartridge_port;
        struct JSM_DISC_DRIVE disc_drive;
        struct JSM_AUDIO_CASSETTE audio_cassette;
    };
};

void physical_io_device_init(struct physical_io_device*, enum IO_CLASSES kind, u32 enabled, u32 connected, u32 input, u32 output);
void physical_io_device_delete(struct physical_io_device*);
void pio_new_button(struct JSM_CONTROLLER* cnt, const char* name, enum JKEYS common_id);

#ifdef __cplusplus
}
#endif


#endif //JSMOOCH_EMUS_PHYSICAL_IO_H
