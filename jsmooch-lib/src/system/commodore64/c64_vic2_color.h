//
// Created by . on 11/26/25.
//

// Thank you https://www.pepto.de/projects/colorvic/
#pragma once

#include "helpers/int.h"

namespace VIC2::color::pepto {

u32 get_abgr(int index, int revision, float brightness, float contrast, float saturation);
};