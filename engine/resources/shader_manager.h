#ifndef ENGINE_RESOURCES_SHADER_MANAGER_H
#define ENGINE_RESOURCES_SHADER_MANAGER_H

#include "shader.h"
#include <QString>
#include <unordered_map>
#include <memory>

// ============================================================
// ShaderManager: 着色器资源管理器
//
// 功能：
//   - 按名称加载/缓存 Shader 对象
//   - 避免重复编译相同的着色器
//   - 提供统一的 Shader 获取接口
//
// 使用示例：
//   ShaderManager mgr;
//   mgr.loadShader("default", "shaders/default.vert", "shaders/default.frag");
//   mgr.loadShader("skybox", "shaders/skybox.vert", "shaders/skybox.frag");
//   Shader *s = mgr.getShader("default");
// ============================================================
class ShaderManager
{
public:
    ShaderManager() = default;

    // 加载并缓存着色器（若名称已存在则覆盖）
    bool loadShader(const QString &name,
                    const QString &vertexPath,
                    const QString &fragmentPath)
    {
        auto shader = std::make_unique<Shader>();
        if (!shader->load(vertexPath, fragmentPath))
        {
            std::cerr << "ERROR: ShaderManager failed to load shader '"
                      << name.toStdString() << "'" << std::endl;
            return false;
        }
        m_shaders[name] = std::move(shader);
        return true;
    }

    // 获取着色器指针（若未加载则返回 nullptr）
    Shader *getShader(const QString &name)
    {
        auto it = m_shaders.find(name);
        if (it != m_shaders.end())
            return it->second.get();
        return nullptr;
    }

    // 检查着色器是否已加载
    bool hasShader(const QString &name) const
    {
        return m_shaders.find(name) != m_shaders.end();
    }

    // 清除所有缓存的着色器
    void clear()
    {
        m_shaders.clear();
    }

    // 获取默认着色器（第一个加载的或名为 "default" 的）
    Shader *getDefaultShader()
    {
        // 优先返回名为 "default" 的
        auto it = m_shaders.find("default");
        if (it != m_shaders.end())
            return it->second.get();

        // 否则返回第一个
        if (!m_shaders.empty())
            return m_shaders.begin()->second.get();

        return nullptr;
    }

private:
    std::unordered_map<QString, std::unique_ptr<Shader>> m_shaders;
};

#endif // ENGINE_RESOURCES_SHADER_MANAGER_H
