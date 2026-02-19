//
// Created by . on 2/19/26.
//

#include "ps1_mdec.h"

#include "ps1_mdec.h"
#include "ps1_bus.h"

namespace PS1 {

void MDEC::write_data(u32 val) {
    printf("\nMDEC write %08x", val);
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

u32 MDEC::read_ctrl() {
    printf("\nWARN MDEC read ctrl");
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