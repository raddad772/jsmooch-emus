//
// Created by . on 8/8/24.
//

#include <string>
#include "build.h"
#include "my_texture_wgpu.h"

#ifdef JSM_WEBGPU
void my_texture::setup(WGPUDevice device, const char *label, u32 twidth, u32 theight) {
    width = twidth;
    height = theight;
    wgpu_device = device;
    tex.desc = {};
    tex.desc.label.data = store_label;
    tex.desc.label.length = snprintf(store_label, sizeof(store_label), "%s", label);
    //tex.desc.label.data = store_label;
    tex.desc.nextInChain = nullptr;
    tex.desc.dimension = WGPUTextureDimension_2D;
    tex.desc.size = {twidth, theight, 1};
    tex.desc.mipLevelCount = 1;
    tex.desc.sampleCount = 1;
    tex.desc.format = WGPUTextureFormat_RGBA8Unorm;
    tex.desc.usage = WGPUTextureUsage_CopyDst | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_RenderAttachment;
    tex.item = wgpuDeviceCreateTexture(wgpu_device, &tex.desc);

    view.desc = {};
    view.desc.format = WGPUTextureFormat_RGBA8Unorm;
    view.desc.dimension = WGPUTextureViewDimension_2D;
    view.desc.baseMipLevel = 0;
    view.desc.mipLevelCount = 1;
    view.desc.baseArrayLayer = 0;
    view.desc.arrayLayerCount = 1;
    view.desc.baseArrayLayer = 0;
    view.desc.aspect = WGPUTextureAspect_All;
    view.item = wgpuTextureCreateView(tex.item, &view.desc);
    is_good = 1;
}


void my_texture::upload_data(void *source_ptr, size_t sz, u32 source_width, u32 source_height)
{
    WGPUImageCopyTexture destination;
    WGPUTextureDataLayout source;
    destination.texture = tex.item;
    destination.mipLevel = 0;
    destination.origin = { 0, 0, 0 }; // equivalent of the offset argument of Queue::writeBuffer
    destination.aspect = WGPUTextureAspect_All; // only relevant for depth/Stencil textures

    source.offset = 0;
    source.bytesPerRow = 4 * source_width;
    source.rowsPerImage = source_height;
    wgpuQueueWriteTexture(wgpuDeviceGetQueue(wgpu_device), &destination, source_ptr, sz, &source, &tex.desc.size);
}

my_texture::~my_texture()
{
    if (tex.item) {
        wgpuTextureDestroy(tex.item);
        wgpuTextureRelease(tex.item);
        tex.item = nullptr;
    }
    if (view.item) {
        wgpuTextureViewRelease(view.item);
        view.item = nullptr;
    }
}

#endif