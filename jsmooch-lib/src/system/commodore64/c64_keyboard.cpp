//
// Created by . on 11/29/25.
//

#include "c64_keyboard.h"

constexpr u32 C64_keyboard_keymap[64] = {
    /*row 0*/ JK_BACKSPACE, JK_ENTER, JK_RIGHT, JK_F7, JK_F1, JK_F3, JK_F5, JK_DOWN,
    /*row 1*/ JK_3, JK_W, JK_A, JK_4, JK_Z, JK_S, JK_E,
    /*row 2*/ JK_5, JK_R, JK_D, JK_6, JK_C, JK_F, JK_T, JK_X,
    /*row 3*/ JK_7, JK_Y, JK_G, JK_8, JK_B, JK_H, JK_U, JK_V,
    /*row 4*/ JK_9, JK_I, JK_J, JK_0, JK_M, JK_K, JK_O, JK_N,
    /*row 5*/ JK_EQUALS, JK_P, JK_L, JK_MINUS, JK_DOT, JK_SEMICOLON, 0, JK_COMMA,
    /*row 6*/ 0, JK_F6, JK_SEMICOLON, JK_NUM_CLEAR, JK_SHIFT, JK_EQUALS, JK_UP, JK_SLASH_FW,
    /*row 7*/ JK_1, JK_LEFT, JK_CTRL, JK_2, JK_SPACE, JK_CMD, JK_Q, 0
};

namespace C64 {

void keyboard::setup(std::vector<physical_io_device> &IOs) {
    physical_io_device &d = IOs.emplace_back();
    d.init(HID_KEYBOARD, 0, 0, 1, 1);
    kbd_ptr.make(IOs, IOs.size()-1);
    d.id = 0;
    d.kind = HID_KEYBOARD;
    d.connected = 1;
    d.enabled = 1;

    JSM_KEYBOARD &kbd = d.keyboard;
    memset(&kbd, 0, sizeof(JSM_KEYBOARD));
    kbd.num_keys = 64;

    for (u32 i = 0; i < kbd.num_keys; i++) {
        kbd.key_defs[i] = static_cast<JKEYS>(C64_keyboard_keymap[i]);
    }
}

u8 keyboard::read_cols(u8 val) {
    if ((last_val == val) && !new_data) return last_out;
    //printf("\nACTUAL READ COLS %02x", val);
    new_data = false;
    u8 out = 0;
    last_val = val;
    for (u32 i = 0; i < 8; i++) {
        if (val & (1 << i)) out |= scan_col(i);
    }

    last_out = out ^ 0xFF;
    return last_out;
}

u8 keyboard::scan_col(u32 num) {
    // PRA is writing...
    //printf("\nSCAN COL %d", num);
    auto &pio = kbd_ptr.get();
    auto &kbd = pio.keyboard;
    u8 out = 0;
    u8 hi3 = num << 3;
    for (u32 i = 0; i < 8; i++) {
        u32 n = hi3 | i;
        if (kbd.key_states[n]) {
            printf("\nKEY IS PRESSED!");
            out |= (1 << i);
        }

    }
    return out;
}

}