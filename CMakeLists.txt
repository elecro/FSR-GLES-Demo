cmake_minimum_required(VERSION 3.2.0)
project (skeleton_gles_imgui C CXX)


set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(thirdparty/glfw)
message("Using GLFW: thirdparty/")
set(GLFW3_LIBRARIES glfw)

include(thirdparty/modules.cmake)

add_subdirectory(src)
