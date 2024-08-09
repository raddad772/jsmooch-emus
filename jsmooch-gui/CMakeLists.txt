set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_executable(jsmooch-gui
        src/full_sys.cpp
        src/full_sys.h
        ../jsmooch-lib/src/component/cpu/z80/z80_opcodes.c
        src/application.h
        src/application.cpp
        src/keymap_translate.cpp
        src/keymap_translate.h
        src/my_texture.cpp
        src/my_texture.h
)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

#FIND_PACKAGE(SDL2 REQUIRED)

#include_directories( ${SDL2_INCLUDE_DIR} )
include_directories(${CMAKE_SOURCE_DIR}/jsmooch-lib/src) # include files from ProjectB

add_subdirectory(vendor/myimgui)
add_subdirectory(vendor/webgpu)
add_subdirectory(vendor/glfw3webgpu)
# [...]


target_link_libraries(jsmooch-gui jsmooch-lib glfw webgpu glfw3webgpu imgui webgpu_dawn webgpu_cpp webgpu_glfw)
#target_link_libraries(jsmooch-gui PRIVATE )

install(TARGETS jsmooch-gui RUNTIME DESTINATION bin)
