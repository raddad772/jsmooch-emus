cmake_minimum_required(VERSION 3.5.0)
project(jsmooch-emus)

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer -O1")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -O1")

# Define DEBUG in Debug build
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

add_subdirectory(jsmooch-lib)

# TODO: Fix jsmooch-gui for Windows
if(NOT WIN32)
    add_subdirectory(jsmooch-gui)
endif()

add_subdirectory(jsmooch-tests)
