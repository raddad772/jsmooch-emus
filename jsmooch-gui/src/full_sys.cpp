#include <assert.h>
#include <stdio.h>
//#include <SDL.h>
#include <stdlib.h>
#include <pwd.h>
#include <unistd.h>
#include <string.h>
#include <cmath>
#include "application.h"
#include "helpers/sys_interface.h"
#include "helpers/sys_present.h"
#include "helpers/debug.h"
#include "full_sys.h"
#include "system/dreamcast/gdi.h"
#include "helpers/physical_io.h"
//#include "system/gb/gb_enums.h"

// mac overlay - 14742566
// i get to    - 88219648

//#define NEWSYS
#define NEWSYS_STEP2
//#define DO_DREAMCAST
//#define SIDELOAD
//#define STOPAFTERAWHILE



#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

u32 get_closest_pow2(u32 b)
{
    //u32 b = MAX(w, h);
    u32 out = 128;
    while(out < b) {
        out <<= 1;
        assert(out < 0x40000000);
    }
    return out;
}

void map_inputs(const u32 *input_buffer, struct system_io* inputs, struct jsm_system *jsm)
{
    struct system_io::CDKRKR *p1 = &inputs->p[0];
    struct system_io::CDKRKR *p2 = &inputs->p[1];
    // Arrows
    if (p1->up) p1->up->state = input_buffer[0];
    if (p1->down) p1->down->state = input_buffer[1];
    if (p1->left) p1->left->state = input_buffer[2];
    if (p1->right) p1->right->state = input_buffer[3];

    // fire buttons
    if (p1->fire1) p1->fire1->state = input_buffer[4];
    if (p1->fire2) p1->fire2->state = input_buffer[5];
    if (p1->fire3) p1->fire3->state = input_buffer[8];

    // Start, select on controller
    if (p1->start) p1->start->state = input_buffer[7];
    if (p1->select) p1->select->state = input_buffer[6];

    // Pause/start on chassis
    if (inputs->ch_pause) inputs->ch_pause->state = input_buffer[7];
}

void test_gdi() {
    struct GDI_image foo;
    GDI_init(&foo);
    const char *homeDir = getenv("HOME");

    if (!homeDir) {
        struct passwd* pwd = getpwuid(getuid());
        if (pwd)
            homeDir = pwd->pw_dir;
    }

    char PATH[500];
    snprintf(PATH, sizeof(PATH), "%s/Documents/emu/rom/dreamcast/crazy_taxi", homeDir);
    printf("\nHEY! %s", PATH);
    GDI_load(PATH, "crazy_taxi/crazytaxi.gdi", &foo);

    GDI_delete(&foo);
}

u32 grab_BIOSes(struct multi_file_set* BIOSes, enum jsm_systems which)
{
    char BIOS_PATH[255];
    char BASE_PATH[255];
    const char *homeDir = getenv("HOME");

    if (!homeDir) {
        struct passwd* pwd = getpwuid(getuid());
        if (pwd)
            homeDir = pwd->pw_dir;
    }

    snprintf(BASE_PATH, sizeof(BASE_PATH), "%s/Documents/emu/bios", homeDir);

    u32 has_bios = 0;
    switch(which) {
        case SYS_SMS1:
        case SYS_SMS2:
            has_bios = 1;
            snprintf(BIOS_PATH, sizeof(BIOS_PATH), "%s/master_system", BASE_PATH);
            mfs_add("bios13fx.sms", BIOS_PATH, BIOSes);
            break;
        case SYS_DREAMCAST:
            has_bios = 1;
            snprintf(BIOS_PATH, sizeof(BIOS_PATH), "%s/dreamcast", BASE_PATH);
            mfs_add("dc_boot.bin", BIOS_PATH, BIOSes);
            mfs_add("dc_flash.bin", BIOS_PATH, BIOSes);
            break;
        case SYS_DMG:
            has_bios = 1;
            snprintf(BIOS_PATH, sizeof(BIOS_PATH), "%s/gameboy", BASE_PATH);
            mfs_add("gb_bios.bin", BIOS_PATH, BIOSes);
            break;
        case SYS_GBC:
            has_bios = 1;
            snprintf(BIOS_PATH, sizeof(BIOS_PATH), "%s/gameboy", BASE_PATH);
            mfs_add("gbc_bios.bin", BIOS_PATH, BIOSes);
            break;
        case SYS_ZX_SPECTRUM_48K:
            has_bios = 1;
            snprintf(BIOS_PATH, sizeof(BIOS_PATH), "%s/zx_spectrum", BASE_PATH);
            mfs_add("zx48.rom", BIOS_PATH, BIOSes);
            break;
        case SYS_ZX_SPECTRUM_128K:
            has_bios = 1;
            snprintf(BIOS_PATH, sizeof(BIOS_PATH), "%s/zx_spectrum", BASE_PATH);
            mfs_add("zx128.rom", BIOS_PATH, BIOSes);
            break;
        case SYS_MAC128K:
            has_bios = 1;
            snprintf(BIOS_PATH, sizeof(BIOS_PATH), "%s/mac", BASE_PATH);
            mfs_add("mac128k.rom", BIOS_PATH, BIOSes);
            break;
        case SYS_MAC512K:
            has_bios = 1;
            snprintf(BIOS_PATH, sizeof(BIOS_PATH), "%s/mac", BASE_PATH);
            mfs_add("mac512k.rom", BIOS_PATH, BIOSes);
            break;
        case SYS_MACPLUS_1MB:
            has_bios = 1;
            snprintf(BIOS_PATH, sizeof(BIOS_PATH), "%s/mac", BASE_PATH);
            mfs_add("macplus_1mb.rom", BIOS_PATH, BIOSes);
            break;
        case SYS_PSX:
        case SYS_GENESIS:
        case SYS_SNES:
        case SYS_NES:
        case SYS_BBC_MICRO:
        case SYS_GG:
        case SYS_ATARI2600:
            has_bios = 0;
            break;
        default:
            printf("\nNO BIOS SWITCH FOR CONSOLE %d", which);
            break;
    }
    return has_bios;
}


void GET_HOME_BASE_SYS(char *out, size_t out_sz, enum jsm_systems which, const char* sec_path, u32 *worked)
{
    char BASE_PATH[500];
    char BASER_PATH[500];
    const char *homeDir = getenv("HOME");

    if (!homeDir) {
        struct passwd* pwd = getpwuid(getuid());
        if (pwd)
            homeDir = pwd->pw_dir;
    }

    snprintf(BASER_PATH, 500, "%s/Documents/emu/rom", homeDir);

    u32 has_bios = 0;
    switch(which) {
        case SYS_SMS1:
        case SYS_SMS2:
            snprintf(out, out_sz, "%s/master_system", BASER_PATH);
            *worked = 1;
            break;
        case SYS_DREAMCAST:
            if (sec_path)
                snprintf(out, out_sz, "%s/dreamcast/%s", BASER_PATH, sec_path);
            else
                snprintf(out, out_sz, "%s/dreamcast", BASER_PATH);
            *worked = 1;
            break;
        case SYS_DMG:
        case SYS_GBC:
            snprintf(out, out_sz, "%s/gameboy", BASER_PATH);
            *worked = 1;
            break;
        case SYS_ATARI2600:
            snprintf(out, out_sz, "%s/atari2600", BASER_PATH);
            *worked = 1;
            break;
        case SYS_NES:
            snprintf(out, out_sz, "%s/nes", BASER_PATH);
            *worked = 1;
            break;
        case SYS_ZX_SPECTRUM_48K:
        case SYS_ZX_SPECTRUM_128K:
            snprintf(out, out_sz, "%s/zx_spectrum", BASER_PATH);
            *worked = 1;
            break;
        case SYS_GENESIS:
            snprintf(out, out_sz, "%s/genesis", BASER_PATH);
            *worked = 1;
            break;
        case SYS_MAC512K:
        case SYS_MAC128K:
        case SYS_MACPLUS_1MB:
            snprintf(out, out_sz, "%s/mac", BASER_PATH);
            *worked = 1;
            break;
        case SYS_PSX:
        case SYS_SNES:
        case SYS_BBC_MICRO:
        case SYS_GG:
            *worked = 0;
            break;
        default:
            *worked = 0;
            printf("\nNO CASE FOR SYSTEM %d", which);
            break;
    }
}

void mfs_add_IP_BIN(struct multi_file_set* mfs)
{
    char BASER_PATH[255];
    char BASE_PATH[255];
    char ROM_PATH[255];
    u32 worked = 0;

    GET_HOME_BASE_SYS(BASE_PATH, 255, SYS_DREAMCAST, nullptr, &worked);
    if (worked == 0) return;

    mfs_add("IP.BIN", BASE_PATH, mfs);
    printf("\nLOADED IP.BIN SIZE %04llx", mfs->files[1].buf.size);
}

u32 grab_ROM(struct multi_file_set* ROMs, enum jsm_systems which, const char* fname, const char* sec_path)
{
    char BASE_PATH[255];
    char ROM_PATH[255];
    u32 worked = 0;

    GET_HOME_BASE_SYS(BASE_PATH, sizeof(BASE_PATH), which, sec_path, &worked);

    if (!worked) return 0;
    mfs_add(fname, BASE_PATH, ROMs);
    //printf("\n%d %s %s", ROMs->files[ROMs->num_files-1].buf.size > 0, BASE_PATH, fname);
    return ROMs->files[ROMs->num_files-1].buf.size > 0;
}

struct physical_io_device* load_ROM_into_emu(struct jsm_system* sys, struct cvec* IOs, struct multi_file_set* mfs)
{
    struct physical_io_device *pio = nullptr;
    switch(sys->kind) {
        case SYS_ZX_SPECTRUM_48K:
        case SYS_ZX_SPECTRUM_128K:
        case SYS_DREAMCAST:
        case SYS_MAC512K:
        case SYS_MAC128K:
        case SYS_MACPLUS_1MB:
            for (u32 i = 0; i < cvec_len(IOs); i++) {
                pio = (struct physical_io_device*)cvec_get(IOs, i);
                if (pio->kind == HID_DISC_DRIVE) {
                    printf("\nINSERT DISC!");
                    pio->disc_drive.insert_disc(sys, pio, mfs);
                    break;
                }
                else if (pio->kind == HID_AUDIO_CASSETTE) {
                    pio->audio_cassette.insert_tape(sys, pio, mfs, NULL);
                    break;
                }
                pio = nullptr;
            }
            return pio;
    }
    pio = nullptr;
    for (u32 i = 0; i < cvec_len(IOs); i++) {
        pio = (struct physical_io_device*)cvec_get(IOs, i);
        if (pio->kind == HID_CART_PORT) break;
        pio = nullptr;
    }
    // TODO: add sram support
    if (pio) pio->cartridge_port.load_cart(sys, mfs, nullptr);
    return pio;
}

void newsys(struct full_system *fsys)
{
#ifdef NEWSYS
    float start = SDL_GetTicks();
    u32 c;
    c = (5460350 * 2) - 100;
    //c = (300000 * 2);
    //c = 400;
    //u32 c2 = 0;180207
    // mine
    // 511071
    // read # at 511070 catches it
    // THEIRS
    // THEIR 511070 does not catch it...
    // tst.b @ 511096, flag is set
    // their cycle goes 1 btw 511112

#ifdef LYCODER
    c = 20000000;
#endif
    //c = 13047918 - 200;
    //c = 73377752 - 1000;
    //c = 47395691 - 2500;
    //c = 47391011 - 150;
    //c =   46528328 - 200;
    //c =     93377808;// - 400;
//    c =  47392303 - 250;
#ifdef LYCODER
    dbg_enable_trace();
#else
    /*dbg_enable_trace();
    dbg.traces.m68000.instruction = 1;
    dbg.traces.m68000.mem = 1;*/
#endif
    sys->step_master(sys, c);
    //                  47 395 691
    // Z80 enable - 47371674
    // wait on interrupt - 47380193
    // IRQ fire - 47380277

    //sys->step_master(sys, 18323050);
    //sys->step_master(sys, 393260);
    /*float end = SDL_GetTicks();
    printf("\nTicks taken: %f", end - start);*/
    //return 0;
    //dbg_unbreak();
    struct framevars fvr;
    sys->get_framevars(sys, &fvr);
    printf("\nBREAK CYCLE: %lld", fvr.master_cycle);
#ifdef NEWSYS_STEP2
    dbg_unbreak();
    dbg_enable_trace();
    dbg.traces.m68000.instruction = 1;
    dbg.traces.m68000.mem = 1;
    dbg.traces.m68000.ifetch = 0;
    dbg.traces.m68000.irq = 1;
    dbg.traces.z80.mem = 0;
    dbg.traces.dma = 0;
    dbg.traces.vdp = 0;
    dbg.traces.vram = 0;
    dbg.traces.fifo = 0;
    dbg.breaks.m68000.irq = 0;
    dbg.traces.z80.instruction = 0;
    sys->step_master(sys, 500);
#endif
    dbg_flush();
    sys->get_framevars(sys, &fvr);
    printf("\nFINAL CYCLE: %lld", fvr.master_cycle);
    return 0;
#endif
}

static void setup_controller(struct system_io* io, struct physical_io_device* pio, u32 pnum)
{
    struct cvec* dbs = &pio->controller.digital_buttons;
    for (u32 i = 0; i < cvec_len(dbs); i++) {
        struct HID_digital_button* db = (struct HID_digital_button*)cvec_get(dbs, i);
        switch(db->common_id) {
            case DBCID_co_up:
                io->p[pnum].up = db;
                continue;
            case DBCID_co_down:
                io->p[pnum].down = db;
                continue;
            case DBCID_co_left:
                io->p[pnum].left = db;
                continue;
            case DBCID_co_right:
                io->p[pnum].right = db;
                continue;
            case DBCID_co_fire1:
                io->p[pnum].fire1 = db;
                continue;
            case DBCID_co_fire2:
                io->p[pnum].fire2 = db;
                continue;
            case DBCID_co_fire3:
                io->p[pnum].fire3 = db;
                continue;
            case DBCID_co_select:
                io->p[pnum].select = db;
                continue;
            case DBCID_co_start:
                io->p[pnum].start = db;
                continue;
        }
    }
}

void full_system::setup_ios()
{
    struct cvec *IOs = &sys->IOs;
    memset(&inputs, 0, sizeof(struct system_io));
    for (u32 i = 0; i < cvec_len(IOs); i++) {
        struct physical_io_device* pio = (struct physical_io_device*)cvec_get(IOs, i);
        switch(pio->kind) {
            case HID_CONTROLLER: {
                if (io.controller1.vec == nullptr) {
                    io.controller1 = make_cvec_ptr(IOs, i);
                    setup_controller(&inputs, pio, 0);
                }
                else {
                    io.controller2 = make_cvec_ptr(IOs, i);
                    setup_controller(&inputs, pio, 1);
                }
                continue; }
            case HID_KEYBOARD: {
                io.keyboard = make_cvec_ptr(IOs, i);
                continue; }
            case HID_DISPLAY: {
                io.display = make_cvec_ptr(IOs, i);
                continue; }
            case HID_CHASSIS: {
                io.chassis = make_cvec_ptr(IOs, i);
                //make_cvec_ptr(IOs, i);
                struct cvec* dbs = &pio->chassis.digital_buttons;
                for (u32 j = 0; j < cvec_len(dbs); j++) {
                    struct HID_digital_button* db = (struct HID_digital_button*)cvec_get(dbs, j);
                    switch(db->common_id) {
                        case DBCID_ch_pause:
                            inputs.ch_pause = db;
                            continue;
                        case DBCID_ch_power:
                            inputs.ch_power = db;
                            continue;
                        case DBCID_ch_reset:
                            inputs.ch_reset = db;
                            continue;
                        default:
                            continue;
                    }
                }
                break; }
            case HID_MOUSE:
                io.mouse = make_cvec_ptr(IOs, i);
                break;
            case HID_DISC_DRIVE:
                io.disk_drive = make_cvec_ptr(IOs, i);
                break;
            case HID_CART_PORT:
                io.cartridge_port = make_cvec_ptr(IOs, i);
                break;
            case HID_AUDIO_CASSETTE:
                io.audio_cassette = make_cvec_ptr(IOs, i);
                break;
            default:
                break;
        }
    }
    assert(io.display.vec);
    assert(io.chassis.vec);


    output.display = &((struct physical_io_device *)cpg(io.display))->display;
}

void full_system::setup_wgpu()
{
    setup_display();
}

void full_system::setup_bios()
{
    enum jsm_systems which = sys->kind;

    struct multi_file_set BIOSes = {};
    mfs_init(&BIOSes);

    u32 has_bios = grab_BIOSes(&BIOSes, which);
    if (has_bios) {
        sys->load_BIOS(sys, &BIOSes);
    }
    mfs_delete(&BIOSes);
}

void full_system::load_default_ROM()
{
    struct cvec *IOs = &sys->IOs;
    enum jsm_systems which = sys->kind;

    struct multi_file_set ROMs;
    mfs_init(&ROMs);
    assert(sys);
    switch(which) {
        case SYS_NES:
            worked = grab_ROM(&ROMs, which, "mario3.nes", nullptr);
            break;
        case SYS_SMS1:
        case SYS_SMS2:
            worked = grab_ROM(&ROMs, which, "sonic.sms", nullptr);
            break;
        case SYS_DMG:
            worked = grab_ROM(&ROMs, which, "marioland2.gb", nullptr);
            break;
        case SYS_GBC:
            worked = grab_ROM(&ROMs, which, "pokemonyellow.gbc", nullptr);
            break;
        case SYS_ATARI2600:
            worked = grab_ROM(&ROMs, which, "space_invaders.a26", nullptr);
            break;
        case SYS_DREAMCAST:
            worked = grab_ROM(&ROMs, which, "crazytaxi.gdi", "crazy_taxi");
            break;
        case SYS_MAC512K:
        case SYS_MAC128K:
        case SYS_MACPLUS_1MB:
            worked = grab_ROM(&ROMs, which, "system1_1.img", nullptr);
            break;
        case SYS_ZX_SPECTRUM_48K:
        case SYS_ZX_SPECTRUM_128K:
            worked = 1;
            break;
        case SYS_GENESIS:
            worked = grab_ROM(&ROMs, which, "sonic.md", nullptr);
            break;
        default:
            printf("\nSYS NOT IMPLEMENTED!");
    }
    if (!worked) {
        printf("\nCouldn't open ROM!");
        return;
    }

    struct physical_io_device* fileioport = load_ROM_into_emu(sys, IOs, &ROMs);
    mfs_delete(&ROMs);
}

void full_system::setup_debugger_interface()
{
    sys->setup_debugger_interface(sys, &dbgr);
    dasm = nullptr;
    for (u32 i = 0; i < cvec_len(&dbgr.views); i++) {
        auto view = (struct debugger_view *)cvec_get(&dbgr.views, i);
        switch(view->kind) {
            case dview_disassembly:
                dasm = &view->disassembly;
                break;
            default:
                break;
        }
        if (dasm) break;
    }
}


void full_system::setup_system(enum jsm_systems which)
{
    // Create our emulator

    sys = new_system(which);

    assert(sys);


    setup_ios();

    setup_bios();

    //    backbuffer.setup(wgpu_device, "backbuffer", bb_width, bb_height);

    load_default_ROM();

#ifdef SIDELOAD
    struct multi_file_set sideload_image;
    mfs_init(&sideload_image);
    grab_ROM(&sideload_image, which, "gl_matrix.elf", "kos");
    mfs_add_IP_BIN(&sideload_image);
    fsys.sys->sideload(sys, &sideload_image);
    mfs_delete(&sideload_image);
#endif

    setup_debugger_interface();
}

void full_system::destroy_system()
{
    if (sys == nullptr) return;
    jsm_delete(sys);
    sys = nullptr;
}

struct framevars full_system::get_framevars() const
{
    struct framevars fv = {};
    sys->get_framevars(sys, &fv);
    return fv;
}

void full_system::setup_display()
{
    struct JSM_DISPLAY_PIXELOMETRY *p = &output.display->pixelometry;
    assert(wgpu_device);

    // Determine final output resolution
    u32 wh = get_closest_pow2(MAX(p->cols.visible, p->rows.visible));
    output.backbuffer_texture.setup(wgpu_device, "emulator backbuffer", wh, wh);

    u32 overscan_x_offset = p->overscan.left;
    u32 overscan_width = p->cols.visible - (p->overscan.left + p->overscan.right);
    u32 overscan_y_offset = p->overscan.top;
    u32 overscan_height = p->rows.visible - (p->overscan.top + p->overscan.bottom);

    // Determine aspect ratio correction
    double visible_width = p->cols.visible;
    double visible_height = p->rows.visible;

    double real_width = output.display->geometry.physical_aspect_ratio.width;
    double real_height = output.display->geometry.physical_aspect_ratio.height;

    // we want a multiplier of 1 in one direction, and >1 in the other
    double visible_how = visible_height / visible_width;  // .5
    double real_how = real_height / real_width;           // .6. real is narrower

    if ((visible_how - real_how) < .01) {
        output.x_scale_mult = 1;
        output.y_scale_mult = 1;
        output.with_overscan.x_size = (float)p->cols.visible;
        output.with_overscan.y_size = (float)p->rows.visible;
        output.without_overscan.x_size = (float)overscan_width;
        output.without_overscan.y_size = (float)overscan_height;
    }
    else if (real_how > visible_how) { // real is narrower, so we stretch vertically. visible= 4:2 .5  real=3:2  .6
        output.x_scale_mult = 1;
        output.y_scale_mult = (real_how / visible_how); // we must
        output.with_overscan.x_size = (float)p->cols.visible;
        output.with_overscan.y_size = (float)(visible_height * output.y_scale_mult);
        output.without_overscan.x_size = (float)overscan_width;
        output.without_overscan.y_size = (float)(overscan_height * output.y_scale_mult);
    }
    else { // real is wider, so we stretch horizontally  //    visible=4:2 = .5    real=5:2 = .4
        output.x_scale_mult = (visible_how / real_how);
        output.y_scale_mult = 1;
        output.with_overscan.x_size = (float)(visible_width * output.x_scale_mult);
        output.with_overscan.y_size = (float)p->rows.visible;
        output.without_overscan.x_size = (float)(overscan_width * output.x_scale_mult);
        output.without_overscan.y_size = (float)overscan_height;
    }
    printf("\nOutput with overscan size: %dx%d", (int)output.with_overscan.x_size, (int)output.with_overscan.y_size);
    printf("\nOutput without overscan size: %dx%d", (int)output.with_overscan.x_size, (int)output.with_overscan.y_size);

    // Calculate UV coords for full buffer
    output.with_overscan.uv0 = ImVec2(0, 0);
    output.with_overscan.uv1 = ImVec2((float)((double)p->cols.visible / (double)output.backbuffer_texture.width),
                                      (float)((double)p->rows.visible / (double)output.backbuffer_texture.height));

    // Calculate UV coords for buffer minus overscan
    // we need the left and top, which may be 0 or 10 or whatever... % of total width
    float total_u = output.with_overscan.uv1.x;
    float total_v = output.with_overscan.uv1.y;

    float start_u = (float)overscan_x_offset / (float)visible_width;
    float start_v = (float)overscan_y_offset / (float)visible_height;
    output.without_overscan.uv0 = ImVec2(start_u * total_u, start_v * total_v);

    float end_u = (float)(p->cols.visible - p->overscan.right) / (float)visible_width;
    float end_v = (float)(p->rows.visible - p->overscan.bottom) / (float)visible_height;
    output.without_overscan.uv1 = ImVec2(end_u * total_u, end_v * total_v);

    if (output.backbuffer_backer) free(output.backbuffer_backer);
    output.backbuffer_backer = malloc(p->cols.visible * p->rows.visible * 4);
    memset(output.backbuffer_backer, 0, p->cols.visible * p->rows.visible * 4);
}

void full_system::present()
{
    struct framevars fv = {};
    sys->get_framevars(sys, &fv);
    jsm_present(sys->kind, (struct physical_io_device *)cpg(io.display), output.backbuffer_backer, output.backbuffer_texture.width, output.backbuffer_texture.height);
    output.backbuffer_texture.upload_data(output.backbuffer_backer, output.backbuffer_texture.width * output.backbuffer_texture.height * 4, output.backbuffer_texture.width, output.backbuffer_texture.height);
}

ImVec2 full_system::output_size() const
{
    float z = output.zoom ? 2.0f : 1.0f;
    auto &v = output.hide_overscan ? output.without_overscan : output.with_overscan;
    return {v.x_size * z, v.y_size * z};
}

ImVec2 full_system::output_uv0() const
{
    auto &v = output.hide_overscan ? output.without_overscan.uv0 : output.with_overscan.uv0;
    return v;
}

ImVec2 full_system::output_uv1() const
{
    auto &v = output.hide_overscan ? output.without_overscan.uv1 : output.with_overscan.uv1;
    return v;
}

void full_system::do_frame() const {
    if (sys) {
        struct framevars fv = {};
        if (!dbg.do_break)
            sys->finish_frame(sys);
        sys->get_framevars(sys, &fv);
    }
    else {
        printf("\nCannot do frame with no system.");
    }
}

int maine(int argc, char** argv)
{
#ifdef STOPAFTERAWHILE
        // 14742566
        if (fv.master_cycle >= ((14742566*2)+500)) {
        //if (fv.master_cycle >= 12991854) {
            dbg_break(">240 FRAMES", fv.master_cycle);
            dbg.do_break = 1;
        }
#endif
/*        if (dbg.do_break) {
            //break;
            if (did_break == 0) {
                //sys->step_master(sys, 1200);
                dbg_enable_trace();
                dbg_enable_cpu_trace(DS_z80);
                //dbg_enable_cpu_trace(DS_m68000);
                dbg.traces.m68000.mem = 1;
                dbg.traces.m68000.instruction = 1;
                dbg.traces.z80.instruction = 1;
                dbg.traces.z80.mem = 1;
                dbg.traces.z80.io = 0;
                dbg.brk_on_NMIRQ = 1;
                dbg.do_break = 0;
                dbg_unbreak();
                printf("\nTHEN BREAK...");
                sys->step_master(sys, 120000);
                dbg_flush();
                dbg_disable_trace();
                break;
            }
            if (did_break == 1) break;
            did_break++;
        }
        //jsm_present(sys->kind, display, window_surface->pixels, 640, 480);
        float end = 1;
        float ticks_taken = end - start;
        float tick_target = 16.7f;
        //if (ticks_taken < tick_target)
            //SDL_Delay(tick_target - ticks_taken);
    }
    // Clean up and be tidy!
    jsm_delete(sys);*/
    return 0;
}
