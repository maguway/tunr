cmake_minimum_required(VERSION 3.20)
project(turn LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FETCHCONTENT_BASE_DIR ${CMAKE_SOURCE_DIR}/vendor)
include(FetchContent)

# ========== GLFW ==========
FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW TRUE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# ========== ImGui ==========
FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG docking
        GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(imgui)

# =============================================
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/vendor/imgui-src)
set(GLFW_DIR  ${CMAKE_SOURCE_DIR}/vendor/glfw-src)

if(NOT EXISTS ${IMGUI_DIR}/imgui.h)
    message(FATAL_ERROR "imgui.h not found")
endif()
if(NOT EXISTS ${GLFW_DIR}/include/GLFW/glfw3.h)
    message(FATAL_ERROR "glfw3.h not found")
endif()
message(STATUS "✅ All headers found!")

# ========== Executable ==========
add_executable(turn
        main.cpp
        src/SerialPortManager.cpp
        src/ui_device.cpp
        src/ui_graph.cpp
)

# ВОТ ЗДЕСЬ МЫ ДОБАВИЛИ ПУТИ К IMGUI И GLFW
target_include_directories(turn PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
        ${GLFW_DIR}/include
)

target_sources(turn PRIVATE
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_link_libraries(turn PRIVATE glfw opengl32)

if(WIN32)
    target_link_options(turn PRIVATE -static -static-libgcc -static-libstdc++)
endif()

if(MSVC)
    target_compile_options(turn PRIVATE /utf-8)
endif()