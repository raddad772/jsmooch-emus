//
// Created by . on 4/14/24.
//

#include <string.h>
#include "stdio.h"

#include "tia.h"
#include "atari2600.h"

void TIA_init(struct atari2600* this)
{
    // TODO
    TIA_reset(this);
}

void TIA_reset(struct atari2600* this)
{
    this->clock.line_cycle = 0;
    this->clock.sy = 0;
    // TODO
}

#define IN_VBLANK_IN (this->clock.sy < this->clock.timing.display_line_start)
#define IN_DISPLAY ((this->clock.sy >= this->clock.timing.display_line_start) && (this->clock.sy < this->clock.timing.vblank_out_start))
#define IN_VBLANK_OUT (this->clock.sy >= this->clock.timing.vblank_out_start)
#define OUT_OF_FRAME (this->clock.sy > (this->clock.timing.vblank_out_lines + this->clock.timing.vblank_out_start))

static u32 TIA_read(struct atari2600* this, u32 addr, u32 *data)
{
    switch((addr & 0x0F) | 0x30) {
        case 0x30: // CXM0P read collision data for M0-P1, M0-P0
            return (this->tia.col.m0_p1 << 7) | (this->tia.col.m0_p0 << 6);
        case 0x31: // CXM1P read collision data for M1-P0, M1-P1
            return (this->tia.col.m1_p0 << 7) | (this->tia.col.m1_p1 << 6);
        case 0x32: // CXP0FB read collision data for P0-PF, P0-BALL
            return (this->tia.col.p0_pf << 7) | (this->tia.col.p0_ball << 6);
        case 0x33: // CXP1FB read collision data for P1-PF, P1-BALL
            return (this->tia.col.p1_pf << 7) | (this->tia.col.p1_ball << 6);
        case 0x34: // CXM0FB read collision data for M0-PF, M0-BALL
            return (this->tia.col.m0_pf << 7) | (this->tia.col.m0_ball << 6);
        case 0x35: // CXM1FB read collision data for M1-PF, M1-BALL
            return (this->tia.col.m1_pf << 7) | (this->tia.col.m1_ball << 6);
        case 0x36: // CXBLPF read collision data for BALL-PF
            return (this->tia.col.ball_pf << 7);
        case 0x37: // CXPPMM read collision data for P0-P1, M0-M1
            return (this->tia.col.p0_p1 << 7) | (this->tia.col.m0_m1 << 6);
        case 0x38: // INPT0 read pot 0
            return 0;
        case 0x39: // INPT1 read pot 1
            return 0;
        case 0x3A: // INPT2 read pot 2
            return 0;
        case 0x3B: // INPT3 read pot 3
            return 0;
        case 0x3C: // INPT4 read pot 4
            return 0;
        case 0x3D: // INPT5 read pot 5
            return 0;
        default:
            printf("\nUnknown TIA read %02x", addr);
            return 0;
    }
}

static void TIA_write(struct atari2600* this, u32 addr, u32 *data)
{
    u32 val = *data;
    switch(addr & 0x3F) {
        case 0x00: // VSYNC
            this->tia.io.vsync = (val >> 1) & 1;
            return;
        case 0x01: // VBLANK
            this->tia.io.vblank = (val >> 1) & 1;
            this->tia.io.inpt_4_5_ctrl = (val >> 6) & 1;
            this->tia.io.inpt_0_3_ctrl = (val >> 7) & 1;
            return;
        case 0x02: // halt processor until end of current scanline
            this->cpu.pins.RDY = 1;
            return;
        case 0x03: // RSYNC
            printf("\nRSYNC?");
            return;

        case 0x04: // NUSIZ0...number and size of missile 0
            return;
        case 0x05: // NUSIZ1...number and size of missile 1
            return;
        case 0x06: // COLUP0 colum-lum player 0 missile 0
            this->tia.io.COLUP0 = (val >> 1) & 0x7F;
            return;
        case 0x07: // COLUP1 colum-lum player 1 missile 1
            this->tia.io.COLUP1 = (val >> 1) & 0x7F;
            return;
        case 0x08: // COLUPF colum-lum playfield and ball
            this->tia.io.COLUPF = (val >> 1) & 0x7F;
            return;
        case 0x09: // COLUBK colum-lum background
            this->tia.io.COLUBK = (val >> 1) & 0x7F;
            return;
        case 0x0A: // CTRLPF control playfield ball size & collissions
            this->tia.io.CTRLPF = val & 0b110111;
            return;
        case 0x0B: // REFP0 reflect player 0
            return;
        case 0x0C: // REFP1 reflect player 1
            return;
        case 0x0D: // PF0 playfield register byte 0
            this->tia.io.pf.pf0 = (val >> 4) & 15;
            return;
        case 0x0E: // PF1 playfield register byte 1
            this->tia.io.pf.pf1 = val & 0xFF;
            return;
        case 0x0F: // PF2 playfield register byte 2
            this->tia.io.pf.pf2 = val & 0xFF;
            return;
        case 0x10: // RESP0 reset player 0 <strobe>
            return;
        case 0x11: // RESP1 reset player 1 <strobe>
            return;
        case 0x12: // RESM0 reset missile 0 <strobe>
            return;
        case 0x13: // RESM1 reset missile 1 <strobe>
            return;
        case 0x14: // RESBL reset ball <strobe>
            return;
        case 0x15: // AUDC0 audio control 0
            return;
        case 0x16: // AUDC1 audio control 1
            return;
        case 0x17: // AUDF0 audio frequency 0
            return;
        case 0x18: // AUDF1 audio frequency 1
            return;
        case 0x19: // AUDV0 audio volume 0
            return;
        case 0x1A: // AUDV1 audio volume 1
            return;
        case 0x1B: // GRP0 graphics player 0
            return;
        case 0x1C: // GRP1 graphics player 1
            return;
        case 0x1D: // ENAM0 enable missile 0
            return;
        case 0x1E: // ENAM1 enable missile 1
            return;
        case 0x1F: // ENABL enable ball
            return;
        case 0x20: // HMP0 horizontal motion player 0
            return;
        case 0x21: // HMP1 horizontal motion player 1
            return;
        case 0x22: // HMM0 horizontal motion missile 0
            return;
        case 0x23: // HMM1 horizontal motion missile 1
            return;
        case 0x24: // HMBL horizontal motion ball
            return;
        case 0x25: // VDELP0 vertical delay player 0
            return;
        case 0x26: // VDELP1 vertical delay player 1
            return;
        case 0x27: // VDELBL vertical delay ball
            return;
        case 0x28: // RESMP0 reset missile 0 to player 0
            return;
        case 0x29: // RESMP1 reset missile 1 to player 1
            return;
        case 0x2A: // HMOVE apply horizontal motion <strobe>
            return;
        case 0x2B: // HMCLR clear horizontal motion registers <strobe>
            return;
        case 0x2C: // CXCLR clear collision latches <strobe>
            memset(&this->tia.col, 0, sizeof(struct atari_TIA_col));
            return;
        default:
            printf("Unknow TIA write to %02x", addr & 0x3F);
            return;
    }

}

void TIA_bus_cycle(struct atari2600* this, u32 addr, u32 *data, u32 rw)
{
    if (rw == 0) *data = TIA_read(this, addr, data);
    else TIA_write(this, addr, data);
}


static void TIA_new_frame(struct atari2600* this)
{
    this->clock.sy = 0;
    this->clock.line_cycle = 0;
    this->clock.master_frame++;
    this->clock.frames_since_restart++;
}

static void TIA_new_line(struct atari2600* this)
{
    this->clock.line_cycle = 0;
    this->cpu.pins.RDY = 0;
    this->clock.sy++;
    if (OUT_OF_FRAME) TIA_new_frame(this);
}


void TIA_draw_pixel(struct atari2600* this)
{
    u32 msx = this->clock.line_cycle - 68;
    u32 pf_x = msx >> 2;
    u32 msy = this->clock.sy - this->clock.timing.display_line_start;
    u32 bo = (msy * 160) + msx;
    u8 color = 0;
    if (!this->tia.io.vblank) {
        // Get pixels for pf, ball, m0, m1, p0, p1
        u32 pf=0, ball=0, m0=0, m1=0, p0=0, p1=0;
        // TODO: get pixel 0 or 1 for each of the variables above, then drawing SHOULD work!!!

        // Get playfield pixel
        color = get_pf_bit(this, pf_x);
        // Get p0 pixel
        // Get p1 pixel
        // Get ball pixel
        // Get m0 pixel
        // Get m1 pixel

        // Do collisions
        this->tia.col.m0_p0 |= m0 & p0;
        this->tia.col.m0_p1 |= m0 & p1;
        this->tia.col.m1_p0 |= m1 & p0;
        this->tia.col.m1_p1 |= m1 & p1;
        this->tia.col.p0_pf |= p0 & pf;
        this->tia.col.p0_ball |= p0 & ball;
        this->tia.col.p1_pf |= p1 & pf;
        this->tia.col.p1_ball |= p1 & ball;
        this->tia.col.m0_pf |= m0 & pf;
        this->tia.col.m0_ball |= m0 & ball;
        this->tia.col.m1_pf |= m1 & pf;
        this->tia.col.m1_ball |= m1 & ball;
        this->tia.col.ball_pf |= ball & pf;
        this->tia.col.p0_p1 |= p0 & p1;
        this->tia.col.m0_m1 |= m0 & m1;

        // Convert pixels to the 4 colors
        u8 COLUP0 = 0, COLUP1 = 0, COLUPF = 0;
        // SCORE-mode assignment
        if ((this->tia.io.CTRLPF & 2) && (!(this->tia.io.CTRLPF & 4))) {
            COLUP0 = p0 || m0 || ((msx < 20) && pf);
            COLUP1 = p1 || m1 || ((msx >= 20) && pf);
            COLUPF = ball;
        }
        else { // Regular-mode assignment
            COLUP0 = p0 || m0;
            COLUP1 = p1 || m1;
            COLUPF = ball || pf;
        }
        if (this->tia.io.CTRLPF & 4) { // CTRLPF.2 set priority
            if (COLUPF) {
                color = this->tia.io.COLUPF;
            }
            else if (COLUP0) {
                color = this->tia.io.COLUP0;
            }
            else if (COLUP1) {
                color = this->tia.io.COLUP1;
            }
            else {
                color = this->tia.io.COLUBK;
            }
        }
        else {
            if (COLUP0) {
                color = this->tia.io.COLUP0;
            }
            else if (COLUP1) {
                color = this->tia.io.COLUP1;
            }
            else if (COLUPF) {
                color = this->tia.io.COLUPF;
            }
            else {
                color = this->tia.io.COLUBK;
            }
        }
    }
    this->tia.cur_output[bo] = color;

}

void TIA_run_cycle(struct atari2600* this)
{
    // A frame is...
    // 40 lines vsync
    // 192 lines NTSC
    //
    if (IN_VBLANK_IN) { // 0...39
        // Don't really do anything.
    }
    else if (IN_DISPLAY) { // display lines
        if (this->clock.line_cycle >= 68) {
            TIA_draw_pixel(this);
        }
    }
    else { // vblank out
        // Don't really do anything.
    }

    this->clock.line_cycle++;
    if (this->clock.line_cycle >= 228) TIA_new_line(this);

}

