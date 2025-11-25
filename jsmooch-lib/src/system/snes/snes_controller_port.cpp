//
// Created by . on 5/9/25.
//

#include "snes_controller_port.h"
#include "snes_bus.h"
#include "component/controller/snes/snes_joypad.h"
namespace SNES::controller {
void port::latch(u8 val)
{
    switch(kind) {
        case kinds::none:
            return;
        case kinds::standard: {
            SNES_joypad *jp = static_cast<SNES_joypad *>(ptr);
            return jp->latch(val); }
        default:
            printf("\nUNKNOWN CONTROLLER KIND! DEATH!");
            return;
        }
}

u8 port::data()
{
    switch(kind) {
        case kinds::none:
            return 0;
        case kinds::standard: {
            SNES_joypad *jp = static_cast<SNES_joypad *>(ptr);
            return jp->data(); }
        default:
            printf("\nUNKNOWN CONTROLLER KINd2!");
            return 0;
    }
}

void port::connect(kinds in_kind, void *in_ptr)
{
    kind = in_kind;
    ptr = in_ptr;
}
}