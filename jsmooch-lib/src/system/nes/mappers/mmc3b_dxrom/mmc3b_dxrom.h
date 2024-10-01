//
// Created by . on 9/27/24.
//

#ifndef JSMOOCH_EMUS_MMC3B_DXROM_H
#define JSMOOCH_EMUS_MMC3B_DXROM_H

struct NES_bus;
struct NES;
void MMC3b_init(struct NES_bus *bus, struct NES *nes, enum NES_mappers kind);

#endif //JSMOOCH_EMUS_MMC3B_DXROM_H
