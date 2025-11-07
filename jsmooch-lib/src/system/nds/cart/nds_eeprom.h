//
// Created by . on 4/5/25.
//

#ifndef JSMOOCH_EMUS_NDS_EEPROM_H
#define JSMOOCH_EMUS_NDS_EEPROM_H

#include "helpers_c/int.h"

struct NDS;
void NDS_eeprom_spi_transaction(struct NDS *this, u32 val);
void NDS_eeprom_setup(struct NDS *this);

#endif //JSMOOCH_EMUS_NDS_EEPROM_H
