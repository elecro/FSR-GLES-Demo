cmake_minimum_required(VERSION 3.2.0)
project (gles_fsr_demo C CXX)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(thirdparty/glfw)
message("Using GLFW: thirdparty/")
set(GLFW3_LIBRARIES glfw)

include(thirdparty/modules.cmake)

find_package(PkgConfig REQUIRED)
#find_package(glfw3 3.3 REQUIRED)
pkg_check_modules(EGL REQUIRED egl)

add_executable(gles_fsr
    src/main.cpp
    src/image_utils.cpp

    ${IMGUI_BASE_DIR}/examples/imgui_impl_opengl3.cpp
    ${IMGUI_BASE_DIR}/examples/imgui_impl_glfw.cpp
    ${IMGUI_BASE_DIR}/imgui_demo.cpp
)

target_link_libraries(gles_fsr PRIVATE glfw imgui_base EGL GLESv2)
target_compile_options(gles_fsr PRIVATE -DIMGUI_IMPL_OPENGL_ES3)

target_include_directories(gles_fsr PRIVATE
    ${IMGUI_BASE_DIR}
    ${IMGUI_BASE_DIR}/examples
)
