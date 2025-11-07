//
// Created by . on 2/26/25.
//

#ifndef JSMOOCH_EMUS_PS1_SIO_H
#define JSMOOCH_EMUS_PS1_SIO_H

// controllers need to be able to set and unset ACK. so they ened to know port 1 or 2
// they need to exchange bytes also
// and get CSn changes

#include "helpers_c/int.h"
#include "helpers_c/scheduler.h"

// so...CPU does write to port...CSn changes
// then a byte exchange happens, OK
//

enum PS1_SIO0_port {
    PS1S0_controller1,
    PS1S0_controller2,
    PS1S0_mem1,
    PS1S0_mem2
};

enum PS1_SIO_device_kinds {
    PS1DK_none,
    PS1DK_digital_pad
};

struct PS1;
struct PS1_SIO_device {
    enum PS1_SIO_device_kinds kind;
    void *device_ptr;

    u32 connected;

    u32 CS, ACK;
    void (*set_CS)(void *ptr, u32 level, u64 clock_cycle);
    u8 (*exchange_byte)(void *ptr, u8 byte, u64 clock_cycle);
};

struct PS1_SIO0 {
    struct {
        struct PS1_SIO0_RX_FIFO {
            u32 head, tail;
            u32 num;
            u8 buf[8];
        } RX_FIFO;
        u32 misc, baud;
        union {
            struct {
                u32 tx_fifo_not_full : 1; // bit 0
                u32 rx_fifo_not_empty : 1; // bit 1
                u32 tx_idle: 1; // bit 2
                u32 rx_parity_error: 1; // bit 3
                u32 _unused1 : 3; // bits 4,5,6
                u32 dsr_input : 1; // bit 7. input for /ACK signal
                u32 _unused2 : 1; // bit 8
                u32 irq_request : 1; // bit 9IRQ signal
                u32 unknown: 1; // bit 10
                u32 baud_timer: 21; // bits 11-31
            };
            u32 u;
        } SIO_STAT;

        union {
            struct {
                u32 baud_reload_factor : 2; // bits 0-1
                u32 character_len : 2; // bits 2-3
                u32 parity_enable : 1; // bit 4
                u32 parity_type : 1; // bit 5
                u32 _unused : 2; // bits 6, 7
                u32 clock_polarity : 1; // bit 8
            };
            u32 u;
        } SIO_MODE;

        union {
            struct {
                u16 tx_enable : 1; // bit 0   1
                u16 dtr_output : 1; // bit 1. this is the CS pin.  2
                u16 rx_enable : 1; // bit 2 4
                u16 _unused1: 1; // bit 3 8
                u16 ack : 1; // bit 4 0x10
                u16 _unused2: 1; // bit 5
                u16 reset : 1; // bit 6
                u16 unknown: 1; // bit 7
                u16 rx_irq_mode : 2; // bits 8-9
                u16 tx_irq_enable : 1; // bit 10
                u16 rx_irq_enable : 1; // bit 11
                u16 dsr_irq_enable : 1; // bit 12. this is ACK's IRQ
                u16 sio0_port_sel : 1; // bit 13. 0=port1, 1=port2
            };
            u16 u;
        } SIO_CTRL;

        // If disconnected, these will be NULL!
        struct PS1_SIO_device *controller1, *controller2;
        struct PS1_SIO_device *mem1, *mem2;
    } io;

    struct {
        u32 rx_signal;
        u32 tx_signal;
    } irq;
    u64 sch_id;
    u32 still_sched;

};

void PS1_SIO0_init(struct PS1 *this);
void PS1_SIO0_update_ACKs(struct PS1 *this, enum PS1_SIO0_port port, u32 level);
void PS1_SIO0_write(struct PS1 *this, u32 addr, u32 sz, u32 val);
u32 PS1_SIO0_read(struct PS1 *this, u32 addr, u32 sz);

#endif //JSMOOCH_EMUS_PS1_SIO_H
