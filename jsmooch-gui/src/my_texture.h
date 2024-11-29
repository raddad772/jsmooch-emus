//
// Created by . on 11/27/24.
//

#ifndef JSMOOCH_EMUS_MY_TEXTURE_H
#define JSMOOCH_EMUS_MY_TEXTURE_H

#include "build.h"

#ifdef JSM_METAL

#endif

#ifdef JSM_OPENGL
#include "my_texture_ogl3.h"
#endif

#ifdef JSM_SDLR3
#include "my_texture_sdlr3.h"
#endif

#endif //JSMOOCH_EMUS_MY_TEXTURE_H
