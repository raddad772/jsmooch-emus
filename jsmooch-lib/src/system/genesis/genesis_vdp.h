//
// Created by . on 6/1/24.
//

#ifndef JSMOOCH_EMUS_GENESIS_VDP_H
#define JSMOOCH_EMUS_GENESIS_VDP_H


#include "helpers/int.h"
#include "helpers/physical_io.h"

struct genesis_vdp {
    u16* cur_output;
    struct physical_io_device* display;
};


#endif //JSMOOCH_EMUS_GENESIS_VDP_H
