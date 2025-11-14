//
// Created by . on 4/19/25.
//

#pragma once

#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/jsm_string.h"

u32 SPC700_disassemble(u32 PC, jsm_debug_read_trace &rt, jsm_string &out, u32 p_p);
