//
// Created by . on 4/19/24.
//

#pragma once

#include <vector>
#include "enums.h"
#include "helpers/sram.h"
#include "helpers/int.h"
#include "helpers/buf.h"
#include "helpers/physical_io.h"
#include "helpers/cvec.h"

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
    JK_QUOTE,
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
    JK_F1,
    JK_F2,
    JK_F3,
    JK_F4,
    JK_F5,
    JK_F6,
    JK_F7,
    JK_GAMEPAD_UP,
    JK_GAMEPAD_DOWN,
    JK_GAMEPAD_LEFT,
    JK_GAMEPAD_RIGHT,
    JK_GAMEPAD_PLUS,
    JK_GAMEPAD_MINUS,
    JK_GAMEPAD_A,
    JK_GAMEPAD_B,
    JK_GAMEPAD_X,
    JK_GAMEPAD_Y,
    JK_GAMEPAD_ZR,
    JK_GAMEPAD_ZL,
    JK_GAMEPAD_R,
    JK_GAMEPAD_L,

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
    DBCID_co_shoulder_left2,
    DBCID_co_shoulder_right2,
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
    HID_AUDIO_CASSETTE,
    HID_TOUCHSCREEN,
    HID_HEX_KEYPAD
};

enum DIGITAL_BUTTON_KIND {
    DBK_BUTTON,
    DBK_SWITCH
};

struct HID_digital_button {
    HID_digital_button() = default;
    char name[40]{};
    DIGITAL_BUTTON_KIND kind{};
    u32 state{};

    u32 id{};

    JKEYS common_id{};

};

struct HID_analog_axis {
    HID_analog_axis() = default;
    char name[40]{};
    u32 value{};
    u32 min{}, max{};
    u32 id{};
};

struct jsm_system;

struct JSM_CONTROLLER {
    char name[50]{};
    std::vector<HID_analog_axis> analog_axes{};
    std::vector<HID_digital_button> digital_buttons{};
};

struct JSM_HEX_KEYPAD {
    u32 key_states[16];
};

struct JSM_KEYBOARD {
    JKEYS key_defs[100];
    u32 key_states[100];
    u32 num_keys;
};



struct JSM_DISPLAY_PIXELOMETRY {
    struct {
        u32 left_hblank{}, right_hblank{};
        u32 visible{};

        u32 max_visible{};
    } cols{};

    struct {
        u32 top_vblank{}, bottom_vblank{};
        u32 visible{};

        u32 max_visible{};
    } rows{};

    struct { // Visible area not seen
        u32 left{}, right{}, top{}, bottom{};
        u32 force{}; // for things like gamegear that draw a whole frame and crop it onto an LCD
    } overscan{};

    struct { // Defines how our output is from the top-left of the frame, as defined starting before left_hblank and above top_vblank
        i32 x{}, y{};
    } offset{};

    struct {
        u32 width{}, height{};
    } outbuf_info{};
};

struct JSM_DISPLAY_GEOMETRY {
    struct {
        u32 width{}, height{};
    } physical_aspect_ratio{};
};


struct JSM_DISPLAY {
    jsm::display_kinds kind{};
    bool enabled{};
    double fps{};
    u32 fps_override_hint{}; // Is it OK to go to a close value near this

    void *output[2]{};
    void *output_debug_metadata[2]{};
    u32 last_written{};
    //u32 last_displayed;
    u16 *cur_output{};

    // geometry, used mostly for event viewer really
    JSM_DISPLAY_PIXELOMETRY pixelometry{};
    JSM_DISPLAY_GEOMETRY geometry{};

    u32 scan_x{}, scan_y{}; // Current electron gun X and Y, as defined inside the geometry above.
    ~JSM_DISPLAY() {
        if (output[0] != nullptr) { free(output[0]); output[0] = nullptr; }
        if (output[1] != nullptr) { free(output[1]); output[1] = nullptr; }
        if (output_debug_metadata[0] != nullptr) { free(output_debug_metadata[0]); output_debug_metadata[0] = nullptr; }
        if (output_debug_metadata[1] != nullptr) { free(output_debug_metadata[1]); output_debug_metadata[1] = nullptr; }
    }

    cvec_ptr<physical_io_device> pio{};

};

struct JSM_MOUSE {
    HID_digital_button *left_button{};
    HID_digital_button *right_button{};
    i32 new_x{}, new_y{};
    cvec_ptr<physical_io_device> pio{};
};

struct JSM_CHASSIS {
    //struct cvec indicators;
    std::vector<HID_digital_button> digital_buttons{};
    cvec_ptr<physical_io_device> pio{};
};

struct JSM_AUDIO_CHANNEL {
    u32 sample_rate{};
    u32 left{}, right{};
    u32 num{};
    u32 low_pass_filter{};
    void *samples[2]{};
    u32 last_written{};
};

struct JSM_TOUCHSCREEN {
    struct {
        i32 x{}, y{}, down{};
    } touch;
    struct {
        i32 width{}, height{};
        i32 x_offset{}, y_offset{};
    } params{};
};

struct JSM_CARTRIDGE_PORT {
    void (*load_cart)(jsm_system *ptr, multi_file_set& mfs, physical_io_device &whichpio){};
    void (*unload_cart)(jsm_system *ptr){};
    persistent_store SRAM{};
};

struct JSM_DISC_DRIVE {
    void (*insert_disc)(jsm_system *ptr, physical_io_device &pio, multi_file_set& mfs){};
    void (*remove_disc)(jsm_system *ptr){};
    void (*close_drive)(jsm_system *ptr){};
    void (*open_drive)(jsm_system *ptr){};
};

struct JSM_AUDIO_CASSETTE {
    void (*insert_tape)(jsm_system *ptr, physical_io_device &pio, multi_file_set& mfs, buf* sram){};
    void (*remove_tape)(jsm_system *ptr){};
    void (*rewind)(jsm_system *ptr){};
    void (*play)(jsm_system *ptr){};
    void (*stop)(jsm_system *ptr){};
};

#include <utility>
struct physical_io_device {
    physical_io_device() {};
    ~physical_io_device();
    // Move constructor
    physical_io_device(physical_io_device&& other) noexcept {
        move_from(std::move(other));
    }

    // Move assignment
    physical_io_device& operator=(physical_io_device&& other) noexcept {
        if (this != &other) {
            destroy_active_member();
            move_from(std::move(other));
        }
        return *this;
    }
    IO_CLASSES kind{};

    bool connected{};
    bool enabled{};
    void *sys_ptr{};
    u32 id{};
    void init(IO_CLASSES kind, bool enabled, bool connected, bool input, bool output);

    u32 input{};
    u32 output{};

    union {
        JSM_CONTROLLER controller{};
        JSM_KEYBOARD keyboard;
        JSM_DISPLAY display;
        JSM_MOUSE mouse;
        JSM_CHASSIS chassis;
        JSM_AUDIO_CHANNEL audio_channel;
        JSM_CARTRIDGE_PORT cartridge_port;
        JSM_DISC_DRIVE disc_drive;
        JSM_AUDIO_CASSETTE audio_cassette;
        JSM_TOUCHSCREEN touchscreen;
        JSM_HEX_KEYPAD hex_keypad;

    };

private:
    void destroy_active_member() noexcept;
    void move_from(physical_io_device&& other) noexcept;
};

void pio_new_button(JSM_CONTROLLER* cnt, const char* name, enum JKEYS common_id);
