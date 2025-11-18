//
// Created by . on 1/22/25.
//

#ifndef JSMOOCH_EMUS_NDS_SPI_H
#define JSMOOCH_EMUS_NDS_SPI_H

#include "helpers/int.h"
struct NDS;


void NDS_SPI_reset(struct NDS *);
u32 NDS_SPI_read(struct NDS *, u32 sz);
void NDS_SPI_write(struct NDS *, u32 sz, u32 val);
void NDS_SPI_release_hold(struct NDS *this);

#endif //JSMOOCH_EMUS_NDS_SPI_H
