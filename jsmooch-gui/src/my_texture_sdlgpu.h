//
// Created by . on 12/3/25.
//

#pragma once

class my_texture_sdlgpu {
};


#include "helpers/int.h"
#include "build.h"
#ifdef JSM_SDLGPU

#include "SDL3/SDL.h"
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_pixels.h"
#include "../vendor/myimgui/imgui.h"

struct my_texture  {
    struct {
        SDL_GPUTexture *ptr;
    } tex{};
    struct {
    } view{};
    u32 height{}, width{};
    u32 is_good{};
    ImVec2 uv0{}, uv1{};
    ImVec2 sz_for_display{};
    ImVec2 zoom_sz_for_display(float zoom) { return {sz_for_display.x * zoom, sz_for_display.y * zoom}; }
    [[nodiscard]] ImTextureRef for_image();
    SDL_GPUDevice *device;
    SDL_GPUTextureCreateInfo info;


    char store_label[500]{};

    ~my_texture();
    void setup(SDL_GPUDevice *device, const char *label, u32 width, u32 height);
    void upload_data(void *source_ptr, size_t sz, u32 source_width, u32 source_height);
};
#endif
