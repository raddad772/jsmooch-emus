#include <stdio.h>
#include <SDL2/SDL.h>
#include <stdlib.h>
#include "helpers/sys_interface.h"
#include "helpers/sys_present.h"

#include "system/gb/gb.h"
#include "component/cpu/sh4/sh4_disassembler.h"

struct read_file_buf {
    size_t sz;
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

int main(int argc, char** argv)
{
    char RFILE[500];
    //char tn[60] = "boot_div-A.gb";
    //sprintf(RFILE, "c:\\dev\\mooneye-tests\\misc\\%s", tn);
    //sprintf(RFILE, "C:\\dev\\personal\\jsmooch-emus\\cmake-build-debug\\jsmooch-gui\\test\\statcount-auto.gb");
    //sprintf(RFILE, "C:\\dev\\personal\\jsmooch-emus\\cmake-build-debug\\jsmooch-gui\\tetris.gb");
    sprintf(RFILE, "C:\\dev\\personal\\jsmooch-emus\\cmake-build-debug\\jsmooch-gui\\256b.bin");

    SDL_Log("Attempting to init SDL");
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
    output_buffers[0] = malloc(256*240*2);
    output_buffers[1] = malloc(256*240*2);
    u32 inputs[50];
    for (u32 i = 0; i < 50; i++) {
        inputs[i] = 0;
    }
    iom.out_buffers[0] = (void *)output_buffers[0];
    iom.out_buffers[1] = (void *)output_buffers[1];

    enum jsm_systems which = SYS_DREAMCAST;
    struct jsm_system* sys = new_system(which, &iom);

    struct read_file_buf ROM;
    struct read_file_buf BIOS;
    open_and_read(RFILE, &ROM);
    u32 has_bios = 0;
    switch(which) {
        case SYS_SMS1:
        case SYS_SMS2:
            has_bios = 1;
            open_and_read("sms_bios.sms", &BIOS);
            break;
        case SYS_DREAMCAST:
            has_bios = 1;
            open_and_read("dc_boot.bin", &BIOS);
            break;
        case SYS_DMG:
            has_bios = 1;
            open_and_read("gb_bios.bin", &BIOS);
            break;
        case SYS_GBC:
            has_bios = 1;
            open_and_read("gbc_bios.bin", &BIOS);
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
    //open_and_read("gbc_bios.bin", &BIOS);
    // Next troubleshoot if IRQs are happening
    // and results of reads from IO are properly handled

    if (has_bios)
        sys->load_BIOS(sys, BIOS.buf, BIOS.sz);
    sys->load_ROM(sys, "", ROM.buf, ROM.sz);

    rfb_cleanup(&ROM);
    if (has_bios)
        rfb_cleanup(&BIOS);

    //sys->play(sys);
    sys->play(sys);
    struct framevars fv;
    SDL_Event event;

    u32 input_buffer[20];
    for (u32 i = 0; i < 20; i++) {
        input_buffer[i] = 0;
    }

    sys->step_master(sys, 20);
    return;

    u32 quit = 0;
    u32 before, after;

    before = SDL_GetTicks();
    while(!quit) {
        while(SDL_PollEvent(&event)) {
            quit = handle_keys_gb(&event, input_buffer);
        }

        sys->map_inputs(sys, input_buffer, sizeof(inputs)/4);
        sys->finish_frame(sys);
        sys->get_framevars(sys, &fv);
        jsm_present(sys->which, fv.last_used_buffer, &iom, window_surface->pixels, 640, 480);
        SDL_UpdateWindowSurface(window);
        SDL_Delay(10);
        printf("\nFrame %d", fv.master_frame);
        fflush(stdout);
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
