# Lesson 构建脚本
# 每个 lesson 是独立的可执行文件

# 使用宏简化创建
macro(add_lesson LESSON_NAME LESSON_DIR)
    file(GLOB LESSON_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${LESSON_DIR}/*.cpp")
    file(GLOB LESSON_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/${LESSON_DIR}/*.h")
    add_executable(${LESSON_NAME} ${LESSON_SOURCES} ${LESSON_HEADERS})
    target_link_libraries(${LESSON_NAME} Qt5::Widgets Qt5::OpenGL OpenGL::GL)
    target_include_directories(${LESSON_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/${LESSON_DIR}")
    # MSVC 多进程编译时，必须给每个 target 显式加 /FS 防止 vc143.pdb 写入冲突
    if(MSVC)
        target_compile_options(${LESSON_NAME} PRIVATE /utf-8 /FS)
    endif()
    # 向源码注入 SHADER_DIR 宏，使 shader 路径变为绝对路径（无论从哪里运行 exe 都有效）
    target_compile_definitions(${LESSON_NAME} PRIVATE SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/${LESSON_DIR}/shaders/")
    # 设置工作目录为课程源码目录，方便加载 shader 文件
    set_target_properties(${LESSON_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${LESSON_DIR}"
        VS_DEBUGGER_ENVIRONMENT "PATH=D:/QT/5.14.2/msvc2017_64/bin;%PATH%\nQT_QPA_PLATFORM_PLUGIN_PATH=D:/QT/5.14.2/msvc2017_64/plugins"
    )
endmacro()

# -------------------------------------------------------
# Lesson 01: 创建OpenGL窗口 - 最简单的OpenGL窗口+清屏
# -------------------------------------------------------
add_lesson(lesson01_hello_window lesson01_hello_window)

# -------------------------------------------------------
# Lesson 02: 你好三角形 - VBO + VAO + 基础着色器
# -------------------------------------------------------
add_lesson(lesson02_hello_triangle lesson02_hello_triangle)

# -------------------------------------------------------
# Lesson 03: 着色器进阶 - uniform变量 + Shader工具类
# -------------------------------------------------------
add_lesson(lesson03_shader_uniform lesson03_shader_uniform)

# -------------------------------------------------------
# Lesson 04: 索引缓冲EBO - 矩形绘制 + 线框模式
# -------------------------------------------------------
add_lesson(lesson04_ebo_rectangle lesson04_ebo_rectangle)

# -------------------------------------------------------
# Lesson 05: 纹理贴图 - QImage加载图片 + 纹理坐标
# -------------------------------------------------------
add_lesson(lesson05_texture lesson05_texture)

# -------------------------------------------------------
# Lesson 06: 变换矩阵 - glm矩阵 + 动画
# -------------------------------------------------------
add_lesson(lesson06_transform lesson06_transform)

# -------------------------------------------------------
# Lesson 07: 3D坐标系统 - Model/View/Projection + 深度测试
# -------------------------------------------------------
add_lesson(lesson07_coordinate_3d lesson07_coordinate_3d)

# -------------------------------------------------------
# Lesson 08: 摄像机 - WASD移动 + 鼠标旋转
# -------------------------------------------------------
add_lesson(lesson08_camera lesson08_camera)

# -------------------------------------------------------
# Lesson 09: 基础光照 - Phong光照模型
# -------------------------------------------------------
add_lesson(lesson09_basic_lighting lesson09_basic_lighting)

# -------------------------------------------------------
# Lesson 10: 材质属性 - 材质结构体 + 光源属性
# -------------------------------------------------------
add_lesson(lesson10_material lesson10_material)

# -------------------------------------------------------
# Lesson 11: 光照贴图 - 漫反射贴图 + 镜面光贴图
# -------------------------------------------------------
add_lesson(lesson11_lighting_maps lesson11_lighting_maps)

# -------------------------------------------------------
# Lesson 12: 投光物 - 方向光/点光源/聚光灯 (按 1/2/3 切换)
# -------------------------------------------------------
add_lesson(lesson12_light_casters lesson12_light_casters)

# -------------------------------------------------------
# Lesson 13: 多光源 - 方向光 + 4个旋转点光源
# -------------------------------------------------------
add_lesson(lesson13_multiple_lights lesson13_multiple_lights)

# -------------------------------------------------------
# Lesson 14: 帧缓冲 FBO + 后期处理 (反相/灰度/锐化/模糊/边缘)
# -------------------------------------------------------
add_lesson(lesson14_framebuffer lesson14_framebuffer)

# -------------------------------------------------------
# Lesson 15: 立方体贴图 + 天空盒 (反射/折射)
# -------------------------------------------------------
add_lesson(lesson15_cubemap lesson15_cubemap)

# -------------------------------------------------------
# Lesson 16: 实例化渲染 (1000颗小行星)
# -------------------------------------------------------
add_lesson(lesson16_instancing lesson16_instancing)

# -------------------------------------------------------
# Lesson 17: 阴影映射 (PCF软化阴影)
# -------------------------------------------------------
add_lesson(lesson17_shadow_mapping lesson17_shadow_mapping)

# -------------------------------------------------------
# Lesson 18: 法线贴图 (程序化砖墙凹凸)
# -------------------------------------------------------
add_lesson(lesson18_normal_mapping lesson18_normal_mapping)

# -------------------------------------------------------
# Lesson 19: 泛光 Bloom (HDR + 高斯模糊 + 色调映射)
# -------------------------------------------------------
add_lesson(lesson19_bloom lesson19_bloom)

# -------------------------------------------------------
# Lesson 20: 几何着色器 (炸裂 + 法线可视化)
# -------------------------------------------------------
add_lesson(lesson20_geometry_shader lesson20_geometry_shader)

# -------------------------------------------------------
# Lesson 21: 延迟渲染 (G-Buffer + 多光源光照Pass)
# -------------------------------------------------------
add_lesson(lesson21_deferred_shading lesson21_deferred_shading)

