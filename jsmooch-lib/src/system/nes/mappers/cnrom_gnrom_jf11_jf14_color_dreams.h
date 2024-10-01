//
// Created by . on 9/30/24.
//

#ifndef JSMOOCH_EMUS_CNROM_GNROM_JF11_JF14_COLOR_DREAMS_H
#define JSMOOCH_EMUS_CNROM_GNROM_JF11_JF14_COLOR_DREAMS_H

// Implements iNES mapper 003 (aka CNROM and "similar")
// 8KB switchable CHR banks. That's pretty much all

struct NES_bus;
struct NES;
void GNROM_JF11_JF14_color_dreams_init(struct NES_bus *bus, struct NES *nes, enum NES_mappers kind);

#endif //JSMOOCH_EMUS_CNROM_H
