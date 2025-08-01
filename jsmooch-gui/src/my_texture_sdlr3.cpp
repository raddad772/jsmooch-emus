//
// Created by . on 11/27/24.
//

#include <string>
#include "build.h"
#include "my_texture_sdlr3.h"

#ifdef JSM_SDLR3

#include "SDL3/SDL.h"
#include "SDL3/SDL_render.h"

SDL_Texture *create_texture(SDL_Renderer *renderer, int width, int height)
{
    return SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, width, height);
}


void my_texture::setup(SDL_Renderer *mrenderer, const char *label, u32 twidth, u32 theight) {
    renderer = mrenderer;
    width = twidth;
    height = theight;
    tex.item = create_texture(renderer, width, height);
    is_good = 1;
}

void my_texture::upload_data(void *source_ptr, size_t sz, u32 source_width, u32 source_height)
{
    SDL_UpdateTexture(tex.item, NULL, source_ptr, source_width * 4);
}

my_texture::~my_texture() {
    if (tex.item != nullptr) {
        SDL_DestroyTexture(tex.item);
    }
    tex.item = nullptr;
}
#endif