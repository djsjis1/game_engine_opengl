cmake_minimum_required(VERSION 3.10)
project(OpenGL_Qt_Course LANGUAGES C CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC 默认 GBK 编码无法处理中文注释，强制 UTF-8
if(MSVC)
    add_compile_options(/utf-8 /FS)
endif()

# Qt 路径配置
set(CMAKE_PREFIX_PATH "D:/QT/5.14.2/msvc2017_64" ${CMAKE_PREFIX_PATH})
   
# 查找 Qt 模块
find_package(Qt5 REQUIRED COMPONENTS Widgets OpenGL)

# 查找 OpenGL (用于获取 lib 路径)
find_package(OpenGL REQUIRED)
     
# ---- Lua 5.4（脚本引擎，本地源码）----
set(LUA_DIR "${CMAKE_SOURCE_DIR}/thirdparty/lua-5.4.7/src")
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

# ---- lua_practice：Lua C API 练习（纯控制台，无 Qt/OpenGL 依赖）----
add_executable(lua_practice lua_practice.cpp)
target_include_directories(lua_practice PRIVATE ${LUA_DIR})
target_link_libraries(lua_practice lua)
if(MSVC)
    target_compile_options(lua_practice PRIVATE /utf-8 /FS)
endif()

# ---- 主项目 ----
add_executable(main_course main.cpp)
target_include_directories(main_course PRIVATE
    "${CMAKE_SOURCE_DIR}/thirdparty/flecs-4.1.6/distr"
    "${CMAKE_SOURCE_DIR}/thirdparty/entt-3.16.0/single_include"
)
target_link_libraries(main_course Qt5::Widgets Qt5::OpenGL OpenGL::GL flecs)
set_target_properties(main_course PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    VS_DEBUGGER_ENVIRONMENT "PATH=D:/QT/5.14.2/msvc2017_64/bin;%PATH%\nQT_QPA_PLATFORM_PLUGIN_PATH=D:/QT/5.14.2/msvc2017_64/plugins"
)
 
# 引入 lesson 子目录
add_subdirectory(lesson)

add_subdirectory(my_demo)
 