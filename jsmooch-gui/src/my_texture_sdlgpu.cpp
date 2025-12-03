//
// Created by . on 12/3/25.
//

#include "my_texture_sdlgpu.h"

#include <cstdlib>

#include "SDL3/SDL.h"
#include "SDL3/SDL_gpu.h"

#ifdef JSM_SDLGPU

SDL_GPUTexture *create_texture(SDL_GPUDevice *device, int width, int height)
{
    SDL_GPUTextureCreateInfo ti = {};
    ti.type = SDL_GPU_TEXTURETYPE_2D;
    ti.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ti.width = width;
    ti.height = height;
    ti.layer_count_or_depth = 1;
    ti.num_levels = 1;
    ti.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ti.sample_count = SDL_GPU_SAMPLECOUNT_1;

    SDL_GPUTexture *tx = SDL_CreateGPUTexture(device, &ti);
    return tx;
    // Create texture
}

void my_texture::setup(SDL_GPUDevice *in_device, const char *label, u32 twidth, u32 theight) {
    device = in_device;
    width = twidth;
    height = theight;
    tex.ptr = create_texture(device, width, height);
    assert(tex.ptr);
    snprintf(store_label, sizeof(store_label), "%s", label);
    printf("\nSETUP TEXTURE %s", store_label);
    is_good = 1;
}

ImTextureRef my_texture::for_image() { return tex.ptr; }

void my_texture::upload_data(void *source_ptr, size_t sz, u32 source_width, u32 source_height)
{
    // FIXME: A real engine would likely keep one around, see what the SDL_GPU backend is doing.
    SDL_GPUTransferBufferCreateInfo transferbuffer_info = {};
    transferbuffer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferbuffer_info.size = width * height * 4;
    SDL_GPUTransferBuffer* transferbuffer = SDL_CreateGPUTransferBuffer(device, &transferbuffer_info);
    IM_ASSERT(transferbuffer != NULL);

    // Copy to transfer buffer
    uint32_t upload_pitch = source_width * 4;
    auto *image_data = static_cast<unsigned char *>(source_ptr);
    void* texture_ptr = SDL_MapGPUTransferBuffer(device, transferbuffer, true);
    for (int y = 0; y < height; y++)
        memcpy((void*)((uintptr_t)texture_ptr + y * upload_pitch), image_data + y * upload_pitch, upload_pitch);
    SDL_UnmapGPUTransferBuffer(device, transferbuffer);

    SDL_GPUTextureTransferInfo transfer_info = {};
    transfer_info.offset = 0;
    transfer_info.transfer_buffer = transferbuffer;

    SDL_GPUTextureRegion texture_region = {};
    texture_region.texture = tex.ptr;
    texture_region.x = (Uint32)0;
    texture_region.y = (Uint32)0;
    texture_region.w = (Uint32)width;
    texture_region.h = (Uint32)height;
    texture_region.d = 1;

    // Upload
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(cmd);
    SDL_UploadToGPUTexture(copy_pass, &transfer_info, &texture_region, false);
    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(cmd);

    SDL_ReleaseGPUTransferBuffer(device, transferbuffer);

}

my_texture::~my_texture() {
}

#endif