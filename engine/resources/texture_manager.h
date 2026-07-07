#ifndef ENGINE_RESOURCES_TEXTURE_MANAGER_H
#define ENGINE_RESOURCES_TEXTURE_MANAGER_H

#include <QOpenGLFunctions_3_3_Core>
#include <QImage>
#include <QString>
#include <unordered_map>
#include <memory>
#include <iostream>

// ============================================================
// TextureManager: 纹理资源管理器
//
// 功能：
//   - 从文件加载纹理（使用 QImage）
//   - 按路径缓存 GLuint，避免重复加载
//   - 自动处理格式转换和 Y 轴翻转
//
// 使用示例：
//   TextureManager mgr;
//   GLuint tex = mgr.loadTexture("assets/wood.png");
//   GLuint tex2 = mgr.loadTexture("assets/wood.png"); // 命中缓存
// ============================================================
class TextureManager : protected QOpenGLFunctions_3_3_Core
{
public:
    TextureManager() = default;

    ~TextureManager()
    {
        clear();
    }

    // 懒初始化 OpenGL 函数（首次使用时自动调用）
    void ensureInit()
    {
        if (!m_initialized)
        {
            initializeOpenGLFunctions();
            m_initialized = true;
        }
    }

    // 禁止拷贝
    TextureManager(const TextureManager &) = delete;
    TextureManager &operator=(const TextureManager &) = delete;

    // 从文件加载纹理（带缓存，同一路径不重复加载）
    GLuint loadTexture(const QString &filePath)
    {
        ensureInit(); // 懒初始化
        // 检查缓存
        auto it = m_textures.find(filePath);
        if (it != m_textures.end())
            return it->second;

        // 加载图片
        QImage img(filePath);
        if (img.isNull())
        {
            std::cerr << "ERROR: TextureManager failed to load image: "
                      << filePath.toStdString() << std::endl;
            return 0;
        }

        // 转换为 OpenGL 友好格式并垂直翻转
        img = img.convertToFormat(QImage::Format_RGBA8888)
                  .mirrored(false, true);

        // 创建 OpenGL 纹理
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        // 设置纹理参数
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // 上传纹理数据
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     img.width(), img.height(), 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, img.bits());
        glGenerateMipmap(GL_TEXTURE_2D);

        glBindTexture(GL_TEXTURE_2D, 0);

        // 缓存
        m_textures[filePath] = texture;
        return texture;
    }

    // 获取已缓存的纹理（若未加载返回 0）
    GLuint getTexture(const QString &filePath) const
    {
        auto it = m_textures.find(filePath);
        return (it != m_textures.end()) ? it->second : 0;
    }

    // 清除指定纹理
    void removeTexture(const QString &filePath)
    {
        auto it = m_textures.find(filePath);
        if (it != m_textures.end())
        {
            glDeleteTextures(1, &it->second);
            m_textures.erase(it);
        }
    }

    // 清除所有缓存的纹理
    void clear()
    {
        for (auto &pair : m_textures)
            glDeleteTextures(1, &pair.second);
        m_textures.clear();
    }

private:
    std::unordered_map<QString, GLuint> m_textures;
    bool m_initialized = false;
};

#endif // ENGINE_RESOURCES_TEXTURE_MANAGER_H
