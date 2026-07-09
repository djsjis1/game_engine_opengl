# ============================================================
# my_demo — Flecs + Qt + OpenGL 渲染 Demo
#
# 支持独立构建和作为子目录引入两种模式。
# 若父项目已定义 game_engine / flecs 目标则直接复用，
# 否则自动创建所需依赖（Flecs 静态库 + 引擎 INTERFACE 库）。
# ============================================================
cmake_minimum_required(VERSION 3.15)
project(MyDemo LANGUAGES C CXX)

# ---- 公共路径（均相对于 my_demo/ 目录） ----
set(ROOT_DIR       "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(THIRDPARTY_DIR "${ROOT_DIR}/thirdparty")
set(FLECS_DIR      "${THIRDPARTY_DIR}/flecs-4.1.6/distr")
set(GLM_DIR        "${THIRDPARTY_DIR}/glm-1.0.3")
set(NLOHMANN_DIR   "${THIRDPARTY_DIR}/nlohmann")
set(SQLITE_DIR     "${THIRDPARTY_DIR}/sqlite")
set(LOG_DIR        "${THIRDPARTY_DIR}/log")
set(ENGINE_DIR     "${ROOT_DIR}/game_engine/engine")
set(SHADER_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/")
set(QT_PATH        "D:/QT/5.14.2/msvc2017_64")
     
# ---- 检测构建模式 ----
# CMAKE_SOURCE_DIR == CMAKE_CURRENT_SOURCE_DIR → 独立构建（顶层项目）
# 否则 → 由父项目通过 add_subdirectory 引入
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(MY_DEMO_STANDALONE TRUE)
else()
    set(MY_DEMO_STANDALONE FALSE)
endif()

# ============================================================
# 依赖配置：只在 game_engine 目标不存在时创建
# ============================================================
if(NOT TARGET game_engine)
    # -- 仅在独立构建时设置全局选项（避免污染父项目） --
    if(MY_DEMO_STANDALONE)
        set(CMAKE_CXX_STANDARD 20)
        set(CMAKE_CXX_STANDARD_REQUIRED ON)

        if(MSVC)
            add_compile_options(/utf-8 /FS /Zc:__cplusplus)
        endif()

        # Qt5（独立构建时需要 find_package，子目录时父项目已执行）
        set(CMAKE_PREFIX_PATH ${QT_PATH} ${CMAKE_PREFIX_PATH})
        find_package(Qt5 REQUIRED COMPONENTS Widgets OpenGL)
        find_package(OpenGL REQUIRED)
    endif()

    # AUTOMOC 始终启用（EditorPanel 等需要 Qt MOC）
    set(CMAKE_AUTOMOC ON)

    # -- Flecs 静态库（完整版，含 C++ API） --
    add_library(flecs STATIC "${FLECS_DIR}/flecs.c")
    target_include_directories(flecs PUBLIC "${FLECS_DIR}")
    if(MSVC)
        target_compile_options(flecs PRIVATE /utf-8 /wd4204 /wd4221)
    endif()

    # -- 引擎核心（header-only INTERFACE 库） --
    add_library(game_engine INTERFACE)
    target_include_directories(game_engine INTERFACE
        "${ENGINE_DIR}"
        "${GLM_DIR}"
        "${NLOHMANN_DIR}"
    )
    target_link_libraries(game_engine INTERFACE
        flecs
        Qt5::OpenGL
        OpenGL::GL
    )
endif()

# ---- SQLite3（数据库） ----
if(NOT TARGET sqlite3)
    add_subdirectory(${SQLITE_DIR} ${CMAKE_BINARY_DIR}/sqlite3)
endif()

# ---- mylog（spdlog 封装日志库） ----
if(NOT TARGET mylog)
    add_subdirectory(${LOG_DIR} ${CMAKE_BINARY_DIR}/mylog)
endif()

# ---- Lua 5.4（脚本引擎，本地源码）----
if(NOT TARGET lua)
    set(LUA_DIR "${THIRDPARTY_DIR}/lua-5.4.7/src")
    set(LUA_SRC
        ${LUA_DIR}/lapi.c ${LUA_DIR}/lauxlib.c ${LUA_DIR}/lbaselib.c
        ${LUA_DIR}/lcode.c ${LUA_DIR}/lcorolib.c ${LUA_DIR}/lctype.c
        ${LUA_DIR}/ldblib.c ${LUA_DIR}/ldebug.c ${LUA_DIR}/ldo.c
        ${LUA_DIR}/ldump.c ${LUA_DIR}/lfunc.c ${LUA_DIR}/lgc.c
        ${LUA_DIR}/linit.c ${LUA_DIR}/liolib.c ${LUA_DIR}/llex.c
        ${LUA_DIR}/lmathlib.c ${LUA_DIR}/lmem.c ${LUA_DIR}/loadlib.c
        ${LUA_DIR}/lobject.c ${LUA_DIR}/lopcodes.c ${LUA_DIR}/loslib.c
        ${LUA_DIR}/lparser.c ${LUA_DIR}/lstate.c ${LUA_DIR}/lstring.c
        ${LUA_DIR}/lstrlib.c ${LUA_DIR}/ltable.c ${LUA_DIR}/ltablib.c
        ${LUA_DIR}/ltm.c ${LUA_DIR}/lundump.c ${LUA_DIR}/lutf8lib.c
        ${LUA_DIR}/lvm.c ${LUA_DIR}/lzio.c
    )
    add_library(lua STATIC ${LUA_SRC})
    target_include_directories(lua PUBLIC ${LUA_DIR})
    if(MSVC)
        target_compile_definitions(lua PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(lua PRIVATE /utf-8)
    endif()
endif()

# ============================================================
# my_demo 可执行文件
# ============================================================
add_executable(my_demo WIN32
    main.cpp
    MainWindow.h    MainWindow.cpp
    RenderWidget.h  RenderWidget.cpp
    Components/MeshComponent.h
    Components/TagComponent.h
    Components/RotateBehavior.h
    Components/CameraComponent.h
    Components/EditorVisible.h
    System/CameraSystem.h
    System/EditorCamera.h
    System/Input.h
    editor/EditorPanel.h   editor/EditorPanel.cpp
    System/Shader.h
    System/MeshFactory.h
    System/RotationSystem.h
    System/ScriptSystem.h
    System/LuaApi.cpp
    Components/ScriptComponent.h
    Engine.h
    System/System.h
)

target_include_directories(my_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(my_demo PRIVATE
    game_engine
    sqlite3
    mylog
    lua
    Qt5::Widgets
    Qt5::WinMain
)

if(MSVC)
    target_compile_options(my_demo PRIVATE /utf-8 /FS)
endif()

# Shader 路径注入（指向 my_demo/Shaders/ 目录）
target_compile_definitions(my_demo PRIVATE
    SHADER_DIR="${SHADER_DIR}"
)

# Visual Studio 调试器：启动时自动找到 Qt DLL
set_target_properties(my_demo PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    VS_DEBUGGER_ENVIRONMENT "PATH=${QT_PATH}/bin;%PATH%\nQT_QPA_PLATFORM_PLUGIN_PATH=${QT_PATH}/plugins"
)
