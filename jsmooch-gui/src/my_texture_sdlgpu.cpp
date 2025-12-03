//
// Created by . on 12/3/25.
//

#include "my_texture_sdlgpu.h"

#include <cstdlib>

#include "SDL3/SDL.h"
#include "SDL3/SDL_gpu.h"

#ifdef JSM_SDLGPU

void create_texture(ImTextureData &td, int width, int height)
{
    return;
    td.Status = ImTextureStatus_WantCreate;
    td.Format = ImTextureFormat_RGBA32;
    td.Width = width;
    td.Height = height;
    td.BytesPerPixel = 4;
    td.Pixels = static_cast<unsigned char*>(malloc(width*height*4));
    memset(td.Pixels, 0, width*height*4);
    td.SetStatus(ImTextureStatus_WantCreate);
    ImGui_ImplSDLGPU3_UpdateTexture(&td);
    assert(td.Status == ImTextureStatus_OK);
}

void my_texture::setup(const char *label, u32 twidth, u32 theight) {
    width = twidth;
    height = theight;
    create_texture(tex.data, width, height);
    is_good = 1;
}

void my_texture::upload_data(void *source_ptr, size_t sz, u32 source_width, u32 source_height)
{
    return;
    tex.data.SetStatus(ImTextureStatus_WantUpdates);
    tex.data.UpdateRect.x = 0;
    tex.data.UpdateRect.y = 0;
    tex.data.UpdateRect.w = source_width;
    tex.data.UpdateRect.h = source_height;
    memcpy(tex.data.Pixels, source_ptr, source_width * source_height * 4);
    ImGui_ImplSDLGPU3_UpdateTexture(&tex.data);
    assert(tex.data.Status == ImTextureStatus_OK);
}

my_texture::~my_texture() {
    if (tex.data.Pixels != nullptr) {
        free(tex.data.Pixels);
    }
    tex.data.Pixels = nullptr;
    tex.data.SetStatus(ImTextureStatus_WantDestroy);
    ImGui_ImplSDLGPU3_UpdateTexture(&tex.data);
    //assert(tex.data.Status == ImTextureStatus_Destroyed);
}

#endif