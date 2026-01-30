//
// Created by . on 2/11/25.
//

#pragma once

#include "helpers/int.h"
namespace R3000 {
struct core;
struct OPCODE;
enum R3000_COP0_reg {
    RCR_PRId = 15,
    RCR_SR = 12,
    RCR_Cause = 13,
    RCR_EPC = 14,
    RCR_BadVaddr = 8,
    RCR_Config = 3,
    RCR_BusCtrl = 2,
    RCR_PortSize = 10,
    RCR_Count = 9,
    RCR_Compare = 11
};

}