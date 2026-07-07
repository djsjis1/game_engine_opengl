#ifndef GAME_ENGINE_EDITOR_CONFIG_H
#define GAME_ENGINE_EDITOR_CONFIG_H

// ============================================================
// 开发时默认路径宏
// 构建时 CMake 会在 build/editor/ 下生成覆盖版本（路径更精确）
// ============================================================

/** @brief Shader 文件目录路径（相对于 game_engine/ 工作目录） */
#define SHADER_DIR "engine/render/shaders/"

/** @brief 资产文件目录路径 */
#define ASSETS_DIR "assets/"

#endif // GAME_ENGINE_EDITOR_CONFIG_H
