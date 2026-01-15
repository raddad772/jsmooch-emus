#include <cassert>
#include <cstdio>
#include <cstring>

#include "helpers/int.h"

#include "gb_bus.h"
#include "gb_ppu.h"
#include "gb_enums.h"
#include "gb_cpu.h"
#include "gb_clock.h"
#include "gb_debugger.h"


namespace GB::PPU {

static u32 GBC_resolve_priority(u32 LCDC0, u32 OAM7, u32 BG7, u32 bg_color, u32 sp_color);
static u32 GB_sp_tile_addr(u32 tn, u32 y, u32 big_sprites, u32 attr, u32 cgb);

bool core::in_window() const {
    return io.window_enable && is_window_line && (clock->lx >= io.wx);
}

static inline u32 GBC_resolve_priority(u32 LCDC0, u32 OAM7, u32 BG7, u32 bg_color, u32 sp_color) {
// 1 = BG
// 2 = OBJ
    if (sp_color == 0) return 1;
    if (!LCDC0) return 2;
    if ((!OAM7) && (!BG7)) return 2;
    if (bg_color > 0) return 1;
    return 2;
}

static inline u32 GB_sp_tile_addr(u32 tn, u32 y, u32 big_sprites, u32 attr, u32 cgb) {
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

void FIFO_item::init(u32 pixel_in, u32 palette_in, u32 cgb_priority_in, u32 sprite_priority_in) {
    pixel = pixel_in;
    palette = palette_in;
    cgb_priority = cgb_priority_in;
    sprite_priority = sprite_priority_in;
    sprite_obj = nullptr;
}

FIFO::FIFO(variants variant_in, u32 max_in) : variant(variant_in), max(max_in) {
    compat_mode = (variant == GBC) ? 0 : 1;

}

void FIFO::set_cgb_mode(bool on) {
    compat_mode = on ? 0 : 1;
}

void FIFO::clear() {
    head = tail = num_items = 0;
}

bool FIFO::empty() const {
    return num_items == 0;
}

FIFO_item* FIFO::push()
{
    if (num_items >= max) {
        return nullptr;
    }
    auto *r = &(items[tail]);
    tail = (tail + 1) % max;
    num_items++;
    return r;
};

// This is for "mixing" on sprite encounter
void FIFO::sprite_mix(u32 bp0, u32 bp1, u32 attr, u32 sprite_num)
{
    u32 sp_palette;
    u32 sp_priority = (attr & 0x80) >> 7;
    if (compat_mode == 1) {
        sp_palette = (attr & 0x10) >> 4;
    }
    else {
        sp_palette = attr & 7;
    }
    u32 flip = attr & 0x20;
    // First fill with transparent pixels
    while (num_items < 8) {
        auto *b = push();
        b->sprite_priority = 0;
        b->cgb_priority = 60;
        b->pixel = 0;
        b->palette = 0;
    }

    for (u32 j = 0; j < 8; j++) {
        u32 r = j;
        if (flip) r = 7 - r;
        u32 i = (r + head) & 7;
        u32 px = ((bp0 & 0x80) >> 7) | ((bp1 & 0x80) >> 6);
        bp0 <<= 1;
        bp1 <<= 1;
        if (compat_mode == 1) {
            // DMG/CGB compatability
            if (items[i].pixel == 0) {
                items[i].palette = sp_palette;
                items[i].sprite_priority = sp_priority;
                items[i].pixel = px;
                items[i].cgb_priority = 0;
            }
        }
        else {
            // GBC mode
            if (((sprite_num < items[i].cgb_priority) && px != 0) || (items[i].pixel == 0)) {
                items[i].palette = sp_palette;
                items[i].sprite_priority = sp_priority;
                items[i].pixel = px;
                items[i].cgb_priority = sprite_num;
            }
        }
    }
}

void FIFO::discard(u32 num) {
    for (u32 i = 0; i < num; i++) {
        pop();
    }
}

FIFO_item* FIFO::peek() {
    if (num_items == 0) return nullptr;
    return &items[head];
}

FIFO_item* FIFO::pop() {
    if (num_items == 0) return &blank;
    auto *r = &items[head];
    head = (head + 1) % max;
    num_items--;
    return r;
};

pixel_slice_fetcher::pixel_slice_fetcher(GB::clock *clock_in, GB::core *bus_in, core *parent, variants variant_in) :
    variant(variant_in),
    ppu(parent),
    clock(clock_in),
    bus(bus_in),
    bg_FIFO(variant_in, 8),
    obj_FIFO(variant_in, 8),
    sprites_queue(variant_in, 10)
{

}

void pixel_slice_fetcher::advance_line() {
    fetch_cycle = 0;
    bg_FIFO.clear();
    obj_FIFO.clear();
    sprites_queue.clear();
    bg_request_x = ppu->io.SCX;
    sp_request = 0;
}

void pixel_slice_fetcher::trigger_window() {
    bg_FIFO.clear();
    bg_request_x = 0;
    fetch_cycle = 0;
}

px *pixel_slice_fetcher::cycle() {
    auto *r = get_px_if_available();
    run_fetch_cycle();
    return r;
}

px *pixel_slice_fetcher::get_px_if_available() {
    out_px.had_pixel = false;
    out_px.bg_or_sp = -1;
    if ((sp_request == 0) && (!bg_FIFO.empty())) { // if we're not in a sprite request, and the BG FIFO isn't empty
        out_px.had_pixel = true;
        u32 has_bg = clock->cgb_enable ? true : ppu->io.bg_window_enable;
        u32 bg_pal = 0;

        auto *bg = bg_FIFO.pop();
        u32 bg_color = bg->pixel;
        u32 has_sp = false;
        i32 sp_color = -1;
        u32 sp_palette;
        u32 use_what = 0; // 0 for BG, 1 for OBJ
        FIFO_item* obj;
        if (clock->cgb_enable)
            bg_pal = bg->palette;

        if (!obj_FIFO.empty()) {
            obj = obj_FIFO.pop();
            sp_color = static_cast<i32>(obj->pixel);
            sp_palette = obj->palette;
        }
        if (ppu->io.obj_enable && (sp_color != -1)) has_sp = true;
        if ((has_sp || (sp_color != -1)) && clock->lx >= 8) {
            bus->dbg_data.row->sprite_pixels[clock->lx-8] = 1;
        }

        // DMG resolve
        if ((has_bg) && (!has_sp)) {
            use_what = 1;
        } else if ((!has_bg) && (has_sp)) {
            use_what = 1;
        } else if (has_bg && has_sp) {
            if (clock->cgb_enable) {
                use_what = GBC_resolve_priority(ppu->io.bg_window_enable, obj->sprite_priority, bg->sprite_priority, bg_color, sp_color);
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
            out_px.color = 0;
        }

        if (use_what == 0) {
        }
        else if (use_what == 1) {
            out_px.bg_or_sp = 0;
            out_px.color = bg_color;
            out_px.palette = bg_pal;
        } else {
            out_px.bg_or_sp = 1;
            out_px.color = sp_color;
            out_px.palette = sp_palette;
        }
    }
    return &out_px;
}

void pixel_slice_fetcher::run_fetch_cycle() {
    // Scan any sprites
    for (u32 i = 0; i < ppu->sprites.num; i++) {
        if ((!ppu->OBJ[i].in_q) && (ppu->OBJ[i].x == clock->lx)) {
            sp_request++;
            sprites_queue.push()->sprite_obj = &ppu->OBJ[i];
            ppu->OBJ[i].in_q = 1;
        }
    }

    u32 tn, addr;
    switch (fetch_cycle) {
    case 0: // nothing
        fetch_cycle = 1;
        break;
    case 1: // tile
        if (ppu->in_window()) {
            addr = ppu->bg_tilemap_addr_window(bg_request_x);
            if (clock->cgb_enable) {
                fetch_cgb_attr = bus->PPU_read(addr + 0x2000);
            }
            tn = bus->PPU_read(addr);
            fetch_addr = ppu->bg_tile_addr_window(tn, fetch_cgb_attr);
        }
        else {
            addr = ppu->bg_tilemap_addr_nowindow(bg_request_x);
            if (clock->cgb_enable) {
                fetch_cgb_attr = bus->PPU_read(addr + 0x2000);
            }
            tn = bus->PPU_read(addr);
            fetch_addr = ppu->bg_tile_addr_nowindow(tn, fetch_cgb_attr);
        }
        fetch_cycle = 2;
        break;
    case 2: // nothing
        fetch_cycle = 3;
        break;
    case 3: // bp0
        fetch_bp0 = bus->PPU_read(fetch_addr);
        fetch_cycle = 4;
        break;
    case 4: // nothing
        fetch_cycle = 5;
        break;
    case 5: // bp1.
        fetch_bp1 = bus->PPU_read(fetch_addr + 1);
        //if (ppu->in_window()) fetch_bp1 = 0x55;
        fetch_cycle = 6;
        break;
    case 6: // attempt background push. also hijack by sprite, so sprite cycle #0
        if (sp_request && (!bg_FIFO.empty())) { // SPRITE HIJACK!
            fetch_cycle = 7;
            //sprites_queue.peek()!.sprite_obj
            fetch_obj = sprites_queue.peek()->sprite_obj;
            fetch_addr = GB_sp_tile_addr(fetch_obj->tile, clock->ly - fetch_obj->y, ppu->io.sprites_big, fetch_obj->attr, clock->cgb_enable);
            break;
        }
        else { // attempt to push to BG FIFO, kind only accepts when empty.
            if (bg_FIFO.empty()) {
                // Push to FIFO
                for (u32 i = 0; i < 8; i++) {
                    auto * b = bg_FIFO.push();
                    if ((clock->cgb_enable) && (fetch_cgb_attr & 0x20)) {
                        // Flipped X
                        b->pixel = (fetch_bp0 & 1) | ((fetch_bp1 & 1) << 1);
                        fetch_bp0 >>= 1;
                        fetch_bp1 >>= 1;
                    }
                    else {
                        // For regular, X not flipped
                        b->pixel = ((fetch_bp0 & 0x80) >> 7) | ((fetch_bp1 & 0x80) >> 6);
                        fetch_bp0 <<= 1;
                        fetch_bp1 <<= 1;
                    }
                    b->palette = 0;
                    if (clock->cgb_enable) {
                        b->palette = fetch_cgb_attr & 7;
                        b->sprite_priority = (fetch_cgb_attr & 0x80) >> 7;
                    }
                }
                bg_request_x += 8;
                // This is the first fetch of the line, so discard up to 7 pixels from the first fetch for scrolling purposes
                if ((ppu->line_cycle < 88) && !ppu->in_window()) {
                    bg_request_x -= 8;
                    // Now discard some pixels for scrolling!
                    u32 sx = ppu->io.SCX & 7;
                    bg_FIFO.discard(sx);
                }
                fetch_cycle = 0; // Restart fetching
            }
        }
        break;
    case 7: // sprite bp0 fetch, cycle 1
        fetch_cycle = 8;
        break;
    case 8: // nothing, cycle 2
        fetch_cycle = 9;
        break;
    case 9: // spr cycle 3
        fetch_cycle = 10;
        spfetch_bp0 = bus->PPU_read(fetch_addr);
        break;
    case 10: // spr cycle 4
        fetch_cycle = 11;
        break;
    case 11: // spr cycle 5. sprite bp1 fetch, mix, & restart
        spfetch_bp1 = bus->PPU_read(fetch_addr + 1);
        obj_FIFO.sprite_mix(spfetch_bp0, spfetch_bp1, fetch_obj->attr, fetch_obj->num);
        sp_request--;
        sprites_queue.pop();
        fetch_cycle = 6; // restart hijack/sprite process or push process
        break;
    }
}

void core::reset()
{
    // Reset variables
    clock->lx = 0;
    clock->ly = 0;
    display->scan_x = display->scan_y = 0;
    line_cycle = 0;
    cycles_til_vblank = 0;

    // Reset IRQs
    //io.STAT_IE = 0; // Interrupt enables
    //io.STAT_IF = 0; // Interrupt flags
    //io.old_mask = 0;

    //eval_STAT();

    // Set mode to OAM search
    set_mode(PPU::OAM_search);
}

core::core(GB::variants variant_in, GB::clock *clock_in, GB::core *parent) :
    variant(variant_in),
    clock(clock_in),
    bus(parent),
    slice_fetcher(clock_in, parent, this, variant_in){

    disable();
}

void core::disable() {
    if (!enabled) return;
    //printf("\nDISABLE PPU %d", clock->master_clock);
    enabled = false;
    if (display) display->enabled = false;
    clock->CPU_can_VRAM = true;
    bus->clock.setCPU_can_OAM(true);
    io.STAT_IF = 0;
    eval_STAT();
}

void core::enable() {
    if (enabled) return;
    enabled = true;
    if (display) {
        display->enabled = true;
        display->scan_x = display->scan_y = 0;
    }
    advance_frame(false);
    clock->lx = 0;
    clock->ly = 0;
    line_cycle = 0;
    cycles_til_vblank = 0;
    io.STAT_IF = 0;
    set_mode(PPU::OAM_search);
    eval_lyc();
    eval_STAT();
}

void core::eval_STAT() {
    u32 mask = io.STAT_IF & io.STAT_IE;
    if ((io.old_mask == 0) && (mask != 0)) {
        bus->cpu.cpu.regs.IF |= 2;
    }
    io.old_mask = mask;
}

u32 core::bg_tilemap_addr_window(u32 wlx) const {
    return (0x9800 | (io.window_tile_map_base << 10) |
        ((clock->wly >> 3) << 5) |
        (wlx >> 3)
    );
}
u32 core::bg_tilemap_addr_nowindow(u32 lx) {
    return (0x9800 | (io.bg_tile_map_base << 10) |
            ((((clock->ly + io.SCY) & 0xFF) >> 3) << 5) |
    (((lx) & 0xFF) >> 3));
}

u32 core::bg_tile_addr_window(u32 tn, u32 attr) const {
    u32 hbits = 0;
    u32 b12 = io.bg_window_tile_data_base ? 0 : ((tn & 0x80) ^ 0x80) << 5;
    u32 ybits = clock->wly & 7;
    if (clock->cgb_enable) {
        hbits = (attr & 8) << 10; // hbits = 0 or 0x2000  hbits = 0x2000;
        if (attr & 0x40) ybits = (7 - ybits);
    }
    return (0x8000 | b12 |
            (tn << 4) |
            (ybits << 1)
           ) + hbits;
}

u32 core::bg_tile_addr_nowindow(u32 tn, u32 attr) const {
    u32 b12 = io.bg_window_tile_data_base ? 0 : ((tn & 0x80) ^ 0x80) << 5;
    u32 hbits = 0;
    u32 ybits = (clock->ly + io.SCY) & 7;
    if (clock->cgb_enable) {
        hbits = (attr & 8) << 10; // hbits = 0 or 0x2000  hbits = 0x2000;
        if (attr & 0x40) ybits = (7 - ybits);
    }
    return (0x8000 | b12 |
            (tn << 4) |
            (ybits << 1)
           ) + hbits;
}

void core::write_OAM(u32 addr, u32 val) {
    if (addr < 0xFEA0) OAM[addr - 0xFE00] = (u8)val;
}

u32 core::read_OAM(u32 addr) {
    if (addr >= 0xFEA0) return 0xFF;
    return OAM[addr - 0xFE00];
}

void core::write_IO(u32 addr, u32 val) {
    switch (addr) {
    case 0xFF01: // Serial write
        /*let nstr = String.fromCharCode(val);
        console_str += nstr;
        console.log(console_str);*/
        break;
    case 0xFF40: // LCDC LCD Control
        if (val & 0x80) enable();
        else disable();
        DBG_EVENT(DBG_GB_EVENT_LCDC_WRITE);

        io.window_tile_map_base = (val & 0x40) >> 6;
        io.window_enable = (val & 0x20) >> 5;
        io.bg_window_tile_data_base = (val & 0x10) >> 4;
        io.bg_tile_map_base = (val & 8) >> 3;
        io.sprites_big = (val & 4) >> 2;
        io.obj_enable = (val & 2) >> 1;
        io.bg_window_enable = val & 1;
        return;
    case 0xFF41: {
        // STAT LCD status
        if (variant == DMG) { // Can cause spurious interrupt writing this
            io.STAT_IE = 0x0F;
            eval_STAT();
        }
        u32 mode0_enable = (val & 8) >> 3;
        u32 mode1_enable = (val & 0x10) >> 4;
        u32 mode2_enable = (val & 0x20) >> 5;
        u32 lylyc_enable = (val & 0x40) >> 6;
        //printf("\n41: frame:%d line:%d line_cycle:%d STAT:$%02x", clock->master_frame, clock->ly, line_cycle, val);

        io.STAT_IE = mode0_enable | (mode1_enable << 1) | (mode2_enable << 2) | (lylyc_enable << 3);
        eval_STAT();
        DBG_EVENT(DBG_GB_EVENT_STAT_WRITE);
        //printf("\nWRITE STAT_IE frame:%d line:%d val:%02x", clock->master_frame, clock->ly, val);
        return; }
    case 0xFF42: // SCY
        DBG_EVENT(DBG_GB_EVENT_SCY_WRITE);
        io.SCY = val;
        return;
    case 0xFF43: // SCX
        DBG_EVENT(DBG_GB_EVENT_SCX_WRITE);
        io.SCX = val;
        return;
    case 0xFF45: // LYC
        io.lyc = val;
        //printf("\n45: frame:%d line:%d line_cycle:%d LYC:$%02x", clock->master_frame, clock->ly, line_cycle, io.lyc);
        if (enabled)  eval_lyc();
        return;
    case 0xFF4A: // window Y
        io.wy = val;
        return;
    case 0xFF4B: // window counter + 7
        io.wx = val + 1;
        return;
    case 0xFF47: // BGP pallete
        //if (!clock->CPU_can_VRAM) return;
        DBG_EVENT(DBG_GB_EVENT_BGP_WRITE);
        bg_palette[0] = static_cast<u8>(val & 3);
        bg_palette[1] = static_cast<u8>((val >> 2) & 3);
        bg_palette[2] = static_cast<u8>((val >> 4) & 3);
        bg_palette[3] = static_cast<u8>((val >> 6) & 3);
        //printf("\nWrite to BG pallette %02x on frame %d", val, clock->master_frame);
        return;
    case 0xFF48: // OBP0 sprite palette 0
        //if (!clock->CPU_can_VRAM) return;
        sp_palette[0][0] = static_cast<u8>(val & 3);
        sp_palette[0][1] = static_cast<u8>((val >> 2) & 3);
        sp_palette[0][2] = static_cast<u8>((val >> 4) & 3);
        sp_palette[0][3] = static_cast<u8>((val >> 6) & 3);
        return;
    case 0xFF49: // OBP1 sprite palette 1
        //if (!clock->CPU_can_VRAM) return;
        sp_palette[1][0] = static_cast<u8>(val & 3);
        sp_palette[1][1] = static_cast<u8>((val >> 2) & 3);
        sp_palette[1][2] = static_cast<u8>((val >> 4) & 3);
        sp_palette[1][3] = static_cast<u8>((val >> 6) & 3);
        return;
    case 0xFF68: // BCPS/BGPI
        if (!clock->cgb_enable) return;
        io.cram_bg_increment = (val & 0x80) >> 7;
        io.cram_bg_addr = val & 0x3F;
        return;
    case 0xFF69: // BG pal write
        if (!clock->cgb_enable) return;
        bg_CRAM[io.cram_bg_addr] = val;
        if (io.cram_bg_increment) io.cram_bg_addr = (io.cram_bg_addr + 1) & 0x3F;
        return;
    case 0xFF6A: // OPS/OPI
        if (!clock->cgb_enable) return;
        io.cram_obj_increment = (val & 0x80) >> 7;
        io.cram_obj_addr = val & 0x3F;
        return;
    case 0xFF6B: // OBJ pal write
        if (!clock->cgb_enable) return;
        obj_CRAM[io.cram_obj_addr] = val;
        if (io.cram_obj_increment) io.cram_obj_addr = (io.cram_obj_addr + 1) & 0x3F;
        return;
    }
}

u32 core::read_IO(u32 addr, u32 val) {
    u32 e;
    u32 mode0_enable, mode1_enable, mode2_enable, lylyc_enable;
    u32 ly;
    switch (addr) {
        case 0xFF40: // LCDC LCD Control
            e = enabled ? 0x80 : 0;
            return e | (io.window_tile_map_base << 6) | (io.window_enable << 5) | (io.bg_window_tile_data_base << 4) |
                (io.bg_tile_map_base << 3) | (io.sprites_big << 2) |
                (io.obj_enable << 1) | (io.bg_window_enable);
        case 0xFF41: {// STAT LCD status
            mode0_enable = io.STAT_IE & 1;
            mode1_enable = (io.STAT_IE & 2) >> 1;
            mode2_enable = (io.STAT_IE & 4) >> 2;
            lylyc_enable = (io.STAT_IE & 8) >> 3;
            u32 mode = enabled ? clock->ppu_mode : 0;
            return mode |
                ((clock->ly == io.lyc) ? 1 : 0) |
                (mode0_enable << 3) |
                (mode1_enable << 4) |
                (mode2_enable << 5) |
                (lylyc_enable << 6); }
        case 0xFF42: // SCY
            return io.SCY;
        case 0xFF43: // SCX
            return io.SCX;
        case 0xFF44: // LY
            //printf("\nRETURNING SCY %d %d %d", clock->ly, clock->master_frame, io.SCY);
            //fflush(stdout);
            ly = clock->ly;
            if ((ly == 153) && (line_cycle > 1)) ly = 0;
            return ly;
        case 0xFF45: // LYC
            return io.lyc;
        case 0xFF4A: // window Y
            return io.wy;
        case 0xFF4B: // window counter + 7
            return (io.wx-1);
        case 0xFF47: // BGP
            //if (!clock->CPU_can_VRAM) return 0xFF;
            return bg_palette[0] | (bg_palette[1] << 2) | (bg_palette[2] << 4) | (bg_palette[3] << 6);
        case 0xFF48: // OBP0
            //if (!clock->CPU_can_VRAM) return 0xFF;
            return sp_palette[0][0] | (sp_palette[0][1] << 2) | (sp_palette[0][2] << 4) | (sp_palette[0][3] << 6);
        case 0xFF49: // OBP1
            //if (!clock->CPU_can_VRAM) return 0xFF;
            return sp_palette[1][0] | (sp_palette[1][1] << 2) | (sp_palette[1][2] << 4) | (sp_palette[1][3] << 6);
        case 0xFF68: // BCPS/BGPI
            if (!clock->cgb_enable) return 0xFF;
            return io.cram_bg_addr | (io.cram_bg_increment << 7);
        case 0xFF69: // BG pal read
            if (!clock->cgb_enable) return 0xFF;
            return bg_CRAM[io.cram_bg_addr];
        case 0xFF6A: // OPS/OPI
            if (!clock->cgb_enable) return 0xFF;
            return io.cram_obj_addr | (io.cram_obj_increment << 7);
        case 0xFF6B: // OBJ pal read
            if (!clock->cgb_enable) return 0xFF;
            return obj_CRAM[io.cram_obj_addr];
    }
    return 0xFF;
}

void core::IRQ_lylyc_up() {
    io.STAT_IF |= 8;
    eval_STAT();
}

void core::IRQ_lylyc_down() {
    io.STAT_IF &= 0xF7;
    eval_STAT();
}

void core::IRQ_mode0_up() {
    io.STAT_IF |= 1;
    eval_STAT();
}

void core::IRQ_mode0_down() {
    io.STAT_IF &= 0xFE;
    eval_STAT();
}

void core::IRQ_vblank_up() {
    cycles_til_vblank = 2;
}

void core::IRQ_mode1_up() {
    io.STAT_IF |= 2;
    eval_STAT();
}

void core::IRQ_mode1_down() {
    io.STAT_IF &= 0xFD;
    eval_STAT();
}

void core::IRQ_mode2_up() {
    io.STAT_IF |= 4;
    eval_STAT();
}

void core::IRQ_mode2_down() {
    io.STAT_IF &= 0xFB;
    eval_STAT();
}


void core::eval_lyc() {
    u32 cly = clock->ly;
    if ((cly == 153) && (io.lyc != 153)) cly = 0;
    if (cly == io.lyc) {
        IRQ_lylyc_up();
    }
    else
        IRQ_lylyc_down();
}

void core::advance_frame(bool update_buffer) {
    debugger_report_frame(bus->dbg.interface);
    clock->ly = 0;
    clock->wly = 0;
    display->scan_y = 0;
    display->scan_x = 0;
    is_window_line = clock->ly == io.wy;
    if (enabled) {
        eval_lyc();
    }
    clock->frames_since_restart++;
    clock->master_frame++;
    if (update_buffer) {
        cur_output = static_cast<u16 *>(display->output[display->last_written]);
        cur_output_debug_metadata = static_cast<u16 *>(display->output_debug_metadata[display->last_written]);
        display->last_written ^= 1;
    }
}

void core::set_mode(modes which) {
    if (clock->ppu_mode == which) return;
    clock->ppu_mode = which;

    switch (which) {
    case PPU::OAM_search: {// 2. after vblank and HBLANK
        clock->setCPU_can_OAM(false);
        clock->CPU_can_VRAM = true;
        if (enabled) {
            bus->IRQ_vblank_down();
            IRQ_mode0_down();
            IRQ_mode1_down();
            IRQ_mode2_up();
        }
        break;}
    case PPU::pixel_transfer: // 3, comes after 2
        IRQ_mode2_down();
        clock->CPU_can_VRAM = false;
        clock->setCPU_can_OAM(false);
        slice_fetcher.advance_line();
        break;
    case PPU::HBLANK: // 0, comes after 3
        bus->cpu.hdma.notify_hblank = true;
        IRQ_mode0_up();
        clock->CPU_can_VRAM = true;
        clock->setCPU_can_OAM(true);
        break;
    case VBLANK: // 1, comes after 0 (hblank)
        IRQ_mode0_down();
        IRQ_mode1_up();
        IRQ_vblank_up();
        clock->CPU_can_VRAM = true;
        clock->setCPU_can_OAM(true);
        break;
    }
}

void core::cycle() {
    // During HBlank and VBlank do nothing...
    display->scan_x++;
    if (clock->ly > 143) {
        if (display->scan_x >= 456) {
            display->scan_y++;
            display->scan_x = 0;
        }
        if (display->scan_y > 153) display->scan_y = 153;
        //return;
    }
    if (clock->ppu_mode < 2) return;

    // Clear sprites
    if (line_cycle == 0) {
        set_mode(PPU::OAM_search); // OAM search
    }

    switch (clock->ppu_mode) {
        case 2: // OAM search 0-80
            // 80 dots long, 2 per entry, find up to 10 sprites 0...n on this line
            OAM_search();
            if (line_cycle == 79) set_mode(PPU::pixel_transfer);
            break;
        case 3: // Pixel transfer. Complicated.
            pixel_transfer();
            if (clock->lx > 167)
                set_mode(PPU::HBLANK);
            break;
    }
}

void core::advance_line() {
    if (window_triggered_on_line) clock->wly++;
    display->scan_x = 0;
    display->scan_y++;
    clock->lx = 0;
    clock->ly++;
    is_window_line |= clock->ly == io.wy;
    window_triggered_on_line = false;
    line_cycle = 0;
    if (clock->ly >= 154)
        advance_frame(true);

    if (clock->ly < 144) {
        bus->dbg_data.row = (&bus->dbg_data.rows[clock->ly]);
        DBGGBROW *rw = bus->dbg_data.row;
        rw->io.SCX = io.SCX;
        rw->io.SCY = io.SCY;
        rw->io.bg_tile_map_base = io.bg_tile_map_base;
        rw->io.window_tile_map_base = io.window_tile_map_base;
        rw->io.window_enable = io.window_enable;
        rw->io.wx = io.wx;
        rw->io.wy = io.wy;
        rw->io.bg_window_tile_data_base = io.bg_window_tile_data_base;
        memset(&rw->sprite_pixels, 0xFF, 160*2);
    }
    if (enabled) {
        eval_lyc();
        if (clock->ly < 144)
            set_mode(PPU::OAM_search); // OAM search
        else if (clock->ly == 144)
            set_mode(PPU::VBLANK); // VBLANK
    }
}

void core::run_cycles(u32 howmany)
{
    // We don't do anything, and in fact are off, if LCD is off
    for (u32 i = 0; i < howmany; i++) {
        if (cycles_til_vblank) {
            cycles_til_vblank--;
            if (cycles_til_vblank == 0)
                bus->IRQ_vblank_up();
        }
        if (enabled) {
            cycle();
            line_cycle++;
            if (line_cycle == 456) advance_line();
        }
        //if (::dbg.do_break) break;
    }
}

void core::quick_boot()
{
    switch (variant) {
    case DMG:
        enabled = true;
        //let val = 0xFC;
        //clock->ly = 90;
        bus->write_IO(0xFF47, 0xFC);
        bus->write_IO(0xFF40, 0x91);
        bus->write_IO(0xFF41, 0x85);
        io.lyc = 0;
        io.SCX = io.SCY = 0;

        advance_frame(true);
        break;
    default: // CGB
        for (u32 i = 0; i < 0x3F; i++) {
            bg_CRAM[i] = 0xFF;
        }
        bus->write_IO(0xFF47, 0xFC);
        bus->write_IO(0xFF40, 0x91);
        bus->write_IO(0xFF41, 0x85);
        io.lyc = 0;
        io.SCX = io.SCY = 0;

        // TODO: add advance_frame?
        break;
    }
}

void core::OAM_search()
{
    u32 i;
    if (line_cycle != 75) return;

    // Check if a sprite is at the right place
    sprites.num = 0;
    sprites.index = 0;
    sprites.search_index = 0;
    for (i = 0; i < 10; i++) {
        OBJ[i].x = 0;
        OBJ[i].y = 0;
        OBJ[i].in_q = 0;
        OBJ[i].num = 60;
    }

    for (i = 0; i < 40; i++) {
        if (sprites.num == 10) break;
        const i32 sy = static_cast<i32>(OAM[sprites.search_index]) - 16;
        i32 sy_bottom = sy + (io.sprites_big ? 16 : 8);
        if ((static_cast<i32>(clock->ly) >= sy) && (static_cast<i32>(clock->ly) < sy_bottom)) {
            OBJ[sprites.num].y = sy;
            OBJ[sprites.num].x = OAM[sprites.search_index + 1];
            OBJ[sprites.num].tile = OAM[sprites.search_index + 2];
            OBJ[sprites.num].attr = OAM[sprites.search_index + 3];
            OBJ[sprites.num].in_q = 0;
            OBJ[sprites.num].num = i;

            sprites.num++;
        }
        sprites.search_index += 4;
    }
}

void core::pixel_transfer()
{
    if ((io.window_enable) && ((clock->lx) == io.wx) && is_window_line && !window_triggered_on_line) {
        slice_fetcher.trigger_window();
        window_triggered_on_line = true;
    }
    auto* p = slice_fetcher.cycle();

    if (p->had_pixel) {
        if (clock->lx > 7) {
            u16 cv;
            if (clock->cgb_enable) {
                if (p->bg_or_sp == 0) {
                    // there are 8 BG palettes
                    // each pixel is 0-4
                    // so it's (4 * palette #) + color
                    u32 n = ((p->palette * 4) + p->color) * 2;
                    cv = (static_cast<u16>(bg_CRAM[n + 1]) << 8) | bg_CRAM[n];
                } else {
                    u32 n = ((p->palette * 4) + p->color) * 2;
                    cv = (static_cast<u16>(obj_CRAM[n + 1]) << 8) | obj_CRAM[n];
                }
            } else {
                if (p->bg_or_sp == 0) {
                    cv = bg_palette[p->color];
                } else {
                    cv = sp_palette[p->palette][p->color];
                }
            }
            cur_output[(clock->ly * 160) + (clock->lx - 8)] = cv;
            cur_output_debug_metadata[(clock->ly * 160) + (clock->lx - 8)] = display->scan_x;
        }
        clock->lx++;
    }
}

}