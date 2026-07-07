cmake_minimum_required(VERSION 3.15)
project(GameEngine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)  # Qt MOC 自动生成

if(MSVC)
    add_compile_options(/utf-8 /FS /Zc:__cplusplus)
endif()

# ---- 路径配置 ----
set(QT_PATH "D:/QT/5.14.2/msvc2017_64")
set(CMAKE_PREFIX_PATH ${QT_PATH} ${CMAKE_PREFIX_PATH})
find_package(Qt5 REQUIRED COMPONENTS Widgets OpenGL)
find_package(OpenGL REQUIRED)

set(THIRDPARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty")
set(FLECS_DIR      "${THIRDPARTY_DIR}/flecs-4.1.6/distr")
set(GLM_DIR        "${THIRDPARTY_DIR}/glm-1.0.3")
set(NLOHMANN_DIR   "${THIRDPARTY_DIR}/nlohmann")

# ---- Flecs ----
add_library(flecs STATIC "${FLECS_DIR}/flecs_no_addons.c")
target_include_directories(flecs PUBLIC "${FLECS_DIR}")
if(MSVC)
    target_compile_options(flecs PRIVATE /wd4204 /wd4221) # C99 指定初始化器警告
endif()

# ---- 引擎核心库 (header-only, 不依赖 Qt::Widgets) ----
set(ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/engine")
add_library(game_engine INTERFACE)
target_include_directories(game_engine INTERFACE
    "${ENGINE_DIR}"
    "${GLM_DIR}"       # glm/glm.hpp
    "${NLOHMANN_DIR}"  # nlohmann/json.hpp
)
# 引擎核心仅依赖: Flecs + OpenGL (QOpenGLFunctions 在 Qt5::OpenGL 中)
target_link_libraries(game_engine INTERFACE
    flecs
    Qt5::OpenGL
    OpenGL::GL
)

# ---- 编辑器可执行文件 (Qt UI 层) ----
set(EDITOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/editor")
add_executable(game_editor WIN32
    "${EDITOR_DIR}/main.cpp"
    "${EDITOR_DIR}/main_window.h"
    "${EDITOR_DIR}/viewport.h"
    "${EDITOR_DIR}/hierarchy_panel.h"
    "${EDITOR_DIR}/inspector_panel.h"
    "${EDITOR_DIR}/asset_browser.h"
    "${EDITOR_DIR}/export_dialog.h"
)
target_include_directories(game_editor PRIVATE "${EDITOR_DIR}")
# 编辑器需要 Qt5::Widgets (UI) + game_engine + Qt5::WinMain (Windows GUI 入口)
target_link_libraries(game_editor PRIVATE game_engine Qt5::Widgets Qt5::WinMain)

# ---- 构建时注入的路径宏 ----
set(SHADER_DIR "${ENGINE_DIR}/render/shaders/")
set(ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/")
configure_file(
    "${EDITOR_DIR}/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/editor/config.h"
    @ONLY
)
# 构建目录优先于源码目录
target_include_directories(game_editor BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/editor")

# VS 调试器配置
set_target_properties(game_editor PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    VS_DEBUGGER_ENVIRONMENT "PATH=${QT_PATH}/bin;%PATH%\nQT_QPA_PLATFORM_PLUGIN_PATH=${QT_PATH}/plugins"
)

# ---- 练习 Demo (外部目录) ----
add_subdirectory(../my_demo ${CMAKE_BINARY_DIR}/my_demo)
