#ifndef ENGINE_RESOURCES_SHADER_H
#define ENGINE_RESOURCES_SHADER_H

#include <QOpenGLFunctions_3_3_Core>
#include <QString>
#include <fstream>
#include <sstream>
#include <iostream>

// ============================================================
// Shader: GLSL 着色器程序封装
//
// 功能：从文件加载、编译、链接着色器，提供 Uniform 设置便捷方法。
//
// 使用方法：
//   Shader shader;
//   shader.load("vertex.vert", "fragment.frag");
//   shader.use();
//   shader.setFloat("uTime", 1.0f);
//   shader.setMat4("uModel", matrix.constData());
// ============================================================
class Shader : protected QOpenGLFunctions_3_3_Core
{
public:
    Shader() = default;

    // 从文件路径加载着色器
    Shader(const QString &vertexPath, const QString &fragmentPath)
    {
        load(vertexPath, fragmentPath);
    }

    ~Shader()
    {
        if (m_program != 0)
        {
            glDeleteProgram(m_program);
            m_program = 0;
        }
    }

    // 禁止拷贝（管理 GL 资源）
    Shader(const Shader &) = delete;
    Shader &operator=(const Shader &) = delete;

    // 支持移动
    Shader(Shader &&other) noexcept : m_program(other.m_program)
    {
        other.m_program = 0;
    }
    Shader &operator=(Shader &&other) noexcept
    {
        if (this != &other)
        {
            if (m_program)
                glDeleteProgram(m_program);
            m_program = other.m_program;
            other.m_program = 0;
        }
        return *this;
    }

    // 加载、编译、链接着色器
    bool load(const QString &vertexPath, const QString &fragmentPath)
    {
        initializeOpenGLFunctions();

        // 1. 从文件读取源码
        std::string vertexCode, fragmentCode;
        std::ifstream vFile, fFile;

        vFile.open(vertexPath.toStdString());
        fFile.open(fragmentPath.toStdString());
        if (!vFile.is_open() || !fFile.is_open())
        {
            std::cerr << "ERROR: Failed to open shader files!" << std::endl;
            std::cerr << "  Vertex: " << vertexPath.toStdString() << std::endl;
            std::cerr << "  Fragment: " << fragmentPath.toStdString() << std::endl;
            return false;
        }

        std::stringstream vStream, fStream;
        vStream << vFile.rdbuf();
        fStream << fFile.rdbuf();
        vertexCode = vStream.str();
        fragmentCode = fStream.str();
        vFile.close();
        fFile.close();

        const char *vCode = vertexCode.c_str();
        const char *fCode = fragmentCode.c_str();

        // 2. 编译顶点着色器
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vCode, nullptr);
        glCompileShader(vertexShader);
        checkCompileErrors(vertexShader, "VERTEX");

        // 3. 编译片段着色器
        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fCode, nullptr);
        glCompileShader(fragmentShader);
        checkCompileErrors(fragmentShader, "FRAGMENT");

        // 4. 链接着色器程序
        m_program = glCreateProgram();
        glAttachShader(m_program, vertexShader);
        glAttachShader(m_program, fragmentShader);
        glLinkProgram(m_program);
        checkCompileErrors(m_program, "PROGRAM");

        // 5. 清理着色器对象
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return true;
    }

    // 激活着色器
    void use() { glUseProgram(m_program); }

    // 获取程序 ID
    GLuint id() const { return m_program; }

    // 判断是否已加载
    bool isValid() const { return m_program != 0; }

    // ---- Uniform 设置 ----
    void setBool(const QString &name, bool value)
    {
        glUniform1i(glGetUniformLocation(m_program, name.toStdString().c_str()), (int)value);
    }
    void setInt(const QString &name, int value)
    {
        glUniform1i(glGetUniformLocation(m_program, name.toStdString().c_str()), value);
    }
    void setFloat(const QString &name, float value)
    {
        glUniform1f(glGetUniformLocation(m_program, name.toStdString().c_str()), value);
    }
    void setVec3(const QString &name, float x, float y, float z)
    {
        glUniform3f(glGetUniformLocation(m_program, name.toStdString().c_str()), x, y, z);
    }
    void setVec3(const QString &name, const float *value)
    {
        glUniform3fv(glGetUniformLocation(m_program, name.toStdString().c_str()), 1, value);
    }
    void setVec4(const QString &name, float x, float y, float z, float w)
    {
        glUniform4f(glGetUniformLocation(m_program, name.toStdString().c_str()), x, y, z, w);
    }
    void setMat3(const QString &name, const float *value)
    {
        glUniformMatrix3fv(glGetUniformLocation(m_program, name.toStdString().c_str()),
                           1, GL_FALSE, value);
    }
    void setMat4(const QString &name, const float *value)
    {
        glUniformMatrix4fv(glGetUniformLocation(m_program, name.toStdString().c_str()),
                           1, GL_FALSE, value);
    }

private:
    void checkCompileErrors(GLuint shader, const QString &type)
    {
        GLint success;
        char infoLog[1024];
        if (type != "PROGRAM")
        {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success)
            {
                glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
                std::cerr << "ERROR: Shader compile error (" << type.toStdString() << "):\n"
                          << infoLog << std::endl;
            }
        }
        else
        {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success)
            {
                glGetProgramInfoLog(shader, 1024, nullptr, infoLog);
                std::cerr << "ERROR: Program link error:\n"
                          << infoLog << std::endl;
            }
        }
    }

    GLuint m_program = 0;
};

#endif // ENGINE_RESOURCES_SHADER_H
