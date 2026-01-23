//
// Created by . on 1/23/26.
//
#include <cassert>
#include "clock.h"
namespace ZXSpectrum {
CLOCK::CLOCK(variants variant_in) : variant(variant_in)
{
    switch(variant_in) {
        case spectrum128:
            screen_bottom = 311;
            screen_right = 228 * 2;
            scanlines_before_frame = 63;
            break;
        case spectrum48:
            screen_bottom = 312;
            screen_right = 224 * 2;
            scanlines_before_frame = 64;
            break;
        default:
            assert(1==2);
    }
}

}