//
// Created by Dave on 2/7/2024.
//

#include "sms_gg_clock.h"

SMSGG::clock::clock(jsm::systems in_variant, jsm::regions in_region) : variant(in_variant), region(in_region) {
    timing.region = ((region == jsm::regions::USA) || (region == jsm::regions::JAPAN)) ? jsm::display_standards::NTSC : jsm::display_standards::PAL;
    if (timing.region == jsm::display_standards::PAL) {
        timing.frame_lines = 313;
        timing.cc_line = 311;
    }
}