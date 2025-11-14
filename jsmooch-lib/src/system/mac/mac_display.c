//
// Created by . on 7/25/24.
//

#include <cstdio>

#include "mac_display.h"
#include "mac_internal.h"


#define SCREEN_WIDTH 512
#define SCREEN_HEIGHT 342

static void scanline_visible(mac* this)
{
    if (this->clock.crt.hpos == 512)  {
        // TODO: hblank
    }
    if (this->clock.crt.hpos >= 512) return;
    if ((this->clock.crt.hpos & 15) == 0) {
        this->display.output_shifter = this->RAM[this->display.read_addr];
        this->display.read_addr++;
        this->ram_contended = 1;
    }
    else this->ram_contended = 0;
    for (u32 i = 0; i < 2; i++) {
        *this->display.cur_output = (this->display.output_shifter >> 15) & 1;
        this->display.cur_output++;
        this->display.output_shifter <<= 1;
    }
}

static void scanline_vblank(mac* this)
{
}

static void new_frame(mac* this)
{
    this->clock.master_frame++;
    this->clock.crt.vpos = 0;
    this->display.crt->scan_x = this->display.crt->scan_y = 0;
    this->display.scanline_func = &scanline_visible;
    // Swap buffer we're drawing to...
    this->display.cur_output = this->display.crt->output[this->display.crt->last_written];
    this->display.crt->last_written ^= 1;
}
// & 0x40

// change bit 14 to be same as reg2 bit6
// ((& 0x40) ^ 0x40) << 8
//

#define DISPLAY_BASE (0x12700 >> 1)
#define SOUND_MAIN (0x1FDOO >> 1)
#define SOUND_ALTERNATE (0x1A100 >> 1)

#define MAC128K_MAIN_BUF (0x1A700 >> 1)
#define MAC128K_ALTERNATE_BUF (0x12700 >> 1)
#define DISPLAY_BASE (0x12700 >> 1)
#define MAC512K_OFFSET (0x60000 >> 1)

static void calc_display_addr(mac* this)
{
    u32 base_addr = DISPLAY_BASE | (0x40 << 8);//| ((this->via.regs.ORB & 0x40) << 8);

    if (this->kind == mac512k) base_addr += MAC512K_OFFSET;

    // we need xpos/16    so >> 4. because each word is 16 pixels
    // and ypos * 32 so << 5       because each line is 32 words
    this->display.read_addr = base_addr +
                              (this->clock.crt.hpos >> 4) + // current X location
                              (this->clock.crt.vpos << 5);  // current y position
}

static void update_irqs(mac* this)
{
    this->display.IRQ_out = this->display.IRQ_signal;
    if (this->display.IRQ_out) {
        this->via.regs.IFR |= 2;
    }
}

static void new_scanline(mac* this)
{
    this->clock.crt.hpos = 0;
    this->display.crt->scan_x = 0;
    this->display.crt->scan_y++;
    this->clock.crt.vpos++;

    switch(this->clock.crt.vpos) {
        case 342: // vblank start
            this->display.IRQ_signal = 1;
            update_irqs(this);
            this->display.scanline_func = &scanline_vblank;
            break;
        case 370: // vblank end, new frame
            this->display.IRQ_signal = 0;
            update_irqs(this);
            new_frame(this);
            this->display.scanline_func = &scanline_visible;
            break;
    }

    calc_display_addr(this);
}

void mac_step_display2(mac* this)
{
    // Draw two pixels
    /*
     * Frame timings are...
     * 512 x 342 visible pixels.  +192 hblank pixels = 704 total
     * 28 vblank lines  = 370 total. = 260480 per frame
     */

    this->display.scanline_func(this);

    this->display.crt->scan_x += 2;
    this->clock.crt.hpos += 2;
    if (this->clock.crt.hpos >= 704) new_scanline(this);
}

u32 mac_display_in_hblank(mac* this)
{
    return this->clock.crt.hpos > 512;
}

void mac_display_reset(mac* this)
{
    /*
Macintosh 128K, the main screen buffer starts at $1A700 and the alternate buffer starts at
$12700; for a 512K Macintosh, add $60000 to these numbers.
     */
    calc_display_addr(this);
    this->display.crt->scan_x = this->display.crt->scan_y = 0;
    this->display.cur_output = this->display.crt->output[this->display.crt->last_written];
    this->display.crt->last_written ^= 1;
    this->clock.crt.hpos = this->clock.crt.vpos = 0;
    this->display.scanline_func = &scanline_visible;
}