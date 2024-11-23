set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

#add_definitions(-DIMGUI_IMPL_WEBGPU_BACKEND_WGPU)
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
        src/my_texture_ogl3.cpp
        src/my_texture_ogl3.h
        src/audiowrap.cpp
        src/audiowrap.h
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


target_link_libraries(jsmooch-gui jsmooch-lib imgui SDL3 "-framework OpenGL")
#target_link_libraries(jsmooch-gui PRIVATE )

install(TARGETS jsmooch-gui RUNTIME DESTINATION bin)
