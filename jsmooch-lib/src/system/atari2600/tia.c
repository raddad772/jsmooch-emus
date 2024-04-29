//
// Created by . on 4/14/24.
//

#include <string.h>
#include "stdio.h"

#include "tia.h"
#include "atari2600.h"

#define dprintf(...) (void)(0)

void TIA_init(struct atari_TIA* this)
{
    memset(this, 0, sizeof(struct atari_TIA));

    TIA_reset(this);
}

void TIA_reset(struct atari_TIA* this)
{
    this->hcounter = 0;
    dprintf("\nTIA RESET HC0");
    this->vcounter = 0;
    this->cpu_RDY = 0;
    this->frames_since_restart = 0;
    this->io.vblank = this->io.vsync = 0;
    for (u32 i = 0; i < TIA_WQ_MAX; i++) {
        this->write_queue[i].active = 0;
    }
}

#define IN_VBLANK_IN (this->vcounter < this->timing.display_line_start)
#define IN_DISPLAY ((this->vcounter >= this->timing.display_line_start) && (this->vcounter < this->timing.vblank_out_start))
#define IN_VBLANK_OUT (this->sy >= this->timing.vblank_out_start)
#define OUT_OF_FRAME (this->vcounter > (this->timing.vblank_out_lines + this->timing.vblank_out_start))

static void flip_buffer(struct atari_TIA* this)
{
    // Update meta state, so emulator knows we have completed a frame
    this->master_frame++;
    this->frames_since_restart++;

    // Flip framebuffer
    this->cur_output = this->display->device.display.output[this->display->device.display.last_written];
    this->display->device.display.last_written ^= 1;

}

static void TIA_WQ_add(struct atari_TIA* this, u32 num, u32 val, u32 delay)
{
    for (u32 i = 0; i < TIA_WQ_MAX; i++) {
        if (this->write_queue[i].active == 0) {
            this->write_queue[i].active = 1;
            this->write_queue[i].delay = delay;
            this->write_queue[i].data = val;
            this->write_queue[i].address = num;
            return;
        }
    }
    printf("\nWRITE QUEUE FULL! ERROR!");
}


// leaned heavy on Ares for this p_width, p_start and p_step logic, and m_ and ball_ versions.
// it's so different from docs... didn't get it til I implemented it
static u32 m_width(struct atari_TIA* this, u32 num)
{
    return 1 << this->m[num].size;
}

static void m_start(struct atari_TIA* this, u32 num)
{
    this->m[num].start_counter = 4;
    this->m[num].starting = 1;
}

static void m_step(struct atari_TIA* this, u32 num, u32 clocks)
{
    while (clocks--) {
        this->m[num].counter++;

        if (this->m[num].counter == 156) m_start(this, num);
        if (this->m[num].counter == 160) this->m[num].counter = 0;

        if (this->m[num].starting && (this->m[num].start_counter-- == 0)) {
            this->m[num].starting = 0;
            this->m[num].pixel_counter = 1;
            this->m[num].width_counter = (i32)m_width(this, num);
        }

        this->m[num].output = 0;
        if (this->m[num].pixel_counter) {
            if (--this->m[num].width_counter == 0) {
                this->m[num].pixel_counter--;
                this->m[num].width_counter = (i32)m_width(this, num);
            }
            this->m[num].output = this->m[num].enable;
        }
    }
}

static u32 p_width(struct atari_TIA* this, u32 num)
{
    switch(this->p[num].size) {
        case 5:
            return 2;
        case 7:
            return 4;
        default:
            return 1;
    }
}

static void p_start(struct atari_TIA* this, u32 num, u32 copy)
{
    this->p[num].copy = copy;
    this->p[num].start_counter = 5;
    this->p[num].starting = 1;
}


static void p_step(struct atari_TIA* this, u32 num, u32 clocks)
{
    u32 size = this->p[num].size;
    if (!clocks) return;
    while (clocks--) {
        this->p[num].counter++;
        u32 first  = size == 1 || size == 3;
        u32 second = size == 2 || size == 3 || size == 6;
        u32 third  = size == 4 || size == 6;

        if (first && this->p[num].counter == 12) p_start(this, num, true);
        if (second && this->p[num].counter == 28) p_start(this, num, true);
        if (third && this->p[num].counter == 60) p_start(this, num, true);
        if (this->p[num].counter == 156) p_start(this, num, false);
        if (this->p[num].counter == 160) this->p[num].counter = 0;

        if (this->p[num].starting && (this->p[num].start_counter-- == 0)) {
            this->p[num].starting = 0;
            this->p[num].pixel_counter = 8;
            this->p[num].width_counter = (i32)p_width(this, num);
        }

        this->p[num].output = 0;
        if (this->p[num].pixel_counter) {
            if (--this->p[num].width_counter == 0) {
                this->p[num].pixel_counter--;
                this->p[num].width_counter = (i32)p_width(this, num);
            }
            if (!this->p[num].copy && this->m[num].locked_to_player && this->p[num].pixel_counter == 4) {
                this->m[num].counter = -4;
            }
            u32 bnum = this->p[num].reflect ? (7 - this->p[num].pixel_counter) : this->p[num].pixel_counter;
            this->p[num].output = (this->p[num].GRP[this->p[num].delay] >> bnum) & 1;
        }

    }
}

static void ball_step(struct atari_TIA* this, u32 clocks)
{
    if (!clocks) return;
    while (clocks--) {
        if (++this->ball.counter == 160) this->ball.counter = 0;

        this->ball.output = this->ball.enable[this->ball.delay] && this->ball.counter < (1 << this->ball.size);
    }
}

static void TIA_WQ_finish(struct atari_TIA* this, struct atari_TIA_WQ_item* item)
{
    item->active = 0;
    u32 val = item->data;
    switch(item->address) {
        case 0x01: // VBLANK
            this->io.vblank = (val >> 1) & 1;
            this->io.inpt_4_5_ctrl = (val >> 6) & 1;
            this->io.inpt_0_3_ctrl = (val >> 7) & 1;
            return;
        case 0x0B: // REFP0 reflect player 0
            this->p[0].reflect = (val >> 3) & 1;
            return;
        case 0x0C: // REFP1 reflect player 1
            this->p[1].reflect = (val >> 3) & 1;
            return;
        case 0x0D: // PF0 playfield register byte 0
            this->io.pf &= 0xFFF0;
            this->io.pf |= ((val >> 4) & 15);
            return;
        case 0x0E: // PF1 playfield register byte 1
            // bit 4 = 7, 5 = 6, 6 =5, 7 = 4, 8 = 3, 9 = 2, 10 = 1, 11 = 0
            this->io.pf &= 0xFF00F;
            this->io.pf |= ((val >> 7) & 1) << 4;
            this->io.pf |= ((val >> 6) & 1) << 5;
            this->io.pf |= ((val >> 5) & 1) << 6;
            this->io.pf |= ((val >> 4) & 1) << 7;
            this->io.pf |= ((val >> 3) & 1) << 8;
            this->io.pf |= ((val >> 2) & 1) << 9;
            this->io.pf |= ((val >> 1) & 1) << 10;
            this->io.pf |= ((val >> 0) & 1) << 11;
            return;
        case 0x0F: // PF2 playfield register byte 2
            this->io.pf &= 0xFFF;
            this->io.pf |= (val & 0xFF) << 12;
            return;
        case 0x1B: // GRP0 graphics player 0
            this->p[0].GRP[0] = val;
            this->p[1].GRP[1] = this->p[1].GRP[0];
            return;
        case 0x1C: // GRP1 graphics player 1
            this->p[1].GRP[0] = val;
            this->p[0].GRP[1] = this->p[0].GRP[0];
            this->ball.enable[1] = this->ball.enable[0];
            return;
        case 0x1D: // ENAM0 enable missile 0
            this->m[0].enable = (val >> 1) & 1;
            return;
        case 0x1E: // ENAM1 enable missile 1
            this->m[1].enable = (val >> 1) & 1;
            return;
        case 0x1F: // ENABL enable ball
            this->ball.enable[0] = (val >> 1) & 1;
            return;
        case 0x20: // HMP0 horizontal motion player 0
            this->p[0].hm = (i32)(val >> 4) & 15;
            return;
        case 0x21: // HMP1 horizontal motion player 1
            this->p[1].hm = (i32)(val >> 4) & 15;
            return;
        case 0x22: // HMM0 horizontal motion missile 0
            this->m[0].hm = (i32)(val >> 4) & 15;
            return;
        case 0x23: // HMM1 horizontal motion missile 1
            this->m[1].hm = (i32)(val >> 4) & 15;
            return;
        case 0x24: // HMBL horizontal motion ball
            this->ball.hm = (i32)(val >> 4) & 15;
            return;
        case 0x2A: // HMOVE apply horizontal motion <strobe>
        {
            p_step(this, 0, this->p[0].hm ^ 8);
            p_step(this, 1, this->p[1].hm ^ 8);
            m_step(this, 0, this->m[0].hm ^ 8);
            m_step(this, 1, this->m[1].hm ^ 8);
            ball_step(this, this->ball.hm ^ 8);
            this->io.hmoved = 1;
            return;
        }
        case 0x2B: // HMCLR clear horizontal motion registers <strobe>
            this->p[0].counter = this->p[1].counter = this->m[0].counter = this->m[1].counter = this->ball.counter = 0;
            return;

    }
}


static void TIA_WQ_cycle(struct atari_TIA* this) {
    for (u32 i = 0; i < TIA_WQ_MAX; i++) {
        if (this->write_queue[i].active) {
            struct atari_TIA_WQ_item* item = &this->write_queue[i];
            item->delay--;
            if (item->delay == 0)
                TIA_WQ_finish(this, item);
        }
    }
}

static u32 TIA_read(struct atari_TIA* this, u32 addr, u32 *data)
{
    switch((addr & 0x0F) | 0x30) {
        case 0x30: // CXM0P read collision data for M0-P1, M0-P0
            return (this->col.m0_p1 << 7) | (this->col.m0_p0 << 6);
        case 0x31: // CXM1P read collision data for M1-P0, M1-P1
            return (this->col.m1_p0 << 7) | (this->col.m1_p1 << 6);
        case 0x32: // CXP0FB read collision data for P0-PF, P0-BALL
            return (this->col.p0_pf << 7) | (this->col.p0_ball << 6);
        case 0x33: // CXP1FB read collision data for P1-PF, P1-BALL
            return (this->col.p1_pf << 7) | (this->col.p1_ball << 6);
        case 0x34: // CXM0FB read collision data for M0-PF, M0-BALL
            return (this->col.m0_pf << 7) | (this->col.m0_ball << 6);
        case 0x35: // CXM1FB read collision data for M1-PF, M1-BALL
            return (this->col.m1_pf << 7) | (this->col.m1_ball << 6);
        case 0x36: // CXBLPF read collision data for BALL-PF
            return (this->col.ball_pf << 7);
        case 0x37: // CXPPMM read collision data for P0-P1, M0-M1
            return (this->col.p0_p1 << 7) | (this->col.m0_m1 << 6);
        case 0x38: // INPT0 read pot 0
            return this->io.INPT[0];
        case 0x39: // INPT1 read pot 1
            return this->io.INPT[1];
        case 0x3A: // INPT2 read pot 2
            return this->io.INPT[2];
        case 0x3B: // INPT3 read pot 3
            return this->io.INPT[3];
        case 0x3C: // INPT4 read pot 4
            return this->io.INPT[4];
        case 0x3D: // INPT5 read pot 5
            return this->io.INPT[5];
        default:
            printf("\nUnknown TIA read %02x", addr);
            return 0;
    }
}


static const i32 centering_offsets[8] = { 3, 3, 3, 3, 3, 6, 3, 10};
static void update_RESMPn(struct atari_TIA* this, u32 num)
{
    // If RESMP behavior is enabled...
    // "As long as Bit 1 is set, the missile is hidden and its horizontal position is
    // centered on the players position. The centering offset is +3 for normal,
    // +6 for double, and +10 quad sized player (that is giving good centering
    // results with missile widths of 2, 4, and 8 respectively).
    if (this->m[num].locked_to_player)
        this->m[num].counter = this->p[num].counter + centering_offsets[this->p[num].size];
    // TODO: do real centering logic here, or is this good!??!
}

static void update_RESMP(struct atari_TIA* this)
{
    update_RESMPn(this, 0);
    update_RESMPn(this, 1);
}

static void TIA_new_frame(struct atari_TIA* this);

static void TIA_vsync(struct atari_TIA* this, u32 val)
{
    // 0...1
    dprintf("\nTIA_vsync! %d %d", val, this->io.vsync);
    if (val && (this->io.vsync != val)) {
        TIA_new_frame(this);
    }
    this->io.vsync = val;
}

static void TIA_write(struct atari_TIA* this, u32 addr, u32 *data)
{
    // b-f, 1b-24
    u32 val = *data;
    i32 msx = (i32)this->hcounter - 68;
#define DELAY(num, howlong) case num: TIA_WQ_add(this, num, val, howlong); return
    switch(addr & 0x3F) {
        DELAY(0x01, 1);
        DELAY(0x0B, 1);
        DELAY(0x0C, 1);
        DELAY(0x0D, 2);
        DELAY(0x0E, 2);
        DELAY(0x0F, 2);
        DELAY(0x1B, 1);
        DELAY(0x1C, 1);
        DELAY(0x1D, 1);
        DELAY(0x1E, 1);
        DELAY(0x1F, 1);
        DELAY(0x20, 2);
        DELAY(0x21, 2);
        DELAY(0x22, 2);
        DELAY(0x23, 2);
        DELAY(0x24, 2);
        DELAY(0x2A, 6);
        DELAY(0x2B, 2);
        case 0x00: // VSYNC
            TIA_vsync(this, (val >> 1) & 1);
            return;
        case 0x02: // halt processor until end of current scanline
            this->cpu_RDY = 1;
            return;
        case 0x03: // RSYNC
            this->hcounter = 0;
            dprintf("\nTIA RSYNC HC0");
            return;
        case 0x04: // NUSIZ0...number and size of missile 0
            this->p[0].size = val & 7;
            this->m[0].size = (val >> 4) & 3;
            return;
        case 0x05: // NUSIZ1...number and size of missile 1
            this->p[1].size = val & 7;
            this->m[1].size = (val >> 4) & 3;
            return;
            return;
        case 0x06: // COLUP0 colum-lum player 0 missile 0
            this->io.COLUP0 = (val >> 1) & 0x7F;
            return;
        case 0x07: // COLUP1 colum-lum player 1 missile 1
            this->io.COLUP1 = (val >> 1) & 0x7F;
            return;
        case 0x08: // COLUPF colum-lum playfield and ball
            this->io.COLUPF = (val >> 1) & 0x7F;
            return;
        case 0x09: // COLUBK colum-lum background
            this->io.COLUBK = (val >> 1) & 0x7F;
            return;
        case 0x0A: // CTRLPF control playfield ball size & collissions
            this->io.CTRLPF.u = val & 0b110111;
            return;
        case 0x10: // RESP0 reset player 0 <strobe>
            this->p[0].counter = -4;
            return;
        case 0x11: // RESP1 reset player 1 <strobe>
            this->p[1].counter = -4;
            return;
        case 0x12: // RESM0 reset missile 0 <strobe>
            this->m[0].counter = -4;
            return;
        case 0x13: // RESM1 reset missile 1 <strobe>
            this->m[1].counter = -4;
            return;
        case 0x14: // RESBL reset ball <strobe>
            this->ball.counter = -4;
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
        case 0x25: // VDELP0 vertical delay player 0
            this->p[0].delay = (i32)val & 1;
            return;
        case 0x26: // VDELP1 vertical delay player 1
            this->p[1].delay = (i32)val & 1;
            return;
        case 0x27: // VDELBL vertical delay ball
            this->ball.delay = ((i32)val & 1);
            return;
        case 0x28: // RESMP0 reset missile 0 to player 0
            this->m[0].locked_to_player = val & 1;
            return;
        case 0x29: // RESMP1 reset missile 1 to player 1
            this->m[1].locked_to_player = val & 1;
            return;
        case 0x2C: // CXCLR clear collision latches <strobe>
            memset(&this->col, 0, sizeof(struct atari_TIA_col));
            return;
        default:
            dprintf("\nUnknow TIA write to %02x", addr & 0x3F);
            return;
    }
#undef DELAY
}

void TIA_bus_cycle(struct atari_TIA* this, u32 addr, u32 *data, u32 rw)
{
    if (rw == 0) *data = TIA_read(this, addr, data);
    else TIA_write(this, addr, data);
}

static void TIA_new_frame(struct atari_TIA* this)
{
    // Update internal state
    this->vcounter = 0;
    this->io.hmoved = 0;

    if (!this->missed_vblank) {
        flip_buffer(this);
    }
    this->missed_vblank = 0;
}

/*
 * Normal (REF=0) : PF0.4-7 PF1.7-0 PF2.0-7  PF0.4-7 PF1.7-0 PF2.0-7
 * Mirror (REF=1) : PF0.4-7 PF1.7-0 PF2.0-7  PF2.7-0 PF1.0-7 PF0.7-4
 */

// Missile size depending on NUSIZx.missile_size
static const u32 M_SIZE[4] = { 1, 2, 4, 8};

// A table to help us exract correct bit based on counter-position inside player sprite and if it's flipped
static const u32 PLAYER_SHIFTS[2][8] = {
        // Normal. MSB first left to right
        {7, 6, 5, 4, 3, 2, 1, 0},
        // Mirrored. LSB first left to right
        {0, 1, 2, 3, 4, 5, 6, 7}
};

// A table to help us extract playfield bits for any of the 40 valid horizontal positions, including with mirroring, and considering the odd way playfields are written
static const u32 PF_SHIFTS[2][40] = {
        // normal
        {   // left half
            16, 17, 18, 19,  // PF0.4-7
            15, 14, 13, 12, 11, 10, 9, 8, // PF1.7-0
            0, 1, 2, 3, 4, 5, 6, 7, // PF2.0-7
            // right half
            16, 17, 18, 19,
            15, 14, 13, 12, 11, 10, 9, 8,
            0, 1, 2, 3, 4, 5, 6, 7
         },
        // mirrored
        {
            // left half
            16, 17, 18, 19,  // PF0.4-7
            15, 14, 13, 12, 11, 10, 9, 8, // PF1.7-0
            0, 1, 2, 3, 4, 5, 6, 7, // PF2.0-7
            // right half PF2.7-0 PF1.0-7 PF0.7-4
            7, 6, 5, 4, 3, 2, 1, 0, // PF2.7-0
            8, 9, 10, 11, 12, 13, 14, 15, // PF1.0-7
            19, 18, 17, 16 // PF0.7-4
        }
};

static const u32 NUSIZ_copies[8][10] = {
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 0  One copy              (X.........)
        {1, 0, 1, 0, 0, 0, 0, 0, 0, 0}, // 1  Two copies - close    (X.X.......)
        {1, 0, 0, 0, 1, 0, 0, 0, 0, 0}, // 2  Two copies - medium   (X...X.....)
        {1, 0, 1, 0, 1, 0, 0, 0, 0, 0}, // 3  Three copies - close  (X.X.X.....)
        {1, 0, 0, 0, 0, 0, 0, 0, 1, 0}, // 4  Two copies - wide     (X.......X.)
        {1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, // 5  Double sized player   (XX........)
        {1, 0, 0, 0, 1, 0, 0, 0, 1, 0}, // 6  Three copies - medium (X...X...X.)
        {1, 1, 1, 1, 0, 0, 0, 0, 0, 0}  // 7  Quad sized player     (XXXX......)
};

static const u32 DIFF_SHIFTS[8] = {
        0, 0, 0, 0, 0, 1, 0, 2
};

static u32 get_player_pixel(struct atari_TIA* this, u32 msx, u32 player_num)
{
    // No player pixels off left side of screen
    if (this->hcounter < 68) return 0;

    // Invisible players give no pixels, skip all the checks!
    if (this->p[player_num].GRP[0] == 0) return 0;

    u32 player_x = this->p[player_num].counter; // Player X
    // If we're not within 80 pixels of the position, return 0
    if ((msx - player_x) >= 80) return 0;

    // We haven't yet gotten to the left edge of the player X...
    if (msx < player_x) return 0;

    u32 pm_num_size = this->p[player_num].size;

    // Check if we're inside it depending on copy mode
    if (!NUSIZ_copies[pm_num_size][player_x >> 3]) return 0;

    u32 player_px = ((msx - player_x) >> DIFF_SHIFTS[pm_num_size]) & 7;
    // Extract bit from GRP
    return (this->p[player_num].GRP[0] >> PLAYER_SHIFTS[this->p[player_num].reflect][player_px]) & 1;
}

static const u32 BALL_SIZES[4] = {1, 2, 4, 8};

static u32 get_ball_pixel(struct atari_TIA* this, u32 msx)
{
    if (this->ball.enable[0] == 0) return 0; // Ball is not enabled
    if (msx < this->ball.counter) return 0; // We aren't at left edge of ball yet

    u32 ball_size = BALL_SIZES[this->io.CTRLPF.ball_size];
    return msx < (this->ball.counter + ball_size);
}

// Get m0 pixel
static u32 get_missile_pixel(struct atari_TIA* this, u32 msx, u32 missile_num)
{
    if (this->m[missile_num].enable == 0) return 0; // Missile not enabled
    if (this->m[missile_num].locked_to_player) return 0; // Missile is hidden and centering
    if (msx < this->m[missile_num].counter) return 0; // We aren't at left edge of missile yet

    u32 missile_size = BALL_SIZES[(this->m[missile_num].size)];
    return msx < (this->m[missile_num].counter + missile_size);
}

void TIA_new_line(struct atari_TIA* this)
{
    this->cpu_RDY = 0;
    this->vcounter++;
    this->hcounter = 0;
    this->io.hmoved = 0;

    if (this->vcounter > 262) { // 312 for PAL. this is just in case a game missed vblank
        printf("\nVCOUNTER>262 update output! %d", this->master_frame + 1);
        this->vcounter = 0;

        flip_buffer(this);
        this->missed_vblank = 1;
    }
}

void TIA_run_cycle(struct atari_TIA* this)
{
    // A frame is...
    // 40 lines vsync
    // 192 lines NTSC
    //
    TIA_WQ_cycle(this);
    u32 hblank = (this->io.hmoved && (this->hcounter < 76)) || this->hcounter < 68;
    i32 screen_x = (i32)this->hcounter - 68;
    i32 screen_y = (i32)this->vcounter - 20; // 40 for PAL

    if (!hblank) {
        ball_step(this, 1);
        p_step(this, 0, 1);
        p_step(this, 1, 1);
        m_step(this, 0, 1);
        m_step(this, 1, 1);
    }

    u8 color = this->io.COLUBK;

    u32 pf, ball, m0, m1, p0, p1;

    // "run" the playfield

    if ((screen_x > 0) && ((screen_x % 4) == 0)) { // every 4 pixels...
        u32 pos = screen_x >> 2;
        u32 bnum = (!this->io.CTRLPF.mirror || pos < 20) ? (pos % 20) : (19 - (pos % 20));
        this->playfield.output = (this->io.pf >> bnum) & 1;
    }

    // Get playfield pixel
    pf = this->playfield.output;

    // Get p0 pixel
    //p0 = get_player_pixel(this, screen_x, 0);
    p0 = this->p[0].output;

    p1 = this->p[1].output;

    // Get ball pixel
    ball = this->ball.output;

    // Get m0 pixel
    m0 = this->m[0].output;
    m1 = this->m[1].output;

    // Do collisions
    this->col.m0_p0 |= m0 & p0;
    this->col.m0_p1 |= m0 & p1;
    this->col.m1_p0 |= m1 & p0;
    this->col.m1_p1 |= m1 & p1;
    this->col.p0_pf |= p0 & pf;
    this->col.p0_ball |= p0 & ball;
    this->col.p1_pf |= p1 & pf;
    this->col.p1_ball |= p1 & ball;
    this->col.m0_pf |= m0 & pf;
    this->col.m0_ball |= m0 & ball;
    this->col.m1_pf |= m1 & pf;
    this->col.m1_ball |= m1 & ball;
    this->col.ball_pf |= ball & pf;
    this->col.p0_p1 |= p0 & p1;
    this->col.m0_m1 |= m0 & m1;

    // Convert pixels to the 4 colors

    if(!this->io.CTRLPF.priority && (pf || ball)) color = this->io.COLUPF;
    if(p1 || m1) color = this->io.COLUP1;
    if(p0 || m0) color = this->io.COLUP0;
    if(this->io.CTRLPF.priority && (pf || ball))  color = this->io.COLUPF;

    if ((screen_x >= 0) && (screen_x < 160) && (screen_y >= 0) && (screen_y < 228)) { // 228 for NTSC, 243 for PAL
        if (this->io.vblank || hblank) color = 0;
        /*if (screen_x == 100) color = 19;
        if (screen_y == 100) color = 19;*/
        u32 bo = (screen_y * 160) + screen_x;
        this->cur_output[bo] = color;
    }

    this->hcounter++;
    if (this->hcounter >= 228) TIA_new_line(this);
}

