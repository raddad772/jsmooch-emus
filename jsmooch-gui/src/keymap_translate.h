//
// Created by . on 8/4/24.
//

#ifndef JSMOOCH_EMUS_KEYMAP_TRANSLATE_H
#define JSMOOCH_EMUS_KEYMAP_TRANSLATE_H

#define IMGUI_DISABLE_OBSOLETE_KEYIO
#include "../vendor/myimgui/imgui.h"
#include "helpers/physical_io.h"

enum ImGuiKey jk_to_imgui(enum JKEYS key_id);

#endif //JSMOOCH_EMUS_KEYMAP_TRANSLATE_H
