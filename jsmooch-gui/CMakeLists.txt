set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_definitions(-DJSM_WEBGPU)
add_definitions(-DIMGUI_IMPL_WEBGPU_BACKEND_DAWN)
include_directories(~/dev/libs/include)
link_directories(~/dev/libs/bin)

#option(DAWN_BUILD_SAMPLES "Enables building Dawn's samples" OFF)
#option(TINT_BUILD_CMD_TOOLS "Build the Tint command line tools" OFF)
#option(TINT_BUILD_DOCS "Build documentation" OFF)
#option(TINT_BUILD_TESTS "Build tests" OFF)

add_executable(jsmooch-gui
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
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

#FIND_PACKAGE(SDL2 REQUIRED)

#include_directories( ${SDL2_INCLUDE_DIR} )
include_directories(${CMAKE_SOURCE_DIR}/jsmooch-lib/src)

add_subdirectory(vendor/myimgui)
#add_subdirectory(vendor/WebGPU-distribution)
#add_subdirectory(vendor/glfw3webgpu)
#find_package(Dawn)

# [...]


#target_link_libraries(jsmooch-gui jsmooch-lib imgui SDL3 "-framework OpenGL")
target_link_libraries(jsmooch-gui jsmooch-lib imgui glfw3 imgui webgpu_dawn dawn_glfw "-framework CoreFoundation" "-framework AppKit" "-framework Cocoa" "-framework IOKit" "-framework OpenGL" "-framework Quartz")
#webgpu glfw3webgpu webgpu_cpp  webgpu_glfw
#target_link_libraries(jsmooch-gui PRIVATE )

install(TARGETS jsmooch-gui RUNTIME DESTINATION bin)
