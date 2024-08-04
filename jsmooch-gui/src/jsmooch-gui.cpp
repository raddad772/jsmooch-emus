#include <assert.h>
#include <stdio.h>
//#include <SDL.h>
#include <stdlib.h>
#include <pwd.h>
#include <unistd.h>
#include <string.h>
#include "application.h"
#include "helpers/sys_interface.h"
#include "helpers/sys_present.h"
#include "helpers/debug.h"
#include "jsmooch-gui.h"

#include "system/dreamcast/gdi.h"
#include "helpers/physical_io.h"

// mac overlay - 14742566
// i get to    - 88219648

//#define NEWSYS
#define NEWSYS_STEP2
//#define DO_DREAMCAST
//#define SIDELOAD
//#define STOPAFTERAWHILE


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
        case SYS_ZX_SPECTRUM:
            has_bios = 1;
            snprintf(BIOS_PATH, sizeof(BIOS_PATH), "%s/zx_spectrum", BASE_PATH);
            mfs_add("zx48.rom", BIOS_PATH, BIOSes);
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
        case SYS_ZX_SPECTRUM:
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

    GET_HOME_BASE_SYS(BASE_PATH, 255, SYS_DREAMCAST, NULL, &worked);
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
    struct physical_io_device *pio = NULL;
    switch(sys->kind) {
        case SYS_ZX_SPECTRUM:
        case SYS_DREAMCAST:
            for (u32 i = 0; i < cvec_len(IOs); i++) {
                pio = (struct physical_io_device*)cvec_get(IOs, i);
                if (pio->kind == HID_DISC_DRIVE) {
                    pio->device.disc_drive.insert_disc(sys, mfs);
                    break;
                }
                else if (pio->kind == HID_AUDIO_CASSETTE) {
                    ///pio->device.audio_cassette.insert_tape(mfs);
                    break;
                }
                pio = NULL;
            }
            return pio;
    }
    pio = NULL;
    for (u32 i = 0; i < cvec_len(IOs); i++) {
        pio = (struct physical_io_device*)cvec_get(IOs, i);
        if (pio->kind == HID_CART_PORT) break;
        pio = NULL;
    }
    // TODO: add sram support
    if (pio) pio->device.cartridge_port.load_cart(sys, mfs, NULL);
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
    struct cvec* dbs = &pio->device.controller.digital_buttons;
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

struct full_system setup_system(enum jsm_systems which)
{
    struct full_system fsys = {};
    struct multi_file_set BIOSes = {};
    mfs_init(&BIOSes);

    // Create our emulator

    fsys.sys = new_system(which);
    struct cvec *IOs = &fsys.sys->IOs;

    u32 has_bios = grab_BIOSes(&BIOSes, which);
    if (has_bios) {
        fsys.sys->load_BIOS(fsys.sys, &BIOSes);
        mfs_delete(&BIOSes);
    }

    struct multi_file_set ROMs;
    mfs_init(&ROMs);
    u32 worked = 0;
    switch(which) {
        case SYS_NES:
            worked = grab_ROM(&ROMs, which, "mario3.nes", NULL);
            break;
        case SYS_SMS1:
        case SYS_SMS2:
            worked = grab_ROM(&ROMs, which, "sonic.sms", NULL);
            break;
        case SYS_DMG:
            worked = grab_ROM(&ROMs, which, "marioland2.gb", NULL);
            break;
        case SYS_GBC:
            worked = grab_ROM(&ROMs, which, "pokemonyellow.gbc", NULL);
            break;
        case SYS_ATARI2600:
            worked = grab_ROM(&ROMs, which, "space_invaders.a26", NULL);
            break;
        case SYS_DREAMCAST:
            worked = grab_ROM(&ROMs, which, "crazytaxi.gdi", "crazy_taxi");
            break;
        case SYS_MAC512K:
        case SYS_MAC128K:
        case SYS_MACPLUS_1MB:
        case SYS_ZX_SPECTRUM:
            worked = 1;
            break;
        case SYS_GENESIS:
            worked = grab_ROM(&ROMs, which, "sonic.md", NULL);
            break;
        default:
            printf("\nSYS NOT IMPLEMENTED!");
    }
    if (!worked) {
        printf("\nCouldn't open ROM!");
        return fsys;
    }

    struct physical_io_device* fileioport = load_ROM_into_emu(fsys.sys, IOs, &ROMs);
    mfs_delete(&ROMs);

#ifdef SIDELOAD
    struct multi_file_set sideload_image;
    mfs_init(&sideload_image);
    grab_ROM(&sideload_image, which, "gl_matrix.elf", "kos");
    mfs_add_IP_BIN(&sideload_image);
    fsys.sys->sideload(sys, &sideload_image);
    mfs_delete(&sideload_image);
#endif
    memset(&fsys.inputs, 0, sizeof(struct system_io));
    for (u32 i = 0; i < cvec_len(IOs); i++) {
        struct physical_io_device* pio = (struct physical_io_device*)cvec_get(IOs, i);
        switch(pio->kind) {
            case HID_CONTROLLER: {
                if (fsys.controller1 == nullptr) {
                    fsys.controller1 = pio;
                    setup_controller(&fsys.inputs, pio, 0);
                }
                else {
                    fsys.controller2 = pio;
                    setup_controller(&fsys.inputs, pio, 1);
                }
                continue; }
            case HID_KEYBOARD: {
                fsys.keyboard = pio;
                continue; }
            case HID_DISPLAY: {
                fsys.display = pio;
                continue; }
            case HID_CHASSIS: {
                fsys.chassis = pio;
                struct cvec* dbs = &fsys.chassis->device.chassis.digital_buttons;
                for (u32 j = 0; j < cvec_len(dbs); j++) {
                    struct HID_digital_button* db = (struct HID_digital_button*)cvec_get(dbs, j);
                    switch(db->common_id) {
                        case DBCID_ch_pause:
                            fsys.inputs.ch_pause = db;
                            continue;
                        case DBCID_ch_power:
                            fsys.inputs.ch_power = db;
                            continue;
                        case DBCID_ch_reset:
                            fsys.inputs.ch_reset = db;
                            continue;
                        default:
                            continue;
                    }
                }
                break; }
            case HID_MOUSE:
                fsys.mouse = pio;
                break;
            case HID_DISC_DRIVE:
                fsys.disk_drive = pio;
                break;
            case HID_CART_PORT:
                fsys.cartridge_port = pio;
                break;
            case HID_AUDIO_CASSETTE:
                fsys.audio_cassette = pio;
                break;
            default:
                break;
        }
    }
    assert(fsys.display);
    assert(fsys.chassis);
    fsys.worked = 1;
    return fsys;
}
void destroy_system(struct full_system *fsys)
{
    if (fsys->sys == nullptr) return;
    jsm_delete(fsys->sys);
    fsys->sys = nullptr;
}

void sys_present(struct full_system *fsys, void *outptr, u32 out_width, u32 out_height)
{
    struct jsm_system *sys = fsys->sys;
    struct framevars fv = {};
    sys->get_framevars(sys, &fv);
    jsm_present(sys->kind, fsys->display, outptr, out_width, out_height);
}

void sys_do_frame(struct full_system *fsys) {
    struct jsm_system *sys = fsys->sys;
    struct framevars fv = {};
    if (!dbg.do_break)
        sys->finish_frame(sys);
    sys->get_framevars(sys, &fv);
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
