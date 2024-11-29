//
// Created by . on 11/27/24.
//

#ifndef JSMOOCH_EMUS_MY_TEXTURE_SDLR3_H
#define JSMOOCH_EMUS_MY_TEXTURE_SDLR3_H

#include "helpers/int.h"
#include "build.h"
#include "SDL3/SDL.h"
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_pixels.h"
#include "../vendor/myimgui/imgui.h"

struct my_texture  {
    struct {
        //WGPUTextureDescriptor desc{};
        //WGPUTexture item{};
        SDL_Texture *item{};
    } tex{};
    struct {
        //WGPUTextureViewDescriptor desc{};
        //WGPUTextureView item{};
    } view{};
    u32 height{}, width{};
    u32 is_good{};
    ImVec2 uv0{}, uv1{};
    ImVec2 sz_for_display{};
    ImVec2 zoom_sz_for_display(float zoom) { return {sz_for_display.x * zoom, sz_for_display.y * zoom}; }
    [[nodiscard]] ImTextureID for_image() { return (ImTextureID)tex.item; }
    SDL_Renderer *renderer;

    char store_label[500]{};

    ~my_texture();
    void setup(SDL_Renderer *renderer, const char *label, u32 width, u32 height);
    void upload_data(void *source_ptr, size_t sz, u32 source_width, u32 source_height);
};


#endif //JSMOOCH_EMUS_MY_TEXTURE_SDLR3_H
