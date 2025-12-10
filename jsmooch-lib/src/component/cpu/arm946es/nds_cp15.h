//
// Created by . on 1/19/25.
//

#pragma once

#include "helpers/int.h"


namespace ARM946ES {

static constexpr u32 ITCM_SIZE = 0x8000;
static constexpr u32 DTCM_SIZE = 0x4000;
static constexpr u32 ITCM_MASK = ITCM_SIZE - 1;
static constexpr u32 DTCM_MASK = DTCM_SIZE - 1;

}

