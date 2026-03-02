//
// Created by Dave on 2/11/2024.
//

// chu chu rocket
// AICA stub
// It'll try to read the 32-bit word @ 0x8000F8 and expect it to have the value 0x43455845, it just hangs if that address contains anything else

#include <cassert>

//#include "gdi.h"
#include "gdrom.h"
#include "helpers/scheduler.h"
#include "helpers/sys_interface.h"
#include "dreamcast.h"
#include "dc_mem.h"
#include "dc_bus.h"
#include "holly.h"
#include "controller.h"

#ifdef DC_SUPPORT_ELF
extern "C" {
#include "vendor/elf-parser/elf-parser.h"
    }
#endif

#define IP_BIN

#define sched_printf(...) (void)0

#define DC_CYCLES_PER_FRAME (DC_CYCLES_PER_SEC / 60)


#define JITTER 448

jsm_system *DC_new()
{
    return new DC::core();
}

void DC_delete(jsm_system *sys) {
    delete sys;
}

namespace DC {

void core::set_audiobuf(audiobuf *ab)
{}

void core::setup_debugger_interface(debugger_interface &intf)
{
    intf.supported_by_core = false;
    printf("\nWARNING: debugger interface not supported on core: dreamcast");
}

void core::copy_fb(u32* where) {
    auto* ptr = reinterpret_cast<u32 *>(VRAM);
    ptr += holly.FB_R_SOF1.field;
    //ptr += 0x00020000;

    //printf("\nRENDER USING PTR %08llx", ptr - ((u32*)VRAM));

    u32* out;
    for (u32 y = 0; y <= holly.FB_R_SIZE.fb_y_size; y++) {
        out = (where + (y * 640));
        for (u32 x = 0; x <= holly.FB_R_SIZE.fb_x_size; x++) {
            auto *rgb = reinterpret_cast<u8 *>(ptr);
            const u32 r = rgb[0];
            const u32 g = rgb[1];
            const u32 b = rgb[2];
            *out = (b << 16) | (g << 8) | (r) | 0xFF000000;
            ptr++;
            out++;
        }
        ptr += (holly.FB_R_SIZE.fb_modulus) - 1;
    }
}

void core::enable_tracing()
{
    // TODO
    assert(1==0);
}

void core::disable_tracing()
{
    // TODO
    assert(1==0);
}

void core::play()
{
        // FOr now we use this to copy framebuffer
    holly.cur_output = static_cast<u32 *>(holly.display->output[0]);
}

void core::pause()
{

}

void core::stop()
{
    copy_fb(holly.cur_output);
    holly.master_frame++;
}

void core::get_framevars(framevars& out)
{
    out.master_cycle = trace_cycles;
    out.master_frame = holly.master_frame;
    out.last_used_buffer = holly.display->last_written;
}

void core::reset()
{
    sh4.reset();
    holly.reset();
    gdrom.reset();
    scheduler.clear();
    holly.master_frame = 0;
    trace_cycles = 0;
    new_frame(false);
}


void core::killall()
{

}

void core::new_frame(bool copy_buf)
{
    clock.frame_cycle = 0;
    clock.frame_start_cycle = static_cast<i64>(trace_cycles);
    holly.master_frame++;
    clock.interrupt.vblank_in_yet = clock.interrupt.vblank_out_yet = 0;
    holly.recalc_frame_timing();
    if (copy_buf) copy_fb(holly.cur_output);
    holly.master_frame++;
    schedule_frame(false);
}

enum frame_events {
    evt_EMPTY=0,
    evt_FRAME_START,
    evt_VBLANK_IN,
    evt_VBLANK_OUT,
    evt_FRAME_END,
};

void core::frame_start(u64 clk) {
    sched_printf("\nFRAME START: %llu", trace_cycles);
    clock.frame_cycle = 0;
    clock.frame_start_cycle = (i64)trace_cycles;
    clock.in_vblank = 1;

}

void core::vblank_in(u64 clk) {
    holly.vblank_in();

}

void core::vblank_out(u64 clk) {
    holly.vblank_out();

}

void core::frame_end(u64 clk) {
    sched_printf("\nEVENT: FRAME END %llu", trace_cycles);
    new_frame(true);
}

static void sch_frame_start(void *ptr, u64 key, u64 clock, u32 jitter) {
    auto *th = static_cast<core *>(ptr);
    th->frame_start(clock - jitter);
}

static void sch_vblank_in(void *ptr, u64 key, u64 clock, u32 jitter) {
    auto *th = static_cast<core *>(ptr);
    th->vblank_in(clock - jitter);
}

static void sch_vblank_out(void *ptr, u64 key, u64 clock, u32 jitter) {
    auto *th = static_cast<core *>(ptr);
    th->vblank_out(clock - jitter);
}

static void sch_frame_end(void *ptr, u64 key, u64 clock, u32 jitter) {
    auto *th = static_cast<core *>(ptr);
    th->frame_end(clock - jitter);
}


void core::schedule_frame(bool is_first)
{
    // events
    // frame start @0.
    // vblank_in_start @?
    // vblank_out_start @?
    // frame end @200mil
    sched_printf("\nScheduling frame @ %lld", clock.frame_start_cycle);
    scheduler.add_or_run_abs(clock.frame_start_cycle, 0, this, &sch_frame_start, nullptr);
    scheduler.add_or_run_abs(clock.frame_start_cycle + clock.interrupt.vblank_in_start, 0, this, &sch_vblank_in, nullptr);
    scheduler.add_or_run_abs(clock.frame_start_cycle + clock.interrupt.vblank_out_start, evt_VBLANK_OUT, this, &sch_vblank_out, nullptr);
    scheduler.only_add_abs_w_tag(clock.frame_start_cycle+DC_CYCLES_PER_FRAME, evt_FRAME_END, this, &sch_frame_end, nullptr, 2);
}

u32 core::finish_frame()
{
    scheduler.run_til_tag(2);
    return 0;
}

u32 core::finish_scanline()
{
    assert(1==0);
    return 0;
}

u32 core::step_master(u32 howmany)
{
    scheduler.run_for_cycles(howmany);
    return 0;
}

void core::load_BIOS(multi_file_set& mfs)
{
    // We expect dc_boot.bin and dc_flash.bin
    u32 found = 0;
    for (u32 i = 0; i < mfs.files.size(); i++) {
        struct read_file_buf* rfb = &mfs.files[i];
        if (!strcmp(rfb->name, "dc_boot.bin")) {
            BIOS.copy(&rfb->buf);
            found++;
        }
        else if (!strcmp(rfb->name, "dc_flash.bin")) {
            flash.buf.copy(&rfb->buf);
            found++;
        }
        else {
            printf("\n UNKNOWN FILE? %s", rfb->name);
        }
    }
    if (found != 2) {
        printf("\nHmmm what?!?!?! DC BIOS LOAD FAILURE!?!?!");
        fflush(stdout);
    }
}

static void DCIO_close_drive(jsm_system *ptr)
{

}

static void DCIO_open_drive(jsm_system *ptr)
{

}

static void DCIO_remove_disc(jsm_system *ptr)
{

}

static void DCIO_insert_disc(jsm_system *ptr, physical_io_device &pio, multi_file_set &mfs)
{
    printf("\nJSM INSERT DISC");
    auto *th = static_cast<core *>(ptr);
    th->gdrom.insert_disc(mfs);
}


static void setup_crt(JSM_DISPLAY *d)
{
    d->kind = jsm::CRT;
    d->enabled = true;

    d->fps = 60;
    d->fps_override_hint = 60;

    d->pixelometry.cols.left_hblank = 0;
    d->pixelometry.cols.right_hblank = 168;
    d->pixelometry.cols.visible = 640;
    d->pixelometry.cols.max_visible = 640;
    d->pixelometry.offset.x = 0;

    d->pixelometry.rows.top_vblank = 0;
    d->pixelometry.rows.visible = 480;
    d->pixelometry.rows.max_visible = 480;
    d->pixelometry.rows.bottom_vblank = 48;
    d->pixelometry.offset.y = 0;

    d->geometry.physical_aspect_ratio.width = 4;
    d->geometry.physical_aspect_ratio.height = 3;

    d->pixelometry.overscan.left = d->pixelometry.overscan.right = d->pixelometry.overscan.top = d->pixelometry.overscan.bottom = 0;
}

void core::describe_io()
{
    if (described_inputs) return;
    IOs.reserve(15);
    described_inputs = true;

    // power and reset buttons
    auto *chassis = &IOs.emplace_back();
    chassis->init(HID_CHASSIS, true, true, true, true);
    HID_digital_button* b;
    b = &chassis->chassis.digital_buttons.emplace_back();
    snprintf(b->name, sizeof(b->name), "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    b = &chassis->chassis.digital_buttons.emplace_back();
    b->common_id = DBCID_ch_reset;
    snprintf(b->name, sizeof(b->name), "Reset");
    b->state = 0;

    // GDROM
    auto *d = &IOs.emplace_back();
    d->init(HID_DISC_DRIVE, true, true, true, false);
    d->disc_drive.insert_disc = &DCIO_insert_disc;
    d->disc_drive.remove_disc = &DCIO_remove_disc;
    d->disc_drive.open_drive = &DCIO_open_drive;
    d->disc_drive.close_drive = &DCIO_close_drive;

    // screen
    d = &IOs.emplace_back();
    d->init(HID_DISPLAY, true, true, false, true);
    setup_crt(&d->display);
    d->display.output[0] = malloc(640 * 480 * 4);
    d->display.output[1] = malloc(640 * 480 * 4);
    d->display.output_debug_metadata[0] = nullptr;
    d->display.output_debug_metadata[1] = nullptr;
    holly.display_ptr.make(IOs, IOs.size()-1);
    holly.cur_output = static_cast<u32 *>(d->display.output[0]);
    d->display.last_written = 1;
    //d->display.last_displayed = 1;

    d = &IOs.emplace_back();
    c1.setup_pio(d, 0, "Controller 1", true);

    holly.display = &holly.display_ptr.get().display;
}


void core::old_ROM_load(multi_file_set& mfs)
{
    buf* b = &mfs.files[0].buf;
    ROM.copy(b);

    u32 offset = 0x8000;

    for (u32 i = 0; i < ROM.size; i++) {
        RAM[offset+i] = static_cast<u8 *>(ROM.ptr)[i];
    }

    //sh4.regs.PC = 0xAC010000; // for like a demo
    sh4.regs.PC = 0xAC008300; // for IP.BIN
    for (u32 i = 0; i < 15; i++) {
        sh4.regs.R[i] = 0;
        if (i < 8) sh4.regs.R_[i] = 0;
    }
    sh4.regs.R[15] = 0x8C00D400;

    // Thanks Senryoku!
    // RTE - Some interrupts jump there instead of having their own RTE, I have NO idea why.
    mainbus_write(0x8C000010, 0x00090009, 4); // nop nop
    mainbus_write(0x8C000014, 0x0009002B, 4); // rte nop
    // RTS
    mainbus_write(0x8C000018, 0x00090009, 4);
    mainbus_write(0x8C00001C, 0x0009000B, 4);

    // Write some values to HOLLY...
    mainbus_write(0x005F8048, 6, 4);          // FB_W_CTRL
    mainbus_write(0x005F8060, 0x00600000, 4); // FB_W_SOF1
    mainbus_write(0x005F8064, 0x00600000, 4); // FB_W_SOF2
    mainbus_write(0x005F8044, 0x0080000D, 4); // FB_R_CTRL
    mainbus_write(0x005F8050, 0x00200000, 4); // FB_R_SOF1
    mainbus_write(0x005F8054, 0x00200000, 4); // FB_R_SOF2

}

// Thank you for this Deecey
void core::CPU_state_after_boot_rom()
{
    //SH4_SR_set(sh4, 0x400000F1);
    sh4.regs.SR_set(0x600000f0);
    sh4.regs.FPSCR_set(0x00040001);

    sh4.regs.R[0] = 0x8c010000; //0xAC0005D8;
    sh4.regs.R[1] = 0x00000808; // 0x00000009;
    sh4.regs.R[2] = 0x8c00e070; // 0xAC00940C;
    sh4.regs.R[3] = 0x8c010000; // 0;
    sh4.regs.R[4] = 0x8c010000; //0xAC008300;
    sh4.regs.R[5] = 0xF4000000;
    sh4.regs.R[6] = 0xF4002000;
    sh4.regs.R[7] = 0x00000044;
    sh4.regs.R[8] = 0;
    sh4.regs.R[9] = 0;
    sh4.regs.R[10] = 0;
    sh4.regs.R[11] = 0;
    sh4.regs.R[12] = 0;
    sh4.regs.R[13] = 0;
    sh4.regs.R[14] = 0;
    sh4.regs.R[15] = 0x8c00f400;

    sh4.regs.R_[0] = 0x600000F0; // 0xDFFFFFFF;
    sh4.regs.R_[1] = 0x00000808; // 0x500000F1;
    sh4.regs.R_[2] = 0x8c00e070; // 0;
    sh4.regs.R_[3] = 0;
    sh4.regs.R_[4] = 0;
    sh4.regs.R_[5] = 0;
    sh4.regs.R_[6] = 0;
    sh4.regs.R_[7] = 0;

    sh4.regs.fb[0].U32[4] = 0x3F266666;
    sh4.regs.fb[0].U32[5] = 0x3FE66666;
    sh4.regs.fb[0].U32[6] = 0x41840000;
    sh4.regs.fb[0].U32[7] = 0x3F800000;
    sh4.regs.fb[0].U32[8] = 0x80000000;
    sh4.regs.fb[0].U32[9] = 0x80000000;
    sh4.regs.fb[0].U32[11] = 0x3F800000;

    sh4.regs.GBR = 0x8C000000;
    sh4.regs.SSR = 0x40000001;
    sh4.regs.SPC = 0x8C000776;
    sh4.regs.SGR = 0x8D000000;
    sh4.regs.DBR = 0x8C000010;
    sh4.regs.VBR = 0x8c000000; //0x8C000000;
    sh4.regs.PR = 0x8c00e09c;//0x0C00043C;
    sh4.regs.FPUL.u = 0;

    sh4.regs.PC = 0xAC008300; // IP.bin start address
}

void core::RAM_state_after_boot_rom(read_file_buf *IPBIN)
{
    memset(&RAM[0x00200000], 0, 0x1000000);

    for (u32 i = 0; i < 16; i++) {
        mainbus_write(0x8C0000E0 + 2 * i, mainbus_read(0x800000FE - 2 * i, 2, false), 2);
    }
    mainbus_write(0xA05F74E4, 0x001FFFFF, 4);

    memcpy(&RAM[0x100], ((u8 *)BIOS.ptr) + 0x100, 0x3F00);
    memcpy(&RAM[0x8000], ((u8 *)BIOS.ptr) + 0x8000, 0x1F800);

    mainbus_write(0x8C0000B0, 0x8C003C00, 4);
    mainbus_write(0x8C0000B4, 0x8C003D80, 4);
    mainbus_write(0x8C0000B8, 0x8C003D00, 4);
    mainbus_write(0x8C0000BC, 0x8C001000, 4);
    mainbus_write(0x8C0000C0, 0x8C0010F0, 4);
    mainbus_write(0x8C0000E0, 0x8C000800, 4);

    mainbus_write(0x8C0000AC, 0xA05F7000, 4);
    mainbus_write(0x8C0000A8, 0xA0200000, 4);
    mainbus_write(0x8C0000A4, 0xA0100000, 4);
    mainbus_write(0x8C0000A0, 0, 4);
    mainbus_write(0x8C00002C, 0, 4);
    mainbus_write(0x8CFFFFF8, 0x8C000128, 4);

    //         // Load IP.bin from disk (16 first sectors of the last track)
    //        // FIXME: Here we assume the last track is the 3rd.
    // TODO
    if (IPBIN) {
        printf("\nLoading IP.BIN...");
        memcpy(&RAM[0x8000], IPBIN->buf.ptr, 0x8000);
    }


    // IP.bin patches
    mainbus_write(0xAC0090Db, 0x5113, 2);
    mainbus_write(0xAC00940A, 0xB, 2);
    mainbus_write(0xAC00940C, 0x9, 2);

    mainbus_write(0x8C000000, 0x00090009, 4);
    mainbus_write(0x8C000004, 0x001B0009, 4);
    mainbus_write(0x8C000008, 0x0009AFFD, 4);

    mainbus_write(0x8C00000C, 0, 2);
    mainbus_write(0x8C00000E, 0, 2);

    mainbus_write(0x8C000010, 0x00090009, 4);
    mainbus_write(0x8C000014, 0x0009002B, 4);

    mainbus_write(0x8C000018, 0x00090009, 4);
    mainbus_write(0x8C00001C, 0x0009000B, 4);

    mainbus_write(0x8C00002C, 0x16,1);
    mainbus_write(0x8C000064, 0x8C008100, 4);
    mainbus_write(0x8C000090, 0, 2);
    mainbus_write(0x8C000092, -128, 2);

    maple.SB_MDST = 0;
    g2.SB_DDST = 0;
}


// Thanks to Deecey for values to write
void core::sideload(multi_file_set& mfs) {
    CPU_state_after_boot_rom();
    RAM_state_after_boot_rom(&mfs.files[1]);


#ifdef DC_SUPPORT_ELF
    if (ends_with(mfs.files[0].name, ".elf")) {
        char YOYO[500];
        snprintf(YOYO, sizeof(YOYO), "%s/%s", mfs.files[0].path, mfs.files[0].name);
        printf("\nOPEN ELF %s", YOYO);
        Elf32_Ehdr eh;        /* elf-header is fixed size */
        i32 fd = open(YOYO, O_RDONLY | O_SYNC);
        Elf32_Sym* sym_tbl = nullptr;
        char *str_tabl = nullptr;

        read_elf_header(fd, &eh);
        if (!is_ELF(eh)) {
            printf("\nNOT ELFT!");
            close(fd);
            return;
        }
        if (is64Bit(eh)) {
            printf("\nIS 64!");
            return;
        } else {
            Elf32_Shdr *sh_table=nullptr;    /* section-header table is variable size */
            //print_elf_header(eh);
            sh4.regs.PC = eh.e_entry;

            sh_table = static_cast<Elf32_Shdr *>(malloc(eh.e_shentsize * eh.e_shnum));
            read_section_header_table(fd, eh, sh_table);

            // Load to RAM and load symbols
            char *sh_str = read_section(fd, sh_table[eh.e_shstrndx]);
            for (int i = 0; i < eh.e_shnum; i++) {
                u32 addr = sh_table[i].sh_addr;
                if (addr >= 0x8C000000) {
                    if (addr >= (0x8C000000+0x1000000)) {
                        printf("\nLoad address too high! %08x", addr);
                        continue;
                    }
                    if ((addr+sh_table[i].sh_size) >= (0x8C000000+0x1000000)) {
                        printf("\nLoad size too high! %08x", sh_table[i].sh_size);
                        continue;
                    }
                    printf("\nELF loading %s to 0x%08x", (sh_str + sh_table[i].sh_name), sh_table[i].sh_addr);
                    memcpy(&RAM[addr - 0x8C000000], &((u8*)mfs.files[0].buf.ptr)[sh_table[i].sh_offset], sh_table[i].sh_size);
                }
                if ((sh_table[i].sh_type == SHT_SYMTAB) || (sh_table[i].sh_type==SHT_DYNSYM)) {
                    u32 symbol_count;
                    sym_tbl = (Elf32_Sym*)read_section(fd, sh_table[i]);
                    u32 str_tbl_ndx = sh_table[i].sh_link;
                    str_tabl = read_section(fd, sh_table[str_tbl_ndx]);

                    symbol_count = (sh_table[i].sh_size/sizeof(Elf32_Sym));
                    char *fname = nullptr;

                    for(int j=0; j < symbol_count; j++) {
                        u32 st_type = ELF32_ST_TYPE(sym_tbl[j].st_info);
                        if (st_type == STT_FILE) {
                            fname = (str_tabl + sym_tbl[j].st_name);
                            continue;
                        }
                        if (st_type == STT_SECTION) continue;
                        if (strlen((str_tabl + sym_tbl[j].st_name)) < 2) continue;

                        if ((st_type == STT_FUNC) || (st_type == STT_OBJECT) || (st_type == STT_NOTYPE)) {
                            enum elf_symbol32_kind esk;
                            switch (st_type) {
                                case STT_FUNC:
                                    esk = esk_function;
                                    break;
                                case STT_OBJECT:
                                    esk = esk_object;
                                    break;
                                case STT_NOTYPE:
                                    esk = esk_unknown;
                                    break;
                                default:
                                    printf("\nUNKNOWN SYMBOL KIND HERE %d", st_type);
                                    esk = esk_unknown;
                                    break;
                            }
                            elf_symbols.add(sym_tbl[j].st_value, fname, (str_tabl + sym_tbl[j].st_name), esk);
                        }
                    }
                }
            }
            printf("\nAlso loaded %d symbols.", elf_symbols.num);

            if (str_tabl) free(str_tabl);
            if (sym_tbl) free(sym_tbl);
            if (sh_str) free(sh_str);
            if (sh_table) free(sh_table);
            close(fd);
        }
    }
    else {
        printf("\nNot ELF, loading as .bin...");
#endif
        memcpy(&RAM[0x10000], mfs.files[0].buf.ptr, mfs.files[0].buf.size);
        sh4.regs.PC = 0xAC010000;
#ifdef DC_SUPPORT_ELF
    }
#endif
}

}