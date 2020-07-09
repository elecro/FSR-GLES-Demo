find_package(PkgConfig REQUIRED)
find_package(glfw3 3.3 REQUIRED)
pkg_check_modules(EGL REQUIRED egl)

add_executable(skeleton
    main.cpp

    ${IMGUI_BASE_DIR}/examples/imgui_impl_opengl3.cpp
    ${IMGUI_BASE_DIR}/examples/imgui_impl_glfw.cpp
    ${IMGUI_BASE_DIR}/imgui_demo.cpp
)

target_link_libraries(skeleton PRIVATE glfw imgui_base EGL GLESv2)
target_compile_options(skeleton PRIVATE -DIMGUI_IMPL_OPENGL_ES3)
target_include_directories(skeleton PRIVATE
    ${IMGUI_BASE_DIR}
    ${IMGUI_BASE_DIR}/examples
)
