cmake_minimum_required(VERSION 3.5.0)
project(jsmooch-emus)

#set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -O1")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -O1")

add_subdirectory(jsmooch-lib)
add_subdirectory(jsmooch-gui)
add_subdirectory(jsmooch-tests)
