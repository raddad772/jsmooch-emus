//
// Created by . on 2/26/25.
//

#include <cstring>
#include "ps1_sio.h"
#include "../ps1_bus.h"
#include <cassert>

#include "system/ps1/ps1_debugger.h"

namespace PS1 {

}

namespace PS1::SIO {
SIO0::SIO0(PS1::core *parent) : bus(parent)

{
    io.SIO_STAT.tx_fifo_not_full = 1;
    io.SIO_STAT.tx_idle = 1;
}

void SIO0::update_rx_signal()
{
    static constexpr u8 num[4] = { 1, 2, 4, 8};
    irq.rx_signal = io.RX_FIFO.num >= num[io.SIO_CTRL.rx_irq_mode];
    io.SIO_STAT.rx_fifo_not_empty = io.RX_FIFO.num > 0;
}

void SIO0::update_IRQs()
{
    printif(ps1.sio0.irq, "\nport: update IRQs. RX.e:%d RX.s:%d DSR.e:%d DSR.s:%d", io.SIO_CTRL.rx_irq_enable, irq.rx_signal, io.SIO_CTRL.dsr_irq_enable, io.SIO_STAT.dsr_input);
    u32 old_signal = io.SIO_STAT.irq_request;
    u32 signal = io.SIO_CTRL.rx_irq_enable && irq.rx_signal;
    signal |= io.SIO_CTRL.tx_irq_enable && irq.tx_signal;
    signal |= io.SIO_CTRL.dsr_irq_enable && io.SIO_STAT.dsr_input;

    io.SIO_STAT.irq_request |= signal; // It is only SET by these, not un-set
    printif(ps1.sio0.irq, "\nport: old IRQ signal:%d new signal:%d . CPU IMASK %08x", old_signal, io.SIO_STAT.irq_request, bus->cpu.io.I_MASK);
    if (!old_signal && io.SIO_STAT.irq_request) {
        printif(ps1.sio0.irq, "\nSIO0 IRQ 0->1");
    }
    bus->set_irq(IRQ_SIO0, signal);
}


void SIO0::update_ACKs(SIO0_device port, u32 level)
{
    u32 cont1_ack =0, cont2_ack = 0, mem1_ack = 0, mem2_ack = 0;
    if (io.controller1) cont1_ack = io.controller1->ACK;
    if (io.controller2) cont2_ack = io.controller2->ACK;
    if (io.mem1) mem1_ack = io.mem1->ACK;
    if (io.mem2) mem2_ack = io.mem2->ACK;

    switch(port) {
        case SIO0_controller1:
            io.controller1->ACK = level;
            cont1_ack = level;
            break;
        case SIO0_controller2:
            io.controller2->ACK = level;
            cont2_ack = level;
            break;
        case SIO0_mem1:
            io.mem1->ACK = level;
            cont2_ack = level;
            break;
        case SIO0_mem2:
            io.mem2->ACK = level;
            cont2_ack = level;
            break;
    }

    u32 new_ack = cont1_ack | cont2_ack | mem1_ack | mem2_ack;

    io.SIO_STAT.dsr_input = new_ack;
    update_IRQs();
}


void SIO0::get_select_port(u32 num, memport *port) const
{
    port->controller = nullptr;
    port->memcard = nullptr;
    switch(num) {
        case 0: // controller1, mem1
            port->controller = io.controller1;
            port->memcard = io.mem1;
            break;
        case 1:
            port->controller = io.controller2;
            port->memcard = io.mem2;
            break;
        default:
            NOGOHERE;
    }
}

void SIO0::send_DTR(u32 port, u32 level)
{
    memport p{};
    get_select_port(port, &p);
    //printf("\nSend DTR port:%d level:%d", port, level);
    if (p.memcard) p.memcard->set_CS(p.memcard->device_ptr, level, bus->clock_current());
    if (p.controller) p.controller->set_CS(p.controller->device_ptr, level, bus->clock_current());
}

void SIO0::write_ctrl(u8 sz, u32 val)
{
    //printf("\nTHING:%d  TRACES:%d", ::dbg.traces.ps1.sio0.rw, ::dbg.trace_on);
    printif(ps1.sio0.rw, "\n\nport: SIO0 WRITE CTRL %04x", val);
    bus->dbg.dvptr->add_printf(PS1D_R3000_RFE, bus->clock.master_cycle_count, DBGLS_TRACE, "SIO0 WRITE CTRL %04x", val);
    u32 old_rx_enable = io.SIO_CTRL.rx_enable;
    u32 old_dtr = io.SIO_CTRL.dtr_output;
    u32 old_select = io.SIO_CTRL.sio0_port_sel;

    val &= ~0b1010000;
    io.SIO_CTRL.u = val & 0xFFFF;

    if (io.SIO_CTRL.reset) {
        io.SIO_CTRL.u = 0;
        io.SIO_STAT.u = 0;
        io.SIO_MODE.u = 0;
        io.RX_FIFO.num = 0;
        io.RX_FIFO.head = io.RX_FIFO.tail = 0;
    }

    u32 new_dtr = io.SIO_CTRL.dtr_output;
    u32 new_select = io.SIO_CTRL.sio0_port_sel;
    //printf("\nWRITTEN DTR:%d  CS:%d .   old_DTR:%d  old_CS:%d", new_dtr, new_select, old_dtr, old_select);

    if ((new_dtr != old_dtr) || (new_select != old_select)) {
        // If select changed, send DTR=0
        if (new_select != old_select) {
            //printf("\nNEW!=OLD so sending DTR:%d CS:0", old_select);
            send_DTR(old_select, 0);
        }
        //printf("\nsending DTR:%d CS:%d", new_dtr, new_select);
        send_DTR(new_select, new_dtr);
    }

    if (old_rx_enable && (!io.SIO_CTRL.rx_enable)) {
        // Clear FIFO
        io.RX_FIFO.head = io.RX_FIFO.tail = io.RX_FIFO.num = 0;
        memset(io.RX_FIFO.buf, 0, 8);
        printif(ps1.sio0.rw, "\nCLEAR FIFO!");
    }

    if (io.SIO_CTRL.ack) {
        io.SIO_CTRL.ack = 0;
        printif(ps1.sio0.rw, "\nprogram: SIO0 ACK!");
        // 3, 4, 5, 9
        io.SIO_STAT.rx_parity_error = 0;
        io.SIO_STAT._unused1 &= 0b100;
        io.SIO_STAT.irq_request = 0; // This may be set back to 1 in update_IRQs() if the IRQs are not disabled!
        bus->set_irq(IRQ_SIO0, 0); // so that it can flip back to 1 and trigger again in update_IRQs() if necessary
    }

    update_rx_signal();
    update_IRQs();
}

void SIO0::write_mode(u8 sz, u32 val)
{
    io.SIO_MODE.u = val & 0x13F;
    printif(ps1.sio0.rw, "\nMODE: %02x", io.SIO_MODE.u);
}

void SIO0::write_stat(u8 sz, u32 val)
{
    // read-only! :-D
}

u8 SIO0::do_exchange_byte(u8 tx_byte)
{
    memport port{};
    get_select_port(io.SIO_CTRL.sio0_port_sel, &port);

    // Determine which port...
    u8 rx_byte = 0xFF;
    if (port.controller) {
        printif(ps1.sio0.rw, "\npad: XCHG!");
        rx_byte &= port.controller->exchange_byte(port.controller->device_ptr, tx_byte, bus->clock_current());
    }
    if (port.memcard) {
        printif(ps1.sio0.rw, "\nMemcard XCHG!");
        rx_byte &= port.memcard->exchange_byte(port.memcard->device_ptr, tx_byte, bus->clock_current());
    }

    printif(ps1.sio0.rw, "\nEXCH BYTE. TX:%02x, RX:%02x", tx_byte, rx_byte);

    return rx_byte;
}

void SIO0_RX_FIFO::pprint()
{
    dbg_printf("\n\nFIFO! %d", num);
    for (u32 i = 0; i < 7; i++) {
        u32 mnum = i;
        dbg_printf("\n");
        if (mnum == head) dbg_printf("(head) ");
        else if (mnum == tail) dbg_printf("(tail) ");
        else dbg_printf("       ");
        dbg_printf("%02x", buf[mnum]);
    }
}


void SIO0_RX_FIFO::push(u8 byte)
{
    printif(ps1.sio0.rw, "\nPush %02x to FIFO!", byte);
    if (num == 8) {
        printif(ps1.sio0.rw, "\nWARNING SIO0 RX FIFO OVERFLOW");
        tail = (tail - 1) & 7;
        num--;
    }

    //u32 num = tail;
    buf[tail] = byte;
    //num++;
    //while((num & 7) != head) {
    //    buf[(num++) & 7] = byte;
    //}

    num++;
    tail = (tail + 1) & 7;
    //pprint_fifo();
}

void scheduled_exchange_byte(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<SIO0 *>(ptr);

    // Check which port
    //printf("\ncyc:%lld do byte exchange. RX ENABLE: %d", clock, io.SIO_CTRL.rx_enable);
    u8 inbyte = th->do_exchange_byte(key);

    if (th->io.SIO_CTRL.rx_enable || th->io.SIO_CTRL.dtr_output) {
        // Push to FIFO
        //printf("\nPUSH TO FIFO!");
        th->io.RX_FIFO.push(inbyte);

        // Trigger IRQ if necessary
        th->update_rx_signal();
        th->update_IRQs();
    }

    // Set SIO_STAT bit
    th->io.SIO_STAT.tx_idle = 1;
    th->io.SIO_STAT.rx_fifo_not_empty = th->io.RX_FIFO.num != 0;
}

void SIO0::write_tx_data(u8 sz, u32 val)
{
    if (!io.SIO_CTRL.tx_enable) return;
    //printf("\nWRITE TX SZ:%d VAL:%02x", sz, val);
    // schedule exchange_byte() for 1023 cycles out!
    io.SIO_STAT.tx_fifo_not_full = 1;
    io.SIO_STAT.tx_idle = 0;
    printif(ps1.sio0.rw, "\nprogram: write tx %02x. Schedule exch. byte @ %lld", val & 0xFF, bus->clock_current() + 1023);

    if (still_sched) {
        printf("\nWARNING MULTIPLE EXCH BYTE SCHEDULED!?");
    }
    sch_id = bus->scheduler.add_or_run_abs(bus->clock_current() + (io.baud * 8), val & 0xFF, this, &scheduled_exchange_byte, &still_sched);
}

static constexpr u32 masksz[5] = {0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF};

void SIO0::write(u32 addr, u8 sz, u32 val)
{
#define R_RX_DATA 0x1F801040
#define R_TX_DATA R_RX_DATA
#define R_SIO_STAT 0x1F801044
#define R_SIO_MODE 0x1F801048
#define R_SIO_CTRL 0x1F80104A
#define R_SIO_MISC 0x1F80104C
#define R_SIO_BAUD 0x1F80104E
    //printf("\nWR SIO0 ADDR:%04x SZ:%d VAL:%02x", addr, sz, val);
    //val &= masksz[sz];
    switch(addr) {
        case R_SIO_CTRL:
            write_ctrl(sz, val);
            return;
        case R_SIO_MODE:
            write_mode(sz, val);
            return;
        case R_SIO_STAT:
            write_stat(sz, val);
            return;
        case R_TX_DATA:
            write_tx_data(sz, val);
            return;
        case R_SIO_MISC:
            io.misc = val;
            return;
        case R_SIO_BAUD:
            io.baud = val & 0xFFFF;
            printif(ps1.sio0.rw, "\n(SIO0) BAUD:%d", io.baud);
            return;
    }
    printf("\nUnhandled SIO write to %08x (%d): %08x", addr, sz, val);
}

u32 SIO0::read_ctrl(u8 sz) const
{
    return io.SIO_CTRL.u;
}

u32 SIO0::read_mode(u8 sz) const
{
    return io.SIO_MODE.u;
}

u32 SIO0::read_stat(u8 sz) const
{
    return io.SIO_STAT.u;
}


u32 SIO0::read_rx_data(u8 sz)
{
    // POP a value from FIFO
    u32 out_val = io.RX_FIFO.buf[io.RX_FIFO.head];
    u32 num = (io.RX_FIFO.head + 1) & 7;

    if (io.RX_FIFO.num > 0) {
        io.RX_FIFO.head = num;

        io.RX_FIFO.num--;
    }
    printif(ps1.sio0.rw, "\nport: read RX. pop %02x from FIFO now at len:%d!", out_val, io.RX_FIFO.num);

    // Now preview the next 3...
    for (u32 i = 1; i < sz; i++) {
        out_val |= io.RX_FIFO.buf[num] << (8 * i);
        num = (num + 1) & 7;
    }

    // Now set the bit...
    io.SIO_STAT.rx_fifo_not_empty = io.RX_FIFO.num != 0;

    update_rx_signal();
    update_IRQs();
    printif(ps1.sio0.rw, "\nprogram: read RX data %02x", out_val);
    //pprint_fifo(&io.RX_FIFO);
    return out_val;
}

u32 SIO0::read(u32 addr, u8 sz)
{
    //printf("\nRD SIO0 ADDR:%04x SZ:%d", addr, sz);
    switch(addr) {
        case R_SIO_CTRL:
            return read_ctrl(sz);
        case R_SIO_MODE:
            return read_mode(sz);
        case R_SIO_STAT:
            return read_stat(sz);
        case R_RX_DATA:
            return read_rx_data(sz);
        case R_SIO_MISC:
            return io.misc;
        case R_SIO_BAUD:
            return io.baud;
    }
    printf("\nUnhandled SIO read from %08x (%d)", addr, sz);
    return 0;
}

#undef R_RX_DATA
#undef R_TX_DATA
#undef R_SIO_STAT
#undef R_SIO_MODE
#undef R_SIO_CTRL
#undef R_SIO_MISC
#undef R_SIO_BAUD

#define R_RX_DATA 0x1F801050
#define R_TX_DATA R_RX_DATA
#define R_SIO_STAT 0x1F801054
#define R_SIO_MODE 0x1F801058
#define R_SIO_CTRL 0x1F80105A
#define R_SIO_MISC 0x1F80105C
#define R_SIO_BAUD 0x1F80105E

void SIO1::write(u32 addr, u8 sz, u32 val) {
    //printf("\nSIO1 WR ADDR:%04x SZ:%d VAL:%02x", addr, sz, val);
    val &= 0xFF;
    switch (addr) {
        case R_SIO_MODE:
            io.MODE.u = val & 0xFFFF;
            return;
    }
    printf("\nUnhandled SIO1 write to %08x (%d): %08x", addr, sz, val);
}

u32 SIO1::read(u32 addr, u8 sz) {
    printf("\nSIO1 RD %08x", addr);
    switch (addr) {
        case R_SIO_MODE:
            return io.MODE.u;
    }
    printf("\nUnhandled SIO1 read from %08x (%d)", addr, sz);
    return masksz[sz];
}
}