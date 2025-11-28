//
// Created by . on 11/26/25.
//

#include <cstdio>

#include "m6581.h"
namespace M6581 {

void core::write_IO(u16 addr, u8 val) {
    printf("\nSID: unhandled write to %02x: %02x", addr, val);
}

u8 core::read_IO(u16 addr, u8 old, bool has_effect) {
    printf("\nSID: unhandled read from %04x", addr);
    return 0xFF;
}

void core::reset() {

}

void core::cycle() {

}
}