#ifndef SHADER_H
#define SHADER_H

#include <QOpenGLFunctions_3_3_Core>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

// ============================================================
// Shader — GLSL 着色器程序封装
//
// 职责：编译顶点/片段着色器 → 链接为 GPU 程序 → 设置 uniform
// 继承 QOpenGLFunctions_3_3_Core 以调用 gl* 函数（Qt 方式）
// ============================================================
class Shader : protected QOpenGLFunctions_3_3_Core
{
public:
    Shader() = default;
    ~Shader() { clear(); }

    // 禁止拷贝（持有 GPU 资源）
    Shader(const Shader &) = delete;
    Shader &operator=(const Shader &) = delete;

    // ---- 加载与编译 ----
    bool load(const std::string &vertPath, const std::string &fragPath)
    {
        initializeOpenGLFunctions();

        std::string vertSrc = readFile(vertPath);
        std::string fragSrc = readFile(fragPath);
        if (vertSrc.empty() || fragSrc.empty())
            return false;

        GLuint vs = compileShader(GL_VERTEX_SHADER, vertSrc);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
        if (!vs || !fs)
            return false;

        m_program = glCreateProgram();
        glAttachShader(m_program, vs);
        glAttachShader(m_program, fs);
        glLinkProgram(m_program);

        GLint ok = 0;
        glGetProgramiv(m_program, GL_LINK_STATUS, &ok);
        if (!ok)
        {
            char log[512];
            glGetProgramInfoLog(m_program, 512, nullptr, log);
            std::cerr << "[Shader] link error: " << log << std::endl;
            glDeleteProgram(m_program);
            m_program = 0;
        }

        glDeleteShader(vs);
        glDeleteShader(fs);
        return ok;
    }

    void use() { glUseProgram(m_program); }
    bool isValid() const { return m_program != 0; }

    void clear()
    {
        if (m_program)
        {
            glDeleteProgram(m_program);
            m_program = 0;
        }
    }

    // ---- Uniform 设置 ----
    void setMat4(const std::string &name, const glm::mat4 &m)
    {
        glUniformMatrix4fv(loc(name), 1, GL_FALSE, glm::value_ptr(m));
    }

    void setMat3(const std::string &name, const glm::mat3 &m)
    {
        glUniformMatrix3fv(loc(name), 1, GL_FALSE, glm::value_ptr(m));
    }

    void setVec3(const std::string &name, const glm::vec3 &v)
    {
        glUniform3fv(loc(name), 1, glm::value_ptr(v));
    }

    void setVec3(const std::string &name, float x, float y, float z)
    {
        glUniform3f(loc(name), x, y, z);
    }

    void setFloat(const std::string &name, float v)
    {
        glUniform1f(loc(name), v);
    }

    void setInt(const std::string &name, int v)
    {
        glUniform1i(loc(name), v);
    }

private:
    std::string readFile(const std::string &path)
    {
        std::ifstream f(path);
        if (!f.is_open())
        {
            std::cerr << "[Shader] cannot open: " << path << std::endl;
            return "";
        }
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    GLuint compileShader(GLenum type, const std::string &src)
    {
        const char *cstr = src.c_str();
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &cstr, nullptr);
        glCompileShader(s);

        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            char log[512];
            glGetShaderInfoLog(s, 512, nullptr, log);
            const char *tname = (type == GL_VERTEX_SHADER) ? "vertex" : "fragment";
            std::cerr << "[Shader] " << tname << " compile error: " << log << std::endl;
            glDeleteShader(s);
            return 0;
        }
        return s;
    }

    GLint loc(const std::string &name)
    {
        return glGetUniformLocation(m_program, name.c_str());
    }

    GLuint m_program = 0;
};

#endif
