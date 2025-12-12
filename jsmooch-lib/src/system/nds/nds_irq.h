//
// Created by . on 1/20/25.
//

#pragma once

#include "helpers/int.h"

namespace NDS {

enum IRQs {
    IRQ_VBLANK = 0,
    IRQ_HBLANK = 1,
    IRQ_VMATCH = 2,
    IRQ_TIMER0 = 3,
    IRQ_TIMER1 = 4,
    IRQ_TIMER2 = 5,
    IRQ_TIMER3 = 6,
    IRQ_SERIAL = 7,
    IRQ_DMA0 = 8,
    IRQ_DMA1 = 9,
    IRQ_DMA2 = 10,
    IRQ_DMA3 = 11,
    IRQ_KEYPAD = 12,
    IRQ_IPC_SYNC = 16,
    IRQ_IPC_SEND_EMPTY = 17,
    IRQ_IPC_RECV_NOT_EMPTY = 18,
    IRQ_CART_DATA_READY = 19,
    IRQ_GXFIFO = 21,
    IRQ_SPI = 23

};

}