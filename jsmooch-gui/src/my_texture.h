//
// Created by . on 11/27/24.
//

#pragma once

#include "build.h"

#ifdef JSM_SDLGPU
#include "my_texture_sdlgpu.h"
#endif

#ifdef JSM_METAL

#endif

#ifdef JSM_OPENGL
#include "my_texture_ogl3.h"
#endif

#ifdef JSM_SDLR3
#include "my_texture_sdlr3.h"
#endif

#ifdef JSM_WEBGPU
#include "my_texture_wgpu.h"
#endif
