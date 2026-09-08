# ============================================================
# 游戏引擎 — Flecs + Qt + OpenGL + Lua + Jolt
#
# 引擎提升为根项目后，ROOT_DIR = CMAKE_CURRENT_SOURCE_DIR。
# lessons/ 为附属 OpenGL 学习课程（独立构建）。
# ============================================================
cmake_minimum_required(VERSION 3.15)
project(MyDemo LANGUAGES C CXX)

# ---- 引擎自检开关：开启后 TestFramework 统计/断言宏生效，
#      Lua engine_stats()/engine_self_check() 可用（AI Agent 排障用）。
#      关闭（默认）时全部展开为空 → Release 构建零开销。
option(ENABLE_ENGINE_TESTING "Enable engine self-check APIs (AI diagnostics)" OFF)
if(ENABLE_ENGINE_TESTING)
    add_compile_definitions(ENABLE_ENGINE_TESTING=1)
endif()

# ---- 公共路径 ----
set(ROOT_DIR       "${CMAKE_CURRENT_SOURCE_DIR}")
set(THIRDPARTY_DIR "${ROOT_DIR}/thirdparty")
set(FLECS_DIR      "${THIRDPARTY_DIR}/flecs-4.1.6/distr")
set(GLM_DIR        "${THIRDPARTY_DIR}/glm-1.0.3")
set(NLOHMANN_DIR   "${THIRDPARTY_DIR}")
set(SQLITE_DIR     "${THIRDPARTY_DIR}/sqlite")
set(LOG_DIR        "${THIRDPARTY_DIR}/log")
set(HFSM2_DIR     "${ROOT_DIR}/thirdparty/HFSM2-2.12.1/include")
set(JOLT_DIR      "${THIRDPARTY_DIR}/JoltPhysics-master")
set(MINIAUDIO_DIR "${THIRDPARTY_DIR}/miniaudio-master")
set(SHADER_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/assets/Shaders/")
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
# 引擎核心源文件（编辑器 my_demo 与游戏运行时 my_game 共享）
# 不含：入口/窗口/Editor——那些是各自目标私有的
# ============================================================
set(ENGINE_SOURCES
    Core/Engine.h
    Core/EngineState.h
    Core/EngineFSM.h
    Core/EngineLog.h
    Core/EngineLog.cpp
    Core/TestFramework.h
    Core/TestFramework.cpp
    Core/System.h
    Core/Input.h
    Core/Time.h
    Core/JobSystem.h
    Core/CommandStack.h
    Core/Easing.h
    Core/Raycast.h
    Core/GamePaths.h
    Core/SaveGame.h
    Core/SceneRouter.h
    Core/NavGrid.h
    Core/SignalQueue.h
    Core/SignalQueue.cpp
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
    Components/AudioComponent.h
    Components/ParticleComponent.h
    Components/LightComponent.h
    Components/EnvironmentComponent.h
    Components/Reflect.h
    Systems/CameraSystem.h
    Systems/RotationSystem.h
    Systems/TransformSystem.h
    Systems/AnimationSystem.h
    Systems/ScriptSystem.h
    Systems/PhysicsSystem.h
    Systems/PhysicsEngine.cpp
    Systems/AudioSystem.h
    Systems/ParticleSystem.h
    Systems/TweenSystem.h
    Systems/TweenSystem.cpp
    Systems/AnimFsmSystem.h
    Systems/AnimFsmSystem.cpp
    Systems/AudioEngine.cpp
    Scripting/LuaApi.h
    Scripting/LuaApi.cpp
    Scripting/LuaApiInternal.h
    Scripting/LuaApiEntity.cpp
    Scripting/LuaApiInput.cpp
    Scripting/LuaApiGameplay.cpp
    Scripting/LuaApiSignalAnim.cpp
    Scripting/ScriptEngine.cpp
    Scripting/ScriptApiRegistry.h
    Scripting/ScriptApiStub.h
    Scripting/LuaAsync.h
    Scripting/LuaAsync.cpp
    Render/RenderWidget.h  Render/RenderWidget.cpp
        Render/ShadowRenderer.h  Render/ShadowRenderer.cpp
        Render/PostProcessor.h  Render/PostProcessor.cpp
        Render/SkyRenderer.h  Render/SkyRenderer.cpp
        Render/ParticleRenderer.h  Render/ParticleRenderer.cpp
        Render/GizmoRenderer.h  Render/GizmoRenderer.cpp
    Render/ObjLoader.h
    Render/Shader.h
    Render/MeshFactory.h
    Render/ResourceManager.h
    Render/AnimationLibrary.h
    Render/SceneSerializer.h
)

# ============================================================
# my_demo 可执行文件（编辑器：引擎核心 + 编辑器 UI）
# ============================================================
add_executable(my_demo WIN32
    main.cpp
    MainWindow.h    MainWindow.cpp
    Editor/EditorPanel.h  Editor/EditorPanel.cpp
    Editor/DragSpinBox.h
    Editor/EditorCommands.h
    Editor/ScriptEditor.h  Editor/ScriptEditor.cpp
    Editor/LuaHighlighter.h
    ${ENGINE_SOURCES}
    Core/SaveGame.h
)

target_include_directories(my_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    "${GLM_DIR}"
    "${NLOHMANN_DIR}"
    "${HFSM2_DIR}"
    "${JOLT_DIR}"
    "${MINIAUDIO_DIR}"
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

# Shader 路径注入（指向 assets/Shaders/ 目录，开发/导出共用）
# ASSET_DIR：资源根目录绝对路径（anims/scripts 等资源一律绝对路径，不依赖工作目录）
target_compile_definitions(my_demo PRIVATE
    SHADER_DIR="${SHADER_DIR}"
    ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/"
)

# Visual Studio 调试器：启动时自动找到 Qt DLL
set_target_properties(my_demo PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    VS_DEBUGGER_ENVIRONMENT "PATH=${QT_PATH}/bin;%PATH%\nQT_QPA_PLATFORM_PLUGIN_PATH=${QT_PATH}/plugins"
)

# ============================================================
# my_game 可执行文件（游戏运行时：引擎核心 + 游戏窗口，无编辑器）
# 导出游戏 = 拷贝 my_game.exe + assets/ + game.json 到目标目录
# ============================================================
add_executable(my_game WIN32
    Game/main_game.cpp
    Game/GameWindow.h  Game/GameWindow.cpp
    ${ENGINE_SOURCES}
)

target_include_directories(my_game PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    "${GLM_DIR}"
    "${NLOHMANN_DIR}"
    "${HFSM2_DIR}"
    "${JOLT_DIR}"
    "${MINIAUDIO_DIR}"
)

target_link_libraries(my_game PRIVATE
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
    target_compile_options(my_game PRIVATE /utf-8 /FS)
    target_compile_definitions(my_game PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# 开发模式回退路径（exe 旁无 assets/ 时用，方便 F5 直接调试运行时）
target_compile_definitions(my_game PRIVATE
    SHADER_DIR="${SHADER_DIR}"
    ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/"
)
add_dependencies(my_game run_reflect_gen)

set_target_properties(my_game PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    VS_DEBUGGER_ENVIRONMENT "PATH=${QT_PATH}/bin;%PATH%\nQT_QPA_PLATFORM_PLUGIN_PATH=${QT_PATH}/plugins"
)

# ============================================================
# unit_tests — 单元测试（GoogleTest，规范见 .qoder/rules/testing_rules.md）
#
# 被测对象全是 header-only 纯逻辑模块（Input/Time/SceneRouter/
# Easing/SaveGame），不链 flecs/Jolt/OpenGL —— 目标极轻、编译秒级、
# 用例毫秒级（规范要求单测 <100ms）。SaveGame 需 QtCore（exe 目录定位）。
#
# gtest 本地源码引入（与 flecs/lua/jolt 同款 add_subdirectory）：
#   放 thirdparty/googletest-main/（含其 CMakeLists.txt）即启用测试；
#   未放置时自动跳过——不阻塞主工程构建（网络无法 FetchContent 的现实取舍，
#   已在 docs/adr 说明）。用本地源码版本，不追 latest tag。
# ============================================================
set(GTEST_DIR "${THIRDPARTY_DIR}/googletest-main")
if(EXISTS "${GTEST_DIR}/CMakeLists.txt")
    # 让 gtest 跟随 /MD 运行库（避免与 Qt 的 CRT 冲突）+ 不装它的东西
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    add_subdirectory(${GTEST_DIR} ${CMAKE_BINARY_DIR}/googletest)

    enable_testing()
    add_executable(unit_tests
        tests/test_main.cpp
        tests/unit/core/Input_test.cpp
        tests/unit/core/Time_test.cpp
        tests/unit/core/SceneRouter_test.cpp
        tests/unit/core/Easing_test.cpp
        tests/unit/core/SaveGame_test.cpp
        tests/unit/core/NavGrid_test.cpp
        tests/unit/core/CommandStack_test.cpp
        tests/unit/core/TestFramework_test.cpp
        Core/EngineLog.cpp # test_main 的 shutdownEngineLog（spdlog 显式收尾）
        Core/TestFramework.cpp # 被测对象：自检/统计框架
    )
    target_include_directories(unit_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${NLOHMANN_DIR}"
    )
    target_compile_definitions(unit_tests PRIVATE ENABLE_ENGINE_TESTING=1) # 测试构建验证完整自检逻辑
    target_link_libraries(unit_tests PRIVATE gtest Qt5::Core mylog)
    if(MSVC)
        target_compile_options(unit_tests PRIVATE /utf-8 /FS)
    endif()
    add_test(NAME unit_tests COMMAND unit_tests)
    set_target_properties(unit_tests PROPERTIES
        VS_DEBUGGER_ENVIRONMENT "PATH=${QT_PATH}/bin;%PATH%"
    )

    # ---- integration_tests：物理/脚本集成测试（真实 Jolt/Lua/flecs）----
    # 链完整 ENGINE_SOURCES（含 Qt 渲染类但测试不实例化 GUI），
    # 规范：tests/integration/<模块>/，每用例 <5s
    add_executable(integration_tests
        tests/test_main.cpp
        tests/integration/physics/PhysicsSystem_test.cpp
        tests/integration/script/ScriptSystem_test.cpp
        tests/integration/particles/ParticleSystem_test.cpp
        tests/integration/animation/AnimationTrack_test.cpp
        tests/integration/tween/TweenSystem_test.cpp
        tests/integration/scene/SceneSnapshot_test.cpp
        tests/integration/resource/ResourceManager_test.cpp
        tests/unit/render/ObjLoader_test.cpp
        ${ENGINE_SOURCES}
    )
    target_include_directories(integration_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${GLM_DIR}" "${NLOHMANN_DIR}" "${HFSM2_DIR}" "${JOLT_DIR}" "${MINIAUDIO_DIR}"
    )
    target_compile_definitions(integration_tests PRIVATE ENABLE_ENGINE_TESTING=1)
    target_link_libraries(integration_tests PRIVATE
        gtest flecs Jolt Qt5::OpenGL OpenGL::GL sqlite3 mylog lua Qt5::Widgets
    )
    if(MSVC)
        target_compile_options(integration_tests PRIVATE /utf-8 /FS)
        target_compile_definitions(integration_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    target_compile_definitions(integration_tests PRIVATE
        SHADER_DIR="${SHADER_DIR}"
        ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/"
    )
    add_dependencies(integration_tests run_reflect_gen)
    add_test(NAME integration_tests COMMAND integration_tests)
    set_target_properties(integration_tests PROPERTIES
        VS_DEBUGGER_ENVIRONMENT "PATH=${QT_PATH}/bin;%PATH%"
    )

    message(STATUS "unit_tests enabled (gtest found at ${GTEST_DIR})")

    # ---- engine_tests：无头引擎测试（不启动 GUI，纯逻辑验证）----
    # 解耦：独立 target，复用 ENGINE_SOURCES，测试代码在 tests/engine/
    # 删除时只需删 tests/engine/ + 这段 CMake
    add_executable(engine_tests
        tests/engine/engine_test_main.cpp
        tests/engine/LuaTestApi.cpp
        ${ENGINE_SOURCES}
    )
    target_include_directories(engine_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${GLM_DIR}" "${NLOHMANN_DIR}" "${HFSM2_DIR}" "${JOLT_DIR}" "${MINIAUDIO_DIR}"
    )
    target_compile_definitions(engine_tests PRIVATE ENABLE_ENGINE_TESTING=1)
    target_link_libraries(engine_tests PRIVATE
        flecs Jolt Qt5::OpenGL OpenGL::GL sqlite3 mylog lua Qt5::Widgets
    )
    if(MSVC)
        target_compile_options(engine_tests PRIVATE /utf-8 /FS)
        target_compile_definitions(engine_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    target_compile_definitions(engine_tests PRIVATE
        SHADER_DIR="${SHADER_DIR}"
        ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/"
    )
    add_dependencies(engine_tests run_reflect_gen)
    set_target_properties(engine_tests PROPERTIES
        VS_DEBUGGER_ENVIRONMENT "PATH=${QT_PATH}/bin;%PATH%"
    )
    message(STATUS "engine_tests enabled (headless engine test)")
else()
    message(STATUS "unit_tests skipped: put googletest-1.14.0 in thirdparty/ to enable")
endif()

# ============================================================
# lessons — OpenGL 渐进式学习课程（21 节，独立可运行）
# 仅依赖 Qt5 + OpenGL，不链引擎库
# ============================================================
add_subdirectory(lessons)
