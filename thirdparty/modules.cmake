set(IMGUI_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui)

if(NOT EXISTS ${IMGUI_BASE_DIR})
    message(WARNING "ImGui is not present at ${IMGUI_BASE_DIR}")
    message(FATAL_ERROR "Maybe the submodule is not downloaded. Run: git submodule update --init")
endif()

add_library(imgui_base STATIC
    ${IMGUI_BASE_DIR}/imgui.cpp
    ${IMGUI_BASE_DIR}/imgui_draw.cpp
    ${IMGUI_BASE_DIR}/imgui_widgets.cpp
)

target_compile_options(imgui_base PUBLIC "-fPIC")
