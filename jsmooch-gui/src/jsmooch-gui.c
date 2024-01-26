#include <stdio.h>
#include <SDL2/SDL.h>
#include <stdlib.h>
#include "helpers/sys_interface.h"
#include "helpers/sys_present.h"

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

int main(int argc, char** argv)
{
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
    output_buffers[0] = malloc(160*144*2);
    output_buffers[1] = malloc(160*144*2);
    u32 inputs[50];
    for (u32 i = 0; i < 50; i++) {
        inputs[i] = 0;
    }
    iom.out_buffers[0] = (void *)output_buffers[0];
    iom.out_buffers[1] = (void *)output_buffers[1];

    struct jsm_system* sys = new_system(SYS_DMG, &iom);

    struct read_file_buf ROM;
    struct read_file_buf BIOS;
    open_and_read("panda.gb", &ROM);
    open_and_read("gb_bios.bin", &BIOS);

    sys->load_BIOS(sys, BIOS.buf, BIOS.sz);
    sys->load_ROM(sys, "Tetris.gb", ROM.buf, ROM.sz);

    rfb_cleanup(&ROM);
    rfb_cleanup(&BIOS);

    //sys->play(sys);
    sys->play(sys);
    struct framevars fv;
    for (u32 f = 0; f < 600; f++) {
        sys->map_inputs(sys, inputs, sizeof(inputs)/4);
        sys->finish_frame(sys);
        sys->get_framevars(sys, &fv);
        jsm_present(sys->which, fv.last_used_buffer, &iom, window_surface->pixels, 640, 480);
        SDL_UpdateWindowSurface(window);
        //SDL_Log("Frame %d", (int)fv.master_frame);
        SDL_Delay(1);
    }

    free(output_buffers[0]);
    free(output_buffers[1]);

    // Clean up and be tidy!
    jsm_delete(sys);
    SDL_DestroyWindowSurface(window);
    SDL_DestroyWindow(window);
}
