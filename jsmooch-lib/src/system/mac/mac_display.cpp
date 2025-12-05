//
// Created by . on 7/25/24.
//

#include <cstdio>

#include "mac_display.h"
#include "mac_bus.h"

namespace mac {

#define SCREEN_WIDTH 512
#define SCREEN_HEIGHT 342

void display::do_sound_and_pwm() {
    // TODO: alternate buffer
    // TODO: sound
    // $1fd00 / $1a100 on 128k
    // andd $60000 for 512k
    u16 v = pwm_base_addr + bus->clock.crt.vpos;
    u16 data = bus->RAM[v & bus->RAM_mask];
    bus->iwm.update_pwm(data & 0xFF);
}


void display::scanline_visible(core *bus)
{
    if ((bus->clock.crt.hpos & 15) == 0) {
        bus->display.output_shifter = bus->RAM[bus->display.read_addr];
        bus->display.read_addr++;
        bus->ram_contended = true;
    }
    else bus->ram_contended = false;
    if (bus->clock.crt.hpos >= 512) return;
    for (u32 i = 0; i < 2; i++) {
        *bus->display.cur_output = (bus->display.output_shifter >> 15) & 1;
        bus->display.cur_output++;
        bus->display.output_shifter <<= 1;
    }
}

void display::scanline_vblank(core* bus)
{
}

void display::new_frame()
{
    bus->clock.master_frame++;
    bus->clock.crt.vpos = 0;
    crt->scan_x = crt->scan_y = 0;
    scanline_func = &scanline_visible;
    // Swap buffer we're drawing to...
    cur_output = static_cast<u8 *>(crt->output[crt->last_written]);
    crt->last_written ^= 1;
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
#define MAC128K_AUDIO_BUF (0x1FD00 >> 1)
#define MAC128K_ALTERNATE_AUDIO_BUF (0x1A100 >> 1)
#define DISPLAY_BASE (0x12700 >> 1)
#define MAC512K_OFFSET (0x60000 >> 1)

void display::calc_display_addr()
{
    u32 base_addr = DISPLAY_BASE | (0x40 << 8);//| ((via.regs.ORB & 0x40) << 8);

    if (bus->kind == mac512k) base_addr += MAC512K_OFFSET;

    // we need xpos/16    so >> 4. because each word is 16 pixels
    // and ypos * 32 so << 5       because each line is 32 words
    read_addr = base_addr +
                              (bus->clock.crt.hpos >> 4) + // current X location
                              (bus->clock.crt.vpos << 5);  // current y position
}

void display::update_irqs()
{
    IRQ_out = IRQ_signal;
    if (IRQ_out) {
        bus->via.regs.IFR |= 2;
        bus->via.irq_sample();
    }
}

display::display(core* parent) : bus(parent)
{
    u32 base_addr = MAC128K_AUDIO_BUF;
    if (bus->kind == mac512k) base_addr += 0x6000;
    pwm_base_addr = base_addr;
}

void display::new_scanline()
{
    bus->clock.crt.hpos = 0;
    crt->scan_x = 0;
    crt->scan_y++;
    bus->clock.crt.vpos++;

    switch(bus->clock.crt.vpos) {
        case 342: // vblank start
            IRQ_signal = 1;
            update_irqs();
            scanline_func = &scanline_vblank;
            break;
        case 370: // vblank end, new frame
            IRQ_signal = 0;
            update_irqs();
            new_frame();
            scanline_func = &scanline_visible;
            break;
    }

    calc_display_addr();
}

void display::step2()
{
    // Draw two pixels
    /*
     * Frame timings are...
     * 512 x 342 visible pixels.  +192 hblank pixels = 704 total
     * 28 vblank lines  = 370 total. = 260480 per frame
     */

    scanline_func(bus);

    crt->scan_x += 2;
    bus->clock.crt.hpos += 2;
    if (bus->clock.crt.hpos == 512)  {
        // do sound and PWM!
        bus->display.do_sound_and_pwm();
    }
    if (bus->clock.crt.hpos >= 704) new_scanline();
}

bool display::in_hblank()
{
    return bus->clock.crt.hpos > 512;
}

void display::reset()
{
    /*
Macintosh 128K, the main screen buffer starts at $1A700 and the alternate buffer starts at
$12700; for a 512K Macintosh, add $60000 to these numbers.
     */
    calc_display_addr();
    crt->scan_x = crt->scan_y = 0;
    cur_output = static_cast<u8 *>(crt->output[crt->last_written]);
    crt->last_written ^= 1;
    bus->clock.crt.hpos = bus->clock.crt.vpos = 0;
    scanline_func = &scanline_visible;
}
}