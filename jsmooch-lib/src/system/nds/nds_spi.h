//
// Created by . on 1/22/25.
//

#ifndef JSMOOCH_EMUS_NDS_SPI_H
#define JSMOOCH_EMUS_NDS_SPI_H

#include "helpers/int.h"
struct NDS;


void NDS_SPI_reset(NDS *);
u32 NDS_SPI_read(NDS *, u32 sz);
void NDS_SPI_write(NDS *, u32 sz, u32 val);
void NDS_SPI_release_hold(NDS *this);

#endif //JSMOOCH_EMUS_NDS_SPI_H
