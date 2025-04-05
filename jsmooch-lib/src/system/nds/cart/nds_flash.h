//
// Created by . on 4/5/25.
//

#ifndef JSMOOCH_EMUS_NDS_FLASH_H
#define JSMOOCH_EMUS_NDS_FLASH_H

#include "helpers/int.h"

struct NDS;
void NDS_flash_spi_transaction(struct NDS *this, u32 val);

#endif //JSMOOCH_EMUS_NDS_FLASH_H
