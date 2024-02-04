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

u32 handle_keys_gb(SDL_Event *event, u32 *input_buffer)
{
    u32 ret = 0;
    switch (event->type) {
        case SDL_KEYDOWN:
            switch (event->key.keysym.sym) {
                case SDLK_LEFT:
                    input_buffer[2] = 1;
                    break;
                case SDLK_RIGHT:
                    input_buffer[3] = 1;
                    break;
                case SDLK_UP:
                    input_buffer[0] = 1;
                    break;
                case SDLK_DOWN:
                    input_buffer[1] = 1;
                    break;
                case SDLK_z:
                    input_buffer[4] = 1;
                    break;
                case SDLK_x:
                    input_buffer[5] = 1;
                    break;
                case SDLK_a:
                    input_buffer[6] = 1;
                    break;
                case SDLK_s:
                    input_buffer[7] = 1;
                    break;
                case SDLK_ESCAPE:
                    ret = 1;
                    break;
            }
            break;
        case SDL_KEYUP:
            switch (event->key.keysym.sym) {
                case SDLK_LEFT:
                    input_buffer[2] = 0;
                    break;
                case SDLK_RIGHT:
                    input_buffer[3] = 0;
                    break;
                case SDLK_UP:
                    input_buffer[0] = 0;
                    break;
                case SDLK_DOWN:
                    input_buffer[1] = 0;
                    break;
                case SDLK_z:
                    input_buffer[4] = 0;
                    break;
                case SDLK_x:
                    input_buffer[5] = 0;
                    break;
                case SDLK_a:
                    input_buffer[6] = 0;
                    break;
                case SDLK_s:
                    input_buffer[7] = 0;
                    break;
            }
            break;

        default:
            break;
    }
    return ret;
}

int main(int argc, char** argv)
{
    char RFILE[500];
    //char tn[60] = "boot_div-A.gb";
    //sprintf(RFILE, "c:\\dev\\mooneye-tests\\misc\\%s", tn);
    //sprintf(RFILE, "C:\\dev\\personal\\jsmooch-emus\\cmake-build-debug\\jsmooch-gui\\test\\statcount-auto.gb");
    sprintf(RFILE, "C:\\dev\\personal\\jsmooch-emus\\cmake-build-debug\\jsmooch-gui\\pyellow.gbc");

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

    struct jsm_system* sys = new_system(SYS_GBC, &iom);

    struct read_file_buf ROM;
    struct read_file_buf BIOS;
    open_and_read(RFILE, &ROM);
    //open_and_read("gb_bios.bin", &BIOS);
    open_and_read("gbc_bios.bin", &BIOS);
    // Next troubleshoot if IRQs are happening
    // and results of reads from IO are properly handled

    sys->load_BIOS(sys, BIOS.buf, BIOS.sz);
    //sys->load_ROM(sys, "Tetris.gb", ROM.buf, ROM.sz);
    sys->load_ROM(sys, "Tetris.gb", ROM.buf, ROM.sz);

    rfb_cleanup(&ROM);
    rfb_cleanup(&BIOS);

    //sys->play(sys);
    sys->play(sys);
    struct framevars fv;
    SDL_Event event;

    u32 input_buffer[20];
    for (u32 i = 0; i < 20; i++) {
        input_buffer[i] = 0;
    }

    /*
	this->controller_in.up = bufptr[0];
	this->controller_in.down = bufptr[1];
	this->controller_in.left = bufptr[2];
	this->controller_in.right = bufptr[3];
	this->controller_in.a = bufptr[4];
	this->controller_in.b = bufptr[5];
	this->controller_in.start = bufptr[6];
	this->controller_in.select = bufptr[7];
     */
    u32 quit = 0;
    while(!quit) {
        while(SDL_PollEvent(&event)) {
            quit = handle_keys_gb(&event, input_buffer);
        }
        if (quit) break;

        sys->map_inputs(sys, input_buffer, sizeof(inputs)/4);
        sys->finish_frame(sys);
        sys->get_framevars(sys, &fv);
        jsm_present(sys->which, fv.last_used_buffer, &iom, window_surface->pixels, 640, 480);
        SDL_UpdateWindowSurface(window);
        SDL_Delay(10);
        fflush(stdout);
    }

    free(output_buffers[0]);
    free(output_buffers[1]);

    // Clean up and be tidy!
    jsm_delete(sys);
    SDL_DestroyWindowSurface(window);
    SDL_DestroyWindow(window);
}
