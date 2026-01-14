//
// Created by Dave on 1/30/2024.
//

#pragma once

#include "helpers/int.h"
namespace SM83 {
u32 disassemble(u32 PC, u32 (*peek)(u32), char *w, size_t sz);
}

