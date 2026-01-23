//
// Created by . on 1/21/26.
//

#include "zxspectrum_bus.h"

namespace ZXSpectrum {

u8 core::ULA_readmem(const u16 addr) const {
    return bank.display[addr & 0x3FFF];
}

}