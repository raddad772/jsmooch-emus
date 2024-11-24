#include <string>
#include "my_texture_ogl3.h"
#include "SDL3/SDL.h"
#include "SDL3/SDL_opengl.h"

static GLuint create_texture()
{
    // Create a OpenGL texture identifier
    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return image_texture;
}

static void upload_to_texture(GLuint dest, void *src, u32 width, u32 height)
{
    glBindTexture(GL_TEXTURE_2D, dest);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, src);

}


void my_texture::setup(const char *label, u32 twidth, u32 theight) {
    width = twidth;
    height = theight;
    tex.item = create_texture();
    is_good = 1;
}

void my_texture::upload_data(void *source_ptr, size_t sz, u32 source_width, u32 source_height)
{
    upload_to_texture(tex.item, source_ptr, source_width, source_height);
}

my_texture::~my_texture() {
    if (tex.item != 0) {
        glDeleteTextures(1, &tex.item);
    }
    tex.item = 0;
}