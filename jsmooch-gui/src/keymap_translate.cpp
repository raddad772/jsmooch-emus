//
// Created by . on 8/4/24.
//
#include <stdio.h>

#include "keymap_translate.h"

enum ImGuiKey jk_to_imgui(enum JKEYS key_id) {
    switch (dbcid_to_default(key_id)) {
        case JK_0:
            return ImGuiKey_0;
        case JK_1:
            return ImGuiKey_1;
        case JK_2:
            return ImGuiKey_2;
        case JK_3:
            return ImGuiKey_3;
        case JK_4:
            return ImGuiKey_4;
        case JK_5:
            return ImGuiKey_5;
        case JK_6:
            return ImGuiKey_6;
        case JK_7:
            return ImGuiKey_7;
        case JK_8:
            return ImGuiKey_8;
        case JK_9:
            return ImGuiKey_9;
        case JK_Q:
            return ImGuiKey_Q;
        case JK_W:
            return ImGuiKey_W;
        case JK_E:
            return ImGuiKey_E;
        case JK_R:
            return ImGuiKey_R;
        case JK_T:
            return ImGuiKey_T;
        case JK_P:
            return ImGuiKey_P;
        case JK_O:
            return ImGuiKey_O;
        case JK_I:
            return ImGuiKey_I;
        case JK_U:
            return ImGuiKey_U;
        case JK_Y:
            return ImGuiKey_Y;
        case JK_A:
            return ImGuiKey_A;
        case JK_S:
            return ImGuiKey_S;
        case JK_D:
            return ImGuiKey_D;
        case JK_F:
            return ImGuiKey_F;
        case JK_G:
            return ImGuiKey_G;
        case JK_L:
            return ImGuiKey_L;
        case JK_K:
            return ImGuiKey_K;
        case JK_J:
            return ImGuiKey_J;
        case JK_H:
            return ImGuiKey_H;
        case JK_Z:
            return ImGuiKey_Z;
        case JK_X:
            return ImGuiKey_X;
        case JK_C:
            return ImGuiKey_C;
        case JK_V:
            return ImGuiKey_V;
        case JK_M:
            return ImGuiKey_M;
        case JK_N:
            return ImGuiKey_N;
        case JK_B:
            return ImGuiKey_B;
        case JK_ENTER:
            return ImGuiKey_Enter;
        case JK_UP:
            return ImGuiKey_UpArrow;
        case JK_DOWN:
            return ImGuiKey_DownArrow;
        case JK_LEFT:
            return ImGuiKey_LeftArrow;
        case JK_RIGHT:
            return ImGuiKey_RightArrow;
        case JK_TAB:
            return ImGuiKey_Tab;
        case JK_TILDE:
            return ImGuiKey_GraveAccent;
        case JK_BACKSPACE:
            return ImGuiKey_Backspace;
        case JK_CAPS:
            return ImGuiKey_CapsLock;
        case JK_SPACE:
            return ImGuiKey_Space;
        case JK_SHIFT:
            return ImGuiKey_LeftShift;
        case JK_RSHIFT:
            return ImGuiKey_RightShift;
        case JK_OPTION:
            return ImGuiKey_LeftAlt;
        case JK_CMD:
            return ImGuiKey_LeftSuper;
        case JK_MINUS:
            return ImGuiKey_Minus;
        case JK_EQUALS:
            return ImGuiKey_Equal;
        case JK_SQUARE_BRACKET_LEFT:
            return ImGuiKey_LeftBracket;
        case JK_SQUARE_BRACKET_RIGHT:
            return ImGuiKey_RightBracket;
        case JK_COMMA:
            return ImGuiKey_Comma;
        case JK_SEMICOLON:
            return ImGuiKey_Semicolon;
        case JK_APOSTROPHE:
            return ImGuiKey_Apostrophe;
        case JK_SLASH_FW:
            return ImGuiKey_Slash;
        case JK_SLASH_BW:
            return ImGuiKey_Backslash;
        case JK_DOT:
            return ImGuiKey_Period;
        case JK_NUM0:
            return ImGuiKey_Keypad0;
        case JK_NUM1:
            return ImGuiKey_Keypad1;
        case JK_NUM2:
            return ImGuiKey_Keypad2;
        case JK_NUM3:
            return ImGuiKey_Keypad3;
        case JK_NUM4:
            return ImGuiKey_Keypad4;
        case JK_NUM5:
            return ImGuiKey_Keypad5;
        case JK_NUM6:
            return ImGuiKey_Keypad6;
        case JK_NUM7:
            return ImGuiKey_Keypad7;
        case JK_NUM8:
            return ImGuiKey_Keypad8;
        case JK_NUM9:
            return ImGuiKey_Keypad9;
        case JK_NUM_ENTER:
            return ImGuiKey_KeypadEnter;
        case JK_NUM_DOT:
            return ImGuiKey_KeypadDecimal;
        case JK_NUM_PLUS:
            return ImGuiKey_KeypadAdd;
        case JK_NUM_MINUS:
            return ImGuiKey_KeypadSubtract;
        case JK_NUM_DIVIDE:
            return ImGuiKey_KeypadDivide;
        case JK_NUM_STAR:
            return ImGuiKey_KeypadMultiply;
        case JK_NUM_LOCK:
            return ImGuiKey_NumLock;
        case JK_NUM_CLEAR:
            return ImGuiKey_KeypadEqual;
        case JK_NONE:
            return ImGuiKey_None;
        case JK_ESC:
            return ImGuiKey_Escape;
        case JK_CTRL:
            return ImGuiKey_LeftCtrl;
        default:
            break;
    }
    printf("\nUnknown key attempted usage: %d", key_id);
    return ImGuiKey_None;
}

enum JKEYS dbcid_to_default(enum JKEYS key_id)
{
    if ((key_id < DBCID_begin) || (key_id > DBCID_end)) return key_id;
    switch(key_id) {
        case DBCID_co_up: return JK_UP;
        case DBCID_co_down: return JK_DOWN;
        case DBCID_co_left: return JK_LEFT;
        case DBCID_co_right: return JK_RIGHT;
        case DBCID_co_fire1: return JK_Z;
        case DBCID_co_fire2: return JK_X;
        case DBCID_co_fire3: return JK_C;
        case DBCID_co_fire4: return JK_A;
        case DBCID_co_fire5: return JK_S;
        case DBCID_co_fire6: return JK_D;
        case DBCID_co_start: return JK_ENTER;
        case DBCID_co_select: return JK_TAB;
        case DBCID_ch_power: return JK_Q;
        case DBCID_ch_pause: return JK_W;
        case DBCID_ch_reset: return JK_E;
        case DBCID_ch_diff_left: return JK_B;
        case DBCID_ch_diff_right: return JK_N;
        case DBCID_ch_game_select: return JK_M;
        default:
            return key_id;
    }
}