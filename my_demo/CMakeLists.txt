# ============================================================
# my_demo — Flecs + Qt + OpenGL 渲染 Demo
#
# 支持独立构建和作为子目录引入两种模式。
# ============================================================
cmake_minimum_required(VERSION 3.15)
project(MyDemo LANGUAGES C CXX)

# ---- 公共路径 ----
set(ROOT_DIR       "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(THIRDPARTY_DIR "${ROOT_DIR}/thirdparty")
set(FLECS_DIR      "${THIRDPARTY_DIR}/flecs-4.1.6/distr")
set(GLM_DIR        "${THIRDPARTY_DIR}/glm-1.0.3")
set(NLOHMANN_DIR   "${THIRDPARTY_DIR}")
set(SQLITE_DIR     "${THIRDPARTY_DIR}/sqlite")
set(LOG_DIR        "${THIRDPARTY_DIR}/log")
set(HFSM2_DIR     "${ROOT_DIR}/thirdparty/HFSM2-2.12.1/include")
set(JOLT_DIR      "${THIRDPARTY_DIR}/JoltPhysics-master")
set(SHADER_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/")
set(QT_PATH        "D:/QT/5.14.2/msvc2017_64")

# ---- 独立构建时设置全局选项，子目录时跳过 ----
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_AUTOMOC ON)
    if(MSVC)
        add_compile_options(/utf-8 /FS /Zc:__cplusplus)
    endif()
    set(CMAKE_PREFIX_PATH ${QT_PATH} ${CMAKE_PREFIX_PATH})
    find_package(Qt5 REQUIRED COMPONENTS Widgets OpenGL)
    find_package(OpenGL REQUIRED)
endif()

# -- Flecs 静态库（只在不存在时创建） --
if(NOT TARGET flecs)
    add_library(flecs STATIC "${FLECS_DIR}/flecs.c")
    target_include_directories(flecs PUBLIC "${FLECS_DIR}")
    if(MSVC)
        target_compile_options(flecs PRIVATE /utf-8 /wd4204 /wd4221)
    endif()
endif()
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

# ---- Jolt Physics（工业级物理引擎）----
if(NOT TARGET Jolt)
    # 作为子目录引入时 Jolt 自动只编主库（不带 Samples/Tests）
    set(GENERATE_DEBUG_SYMBOLS ON CACHE BOOL "" FORCE)
    set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)  # 加快链接
    set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)            # 不让 Jolt 劫持全局编译选项
    add_subdirectory(${JOLT_DIR}/Build ${CMAKE_BINARY_DIR}/jolt)
endif()

# ============================================================
# my_demo 可执行文件
# ============================================================
add_executable(my_demo WIN32
    main.cpp
    MainWindow.h    MainWindow.cpp
    Core/Engine.h
    Core/EngineState.h
    Core/EngineFSM.h
    Core/System.h
    Core/Input.h
    Core/Time.h
    Core/JobSystem.h
    Core/CommandStack.h
    Core/Easing.h
    Core/Raycast.h
    Components/MeshComponent.h
    Components/TagComponent.h
    Components/RotateBehavior.h
    Components/CameraComponent.h
    Components/TransformComponent.h
    Components/MaterialComponent.h
    Components/EditorVisible.h
    Components/ScriptComponent.h
    Components/SignalComponent.h
    Components/RigidbodyComponent.h
    Components/BoxColliderComponent.h
    Components/AnimationComponent.h
    Components/HealthComponent.h
    Components/SpriteComponent.h
    Components/Reflect.h
    Systems/CameraSystem.h
    Systems/RotationSystem.h
    Systems/TransformSystem.h
    Systems/AnimationSystem.h
    Systems/ScriptSystem.h
    Systems/PhysicsSystem.h
    Systems/PhysicsEngine.cpp
    Scripting/LuaApi.h
    Scripting/LuaApi.cpp
    Scripting/ScriptEngine.cpp
    Scripting/LuaAsync.h
    Scripting/LuaAsync.cpp
    Render/RenderWidget.h  Render/RenderWidget.cpp
    Render/Shader.h
    Render/MeshFactory.h
    Render/ResourceManager.h
    Render/AnimationLibrary.h
    Render/SceneSerializer.h
    Editor/EditorPanel.h  Editor/EditorPanel.cpp
    Editor/EditorCommands.h
)

target_include_directories(my_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    "${GLM_DIR}"
    "${NLOHMANN_DIR}"
    "${HFSM2_DIR}"
    "${JOLT_DIR}"
)

target_link_libraries(my_demo PRIVATE
    flecs
    Jolt
    Qt5::OpenGL
    OpenGL::GL
    sqlite3
    mylog
    lua
    Qt5::Widgets
    Qt5::WinMain
)

if(MSVC)
    target_compile_options(my_demo PRIVATE /utf-8 /FS)
    # strncpy 等 C 函数的 C4996 安全警告（项目已用 sizeof-1 限长，安全）
    target_compile_definitions(my_demo PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# ---- mini-UHT：反射代码生成器 ----
# 扫描 Components/*.h 的 REFLECT 宏标记 → 生成 Generated/ComponentReflection.inl
# 每次构建 my_demo 前自动执行（内容不变则不写文件，避免无谓重编）
add_executable(reflect_gen tools/reflect_gen.cpp)
set_target_properties(reflect_gen PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
if(MSVC)
    target_compile_options(reflect_gen PRIVATE /utf-8)
endif()

add_custom_target(run_reflect_gen
    COMMAND $<TARGET_FILE:reflect_gen>
        "${CMAKE_CURRENT_SOURCE_DIR}/Components"
        "${CMAKE_CURRENT_SOURCE_DIR}/Generated/ComponentReflection.inl"
    COMMENT "reflect_gen: 扫描 REFLECT 宏标记生成反射注册"
)
add_dependencies(run_reflect_gen reflect_gen) # 先编生成器再跑
add_dependencies(my_demo run_reflect_gen)

# Shader 路径注入（指向 my_demo/Shaders/ 目录）
# ASSET_DIR：资源根目录绝对路径（anims/scripts 等资源一律绝对路径，不依赖工作目录）
target_compile_definitions(my_demo PRIVATE
    SHADER_DIR="${SHADER_DIR}"
    ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/"
)

# Visual Studio 调试器：启动时自动找到 Qt DLL
set_target_properties(my_demo PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    VS_DEBUGGER_ENVIRONMENT "PATH=${QT_PATH}/bin;%PATH%\nQT_QPA_PLATFORM_PLUGIN_PATH=${QT_PATH}/plugins"
)
