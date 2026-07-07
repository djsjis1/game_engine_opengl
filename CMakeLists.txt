cmake_minimum_required(VERSION 3.10)
project(OpenGL_Qt_Course LANGUAGES CXX)
 
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
     
# 设置为当前练手的可执行文件
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

# 引入 my_demo 子目录（Flecs + 引擎 Demo）
add_subdirectory(my_demo)
 