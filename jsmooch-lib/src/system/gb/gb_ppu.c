#include <assert.h>
#include <stdio.h>

#include "helpers/int.h"
#include "gb_ppu.h"
#include "gb_enums.h"
#include "gb_bus.h"
#include "gb_cpu.h"
#include "gb_clock.h"

struct GB_FIFO_item* GB_FIFO_pop(struct GB_FIFO* this);
static struct GB_px* GB_pixel_slice_fetcher_get_px_if_available(struct GB_pixel_slice_fetcher* this);
static void GB_pixel_slice_fetcher_run_fetch_cycle(struct GB_pixel_slice_fetcher* this);
static u32 GB_PPU_in_window(struct GB_PPU* this);
static u32 GB_PPU_bg_tilemap_addr_window(struct GB_PPU* this, u32 wlx);
static u32 GB_PPU_bg_tilemap_addr_nowindow(struct GB_PPU* this, u32 lx);
static u32 GB_PPU_bg_tile_addr_window(struct GB_PPU* this, u32 tn, u32 attr);
static u32 GB_PPU_bg_tile_addr_nowindow(struct GB_PPU* this, u32 tn, u32 attr);
static void GB_PPU_disable(struct GB_PPU* this);
static void GB_PPU_eval_STAT(struct GB_PPU* this);
static void GB_PPU_eval_lyc(struct GB_PPU* this);
static void GB_PPU_advance_frame(struct GB_PPU* this, u32 update_buffer);
static void GB_PPU_set_mode(struct GB_PPU* this, enum GB_PPU_modes which);;
static void GB_PPU_IRQ_mode2_down(struct GB_PPU* this);
static void GB_PPU_IRQ_mode2_up(struct GB_PPU* this);
static void GB_PPU_IRQ_mode1_down(struct GB_PPU* this);
static void GB_PPU_IRQ_mode1_up(struct GB_PPU* this);
static void GB_PPU_IRQ_vblank_up(struct GB_PPU* this);
static void GB_PPU_IRQ_mode0_down(struct GB_PPU* this);
static void GB_PPU_IRQ_mode0_up(struct GB_PPU* this);
static void GB_PPU_IRQ_lylyc_up(struct GB_PPU* this);
static void GB_PPU_IRQ_lylyc_down(struct GB_PPU* this);

void GB_PPU_bus_write_IO(struct GB_bus* bus, u32 addr, u32 val);
u32 GB_PPU_bus_read_IO(struct GB_bus* bus, u32 addr, u32 val);
static void GB_PPU_OAM_search(struct GB_PPU* this);
static void GB_PPU_pixel_transfer(struct GB_PPU *this);
void GB_PPU_write_OAM(struct GB_PPU* this, u32 addr, u32 val);
u32 GB_PPU_read_OAM(struct GB_PPU* this, u32 addr);
u32 GBC_resolve_priority(u32 LCDC0, u32 OAM7, u32 BG7, u32 bg_color, u32 sp_color);
u32 GB_sp_tile_addr(u32 tn, u32 y, u32 big_sprites, u32 attr, u32 cgb);

inline u32 GBC_resolve_priority(u32 LCDC0, u32 OAM7, u32 BG7, u32 bg_color, u32 sp_color) {
// 1 = BG
// 2 = OBJ
    if (sp_color == 0) return 1;
    if (!LCDC0) return 2;
    if ((!OAM7) && (!BG7)) return 2;
    if (bg_color > 0) return 1;
    return 2;
}

inline u32 GB_sp_tile_addr(u32 tn, u32 y, u32 big_sprites, u32 attr, u32 cgb) {
    u32 y_flip = attr & 0x40;
    if (big_sprites) {
        tn &= 0xFE;
        if (y_flip) y = 15 - y;
        if (y > 7) tn++;
        return (0x8000 | (tn << 4) | ((y & 7) << 1));
    }
    u32 hbits = 0;
    if (y_flip) y = 7 - y;
    if (cgb) {
        if (attr & 8) hbits = 0x2000;
    }
    return (0x8000 | (tn << 4) | (y << 1)) + hbits;
}

static void GB_PPU_sprite_init(struct GB_PPU_sprite* this) {
    this->x = this->y = this->tile = this->attr = this->in_q = 0;
    this->num = 60;
}

static void GB_FIFO_item_init(struct GB_FIFO_item* this, u32 pixel, u32 palette, u32 cgb_priority, u32 sprite_priority) {
    this->pixel = pixel;
    this->palette = palette;
    this->cgb_priority = cgb_priority;
    this->sprite_priority = sprite_priority;
    this->sprite_obj = NULL; 
}

static void GB_FIFO_init(struct GB_FIFO *this, enum GB_variants variant, u32 max) {
    this->variant = variant;
    this->compat_mode = 1;
    this->max = max;
    if (this->variant == GBC) { this->compat_mode = 0; }

    for (u32 i = 0; i < max; i++) {
        GB_FIFO_item_init(&this->items[i], 0, 0, 0, 0);
    }
    this->head = 0;
    this->tail = 0;
    this->num_items = 0;
}

static void GB_FIFO_set_cgb_mode(struct GB_FIFO *this, u32 on) {
    this->compat_mode = on ? 0 : 1;
}


static void GB_FIFO_clear(struct GB_FIFO *this) {
    this->head = this->tail = this->num_items = 0;
}

static u32 GB_FIFO_empty(struct GB_FIFO *this) {
    return this->num_items == 0;
}

static struct GB_FIFO_item* GB_FIFO_push(struct GB_FIFO* this)
{
    if (this->num_items >= this->max) {
        return NULL;
    }
    struct GB_FIFO_item* r = &(this->items[this->tail]);
    this->tail = (this->tail + 1) % this->max;
    this->num_items++;
    return r;
};

// This is for "mixing" on sprite encounter
static void GB_FIFO_sprite_mix(struct GB_FIFO *this, u32 bp0, u32 bp1, u32 attr, u32 sprite_num)
{
    u32 sp_palette;
    u32 sp_priority = (attr & 0x80) >> 7;
    if (this->compat_mode == 1) {
        sp_palette = (attr & 0x10) >> 4;
    }
    else {
        sp_palette = attr & 7;
    }
    u32 flip = attr & 0x20;
    // First fill with transparent pixels
    while (this->num_items < 8) {
        struct GB_FIFO_item* b = GB_FIFO_push(this);
        b->sprite_priority = 0;
        b->cgb_priority = 60;
        b->pixel = 0;
        b->palette = 0;
    }

    for (u32 j = 0; j < 8; j++) {
        u32 r = j;
        if (flip) r = 7 - r;
        u32 i = (r + this->head) & 7;
        u32 px = ((bp0 & 0x80) >> 7) | ((bp1 & 0x80) >> 6);
        bp0 <<= 1;
        bp1 <<= 1;
        if (this->compat_mode == 1) {
            // DMG/CGB compatability
            if (this->items[i].pixel == 0) {
                this->items[i].palette = sp_palette;
                this->items[i].sprite_priority = sp_priority;
                this->items[i].pixel = px;
                this->items[i].cgb_priority = 0;
            }
        }
        else {
            // GBC mode
            if (((sprite_num < this->items[i].cgb_priority) && px != 0) || (this->items[i].pixel == 0)) {
                this->items[i].palette = sp_palette;
                this->items[i].sprite_priority = sp_priority;
                this->items[i].pixel = px;
                this->items[i].cgb_priority = sprite_num;
            }
        }
    }
}

static void GB_FIFO_discard(struct GB_FIFO* this, u32 num) {
    for (u32 i = 0; i < num; i++) {
        GB_FIFO_pop(this);
    }
}

struct GB_FIFO_item* GB_FIFO_peek(struct GB_FIFO* this) {
    if (this->num_items == 0) return NULL;
    return &this->items[this->head];
}

struct GB_FIFO_item* GB_FIFO_pop(struct GB_FIFO* this) {
    if (this->num_items == 0) return &this->blank;
    struct GB_FIFO_item* r = &this->items[this->head];
    this->head = (this->head + 1) % this->max;
    this->num_items--;
    return r;
};

static void GB_px_init(struct GB_px* this) {
    this->had_pixel = this->color = this->bg_or_sp = this->palette = 0;
};

static void GB_pixel_slice_fetcher_init(struct GB_pixel_slice_fetcher* this, enum GB_variants variant, struct GB_clock* clock, struct GB_bus* bus) {
    this->clock = clock;
    this->bus = bus;
    this->variant = variant;
    this->fetch_cycle = this->fetch_addr = this->fetch_bp0 = this->fetch_bp1 = this->fetch_cgb_attr = this->bg_request_x = this->sp_request = this->sp_min = 0;

    this->ppu = NULL;
    this->fetch_obj = NULL;

    GB_FIFO_init(&this->bg_FIFO, variant, 8);
    GB_FIFO_init(&this->obj_FIFO, variant, 8);
    GB_FIFO_init(&this->sprites_queue, variant, 10);
    GB_px_init(&this->out_px);
}

static void GB_pixel_slice_fetcher_advance_line(struct GB_pixel_slice_fetcher* this) {
    this->fetch_cycle = 0;
    GB_FIFO_clear(&this->bg_FIFO);
    GB_FIFO_clear(&this->obj_FIFO);
    GB_FIFO_clear(&this->sprites_queue);
    this->bg_request_x = this->ppu->io.SCX;
    this->sp_request = 0;
    this->sp_min = 0;
}

static void GB_pixel_slice_fetcher_trigger_window(struct GB_pixel_slice_fetcher* this) {
    GB_FIFO_clear(&this->bg_FIFO);
    this->bg_request_x = 0;
    this->fetch_cycle = 0;
}

static struct GB_px *GB_pixel_slice_fetcher_cycle(struct GB_pixel_slice_fetcher* this) {
    struct GB_px *r = GB_pixel_slice_fetcher_get_px_if_available(this);
    GB_pixel_slice_fetcher_run_fetch_cycle(this);
    return r;
}

static struct GB_px *GB_pixel_slice_fetcher_get_px_if_available(struct GB_pixel_slice_fetcher* this) {
    this->out_px.had_pixel = false;
    this->out_px.bg_or_sp = -1;
    if ((this->sp_request == 0) && (!GB_FIFO_empty(&this->bg_FIFO))) { // if we're not in a sprite request, and the BG FIFO isn't empty
        this->out_px.had_pixel = true;
        u32 has_bg = this->clock->cgb_enable? true : this->ppu->io.bg_window_enable;
        u32 bg_pal = 0;

        struct GB_FIFO_item* bg = GB_FIFO_pop(&this->bg_FIFO);
        u32 bg_color = bg->pixel;
        u32 has_sp = false;
        i32 sp_color = -1;
        u32 sp_palette;
        u32 use_what = 0; // 0 for BG, 1 for OBJ
        struct GB_FIFO_item* obj;
        if (this->clock->cgb_enable)
            bg_pal = bg->palette;

        if (!GB_FIFO_empty(&this->obj_FIFO)) {
            obj = GB_FIFO_pop(&this->obj_FIFO);
            sp_color = (i32)obj->pixel;
            sp_palette = obj->palette;
        }
        if (this->ppu->io.obj_enable && (sp_color != -1)) has_sp = true;

        // DMG resolve
        if ((has_bg) && (!has_sp)) {
            use_what = 1;
        } else if ((!has_bg) && (has_sp)) {
            use_what = 1;
        } else if (has_bg && has_sp) {
            if (this->clock->cgb_enable) {
                use_what = GBC_resolve_priority(this->ppu->io.bg_window_enable, obj->sprite_priority, bg->sprite_priority, bg_color, sp_color);
            } else {
                if (obj->sprite_priority && (bg_color != 0)) // "If the OBJ pixel has its priority bit set, and the BG pixel's ID is not 0, pick the BG pixel."
                    use_what = 1; // BG
                else if (sp_color == 0) // "If the OBJ pixel is 0, pick the BG pixel; "
                    use_what = 1; // BG
                else // "otherwise, pick the OBJ pixel"
                {
                    use_what = 2; // sprite
                }
            }
        } else {
            use_what = 0;
            this->out_px.color = 0;
        }

        if (use_what == 0) {
        }
        else if (use_what == 1) {
            this->out_px.bg_or_sp = 0;
            this->out_px.color = bg_color;
            this->out_px.palette = bg_pal;
        } else {
            this->out_px.bg_or_sp = 1;
            this->out_px.color = sp_color;
            this->out_px.palette = sp_palette;
        }
    }
    return &this->out_px;
}

static void GB_pixel_slice_fetcher_run_fetch_cycle(struct GB_pixel_slice_fetcher *this) {
    // Scan any sprites
    for (u32 i = 0; i < this->ppu->sprites.num; i++) {
        if ((!this->ppu->OBJ[i].in_q) && (this->ppu->OBJ[i].x == this->clock->lx)) {
            this->sp_request++;
            GB_FIFO_push(&this->sprites_queue)->sprite_obj = &this->ppu->OBJ[i];
            this->ppu->OBJ[i].in_q = 1;
        }
    }

    u32 tn, addr;
    switch (this->fetch_cycle) {
    case 0: // nothing
        this->fetch_cycle = 1;
        break;
    case 1: // tile
        if (GB_PPU_in_window(this->ppu)) {
            addr = GB_PPU_bg_tilemap_addr_window(this->ppu, this->bg_request_x);
            if (this->clock->cgb_enable) {
                this->fetch_cgb_attr = GB_bus_PPU_read(this->bus, addr + 0x2000);
            }
            tn = GB_bus_PPU_read(this->bus, addr);
            this->fetch_addr = GB_PPU_bg_tile_addr_window(this->ppu, tn, this->fetch_cgb_attr);
        }
        else {
            addr = GB_PPU_bg_tilemap_addr_nowindow(this->ppu, this->bg_request_x);
            if (this->clock->cgb_enable) {
                this->fetch_cgb_attr = GB_bus_PPU_read(this->bus, addr + 0x2000);
            }
            tn = GB_bus_PPU_read(this->bus, addr);
            this->fetch_addr = GB_PPU_bg_tile_addr_nowindow(this->ppu, tn, this->fetch_cgb_attr);
        }
        this->fetch_cycle = 2;
        break;
    case 2: // nothing
        this->fetch_cycle = 3;
        break;
    case 3: // bp0
        this->fetch_bp0 = GB_bus_PPU_read(this->bus, this->fetch_addr);
        this->fetch_cycle = 4;
        break;
    case 4: // nothing
        this->fetch_cycle = 5;
        break;
    case 5: // bp1.
        this->fetch_bp1 = GB_bus_PPU_read(this->bus, this->fetch_addr + 1);
        //if (this->ppu->in_window()) this->fetch_bp1 = 0x55;
        this->fetch_cycle = 6;
        break;
    case 6: // attempt background push. also hijack by sprite, so sprite cycle #0
        if (this->sp_request && (!GB_FIFO_empty(&this->bg_FIFO))) { // SPRITE HIJACK!
            this->fetch_cycle = 7;
            //this->sprites_queue.peek()!.sprite_obj
            this->fetch_obj = GB_FIFO_peek(&this->sprites_queue)->sprite_obj;
            this->fetch_addr = GB_sp_tile_addr(this->fetch_obj->tile, this->clock->ly - this->fetch_obj->y, this->ppu->io.sprites_big, this->fetch_obj->attr, this->clock->cgb_enable);
            break;
        }
        else { // attempt to push to BG FIFO, kind only accepts when empty.
            if (GB_FIFO_empty(&this->bg_FIFO)) {
                // Push to FIFO
                for (u32 i = 0; i < 8; i++) {
                    struct GB_FIFO_item* b = GB_FIFO_push(&this->bg_FIFO);
                    if ((this->clock->cgb_enable) && (this->fetch_cgb_attr & 0x20)) {
                        // Flipped X
                        b->pixel = (this->fetch_bp0 & 1) | ((this->fetch_bp1 & 1) << 1);
                        this->fetch_bp0 >>= 1;
                        this->fetch_bp1 >>= 1;
                    }
                    else {
                        // For regular, X not flipped
                        b->pixel = ((this->fetch_bp0 & 0x80) >> 7) | ((this->fetch_bp1 & 0x80) >> 6);
                        this->fetch_bp0 <<= 1;
                        this->fetch_bp1 <<= 1;
                    }
                    b->palette = 0;
                    if (this->clock->cgb_enable) {
                        b->palette = this->fetch_cgb_attr & 7;
                        b->sprite_priority = (this->fetch_cgb_attr & 0x80) >> 7;
                    }
                }
                this->bg_request_x += 8;
                // This is the first fetch of the line, so discard up to 7 pixels from the first fetch for scrolling purposes
                if ((this->ppu->line_cycle < 88) && (!GB_PPU_in_window(this->ppu))) {
                    this->bg_request_x -= 8;
                    // Now discard some pixels for scrolling!
                    u32 sx = this->ppu->io.SCX & 7;
                    GB_FIFO_discard(&this->bg_FIFO, sx);
                }
                this->fetch_cycle = 0; // Restart fetching
            }
        }
        break;
    case 7: // sprite bp0 fetch, cycle 1
        this->fetch_cycle = 8;
        break;
    case 8: // nothing, cycle 2
        this->fetch_cycle = 9;
        break;
    case 9: // spr cycle 3
        this->fetch_cycle = 10;
        this->spfetch_bp0 = GB_bus_PPU_read(this->bus, this->fetch_addr);
        break;
    case 10: // spr cycle 4
        this->fetch_cycle = 11;
        break;
    case 11: // spr cycle 5. sprite bp1 fetch, mix, & restart
        this->spfetch_bp1 = GB_bus_PPU_read(this->bus, this->fetch_addr + 1);
        GB_FIFO_sprite_mix(&this->obj_FIFO, this->spfetch_bp0, this->spfetch_bp1, this->fetch_obj->attr, this->fetch_obj->num);
        this->sp_request--;
        GB_FIFO_pop(&this->sprites_queue);
        this->fetch_cycle = 6; // restart hijack/sprite process or push process
        break;
    }
}

static void GB_PPU_sprites_init(struct GB_PPU_sprites* this) {
    this->num = this->index = this->search_index = 0;
}

void GB_PPU_reset(struct GB_PPU* this)
{
    // Reset variables
    this->clock->lx = 0;
    this->clock->ly = 0;
    this->display->scan_x = this->display->scan_y = 0;
    this->line_cycle = 0;
    this->display_update = false;
    this->cycles_til_vblank = 0;

    // Reset IRQs
    //this->io.STAT_IE = 0; // Interrupt enables
    //this->io.STAT_IF = 0; // Interrupt flags
    //this->io.old_mask = 0;

    //this->eval_STAT();

    // Set mode to OAM search
    GB_PPU_set_mode(this, OAM_search);
    this->first_reset = false;
}

u32 GB_PPU_bus_read_OAM(struct GB_bus* bus, u32 addr) {
    return GB_PPU_read_OAM(bus->ppu, addr);
}

void GB_PPU_bus_write_OAM(struct GB_bus* bus, u32 addr, u32 val) {
    GB_PPU_write_OAM(bus->ppu, addr, val);
}

void GB_PPU_init(struct GB_PPU* this, enum GB_variants variant, struct GB_clock* clock, struct GB_bus* bus) {
    u32 i;
    this->variant = variant;
    this->clock = clock;
    this->bus = bus;

    this->bus->read_OAM = &GB_PPU_bus_read_OAM;
    this->bus->write_OAM = &GB_PPU_bus_write_OAM;

    GB_pixel_slice_fetcher_init(&this->slice_fetcher, variant, clock, bus);

    this->line_cycle = 0;
    this->enabled = this->display_update = FALSE;
    this->cycles_til_vblank = 0;

    this->bg_palette[0] = this->bg_palette[1] = this->bg_palette[2] = this->bg_palette[3] = 0;
    for (u32 j = 0; j < 2; j++) {
        for (i = 0; i < 4; i++) {
            this->sp_palette[j][i] = 0;
        }
    }

    for (i = 0; i < 160; i++) {
        this->OAM[i] = 0;
    }

    for (i = 0; i < 10; i++) {
        GB_PPU_sprite_init(&this->OBJ[i]);
    }

    GB_PPU_sprites_init(&this->sprites);

    this->first_reset = TRUE;
    this->is_window_line = this->window_triggered_on_line = this->display_update = FALSE;

    bus->ppu = this;
    this->slice_fetcher.ppu = this;

    this->io.sprites_big = this->io.lyc = this->io.STAT_IF = this->io.STAT_IE = this->io.old_mask = 0;
    this->io.window_tile_map_base = this->io.window_enable = this->io.bg_window_tile_data_base = this->io.bg_tile_map_base = 0;
    this->io.obj_enable = this->io.bg_window_enable = 1;
    this->io.SCX = this->io.SCY = this->io.wx = this->io.wy = 0;
    this->io.cram_bg_increment = this->io.cram_bg_addr = this->io.cram_obj_increment = this->io.cram_obj_addr = 0;

    GB_PPU_disable(this);
}

static void GB_PPU_disable(struct GB_PPU* this) {
    if (!this->enabled) return;
    //printf("\nDISABLE PPU %d", this->clock->master_clock);
    this->enabled = FALSE;
    this->display->enabled = 0;
    this->clock->CPU_can_VRAM = 1;
    GB_clock_setCPU_can_OAM(this->clock, 1);
    this->io.STAT_IF = 0;
    GB_PPU_eval_STAT(this);
}

static void GB_PPU_enable(struct GB_PPU *this) {
    if (this->enabled) return;
    //printf("\nENABLE PPU %d", this->clock->master_clock);
    this->enabled = TRUE;
    this->display->enabled = 1;
    this->display->scan_x = this->display->scan_y = 0;
    GB_PPU_advance_frame(this, false);
    this->clock->lx = 0;
    this->clock->ly = 0;
    this->line_cycle = 0;
    this->cycles_til_vblank = 0;
    this->io.STAT_IF = 0;
    GB_PPU_set_mode(this, OAM_search);
    GB_PPU_eval_lyc(this);
    GB_PPU_eval_STAT(this);
}


static void GB_PPU_eval_STAT(struct GB_PPU* this) {
    u32 mask = this->io.STAT_IF & this->io.STAT_IE;
    if ((this->io.old_mask == 0) && (mask != 0)) {
        this->bus->cpu->cpu.regs.IF |= 2;
    }
    //else {
    //    assert(1 == 0);
    //}
    this->io.old_mask = mask;
}

static u32 GB_PPU_in_window(struct GB_PPU* this) {
    return this->io.window_enable && this->is_window_line && (this->clock->lx >= this->io.wx);
}

static u32 GB_PPU_bg_tilemap_addr_window(struct GB_PPU* this, u32 wlx) {
    return (0x9800 | (this->io.window_tile_map_base << 10) |
        ((this->clock->wly >> 3) << 5) |
        (wlx >> 3)
    );
}

static u32 GB_PPU_bg_tilemap_addr_nowindow(struct GB_PPU* this, u32 lx) {
    return (0x9800 | (this->io.bg_tile_map_base << 10) |
            ((((this->clock->ly + this->io.SCY) & 0xFF) >> 3) << 5) |
    (((lx) & 0xFF) >> 3));
}

#define HACK8TO16(x) x | ((x << 8) & 0x00FF00FF) | (())
/*
static const unsigned int B[] = {0x55555555, 0x33333333, 0x0F0F0F0F, 0x00FF00FF};
static const unsigned int S[] = {1, 2, 4, 8};
counter = (counter | (counter << S[3])) & B[3];
counter = (counter | (counter << S[2])) & B[2];
counter = (counter | (counter << S[1])) & B[1];
counter = (counter | (counter << S[0])) & B[0];

y = (y | (y << S[3])) & B[3];
y = (y | (y << S[2])) & B[2];
y = (y | (y << S[1])) & B[1];
y = (y | (y << S[0])) & B[0];
*/
static u32 GB_PPU_bg_tile_addr_window(struct GB_PPU* this, u32 tn, u32 attr) {
    u32 hbits = 0;
    u32 b12 = this->io.bg_window_tile_data_base ? 0 : ((tn & 0x80) ^ 0x80) << 5;
    u32 ybits = this->clock->wly & 7;
    if (this->clock->cgb_enable) {
        hbits = (attr & 8) << 10; // hbits = 0 or 0x2000  hbits = 0x2000;
        if (attr & 0x40) ybits = (7 - ybits);
    }
    return (0x8000 | b12 |
            (tn << 4) |
            (ybits << 1)
           ) + hbits;
}

static u32 GB_PPU_bg_tile_addr_nowindow(struct GB_PPU* this, u32 tn, u32 attr) {
    u32 b12 = this->io.bg_window_tile_data_base ? 0 : ((tn & 0x80) ^ 0x80) << 5;
    u32 hbits = 0;
    u32 ybits = (this->clock->ly + this->io.SCY) & 7;
    if (this->clock->cgb_enable) {
        hbits = (attr & 8) << 10; // hbits = 0 or 0x2000  hbits = 0x2000;
        if (attr & 0x40) ybits = (7 - ybits);
    }
    return (0x8000 | b12 |
            (tn << 4) |
            (ybits << 1)
           ) + hbits;
}

void GB_PPU_write_OAM(struct GB_PPU* this, u32 addr, u32 val) {
    if (addr < 0xFEA0) this->OAM[addr - 0xFE00] = (u8)val;
}

u32 GB_PPU_read_OAM(struct GB_PPU* this, u32 addr) {
    if (addr >= 0xFEA0) return 0xFF;
    return this->OAM[addr - 0xFE00];
}

void GB_PPU_bus_write_IO(struct GB_bus* bus, u32 addr, u32 val) {
    struct GB_PPU* this = bus->ppu;
    switch (addr) {
    case 0xFF01: // Serial write
        /*let nstr = String.fromCharCode(val);
        this->console_str += nstr;
        console.log(this->console_str);*/
        break;
    case 0xFF40: // LCDC LCD Control
        if (val & 0x80) GB_PPU_enable(this);
        else GB_PPU_disable(this);

        this->io.window_tile_map_base = (val & 0x40) >> 6;
        this->io.window_enable = (val & 0x20) >> 5;
        this->io.bg_window_tile_data_base = (val & 0x10) >> 4;
        this->io.bg_tile_map_base = (val & 8) >> 3;
        this->io.sprites_big = (val & 4) >> 2;
        this->io.obj_enable = (val & 2) >> 1;
        this->io.bg_window_enable = val & 1;
        return;
    case 0xFF41: // STAT LCD status
        if (this->variant == DMG) { // Can cause spurious interrupt writing this
            this->io.STAT_IE = 0x0F;
            GB_PPU_eval_STAT(this);
        }
        u32 mode0_enable = (val & 8) >> 3;
        u32 mode1_enable = (val & 0x10) >> 4;
        u32 mode2_enable = (val & 0x20) >> 5;
        u32 lylyc_enable = (val & 0x40) >> 6;
        this->io.STAT_IE = mode0_enable | (mode1_enable << 1) | (mode2_enable << 2) | (lylyc_enable << 3);
        GB_PPU_eval_STAT(this);
        return;
    case 0xFF42: // SCY
        this->io.SCY = val;
        return;
    case 0xFF43: // SCX
        this->io.SCX = val;
        return;
    case 0xFF45: // LYC
        this->io.lyc = val;
        //console.log('LYC:', this->io.lyc)
        if (this->enabled) GB_PPU_eval_lyc(this);
        return;
    case 0xFF4A: // window Y
        this->io.wy = val;
        return;
    case 0xFF4B: // window counter + 7
        this->io.wx = val + 1;
        return;
    case 0xFF47: // BGP pallete
        //if (!this->clock->CPU_can_VRAM) return;
        this->bg_palette[0] = (u8)(val & 3);
        this->bg_palette[1] = (u8)((val >> 2) & 3);
        this->bg_palette[2] = (u8)((val >> 4) & 3);
        this->bg_palette[3] = (u8)((val >> 6) & 3);
        //printf("\nWrite to BG pallette %02x on frame %d", val, this->clock->master_frame);
        return;
    case 0xFF48: // OBP0 sprite palette 0
        //if (!this->clock->CPU_can_VRAM) return;
        this->sp_palette[0][0] = (u8)(val & 3);
        this->sp_palette[0][1] = (u8)((val >> 2) & 3);
        this->sp_palette[0][2] = (u8)((val >> 4) & 3);
        this->sp_palette[0][3] = (u8)((val >> 6) & 3);
        return;
    case 0xFF49: // OBP1 sprite palette 1
        //if (!this->clock->CPU_can_VRAM) return;
        this->sp_palette[1][0] = (u8)(val & 3);
        this->sp_palette[1][1] = (u8)((val >> 2) & 3);
        this->sp_palette[1][2] = (u8)((val >> 4) & 3);
        this->sp_palette[1][3] = (u8)((val >> 6) & 3);
        return;
    case 0xFF68: // BCPS/BGPI
        if (!this->clock->cgb_enable) return;
        this->io.cram_bg_increment = (val & 0x80) >> 7;
        this->io.cram_bg_addr = val & 0x3F;
        return;
    case 0xFF69: // BG pal write
        if (!this->clock->cgb_enable) return;
        this->bg_CRAM[this->io.cram_bg_addr] = val;
        if (this->io.cram_bg_increment) this->io.cram_bg_addr = (this->io.cram_bg_addr + 1) & 0x3F;
        return;
    case 0xFF6A: // OPS/OPI
        if (!this->clock->cgb_enable) return;
        this->io.cram_obj_increment = (val & 0x80) >> 7;
        this->io.cram_obj_addr = val & 0x3F;
        return;
    case 0xFF6B: // OBJ pal write
        if (!this->clock->cgb_enable) return;
        this->obj_CRAM[this->io.cram_obj_addr] = val;
        if (this->io.cram_obj_increment) this->io.cram_obj_addr = (this->io.cram_obj_addr + 1) & 0x3F;
        return;
    }
}

u32 GB_PPU_bus_read_IO(struct GB_bus* bus, u32 addr, u32 val){
    u32 e;
    u32 mode0_enable, mode1_enable, mode2_enable, lylyc_enable;
    u32 ly;
    struct GB_PPU* this = bus->ppu;
    switch (addr) {
        case 0xFF40: // LCDC LCD Control
            e = this->enabled ? 0x80 : 0;
            return e | (this->io.window_tile_map_base << 6) | (this->io.window_enable << 5) | (this->io.bg_window_tile_data_base << 4) |
                (this->io.bg_tile_map_base << 3) | (this->io.sprites_big << 2) |
                (this->io.obj_enable << 1) | (this->io.bg_window_enable);
        case 0xFF41: // STAT LCD status
            mode0_enable = this->io.STAT_IE & 1;
            mode1_enable = (this->io.STAT_IE & 2) >> 1;
            mode2_enable = (this->io.STAT_IE & 4) >> 2;
            lylyc_enable = (this->io.STAT_IE & 8) >> 3;
            return this->clock->ppu_mode |
                ((this->clock->ly == this->io.lyc) ? 1 : 0) |
                (mode0_enable << 3) |
                (mode1_enable << 4) |
                (mode2_enable << 5) |
                (lylyc_enable << 6);
        case 0xFF42: // SCY
            return this->io.SCY;
        case 0xFF43: // SCX
            return this->io.SCX;
        case 0xFF44: // LY
            //printf("\nRETURNING SCY %d %d %d", this->clock->ly, this->clock->master_frame, this->io.SCY);
            //fflush(stdout);
            ly = this->clock->ly;
            if ((ly == 153) && (this->line_cycle > 1)) ly = 0;
            return ly;
        case 0xFF45: // LYC
            return this->io.lyc;
        case 0xFF4A: // window Y
            return this->io.wy;
        case 0xFF4B: // window counter + 7
            return (this->io.wx-1);
        case 0xFF47: // BGP
            //if (!this->clock->CPU_can_VRAM) return 0xFF;
            return this->bg_palette[0] | (this->bg_palette[1] << 2) | (this->bg_palette[2] << 4) | (this->bg_palette[3] << 6);
        case 0xFF48: // OBP0
            //if (!this->clock->CPU_can_VRAM) return 0xFF;
            return this->sp_palette[0][0] | (this->sp_palette[0][1] << 2) | (this->sp_palette[0][2] << 4) | (this->sp_palette[0][3] << 6);
        case 0xFF49: // OBP1
            //if (!this->clock->CPU_can_VRAM) return 0xFF;
            return this->sp_palette[1][0] | (this->sp_palette[1][1] << 2) | (this->sp_palette[1][2] << 4) | (this->sp_palette[1][3] << 6);
        case 0xFF68: // BCPS/BGPI
            if (!this->clock->cgb_enable) return 0xFF;
            return this->io.cram_bg_addr | (this->io.cram_bg_increment << 7);
        case 0xFF69: // BG pal read
            if (!this->clock->cgb_enable) return 0xFF;
            return this->bg_CRAM[this->io.cram_bg_addr];
        case 0xFF6A: // OPS/OPI
            if (!this->clock->cgb_enable) return 0xFF;
            return this->io.cram_obj_addr | (this->io.cram_obj_increment << 7);
        case 0xFF6B: // OBJ pal read
            if (!this->clock->cgb_enable) return 0xFF;
            return this->obj_CRAM[this->io.cram_obj_addr];
    }
    return 0xFF;
}

static void GB_PPU_IRQ_lylyc_up(struct GB_PPU* this) {
    this->io.STAT_IF |= 8;
    GB_PPU_eval_STAT(this);
}

static void GB_PPU_IRQ_lylyc_down(struct GB_PPU *this) {
    this->io.STAT_IF &= 0xF7;
    GB_PPU_eval_STAT(this);
}

static void GB_PPU_IRQ_mode0_up(struct GB_PPU *this) {
    this->io.STAT_IF |= 1;
    GB_PPU_eval_STAT(this);
}

static void GB_PPU_IRQ_mode0_down(struct GB_PPU *this) {
    this->io.STAT_IF &= 0xFE;
    GB_PPU_eval_STAT(this);
}

static void GB_PPU_IRQ_vblank_up(struct GB_PPU *this) {
    this->cycles_til_vblank = 2;
}

static void GB_PPU_IRQ_mode1_up(struct GB_PPU *this) {
    this->io.STAT_IF |= 2;
    GB_PPU_eval_STAT(this);
}

static void GB_PPU_IRQ_mode1_down(struct GB_PPU *this) {
    this->io.STAT_IF &= 0xFD;
    GB_PPU_eval_STAT(this);
}

static void GB_PPU_IRQ_mode2_up(struct GB_PPU *this) {
    this->io.STAT_IF |= 4;
    GB_PPU_eval_STAT(this);
}

static void GB_PPU_IRQ_mode2_down(struct GB_PPU *this) {
    this->io.STAT_IF &= 0xFB;
    GB_PPU_eval_STAT(this);
}


static void GB_PPU_eval_lyc(struct GB_PPU *this) {
    u32 cly = this->clock->ly;
    if ((cly == 153) && (this->io.lyc != 153)) cly = 0;
    if (cly == this->io.lyc)
        GB_PPU_IRQ_lylyc_up(this);
    else
        GB_PPU_IRQ_lylyc_down(this);
}

static void GB_PPU_advance_frame(struct GB_PPU *this, u32 update_buffer) {
    this->clock->ly = 0;
    this->clock->wly = 0;
    this->display->scan_y = 0;
    this->is_window_line = this->clock->ly == this->io.wy;
    if (this->enabled) {
        GB_PPU_eval_lyc(this);
        this->display_update = true;
    }
    this->clock->frames_since_restart++;
    this->clock->master_frame++;
    if (update_buffer) {
        this->cur_output = this->display->output[this->display->last_written];
        this->display->last_written ^= 1;
    }
}

static void GB_PPU_set_mode(struct GB_PPU *this, enum GB_PPU_modes which) {
    if (this->clock->ppu_mode == which) return;
    this->clock->ppu_mode = which;

    switch (which) {
    case OAM_search: // 2. after vblank, so after 1
        GB_clock_setCPU_can_OAM(this->clock, 0);
        this->clock->CPU_can_VRAM = 1;
        if (this->enabled) {
            this->bus->IRQ_vblank_down(this->bus);
            GB_PPU_IRQ_mode1_down(this);
            GB_PPU_IRQ_mode2_up(this);
        }
        break;
    case pixel_transfer: // 3, comes after 2
        GB_PPU_IRQ_mode2_down(this);
        this->clock->CPU_can_VRAM = 0;
        GB_clock_setCPU_can_OAM(this->clock, 0);
        GB_pixel_slice_fetcher_advance_line(&this->slice_fetcher);
        break;
    case HBLANK: // 0, comes after 3
        this->bus->cpu->hdma.notify_hblank = true;
        GB_PPU_IRQ_mode0_up(this);
        this->clock->CPU_can_VRAM = 1;
        GB_clock_setCPU_can_OAM(this->clock, 1);
        break;
    case VBLANK: // 1, comes after 0
        GB_PPU_IRQ_mode0_down(this);
        GB_PPU_IRQ_mode1_up(this);
        GB_PPU_IRQ_vblank_up(this);
        this->clock->CPU_can_VRAM = 1;
        GB_clock_setCPU_can_OAM(this->clock, 1);
        break;
    }
}

void GB_PPU_cycle(struct GB_PPU* this) {
    // During HBlank and VBlank do nothing...
    if (this->clock->ly > 143) return;
    if (this->clock->ppu_mode < 2) return;

    // Clear sprites
    if (this->line_cycle == 0) {
        GB_PPU_set_mode(this, 2); // OAM search
    }

    switch (this->clock->ppu_mode) {
        case 2: // OAM search 0-80
            // 80 dots long, 2 per entry, find up to 10 sprites 0...n on this line
            GB_PPU_OAM_search(this);
            if (this->line_cycle == 79) GB_PPU_set_mode(this, 3);
            break;
        case 3: // Pixel transfer. Complicated.
            GB_PPU_pixel_transfer(this);
            if (this->clock->lx > 167)
                GB_PPU_set_mode(this, 0);
            break;
    }
}

void GB_PPU_advance_line(struct GB_PPU* this) {
    if (this->window_triggered_on_line) this->clock->wly++;
    this->display->scan_x = 0;
    this->display->scan_y++;
    this->clock->lx = 0;
    this->clock->ly++;
    this->is_window_line |= this->clock->ly == this->io.wy;
    this->window_triggered_on_line = false;
    this->line_cycle = 0;
    if (this->clock->ly >= 154)
        GB_PPU_advance_frame(this, true);
    if (this->enabled) {
        GB_PPU_eval_lyc(this);
        if (this->clock->ly < 144)
            GB_PPU_set_mode(this, OAM_search); // OAM search
        else if (this->clock->ly == 144)
            GB_PPU_set_mode(this, VBLANK); // VBLANK
    }
}

void GB_PPU_run_cycles(struct GB_PPU *this, u32 howmany)
{
    // We don't do anything, and in fact are off, if LCD is off
    for (u32 i = 0; i < howmany; i++) {
        if (this->cycles_til_vblank) {
            this->cycles_til_vblank--;
            if (this->cycles_til_vblank == 0)
                this->bus->IRQ_vblank_up(this->bus);
        }
        if (this->enabled) {
            GB_PPU_cycle(this);
            this->line_cycle++;
            this->display->scan_x++;
            if (this->line_cycle == 456) GB_PPU_advance_line(this);
        }
        //if (dbg.do_break) break;
    }
}

void GB_PPU_quick_boot(struct GB_PPU* this)
{
    switch (this->variant) {
    case DMG:
        this->enabled = true;
        //let val = 0xFC;
        //this->clock->ly = 90;
        this->bus->write_IO(this->bus, 0xFF47, 0xFC);
        this->bus->write_IO(this->bus, 0xFF40, 0x91);
        this->bus->write_IO(this->bus, 0xFF41, 0x85);
        this->io.lyc = 0;
        this->io.SCX = this->io.SCY = 0;

        GB_PPU_advance_frame(this, true);
        break;
    default: // CGB
        for (u32 i = 0; i < 0x3F; i++) {
            this->bg_CRAM[i] = 0xFF;
        }
        this->bus->write_IO(this->bus, 0xFF47, 0xFC);
        this->bus->write_IO(this->bus, 0xFF40, 0x91);
        this->bus->write_IO(this->bus, 0xFF41, 0x85);
        this->io.lyc = 0;
        this->io.SCX = this->io.SCY = 0;

        // TODO: add advance_frame?
        break;
    }
}

static void GB_PPU_OAM_search(struct GB_PPU* this)
{
    u32 i;
    if (this->line_cycle != 75) return;

    // Check if a sprite is at the right place
    this->sprites.num = 0;
    this->sprites.index = 0;
    this->sprites.search_index = 0;
    for (i = 0; i < 10; i++) {
        this->OBJ[i].x = 0;
        this->OBJ[i].y = 0;
        this->OBJ[i].in_q = 0;
        this->OBJ[i].num = 60;
    }

    for (i = 0; i < 40; i++) {
        if (this->sprites.num == 10) break;
        u32 sy = this->OAM[this->sprites.search_index] - 16;
        u32 sy_bottom = sy + (this->io.sprites_big ? 16 : 8);
        if ((this->clock->ly >= sy) && (this->clock->ly < sy_bottom)) {
            this->OBJ[this->sprites.num].y = sy;
            this->OBJ[this->sprites.num].x = this->OAM[this->sprites.search_index + 1] - 1;
            this->OBJ[this->sprites.num].tile = this->OAM[this->sprites.search_index + 2];
            this->OBJ[this->sprites.num].attr = this->OAM[this->sprites.search_index + 3];
            this->OBJ[this->sprites.num].in_q = 0;
            this->OBJ[this->sprites.num].num = i;

            this->sprites.num++;
        }
        this->sprites.search_index += 4;
    }
}

static void GB_PPU_pixel_transfer(struct GB_PPU *this)
{
    if ((this->io.window_enable) && ((this->clock->lx) == this->io.wx) && this->is_window_line && !this->window_triggered_on_line) {
        GB_pixel_slice_fetcher_trigger_window(&this->slice_fetcher);
        this->window_triggered_on_line = true;
    }
    struct GB_px* p = GB_pixel_slice_fetcher_cycle(&this->slice_fetcher);

    if (p->had_pixel) {
        if (this->clock->lx > 7) {
            u16 cv;
            if (this->clock->cgb_enable) {
                if (p->bg_or_sp == 0) {
                    // there are 8 BG palettes
                    // each pixel is 0-4
                    // so it's (4 * palette #) + color
                    u32 n = ((p->palette * 4) + p->color) * 2;
                    cv = (((u16)this->bg_CRAM[n+1]) << 8) | this->bg_CRAM[n];
                } else {
                    u32 n = ((p->palette * 4) + p->color) * 2;
                    cv = (((u16)this->obj_CRAM[n+1]) << 8) | this->obj_CRAM[n];
                }
            } else {
                if (p->bg_or_sp == 0) {
                    cv = this->bg_palette[p->color];
                } else {
                    cv = this->sp_palette[p->palette][p->color];
                }
            }
            this->cur_output[(this->clock->ly * 160) + (this->clock->lx - 8)] = cv;
        }
        this->clock->lx++;
    }
}