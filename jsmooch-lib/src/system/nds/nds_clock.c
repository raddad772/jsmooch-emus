//
// Created by . on 12/4/24.
//

#include "nds_clock.h"

void NDS_clock_init(struct NDS_clock *this)
{
    this->scanline_start_cycle = this->scanline_start_cycle_next = 0;
    this->master_cycle_count7 = 0;
    this->master_cycle_count9 = 0;
}

void NDS_clock_reset(struct NDS_clock *this)
{}
