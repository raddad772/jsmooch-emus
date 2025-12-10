//
// Created by . on 1/18/25.
//

#pragma once
#include "helpers/int.h"
namespace ARM946ES {
struct thumb2_instruction;
void decode_thumb2(u16 opc, thumb2_instruction *ins);
}
