//
// Created by . on 1/17/25.
//

#ifndef JSMOOCH_EMUS_FLASH_H
#define JSMOOCH_EMUS_FLASH_H

void GBA_cart_write_flash(struct GBA *, u32 addr, u32 sz, u32 access, u32 val);
u32 GBA_cart_read_flash(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect);

#endif //JSMOOCH_EMUS_FLASH_H
