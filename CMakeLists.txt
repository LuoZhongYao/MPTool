project(MPTool)
cmake_minimum_required(VERSION 3.8)
set(CMAKE_C_FLAGS_DEBUG "-g -fsanitize=address")

add_subdirectory(rtlmp)
add_subdirectory(transport)
add_subdirectory(ui)

