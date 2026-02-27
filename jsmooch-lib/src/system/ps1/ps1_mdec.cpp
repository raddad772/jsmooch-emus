//
// Created by . on 2/19/26.
//

// I followed Ares pretty closely on this one, thanks Ares!

#include "ps1_mdec.h"

#include "ps1_mdec.h"
#include "ps1_bus.h"

namespace PS1 {


void GFIFOUT::push(u32 val) {
    words[tail] = val;
    if (len >= MDEC_NUMHWORDS_OUT) {
        printf("\n(MDEC FIFO OUT) WARN PUSH TO FULL FIFO!");
        ::dbg_break("MDEC FIFO FULL", 0);
        return;
    }
    tail = (tail + 1) & (MDEC_NUMHWORDS_OUT-1);
    len++;
}

u32 GFIFOUT::pop() {
    u32 v = words[head];
    if (len > 0) {
        head = (head + 1) % MDEC_NUMHWORDS_OUT;
        len--;
    }
    return v;
}

void GFIFOUT::reset() {
    head = tail = len = 0;
}

void GFIFOIN::push(u32 val) {
    if (len >= MDEC_NUMHWORDS_IN) {
        printf("\n(MDEC FIFO IN) WARN PUSH TO FULL FIFO!");
        ::dbg_break("MDEC FIFO_IN FULL", 0);
        return;
    }
    words[tail] = val;
    if (len < MDEC_NUMHWORDS_IN) {
        tail = (tail + 1) % MDEC_NUMHWORDS_IN;
        len++;
    }
}

u32 GFIFOIN::pop() {
    u32 v = words[head];
    if (len > 0) {
        head = (head + 1) % MDEC_NUMHWORDS_IN;
        len--;
    }
    return v;
}

void GFIFOIN::reset() {
    head = tail = len = 0;
}
    static inline u32 BGR24to15(u32 c)
{
    return (((c >> 19) & 0x1F) << 10) |
           (((c >> 11) & 0x1F) << 5) |
           ((c >> 3) & 0x1F);
}

#define CLAMP(x, low, high) ((x) < (low) ? (low) : ((x) > (high) ? (high) : (x)))

static constexpr u8 zigzag[64] = {
    0,  1,  5,  6, 14, 15, 27, 28,
    2,  4,  7, 13, 16, 26, 29, 42,
    3,  8, 12, 17, 25, 30, 41, 43,
    9, 11, 18, 24, 31, 40, 44, 53,
   10, 19, 23, 32, 39, 45, 52, 54,
   20, 22, 33, 38, 46, 51, 55, 60,
   21, 34, 37, 47, 50, 56, 59, 61,
   35, 36, 48, 49, 57, 58, 62, 63,
 };

static constexpr u8 zagzig[64] = {
        0,  1,  8, 16,  9,  2,  3, 10,
       17, 24, 32, 25, 18, 11,  4,  5,
       12, 19, 26, 33, 40, 48, 41, 34,
       27, 20, 13,  6,  7, 14, 21, 28,
       35, 42, 49, 56, 57, 50, 43, 36,
       29, 22, 15, 23, 30, 37, 44, 51,
       58, 59, 52, 45, 38, 31, 39, 46,
       53, 60, 61, 54, 47, 55, 62, 63,
     };

void MDEC::decodeIDCT0(i16 *source, i16 *target) {
    for (u32 x = 0; x < 8; x++) {
        for (u32 y = 0; y < 8; y++) {
            i32 sum = 0;
            for (u32 z = 0; z < 8; z++) {
                sum += source[y + z * 8] * BLOCK.scale[x + z * 8];
            }
            target[x + y * 8] = static_cast<i16>((sum + 0x8000) >> 16);
        }
    }
}

void MDEC::decodeIDCT1(i16 *source, i16 *target) {
    for (u32 x = 0; x < 8; x++) {
        for (u32 y = 0; y < 8; y++) {
            i32 sum = 0;
            for (u32 z = 0; z < 8; z++) {
                sum += source[y + z * 8] * BLOCK.scale[x + z * 8];
            }
            i32 r = (static_cast<i32>(((sum + 0x8000) >> 16) & 0x1FF) << 23) >> 23;
            r = CLAMP(r, -128, 127);
            target[x + y * 8] = r;
        }
    }
}

void MDEC::convert_y(u32 *out, i16 *luma) {
    for (u32 y = 0; y < 8; y++) {
        for (u32 x = 0 ; x < 8; x++) {
            i16 Y = static_cast<i16>((static_cast<i32>(luma[x + y * 8]) << 22) >> 22);
            Y += 128;
            Y = CLAMP(Y, 0, 255);
            out[x + y * 8] = Y;
        }
    }
}

void MDEC::convert_yuv(u32 *out, i16 *luma, u32 bx, u32 by) {
    for(u32 y = 0; y < 8; y++) {
        for(u32 x = 0; x < 8; x++) {
            i16 Y  = luma[x + y * 8];
            i16 Cb = BLOCK.cb[((x + bx) >> 1) + ((y + by) >> 1) * 8];
            i16 Cr = BLOCK.cr[((x + bx) >> 1) + ((y + by) >> 1) * 8];

            s32 R = Y + (1.402 * Cr);
            s32 G = Y - (0.334 * Cb) - (0.714 * Cr);
            s32 B = Y + (1.722 * Cb);

            R += 128;
            G += 128;
            B += 128;
            R = CLAMP(R, 0, 255);
            G = CLAMP(G, 0, 255);
            B = CLAMP(B, 0, 255);

            out[(x + bx) + (y + by) * 16] = R << 0 | G << 8 | B << 16;
        }
    }

}

bool MDEC::decode_block(i16 *block, u8 *table) {
    memset(block, 0, sizeof(*block) * 64);
    if (fifo_in.len == 0) return false;
    u16 dct = fifo_in.pop();
    while (dct == 0xfe00) { // Pad-words!
        if (fifo_in.len == 0) return false;
        dct = fifo_in.pop();
    }
    i32 current = (static_cast<i32>(dct) << 22) >> 22;
    u16 qfactor = dct >> 10;
    i32 value = current * table[0];

    for (u32 n = 0; n < 64;) {
        if (qfactor == 0) value = current << 1;
        value = CLAMP(value, -1024, 1023);
        if (qfactor > 0) {
            block[zagzig[n]] = value;
        }
        else if (qfactor == 0) {
            block[n] = value;
        }
        if (fifo_in.len == 0) return false;
        u16 rle = fifo_in.pop();
        current = (static_cast<i32>(rle) << 22) >> 22;
        n += (rle >> 10) + 1;
        if (n >= 64) break;

        value = (current * table[n] * qfactor + 4) >> 3;
    }

    i16 array[64];
    decodeIDCT0(block, array);
    decodeIDCT1(array, block);
    return true;
}

void MDEC::do_decode() {
    while (fifo_in.len > 0) {
        if (io.stat.output_depth <= 1) {
            if (!decode_block(BLOCK.y0, BLOCK.luma)) return;
            convert_y(output, BLOCK.y0);
        }
        else {
            if (!decode_block(BLOCK.cr, BLOCK.chroma)) return;
            if (!decode_block(BLOCK.cb, BLOCK.chroma)) return;
            if (!decode_block(BLOCK.y0, BLOCK.luma)) return;
            if (!decode_block(BLOCK.y1, BLOCK.luma)) return;
            if (!decode_block(BLOCK.y2, BLOCK.luma)) return;
            if (!decode_block(BLOCK.y3, BLOCK.luma)) return;

            convert_yuv(output, BLOCK.y0, 0, 0);
            convert_yuv(output, BLOCK.y1, 8, 0);
            convert_yuv(output, BLOCK.y2, 0, 8);
            convert_yuv(output, BLOCK.y3, 8, 8);
        }
        // 4-bit output
        if (io.stat.output_depth == 0) {
            for(u32 index = 0; index < 64; index += 8) {
                u32 a = (output[index + 0] >> 4) <<  0;
                u32 b = (output[index + 1] >> 4) <<  4;
                u32 c = (output[index + 2] >> 4) <<  8;
                u32 d = (output[index + 3] >> 4) << 12;
                u32 e = (output[index + 4] >> 4) << 16;
                u32 f = (output[index + 5] >> 4) << 20;
                u32 g = (output[index + 6] >> 4) << 24;
                u32 h = (output[index + 7] >> 4) << 28;
                fifo_out.push(a | b | c | d | e | f | g | h);
            }
        }
        // 8-bit output
        if (io.stat.output_depth == 1) {
            for(u32 index = 0; index < 64; index += 4) {
                u32 a = output[index + 0] <<  0;
                u32 b = output[index + 1] <<  8;
                u32 c = output[index + 2] << 16;
                u32 d = output[index + 3] << 24;
                fifo_out.push(a | b | c | d);
            }
        }

        // 15-bit output
        if (io.stat.output_depth == 3) {
            for(u32 index = 0; index < 256; index += 2) {
                u32 a = BGR24to15(output[index + 0]) | (io.stat.output_mask_bit << 15);
                u32 b = (BGR24to15(output[index + 1]) << 16) | (io.stat.output_mask_bit << 15);
                fifo_out.push(a | b);
            }
        }

        // 24-bit output
        if (io.stat.output_depth == 2) {
            u32 index = 0;
            u32 state = 0;
            u32 rgb = 0;
            while(index < 256) {
                switch(state) {
                    case 0:
                        rgb = output[index++];
                        break;
                    case 1:
                        rgb |= output[index] << 24;
                        fifo_out.push(rgb);
                        rgb = output[index++] >> 8;
                        break;
                    case 2:
                        rgb |= output[index] << 16;
                        fifo_out.push(rgb);
                        rgb = output[index++] >> 16;
                        break;
                    case 3:
                        rgb |= output[index++] << 8;
                        fifo_out.push(rgb);
                        break;
                }
                state = state + 1 & 3;
            }
        }
    }
    printf("\nDATA LEFT: %d", fifo_in.len);
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
                BLOCK.luma[io.offset++ & 63] = val & 0xFF;
                BLOCK.luma[io.offset++ & 63] = (val >> 8) & 0xFF;
                BLOCK.luma[io.offset++ & 63] = (val >> 16) & 0xFF;
                BLOCK.luma[io.offset++ & 63] = (val >> 24) & 0xFF;
            }
            else {
                BLOCK.chroma[io.offset++ & 63] = val & 0xFF;
                BLOCK.chroma[io.offset++ & 63] = (val >> 8) & 0xFF;
                BLOCK.chroma[io.offset++ & 63] = (val >> 16) & 0xFF;
                BLOCK.chroma[io.offset++ & 63] = (val >> 24) & 0xFF;
            }
            io.num_remaining_param_words -= 2;
            if (io.num_remaining_param_words == 0) {
                io.mode = MM_Idle;
            }
            break;
        case MM_SetScaleTable:
            BLOCK.scale[io.offset++ & 63] = val & 0xFFFF;
            BLOCK.scale[io.offset++ & 63] = (val >> 16) & 0xFFFF;
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