#include <stdio.h>
#include <SDL.h>
#include <stdlib.h>
#include <pwd.h>
#include <unistd.h>
#include "helpers/sys_interface.h"
#include "helpers/sys_present.h"
#include "helpers/debug.h"

#include "system/dreamcast/gdi.h"

struct read_file_buf {
    u64 sz;
    void *buf;
};

int open_and_read(char *fname, struct read_file_buf *rfb)
{
    FILE *fil = fopen(fname, "rb");
    fseek(fil, 0L, SEEK_END);
    rfb->sz = ftell(fil);

    fseek(fil, 0L, SEEK_SET);
    rfb->buf = malloc(rfb->sz);
    fread(rfb->buf, sizeof(char), rfb->sz, fil);

    fclose(fil);
    return 0;
}

void rfb_cleanup(struct read_file_buf *rfb)
{
    if (rfb->buf != NULL) {
        free(rfb->buf);
        rfb->buf = NULL;
    }
    rfb->sz = 0;
}

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
    GDI_load("c:\\dev\\rom\\dc\\crazytaxi", "crazytaxi.gdi", &foo);

    GDI_delete(&foo);
}

u32 grab_BIOS(struct read_file_buf* BIOS, enum jsm_systems which)
{
    char BIOS_PATH[500];
    const char *homeDir = getenv("HOME");

    if (!homeDir) {
        struct passwd* pwd = getpwuid(getuid());
        if (pwd)
            homeDir = pwd->pw_dir;
    }

    char *bp = BIOS_PATH;
    bp += sprintf(bp, "%s", homeDir);
    bp += sprintf(bp, "/Documents/emu/bios/");

    u32 has_bios = 0;
    switch(which) {
        case SYS_SMS1:
        case SYS_SMS2:
            has_bios = 1;
            bp += sprintf(bp, "master_system/bios13fx.sms");
            open_and_read(BIOS_PATH, BIOS);
            break;
        case SYS_DREAMCAST:
            has_bios = 1;
            bp += sprintf(bp, "dreamcast/dc_boot.bin");
            open_and_read(BIOS_PATH, BIOS);
            break;
        case SYS_DMG:
            has_bios = 1;
            bp += sprintf(bp, "gameboy/gb_bios.bin");
            open_and_read(BIOS_PATH, BIOS);
            break;
        case SYS_GBC:
            has_bios = 1;
            bp += sprintf(bp, "gameboy/gbc_bios.bin");
            open_and_read(BIOS_PATH, BIOS);
            break;
        case SYS_PSX:
        case SYS_ZX_SPECTRUM:
        case SYS_GENESIS:
        case SYS_SNES:
        case SYS_NES:
        case SYS_BBC_MICRO:
        case SYS_GG:
            has_bios = 0;
            break;
    }
    return has_bios;
}

u32 grab_ROM(struct read_file_buf* ROM, enum jsm_systems which, const char* fname)
{
    char ROM_PATH[500];
    u32 worked = 0;

    const char *homeDir = getenv("HOME");

    if (!homeDir) {
        struct passwd* pwd = getpwuid(getuid());
        if (pwd)
            homeDir = pwd->pw_dir;
    }

    char *rp = ROM_PATH;
    rp += sprintf(rp, "%s/Documents/emu/rom/", homeDir);

    u32 has_bios = 0;
    switch(which) {
        case SYS_SMS1:
        case SYS_SMS2:
            rp += sprintf(rp, "master_system/");
            worked = 1;
            break;
        case SYS_DREAMCAST:
            rp += sprintf(rp, "dreamcast/");
            worked = 1;
            break;
        case SYS_DMG:
        case SYS_GBC:
            rp += sprintf(rp, "gameboy/");
            worked = 1;
            break;
        case SYS_NES:
            rp += sprintf(rp, "nes/");
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
    }
    if (!worked) return 0;
    rp += sprintf(rp, "%s", fname);
    open_and_read(ROM_PATH, ROM);
    return ROM->sz > 0;
}

int main(int argc, char** argv)
{
    char RFILE[500];
    enum jsm_systems which = SYS_DREAMCAST;

    struct read_file_buf BIOS;

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
    }
    iom.out_buffers[0] = (void *)output_buffers[0];
    iom.out_buffers[1] = (void *)output_buffers[1];

    struct jsm_system* sys = new_system(which, &iom);
    //SDL_Log("\n2");


    //SDL_Log("\n3");
    u32 has_bios = grab_BIOS(&BIOS, which);
    if (has_bios) {
        sys->load_BIOS(sys, BIOS.buf, BIOS.sz);
        rfb_cleanup(&BIOS);
    }

    struct read_file_buf ROM;
    u32 worked = grab_ROM(&ROM, which, "IP.BIN");
    if (!worked) {
        printf("\nCouldn't open ROM!");
        return -1;
    }
    sys->load_ROM(sys, "", ROM.buf, ROM.sz);
    rfb_cleanup(&ROM);


    //sys->play(sys);
    struct framevars fv;
    SDL_Event event;

    u32 input_buffer[20];
    for (u32 i = 0; i < 20; i++) {
        input_buffer[i] = 0;
    }

    //SDL_Log("\n4");


    //dbg_enable_trace();
    /*sys->step_master(sys, 5500000);
    sys->stop(sys);
    dbg_flush();
    jsm_present(sys->which, 0, &iom, window_surface->pixels, 640, 480);
    SDL_UpdateWindowSurface(window);
    SDL_Delay(20000);
    return 0;*/
    //return;

    //u32 b = SDL_GetTicks();
    /*float rend = ((float)b) /
                 1000.0f;
    float rstart = ((float)a) / 1000.0f;
    float r = rend - rstart;*/


    //sys->step_master(sys, 70);
    //dbg_flush();

    //sys->get_framevars(sys, &fv); printf("\nCYCLES! %llu (%llu)", fv.master_cycle, fv.master_cycle - first);

    //SDL_Log("TIME! %f", r);
    u32 quit = 0;
    u32 before, after;

    before = SDL_GetTicks();
    u32 did_break = 0;
    while(!quit) {
        float start = SDL_GetTicks();
        while(SDL_PollEvent(&event)) {
            quit = handle_keys_gb(&event, input_buffer);
        }

        //sys->map_inputs(sys, input_buffer, sizeof(inputs)/4);
        sys->finish_frame(sys);
        sys->get_framevars(sys, &fv);
        if (dbg.do_break) {
            if (did_break == 0) {
                dbg_enable_trace();
                dbg.do_break = 0;
                sys->step_master(sys, 150);
                dbg_flush();
                dbg_disable_trace();
            }
            did_break++;
        }
        //jsm_present(sys->which, fv.last_used_buffer, &iom, window_surface->pixels, 640, 480);
        jsm_present(sys->which, 0, &iom, window_surface->pixels, 640, 480);
        SDL_UpdateWindowSurface(window);
        float end = SDL_GetTicks();
        float ticks_taken = end - start;
        printf("\n%f", ticks_taken);
        float tick_target = 16.7f;
        if (ticks_taken < tick_target)
            SDL_Delay(tick_target - ticks_taken);
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
