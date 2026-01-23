//
// Created by . on 5/2/24.
//

#pragma once

#include "helpers/int.h"
#include "helpers/sys_interface.h"

namespace ZXSpectrum {
    enum variants {
        spectrum48,
        spectrum128,
        spectrumplus2,
        spectrumplus3
    };
}
jsm_system *ZXSpectrum_new(ZXSpectrum::variants variant);
void ZXSpectrum_delete(jsm_system* system);
