//
// Created by . on 2/26/25.
//

#include <string.h>
#include "ps1_sio.h"
#include "../ps1_bus.h"

void PS1_SIO0_init(struct PS1 *this)
{
    this->sio0.io.SIO_STAT.tx_fifo_not_full = 1;
    this->sio0.io.SIO_STAT.tx_idle = 1;
}

static void update_rx_signal(struct PS1 *this)
{
    static const u8 num[4] = { 1, 2, 4, 8};
    this->sio0.irq.rx_signal = this->sio0.io.RX_FIFO.num >= num[this->sio0.io.SIO_CTRL.rx_irq_mode];
    this->sio0.io.SIO_STAT.rx_fifo_not_empty = this->sio0.io.RX_FIFO.num > 0;
}

static void update_IRQs(struct PS1 *this)
{
    printif(ps1.sio0.irq, "\nport: update IRQs. RX.e:%d RX.s:%d DSR.e:%d DSR.s:%d", this->sio0.io.SIO_CTRL.rx_irq_enable, this->sio0.irq.rx_signal, this->sio0.io.SIO_CTRL.dsr_irq_enable, this->sio0.io.SIO_STAT.dsr_input);
    u32 old_signal = this->sio0.io.SIO_STAT.irq_request;
    u32 signal = this->sio0.io.SIO_CTRL.rx_irq_enable && this->sio0.irq.rx_signal;
    signal |= this->sio0.io.SIO_CTRL.tx_irq_enable && this->sio0.irq.tx_signal;
    signal |= this->sio0.io.SIO_CTRL.dsr_irq_enable && this->sio0.io.SIO_STAT.dsr_input;

    this->sio0.io.SIO_STAT.irq_request |= signal; // It is only SET by these, not un-set
    printif(ps1.sio0.irq, "\nport: old IRQ signal:%d new signal:%d", old_signal, this->sio0.io.SIO_STAT.irq_request);
    if (!old_signal && this->sio0.io.SIO_STAT.irq_request) {
        printif(ps1.sio0.irq, "\nSIO0 IRQ 0->1");
    }
    PS1_set_irq(this, PS1IRQ_SIO0, signal);
}

struct PS1_SIO0_memport {
    struct PS1_SIO_device *memcard, *controller;
};

void PS1_SIO0_update_ACKs(struct PS1 *bus, enum PS1_SIO0_port port, u32 level)
{
    struct PS1_SIO0 *this = &bus->sio0;
    u32 cont1_ack =0, cont2_ack = 0, mem1_ack = 0, mem2_ack = 0;
    if (this->io.controller1) cont1_ack = this->io.controller1->ACK;
    if (this->io.controller2) cont2_ack = this->io.controller2->ACK;
    if (this->io.mem1) mem1_ack = this->io.mem1->ACK;
    if (this->io.mem2) mem2_ack = this->io.mem2->ACK;

    switch(port) {
        case PS1S0_controller1:
            this->io.controller1->ACK = level;
            cont1_ack = level;
            break;
        case PS1S0_controller2:
            this->io.controller2->ACK = level;
            cont2_ack = level;
            break;
        case PS1S0_mem1:
            this->io.mem1->ACK = level;
            cont2_ack = level;
            break;
        case PS1S0_mem2:
            this->io.mem2->ACK = level;
            cont2_ack = level;
            break;
    }

    u32 new_ack = cont1_ack | cont2_ack | mem1_ack | mem2_ack;

    this->io.SIO_STAT.dsr_input = new_ack;
    update_IRQs(bus);
}


static void get_select_port(struct PS1 *bus, u32 num, struct PS1_SIO0_memport *port)
{
    port->controller = NULL;
    port->memcard = NULL;
    switch(num) {
        case 0: // controller1, mem1
            port->controller = bus->sio0.io.controller1;
            port->memcard = bus->sio0.io.mem1;
            break;
        case 1:
            port->controller = bus->sio0.io.controller2;
            port->memcard = bus->sio0.io.mem2;
            break;
    }
}

static void send_DTR(struct PS1 *this, u32 port, u32 level)
{
    struct PS1_SIO0_memport p;
    get_select_port(this, port, &p);
    if (p.memcard) p.memcard->set_CS(p.memcard->device_ptr, level, PS1_clock_current(this));
    if (p.controller) p.controller->set_CS(p.controller->device_ptr, level, PS1_clock_current(this));
}

static void write_ctrl(struct PS1 *this, u32 sz, u32 val)
{
    printif(ps1.sio0.rw, "\nport: SIO0 WRITE CTRL %04x", val);
    u32 old_rx_enable = this->sio0.io.SIO_CTRL.rx_enable;
    u32 old_dtr = this->sio0.io.SIO_CTRL.dtr_output;
    u32 old_select = this->sio0.io.SIO_CTRL.sio0_port_sel;

    val &= ~0b1010000;
    this->sio0.io.SIO_CTRL.u = val & 0xFFFF;

    if (this->sio0.io.SIO_CTRL.reset) {
        this->sio0.io.SIO_CTRL.u = 0;
        this->sio0.io.SIO_STAT.u = 0;
        this->sio0.io.SIO_MODE.u = 0;
        this->sio0.io.RX_FIFO.num = 0;
        this->sio0.io.RX_FIFO.head = this->sio0.io.RX_FIFO.tail = 0;
    }

    u32 new_dtr = this->sio0.io.SIO_CTRL.dtr_output;
    u32 new_select = this->sio0.io.SIO_CTRL.sio0_port_sel;

    if ((new_dtr != old_dtr) || (new_select != old_select)) {
        // If select changed, send DTR=0
        if (new_select != old_select) {
            send_DTR(this, old_select, 0);
        }

        send_DTR(this, new_select, new_dtr);
    }

    if (old_rx_enable && (!this->sio0.io.SIO_CTRL.rx_enable)) {
        // Clear FIFO
        this->sio0.io.RX_FIFO.head = this->sio0.io.RX_FIFO.tail = this->sio0.io.RX_FIFO.num = 0;
        memset(this->sio0.io.RX_FIFO.buf, 0, 8);
        printif(ps1.sio0.rw, "\nCLEAR FIFO!");
    }

    if (this->sio0.io.SIO_CTRL.ack) {
        this->sio0.io.SIO_CTRL.ack = 0;
        printif(ps1.sio0.rw, "\nprogram: SIO0 ACK!");
        // 3, 4, 5, 9
        this->sio0.io.SIO_STAT.rx_parity_error = 0;
        this->sio0.io.SIO_STAT._unused1 &= 0b100;
        this->sio0.io.SIO_STAT.irq_request = 0; // This may be set back to 1 in update_IRQs() if the IRQs are not disabled!
        PS1_set_irq(this, PS1IRQ_SIO0, 0); // so that it can flip back to 1 and trigger again in update_IRQs() if necessary
    }

    update_rx_signal(this);
    update_IRQs(this);
}

static void write_mode(struct PS1 *this, u32 sz, u32 val)
{
    this->sio0.io.SIO_MODE.u = val & 0xFFFF;
}

static void write_stat(struct PS1 *this, u32 sz, u32 val)
{
    // read-only! :-D
}

static u8 do_exchange_byte(struct PS1 *this, u8 tx_byte)
{
    struct PS1_SIO0_memport port;
    get_select_port(this, this->sio0.io.SIO_CTRL.sio0_port_sel, &port);

    // Determine which port...
    u8 rx_byte = 0xFF;
    if (port.controller) {
        printif(ps1.sio0.rw, "\npad: XCHG!");
        rx_byte &= port.controller->exchange_byte(port.controller->device_ptr, tx_byte, PS1_clock_current(this));
    }
    if (port.memcard) {
        printif(ps1.sio0.rw, "\nMemcard XCHG!");
        rx_byte &= port.memcard->exchange_byte(port.memcard->device_ptr, tx_byte, PS1_clock_current(this));
    }

    printif(ps1.sio0.rw, "\nEXCH BYTE. TX:%02x, RX:%02x", tx_byte, rx_byte);

    return rx_byte;
}

static void pprint_fifo(struct PS1_SIO0_RX_FIFO *this)
{
    dbg_printf("\n\nFIFO! %d", this->num);
    for (u32 i = 0; i < 7; i++) {
        u32 num = i;
        dbg_printf("\n");
        if (num == this->head) dbg_printf("(head) ");
        else if (num == this->tail) dbg_printf("(tail) ");
        else dbg_printf("       ");
        dbg_printf("%02x", this->buf[num]);
    }
}


static void push_rx_FIFO(struct PS1_SIO0_RX_FIFO *this, u8 byte)
{
    printif(ps1.sio0.rw, "\nPush %02x to FIFO!", byte);
    if (this->num == 8) {
        printif(ps1.sio0.rw, "\nWARNING SIO0 RX FIFO OVERFLOW");
        this->tail = (this->tail - 1) & 7;
        this->num--;
    }

    //u32 num = this->tail;
    this->buf[this->tail] = byte;
    //num++;
    //while((num & 7) != this->head) {
    //    this->buf[(num++) & 7] = byte;
    //}

    this->num++;
    this->tail = (this->tail + 1) & 7;
    //pprint_fifo(this);
}

static void scheduled_exchange_byte(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct PS1 *this = (struct PS1 *)ptr;

    // Check which port
    //printf("\ncyc:%lld do byte exchange. RX ENABLE: %d", clock, this->sio0.io.SIO_CTRL.rx_enable);
    u8 inbyte = do_exchange_byte(this, key);

    if (this->sio0.io.SIO_CTRL.rx_enable || this->sio0.io.SIO_CTRL.dtr_output) {
        // Push to FIFO
        //printf("\nPUSH TO FIFO!");
        push_rx_FIFO(&this->sio0.io.RX_FIFO, inbyte);

        // Trigger IRQ if necessary
        update_rx_signal(this);
        update_IRQs(this);
    }

    // Set SIO_STAT bit
    this->sio0.io.SIO_STAT.tx_idle = 1;
    this->sio0.io.SIO_STAT.rx_fifo_not_empty = this->sio0.io.RX_FIFO.num != 0;
}

static void write_tx_data(struct PS1 *this, u32 sz, u32 val)
{
    if (!this->sio0.io.SIO_CTRL.tx_enable) return;
    // schedule exchange_byte() for 1023 cycles out!
    this->sio0.io.SIO_STAT.tx_fifo_not_full = 1;
    this->sio0.io.SIO_STAT.tx_idle = 0;
    printif(ps1.sio0.rw, "\nprogram: write tx %02x. Schedule exch. byte @ %lld", val & 0xFF, PS1_clock_current(this) + 1023);

    if (this->sio0.still_sched) {
        printf("\nWARNING MULTIPLE EXCH BYTE SCHEDULED!?");
    }
    this->sio0.sch_id = scheduler_add_or_run_abs(&this->scheduler, PS1_clock_current(this) + (this->sio0.io.baud * 8), val & 0xFF, this, &scheduled_exchange_byte, &this->sio0.still_sched);
}

void PS1_SIO0_write(struct PS1 *this, u32 addr, u32 sz, u32 val)
{
#define R_RX_DATA 0x1F801040
#define R_TX_DATA R_RX_DATA
#define R_SIO_STAT 0x1F801044
#define R_SIO_MODE 0x1F801048
#define R_SIO_CTRL 0x1F80104A
#define R_SIO_MISC 0x1F80104C
#define R_SIO_BAUD 0x1F80104E
    switch(addr) {
        case R_SIO_CTRL:
            assert(sz==2);
            write_ctrl(this, sz, val);
            return;
        case R_SIO_MODE:
            assert(sz==2);
            write_mode(this, sz, val);
            return;
        case R_SIO_STAT:
            write_stat(this, sz, val);
            return;
        case R_TX_DATA:
            write_tx_data(this, sz, val);
            return;
        case R_SIO_MISC:
            assert(sz==2);
            this->sio0.io.misc = val;
            return;
        case R_SIO_BAUD:
            assert(sz==2);
            this->sio0.io.baud = val & 0xFFFF;
            return;
    }
    printf("\nUnhandled SIO write to %08x (%d): %08x", addr, sz, val);
}

static u32 read_ctrl(struct PS1 *this, u32 sz)
{
    return this->sio0.io.SIO_CTRL.u;
}

static u32 read_mode(struct PS1 *this, u32 sz)
{
    return this->sio0.io.SIO_MODE.u;
}

static u32 read_stat(struct PS1 *this, u32 sz)
{
    printf("\nREAD STAT:%04x", this->sio0.io.SIO_STAT.u);
    return this->sio0.io.SIO_STAT.u;
}


static u32 read_rx_data(struct PS1 *this, u32 sz)
{
    // POP a value from FIFO
    u32 out_val = this->sio0.io.RX_FIFO.buf[this->sio0.io.RX_FIFO.head];
    u32 num = (this->sio0.io.RX_FIFO.head + 1) & 7;

    if (this->sio0.io.RX_FIFO.num > 0) {
        this->sio0.io.RX_FIFO.head = num;

        this->sio0.io.RX_FIFO.num--;
    }
    printif(ps1.sio0.rw, "\nport: read RX. pop %02x from FIFO now at len:%d!", out_val, this->sio0.io.RX_FIFO.num);

    // Now preview the next 3...
    for (u32 i = 1; i < sz; i++) {
        out_val |= this->sio0.io.RX_FIFO.buf[num] << (8 * i);
        num = (num + 1) & 7;
    }

    // Now set the bit...
    this->sio0.io.SIO_STAT.rx_fifo_not_empty = this->sio0.io.RX_FIFO.num != 0;

    update_rx_signal(this);
    update_IRQs(this);
    printif(ps1.sio0.rw, "\nprogram: read RX data %02x", out_val);
    //pprint_fifo(&this->sio0.io.RX_FIFO);
    return out_val;
}

u32 PS1_SIO0_read(struct PS1 *this, u32 addr, u32 sz)
{
    switch(addr) {
        case R_SIO_CTRL:
            assert(sz==2);
            return read_ctrl(this, sz);
        case R_SIO_MODE:
            assert(sz==2);
            return read_mode(this, sz);
        case R_SIO_STAT:
            return read_stat(this, sz);
        case R_RX_DATA:
            return read_rx_data(this, sz);
        case R_SIO_MISC:
            assert(sz==2);
            return this->sio0.io.misc;
        case R_SIO_BAUD:
            assert(sz==2);
            return this->sio0.io.baud;
    }
    printf("\nUnhandled SIO read from %08x (%d)", addr, sz);
    return 0;
}
