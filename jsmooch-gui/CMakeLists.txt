set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

add_definitions(-DJSM_SDLGPU)

include_directories(~/dev/libs/include)
link_directories(~/dev/libs/bin)

#option(DAWN_BUILD_SAMPLES "Enables building Dawn's samples" OFF)
#option(TINT_BUILD_CMD_TOOLS "Build the Tint command line tools" OFF)
#option(TINT_BUILD_DOCS "Build documentation" OFF)
#option(TINT_BUILD_TESTS "Build tests" OFF)

add_executable(jsmooch-gui
        src/my_texture_sdlgpu.cpp
        src/my_texture_sdlgpu.h)

# Add application src
target_sources(
        jsmooch-gui 
        PRIVATE 
        src/full_sys.cpp
        src/full_sys.h
        src/application.h
        src/application.cpp
        src/keymap_translate.cpp
        src/keymap_translate.h
        src/audiowrap.cpp
        src/audiowrap.h
        src/build.h
        src/my_texture.h
        src/my_texture_sdlr3.cpp
        src/my_texture_sdlr3.h
        src/my_texture_wgpu.h
        src/my_texture_ogl3.h
        src/my_texture_ogl3.cpp
        src/my_texture_wgpu.cpp
)

# Add a subset of ImGui source files directly to the executable to fit our use case
target_sources(
        jsmooch-gui
        PRIVATE

        # Among the different backends available, we are interested in connecting
        # the GUI to GLFW andWebGPU:
        #vendor/myimgui/backends/imgui_impl_wgpu.h
        #vendor/myimgui/backends/imgui_impl_wgpu.cpp
        #vendor/myimgui/backends/imgui_impl_glfw.h
        #vendor/myimgui/backends/imgui_impl_glfw.cpp
        vendor/myimgui/backends/imgui_impl_sdl3.h
        vendor/myimgui/backends/imgui_impl_sdl3.cpp
        vendor/myimgui/backends/imgui_impl_sdlgpu3.h
        vendor/myimgui/backends/imgui_impl_sdlgpu3.cpp


        # Bonus to add some C++ specific features (the core ImGUi is a C library)
        vendor/myimgui/misc/cpp/imgui_stdlib.h
        vendor/myimgui/misc/cpp/imgui_stdlib.cpp

        # The core ImGui files
        vendor/myimgui/imconfig.h
        vendor/myimgui/imgui.h
        vendor/myimgui/imgui.cpp
        vendor/myimgui/imgui_draw.cpp
        vendor/myimgui/imgui_internal.h
        vendor/myimgui/imgui_tables.cpp
        vendor/myimgui/imgui_widgets.cpp
        vendor/myimgui/imstb_rectpack.h
        vendor/myimgui/imstb_textedit.h
        vendor/myimgui/imstb_truetype.h
)

#FIND_PACKAGE(SDL2 REQUIRED)
#include_directories( ${SDL2_INCLUDE_DIR} )

target_include_directories(
    jsmooch-gui
    PRIVATE
    ${CMAKE_SOURCE_DIR}/jsmooch-lib/src
    vendor/myimgui
)

#add_subdirectory(vendor/WebGPU-distribution)
#add_subdirectory(vendor/glfw3webgpu)
#find_package(Dawn)

# [...]

target_link_libraries(jsmooch-gui jsmooch-lib SDL3)
#target_link_libraries(jsmooch-gui jsmooch-lib glfw3 webgpu_dawn dawn_glfw "-framework CoreFoundation" "-framework AppKit" "-framework Cocoa" "-framework IOKit" "-framework OpenGL" "-framework Quartz")
#webgpu glfw3webgpu webgpu_cpp  webgpu_glfw
#target_link_libraries(jsmooch-gui PRIVATE )

install(TARGETS jsmooch-gui RUNTIME DESTINATION bin)
