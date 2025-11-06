//
// Created by . on 1/20/25.
//

#ifndef JSMOOCH_EMUS_NDS_IRQ_H
#define JSMOOCH_EMUS_NDS_IRQ_H

#include "helpers/int.h"
struct NDS;
void NDS_update_IFs(struct NDS*, u32 bitnum);
void NDS_update_IFs_card(struct NDS*, u32 bitnum);
void NDS_update_IF9(struct NDS*, u32 bitnum);
void NDS_update_IF7(struct NDS*, u32 bitnum);
void NDS_eval_irqs(struct NDS*);
void NDS_eval_irqs_9(struct NDS*);
void NDS_eval_irqs_7(struct NDS*);

enum NDS_IRQs {
    NDS_IRQ_VBLANK = 0,
    NDS_IRQ_HBLANK = 1,
    NDS_IRQ_VMATCH = 2,
    NDS_IRQ_TIMER0 = 3,
    NDS_IRQ_TIMER1 = 4,
    NDS_IRQ_TIMER2 = 5,
    NDS_IRQ_TIMER3 = 6,
    NDS_IRQ_SERIAL = 7,
    NDS_IRQ_DMA0 = 8,
    NDS_IRQ_DMA1 = 9,
    NDS_IRQ_DMA2 = 10,
    NDS_IRQ_DMA3 = 11,
    NDS_IRQ_KEYPAD = 12,
    NDS_IRQ_IPC_SYNC = 16,
    NDS_IRQ_IPC_SEND_EMPTY = 17,
    NDS_IRQ_IPC_RECV_NOT_EMPTY = 18,
    NDS_IRQ_CART_DATA_READY = 19,
    NDS_IRQ_GXFIFO = 21,
    NDS_IRQ_SPI = 23

};


#endif //JSMOOCH_EMUS_NDS_IRQ_H
