//
// Created by . on 2/19/26.
//

#include "ps1_mdec.h"

#include "ps1_mdec.h"
#include "ps1_bus.h"

namespace PS1 {


void GFIFOUT::push(u16 val) {
    if (len >= MDEC_NUMHWORDS_OUT) {
        printf("\n(MDEC FIFO OUT) WARN PUSH TO FULL FIFO!");
        return;
    }
    words[tail] = val;
    tail = (tail + 1) & (MDEC_NUMHWORDS_OUT-1);
    len++;
}

u16 GFIFOUT::pop() {
    u16 v = words[head];
    if (len > 0) {
        head = (head + 1) % MDEC_NUMHWORDS_OUT;
        len--;
    }
    return v;
}

void GFIFOUT::reset() {
    head = tail = len = 0;
}

void GFIFOIN::push(u16 val) {
    if (len >= MDEC_NUMHWORDS_IN) {
        printf("\n(MDEC FIFO IN) WARN PUSH TO FULL FIFO!");
        return;
    }
    words[tail] = val;
    tail = (tail + 1) & (MDEC_NUMHWORDS_IN-1);
    len++;
}

u16 GFIFOIN::pop() {
    u16 v = words[head];
    if (len > 0) {
        head = (head + 1) % MDEC_NUMHWORDS_IN;
        len--;
    }
    return v;
}

void GFIFOIN::reset() {
    head = tail = len = 0;
}

    
void MDEC::write_data(u32 val) {
    //printf("\nMDEC write %08x", val);
}

u32 MDEC::read_data() {
    printf("\nMDEC read");
    return 0;
}

void MDEC::write_ctrl(u32 val) {
    printf("\nMDEC write ctrl %08x", val);
    if (val & 0x80000000) abort();
    else {
        io.stat.data_in_req = (val >> 30) & 1;
        io.stat.data_out_req = (val >> 29) & 1;
    }

}

u32 MDEC::mainbus_read(u32 addr, u8 sz) {
    if (sz == 1) {
        return mainbus_read(addr, 4) >> 8 * (addr & 3);
    }
    if (sz == 2) {
        return mainbus_read(addr, 4) >> 8 * (addr & 3);
    }
    if (addr & 4) return read_ctrl();
    return read_data();
}

void MDEC::mainbus_write(u32 addr, u8 sz, u32 val) {
    if (sz == 1) {
        return mainbus_write(addr, 4, val << (8 * (addr & 3)));
    }
    if (sz == 2) {
        return mainbus_write(addr, 4, val << (8 * (addr & 3)));
    }
    if (addr & 4) write_ctrl(val);
    else write_data(val);
}


u32 MDEC::read_ctrl() {
    //printf("\nWARN MDEC read ctrl");
    u32 o = io.stat.u & 0b11100111111111111111111111111111;
    //  28    Data-In Request  (set when DMA0 enabled and ready to receive data)
    // 27    Data-Out Request (set when DMA1 enabled and ready to send data)
    u32 dirq = io.stat.data_in_req && bus->dma.channels[0].enable;
    u32 dorq = io.stat.data_out_req && bus->dma.channels[1].enable;
    o |= (dirq << 28) | (dorq << 27);
    return o;
}

void MDEC::abort() {
    printf("\nMDEC abort recv");
    io.stat.u = 0x80040000;
}

}