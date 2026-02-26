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
    switch (io.mode) {
        case MM_Idle:
            switch (val >> 29) {
            case 1:
                    io.mode = MM_DecodeMacroblock;
                    io.offset = 0;
                    io.num_remaining_param_words = (val & 0xFFFF) << 1;
                    if (io.num_remaining_param_words == 0) {
                        io.mode = MM_Idle;
                    }
                    break;
            case 2:
                    io.mode = MM_SetQuantTable;
                    io.offset = 0;
                    io.num_remaining_param_words = val & 1 ? 64 : 32;
                    break;
            case 3:
                    io.mode = MM_SetScaleTable;
                    io.offset = 0;
                    io.num_remaining_param_words = 64;
                    break;
            }
            if (io.mode != MM_Idle) {
                io.stat.output_mask_bit = (val >> 25) & 1;
                io.stat.output_sign = (val >> 26) & 1;
                io.stat.output_depth = (val >> 27) & 3;
                fifo_out.reset();
            }
            break;
        case MM_DecodeMacroblock:
            fifo_in.push(val & 0xFFFF);
            fifo_in.push(val >> 16);
            io.num_remaining_param_words -= 2;
            if (io.num_remaining_param_words == 0) {
                do_decode();
                io.mode = MM_Idle;
            }
            break;
        case MM_SetQuantTable:
            if (io.offset < 64) {
                block.luma[io.offset++ & 63] = val & 0xFF;
                block.luma[io.offset++ & 63] = (val >> 8) & 0xFF;
                block.luma[io.offset++ & 63] = (val >> 16) & 0xFF;
                block.luma[io.offset++ & 63] = (val >> 24) & 0xFF;
            }
            else {
                block.chroma[io.offset++ & 63] = val & 0xFF;
                block.chroma[io.offset++ & 63] = (val >> 8) & 0xFF;
                block.chroma[io.offset++ & 63] = (val >> 16) & 0xFF;
                block.chroma[io.offset++ & 63] = (val >> 24) & 0xFF;
            }
            io.num_remaining_param_words -= 2;
            if (io.num_remaining_param_words == 0) {
                io.mode = MM_Idle;
            }
            break;
        case MM_SetScaleTable:
            block.scale[io.offset++ & 63] = val & 0xFFFF;
            block.scale[io.offset++ & 63] = (val >> 16) & 0xFFFF;
            io.num_remaining_param_words -= 2;
            if (io.num_remaining_param_words == 0) {
                io.mode = MM_Idle;
            }
            break;
    }
}

u32 MDEC::read_data() {
    if (fifo_out.len == 0) {
        printf("\n(MDEC) read when output FIFO empty?");
    }
    return fifo_out.pop();
}

void MDEC::write_ctrl(u32 val) {
    if (val & 0x80000000) { // reset
        io.mode = MM_Idle;
        io.offset = 0;
        num_param_words = 0;
        io.stat.current_block = 4;
        io.stat.output_mask_bit = 0;
        io.stat.output_sign = 0;
        io.stat.output_depth = 0;
        io.stat.data_out_req = 0;
        io.stat.data_in_req = 0;
    }
    io.stat.data_in_req = (val >> 30) & 1;
    io.stat.data_out_req = (val >> 29) & 1;
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
    u32 o = ((io.num_remaining_param_words >> 1) - 1) & 0xFFFF;
    o |= io.stat.current_block << 16;
    o |= io.stat.output_mask_bit << 23;
    o |= io.stat.output_sign << 24;
    o |= io.stat.output_depth << 25;
    u32 dirq = io.stat.data_in_req && fifo_in.len == 0;
    u32 dorq = io.stat.data_out_req && fifo_out.len >= 32;
    o |= (dirq << 28) | (dorq << 27);
    o |= (io.mode != MM_Idle) << 29;
    o |= (fifo_in.len >= 64) << 30;
    o |= (fifo_out.len == 0) << 31;
    return o;
}


}