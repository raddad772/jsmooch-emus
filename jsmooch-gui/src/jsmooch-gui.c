#include <stdio.h>
#include <SDL.h>
#include <stdlib.h>
#include <pwd.h>
#include <unistd.h>
#include "helpers/sys_interface.h"
#include "helpers/sys_present.h"
#include "helpers/debug.h"

#include "system/dreamcast/gdi.h"


//#define DO_DREAMCAST

u32 handle_keys_gb(SDL_Event *event, u32 *input_buffer) {
    u32 ret = 0;
    i32 evt = event->type == SDL_KEYDOWN ? 1 : event->type == SDL_KEYUP ? 0 : -1;
    if (evt > 0) {
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
            case SDLK_a:
                input_buffer[6] = evt;
                break;
            case SDLK_s:
                input_buffer[7] = evt;
                break;
            case SDLK_ESCAPE:
                ret = 1;
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
    sprintf(PATH, "%s/Documents/emu/rom/dreamcast/crazy_taxi", homeDir);
    printf("\nHEY! %s", PATH);
    GDI_load(PATH, "crazytaxi.gdi", &foo);

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

    sprintf(BASE_PATH, "%s/Documents/emu/bios", homeDir);

    u32 has_bios = 0;
    switch(which) {
        case SYS_SMS1:
        case SYS_SMS2:
            has_bios = 1;
            sprintf(BIOS_PATH, "%s/master_system", BASE_PATH);
            mfs_add("bios13fx.sms", BIOS_PATH, BIOSes);
            break;
        case SYS_DREAMCAST:
            has_bios = 1;
            sprintf(BIOS_PATH, "%s/dreamcast", BASE_PATH);
            mfs_add("dc_boot.bin", BIOS_PATH, BIOSes);
            mfs_add("dc_flash.bin", BIOS_PATH, BIOSes);
            break;
        case SYS_DMG:
            has_bios = 1;
            sprintf(BIOS_PATH, "%s/gameboy", BASE_PATH);
            mfs_add("gb_bios.bin", BIOS_PATH, BIOSes);
            break;
        case SYS_GBC:
            has_bios = 1;
            sprintf(BIOS_PATH, "%s/gameboy", BASE_PATH);
            mfs_add("gbc_bios.bin", BIOS_PATH, BIOSes);
            break;
        case SYS_PSX:
        case SYS_ZX_SPECTRUM:
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

u32 grab_ROM(struct multi_file_set* ROMs, enum jsm_systems which, const char* fname)
{
    char BASER_PATH[255];
    char BASE_PATH[255];
    char ROM_PATH[255];
    u32 worked = 0;

    const char *homeDir = getenv("HOME");

    if (!homeDir) {
        struct passwd* pwd = getpwuid(getuid());
        if (pwd)
            homeDir = pwd->pw_dir;
    }

    sprintf(BASER_PATH, "%s/Documents/emu/rom", homeDir);

    u32 has_bios = 0;
    switch(which) {
        case SYS_SMS1:
        case SYS_SMS2:
            sprintf(BASE_PATH, "%s/master_system", BASER_PATH);
            worked = 1;
            break;
        case SYS_DREAMCAST:
            sprintf(BASE_PATH, "%s/dreamcast/crazy_taxi", BASER_PATH);
            worked = 1;
            break;
        case SYS_DMG:
        case SYS_GBC:
            sprintf(BASE_PATH, "%s/gameboy", BASER_PATH);
            worked = 1;
            break;
        case SYS_ATARI2600:
            sprintf(BASE_PATH, "%s/atari2600", BASER_PATH);
            worked = 1;
            break;
        case SYS_NES:
            sprintf(BASE_PATH, "%s/nes", BASER_PATH);
            worked = 1;
            break;
        case SYS_PSX:
        case SYS_ZX_SPECTRUM:
        case SYS_GENESIS:
        case SYS_SNES:
        case SYS_BBC_MICRO:
        case SYS_GG:
            worked = 0;
            break;
        default:
            worked = 0;
            printf("\nNO CASE FOR SYSTEM %d", which);
            break;
    }
    if (!worked) return 0;
    mfs_add(fname, BASE_PATH, ROMs);
    //printf("\n%d %s %s", ROMs->files[ROMs->num_files-1].buf.size > 0, BASE_PATH, fname);
    return ROMs->files[ROMs->num_files-1].buf.size > 0;
}

int main(int argc, char** argv)
{
#ifdef DO_DREAMCAST
    enum jsm_systems which = SYS_DREAMCAST;
#else
    enum jsm_systems which = SYS_ATARI2600;
    //enum jsm_systems which = SYS_NES;
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
    struct JSM_IOmap iom;

    u16 *output_buffers[2];
    u32 inputs[50];
    for (u32 i = 0; i < 50; i++) {
        inputs[i] = 0;
    }
    switch(which) {
        case SYS_SMS1:
        case SYS_SMS2:
        case SYS_GG:
        case SYS_NES:
            output_buffers[0] = malloc(256*240*2);
            output_buffers[1] = malloc(256*240*2);
            break;
        case SYS_DREAMCAST:
            output_buffers[0] = malloc(640*480*4);
            output_buffers[1] = malloc(640*480*4);
            break;
        case SYS_DMG:
        case SYS_GBC:
            output_buffers[0] = malloc(160*144*4);
            output_buffers[1] = malloc(160*144*4);
            break;
        case SYS_ATARI2600:
            output_buffers[0] = malloc(160*240);
            output_buffers[1] = malloc(160*240);
            break;
    }
    iom.out_buffers[0] = (void *)output_buffers[0];
    iom.out_buffers[1] = (void *)output_buffers[1];

    struct jsm_system* sys = new_system(which, &iom);

    u32 has_bios = grab_BIOSes(&BIOSes, which);
    if (has_bios) {
        sys->load_BIOS(sys, &BIOSes);
    }
    mfs_delete(&BIOSes);

    struct multi_file_set ROMs;
    mfs_init(&ROMs);
#ifdef DO_DREAMCAST
    u32 worked = grab_ROM(&ROMs, which, "crazytaxi.gdi");
#else
    u32 worked = grab_ROM(&ROMs, which, "frogger.a26");
    //u32 worked = grab_ROM(&ROMs, which, "mario3.nes");
#endif
    if (!worked) {
        printf("\nCouldn't open ROM!");
        return -1;
    }
    sys->load_ROM(sys, &ROMs);
    mfs_delete(&ROMs);

    struct framevars fv;
    SDL_Event event;

    u32 input_buffer[20];
    for (u32 i = 0; i < 20; i++) {
        input_buffer[i] = 0;
    }

#ifdef DO_DREAMCAST

    //SDL_Log("\n4");

    //dbg_enable_trace();
    //sys->step_master(sys, 2621310); // 2621319 is out-of-bounds write due to memory at 8C00B7BC being written wrong.
                                    // memory is written at cycle 58615 PC: 8c0000ea val: 0000
    //sys->step_master(sys, 58600);
    //dbg_enable_trace();
    //sys->step_master(sys, 63);
    //dbg_LT_clear();
    //dbg_disable_trace();
    dbg_enable_trace();
    //sys->step_master(sys, 11494000); // end of copy BIOS to RAM and start exec inRAM at 8c000100
    //sys->step_master(sys, 2000000000); //
    sys->step_master(sys, 20000000); //
    //sys->step_master(sys, 1000000000); // end of copy BIOS to RAM and start exec inRAM at 8c000100
    dbg_unbreak();
    dbg_enable_trace();
    //printf("\n-----traces");
    //sys->step_master(sys, 800);
    dbg_LT_dump();
    dbg_flush();
    return 0;
    /*jsm_present(sys->which, 0, &iom, window_surface->pixels, 640, 480);
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
    //SDL_Log("TIME! %f", r);
    u32 quit = 0;
    u32 before, after;

    before = SDL_GetTicks();
    u32 did_break = 0;
    u32 loops = 0;
    while(!quit) {
        //printf("\nFRAME!"); fflush(stdout);
        float start = SDL_GetTicks();
        while(SDL_PollEvent(&event)) {
            quit = handle_keys_gb(&event, input_buffer);
        }

        sys->map_inputs(sys, input_buffer, sizeof(inputs)/4);
        sys->finish_frame(sys);
        sys->get_framevars(sys, &fv);
        if (dbg.do_break) {
            //break;
            if (did_break == 0) {
                dbg_enable_trace();
                dbg.do_break = 0;
                dbg_unbreak();
                sys->step_master(sys, 150);
                dbg_flush();
                dbg_disable_trace();
            }
            if (did_break == 1) break;
            did_break++;
        }
        //jsm_present(sys->which, fv.last_used_buffer, &iom, window_surface->pixels, 640, 480);
        jsm_present(sys->which, 0, &iom, window_surface->pixels, 640, 480);
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

    free(output_buffers[0]);
    free(output_buffers[1]);

    // Clean up and be tidy!
    jsm_delete(sys);
    SDL_DestroyWindowSurface(window);
    SDL_DestroyWindow(window);
}
