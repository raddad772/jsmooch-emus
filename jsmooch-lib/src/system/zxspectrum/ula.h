//
// Created by . on 5/2/24.
//

#ifndef JSMOOCH_EMUS_ULA_H
#define JSMOOCH_EMUS_ULA_H

#include "helpers/int.h"


enum ZXSpectrum_variants {
    ZXS_spectrum48,
    ZXS_spectrum128,
    ZXS_spectrumplus2,
    ZXS_spectrumplus3
};

struct ZXSpectrum;

struct ZXSpectrum_ULA {
    void (*scanline_func)(struct ZXSpectrum*);
    struct physical_io_device* display;
    struct cvec* keyboard_devices;
    u32 keyboard_device_index;
    enum ZXSpectrum_variants variant;

    u32 first_vblank;
    i32 screen_x;
    i32 screen_y;
    u8* cur_output;

    u32 bg_shift;
    u32 next_bg_shift;
    struct {
        u32 colors[2];
        u32 flash;
    } attr;
    u32 next_attr;

    struct {
        u32 border_color;
    } io;
};

void ZXSpectrum_ULA_init(struct ZXSpectrum* bus, enum ZXSpectrum_variants variant);
void ZXSpectrum_ULA_delete(struct ZXSpectrum_ULA*);

void ZXSpectrum_ULA_reset(struct ZXSpectrum* bus);
void ZXSpectrum_ULA_cycle(struct ZXSpectrum* bus);

void ZXSpectrum_ULA_reg_write(struct ZXSpectrum* bus, u32 addr, u32 val);
u32 ZXSpectrum_ULA_reg_read(struct ZXSpectrum* bus, u32 addr);

#endif //JSMOOCH_EMUS_ULA_H
