#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#include <stdlib.h>
#include <pwd.h>
#include <unistd.h>
#include "helpers/sys_interface.h"
#include "helpers/sys_present.h"
#include "helpers/debug.h"

#include "system/dreamcast/gdi.h"
#include "helpers/physical_io.h"

// mac overlay - 14742566
// i get to    - 88219648

//#define NEWSYS
#define NEWSYS_STEP2
//#define DO_DREAMCAST
//#define SIDELOAD
//#define STOPAFTERAWHILE

struct system_io {
    struct CDKRKR {
        struct HID_digital_button* up;
        struct HID_digital_button* down;
        struct HID_digital_button* left;
        struct HID_digital_button* right;
        struct HID_digital_button* fire1;
        struct HID_digital_button* fire2;
        struct HID_digital_button* fire3;
        struct HID_digital_button* start;
        struct HID_digital_button* select;
    } p[2];
    struct kb {
        //SDL_KeyCode
        //struct HID_digital_button* 1
    };
    struct HID_digital_button* ch_power;
    struct HID_digital_button* ch_reset;
    struct HID_digital_button* ch_pause;
};


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


u32 handle_keys_gb(SDL_Event *event, u32 *input_buffer) {
    u32 ret = 0;
    i32 evt = event->type == SDL_KEYDOWN ? 1 : event->type == SDL_KEYUP ? 0 : -1;
    if (evt >= 0) {
        switch (event->key.keysym.sym) {
            case SDLK_LEFT:
                input_buffer[2] = evt;
                break;
            case SDLK_RIGHT:
                input_buffer[3] = evt;
                break;
            case SDLK_UP:
                input_buffer[0] = evt;
                break;
            case SDLK_DOWN:
                input_buffer[1] = evt;
                break;
            case SDLK_z:
                input_buffer[4] = evt;
                break;
            case SDLK_x:
                input_buffer[5] = evt;
                break;
            case SDLK_c:
                input_buffer[8] = evt;
                break;
            case SDLK_a:
                input_buffer[6] = evt;
                break;
            case SDLK_s:
                input_buffer[7] = evt;
                break;
            case SDLK_ESCAPE:
                ret = 1;
                break;
            default:
                break;
        }
    }
    return ret;
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
    printf("\nLOADED IP.BIN SIZE %04x", mfs->files[1].buf.size);
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



int main(int argc, char** argv)
{
#ifdef DO_DREAMCAST
    enum jsm_systems which = SYS_DREAMCAST;
#else
    //enum jsm_systems which = SYS_ATARI2600;
    //enum jsm_systems which = SYS_GENESIS;
    //enum jsm_systems which = SYS_ZX_SPECTRUM;
    enum jsm_systems which = SYS_MAC512K;
#endif
    //test_gdi();
    //return 0;

    struct multi_file_set BIOSes;
    mfs_init(&BIOSes);

    if(SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        return -1;
    }
    SDL_Window *window = SDL_CreateWindow("JSMooCh",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          640, 480,
                                          0);

    if(!window)
        return -2;

    SDL_Surface *window_surface = SDL_GetWindowSurface(window);

    if(!window_surface)
        return -3;

    // Create our emulator

    struct jsm_system* sys = new_system(which);
    struct cvec *IOs = &sys->IOs;

    u32 has_bios = grab_BIOSes(&BIOSes, which);
    if (has_bios) {
        sys->load_BIOS(sys, &BIOSes);
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
            worked = grab_ROM(&ROMs, which, "tetris.gb", NULL);
            break;
        case SYS_GBC:
            worked = grab_ROM(&ROMs, which, "wario3.gbc", NULL);
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
        return -1;
    }

    struct physical_io_device* fileioport = load_ROM_into_emu(sys, IOs, &ROMs);
    mfs_delete(&ROMs);

#ifdef SIDELOAD
    struct multi_file_set sideload_image;
    mfs_init(&sideload_image);
    grab_ROM(&sideload_image, which, "gl_matrix.elf", "kos");
    mfs_add_IP_BIN(&sideload_image);
    sys->sideload(sys, &sideload_image);
    mfs_delete(&sideload_image);
#endif
    struct system_io inputs;
    memset(&inputs, 0, sizeof(struct system_io));

    struct physical_io_device* controller1 = NULL;
    struct physical_io_device* controller2 = NULL;
    struct physical_io_device* display = NULL;
    struct physical_io_device* chassis = NULL;
    for (u32 i = 0; i < cvec_len(IOs); i++) {
        struct physical_io_device* pio = (struct physical_io_device*)cvec_get(IOs, i);
        switch(pio->kind) {
            case HID_CONTROLLER: {
                if (controller1 == NULL) {
                    controller1 = pio;
                    setup_controller(&inputs, pio, 0);
                }
                else {
                    controller2 = pio;
                    setup_controller(&inputs, pio, 1);
                }
                continue; }
            case HID_KEYBOARD: {
                controller1 = pio;
                continue; }
            case HID_DISPLAY: {
                display = pio;
                continue; }
            case HID_CHASSIS: {
                chassis = pio;
                struct cvec* dbs = &chassis->device.chassis.digital_buttons;
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
            default:
                break;
        }
    }
    assert(controller1);
    assert(display);
    assert(chassis);

    struct framevars fv;
    SDL_Event event;

    u32 input_buffer[20];
    for (u32 i = 0; i < 20; i++) {
        input_buffer[i] = 0;
    }

    #ifdef DO_DREAMCAST
        //dbg_disable_trace();
        //dbg_enable_trace();
        sys->step_master(sys, 300000000); //
        //sys->step_master(sys, 6555144);
        //sys->step_master(sys, 7328221); //
        dbg_unbreak();
        dbg_enable_trace();
        sys->step_master(sys, 500);
        dbg_LT_dump();
        dbg_flush();
        return 0;
    /*jsm_present(sys->kind, 0, &iom, window_surface->pixels, 640, 480);
    SDL_UpdateWindowSurface(window);
    SDL_Delay(20000);
    return 0;*/

    //u32 b = SDL_GetTicks();
    /*float rend = ((float)b) /
                 1000.0f;
    float rstart = ((float)a) / 1000.0f;
    float r = rend - rstart;*/


    //sys->step_master(sys, 720);
    //dbg_flush();

    //sys->get_framevars(sys, &fv); printf("\nCYCLES! %llu (%llu)", fv.master_cycle, fv.master_cycle - first);
#endif

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
    //SDL_Log("TIME! %f", r);
    u32 quit = 0;
    u32 before, after;

    before = SDL_GetTicks();
    u32 did_break = 0;
    u32 loops = 0;
    while(!quit) {
        float start = SDL_GetTicks();
        while(SDL_PollEvent(&event)) {
            quit = handle_keys_gb(&event, input_buffer);
        }

        map_inputs(input_buffer, &inputs, sys);
        sys->finish_frame(sys);
        //printf("\nFRAME FINISHED!");
        sys->get_framevars(sys, &fv);
        //printf("MCYCLE: %lld", fv.master_cycle);
        //printf("\nFRAME! %lld", fv.master_frame); fflush(stdout);
        /*if (fv.master_cycle >= 120965401) {
            dbg_break();
            dbg.do_break = 1;
        }*/
        //if (fv.master_cycle >= 120965401) {
#ifdef STOPAFTERAWHILE
        // 14742566
        if (fv.master_cycle >= ((14742566*2)+500)) {
        //if (fv.master_cycle >= 12991854) {
            dbg_break(">240 FRAMES", fv.master_cycle);
            dbg.do_break = 1;
        }
#endif
        if (dbg.do_break) {
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
        //jsm_present(sys->kind, fv.last_used_buffer, &iom, window_surface->pixels, 640, 480);
        jsm_present(sys->kind, display, window_surface->pixels, 640, 480);
        SDL_UpdateWindowSurface(window);
        float end = SDL_GetTicks();
        float ticks_taken = end - start;
        //printf("\n%llu", fv.master_frame);
        //printf("\n%f", ticks_taken);
        float tick_target = 16.7f;
        if (ticks_taken < tick_target)
            SDL_Delay(tick_target - ticks_taken);
        //loops++;
        //if (loops > 5) break;
        //fflush(stdout);
    }
    /*after = SDL_GetTicks();
    float rend = ((float)after) /
     1000.0f;
    float rstart = ((float)before) / 1000.0f;
    float r = rend - rstart;
    printf("\nTIME %f", r);
    float num_cycles = (float)(((struct GB *)sys->ptr)->cpu.cpu.trace_cycles);
    printf("\nNUM CYCLES %f", num_cycles);
    float cycles_per_second = num_cycles / r;
    //printf("\nCYCLES PER SECOND %f", cycles_per_second);
    SDL_Log("\nCYCLES PER SECOND %f", cycles_per_second);
    fflush(stdout);*/

    // Clean up and be tidy!
    jsm_delete(sys);
    SDL_DestroyWindowSurface(window);
    SDL_DestroyWindow(window);
}
