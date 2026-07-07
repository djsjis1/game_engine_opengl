#ifndef GAME_ENGINE_RENDER_SHADER_H
#define GAME_ENGINE_RENDER_SHADER_H

#include <QOpenGLFunctions_3_3_Core>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

// ============================================================
// Shader — GLSL 着色器封装（编译、链接、uniform 传值）
//
// 支持移动语义，禁止拷贝（保护 GPU 资源）。
// 从文件加载 GLSL 源码，编译失败自动打印错误日志。
// ============================================================
class Shader : protected QOpenGLFunctions_3_3_Core
{
public:
    Shader() = default;

    Shader(const std::string &vertPath, const std::string &fragPath)
    {
        load(vertPath, fragPath);
    }

    ~Shader()
    {
        if (m_program)
            glDeleteProgram(m_program);
    }

    // 禁止拷贝，允许移动
    Shader(const Shader &) = delete;
    Shader &operator=(const Shader &) = delete;
    Shader(Shader &&o) noexcept : m_program(o.m_program) { o.m_program = 0; }
    Shader &operator=(Shader &&o) noexcept
    {
        if (this != &o)
        {
            if (m_program)
                glDeleteProgram(m_program);
            m_program = o.m_program;
            o.m_program = 0;
        }
        return *this;
    }

    // ---- 从文件加载并编译链接 ----
    bool load(const std::string &vertPath, const std::string &fragPath)
    {
        initializeOpenGLFunctions();

        // 读取顶点着色器源码
        std::ifstream vf(vertPath);
        std::ifstream ff(fragPath);
        if (!vf.is_open() || !ff.is_open())
        {
            std::cerr << "[Shader] Failed to open: " << vertPath << " or " << fragPath << std::endl;
            return false;
        }

        std::stringstream vss, fss;
        vss << vf.rdbuf();
        fss << ff.rdbuf();
        std::string vSrc = vss.str();
        std::string fSrc = fss.str();
        const char *vc = vSrc.c_str();
        const char *fc = fSrc.c_str();

        // 编译顶点着色器
        GLuint vsId = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vsId, 1, &vc, nullptr);
        glCompileShader(vsId);
        if (!check(vsId, "VERTEX"))
        {
            glDeleteShader(vsId);
            return false;
        }

        // 编译片段着色器
        GLuint fsId = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fsId, 1, &fc, nullptr);
        glCompileShader(fsId);
        if (!check(fsId, "FRAGMENT"))
        {
            glDeleteShader(vsId);
            glDeleteShader(fsId);
            return false;
        }

        // 链接程序
        m_program = glCreateProgram();
        glAttachShader(m_program, vsId);
        glAttachShader(m_program, fsId);
        glLinkProgram(m_program);
        if (!check(m_program, "PROGRAM"))
        {
            glDeleteShader(vsId);
            glDeleteShader(fsId);
            return false;
        }

        // 链接完成，释放着色器对象
        glDeleteShader(vsId);
        glDeleteShader(fsId);
        return true;
    }

    // ---- 使用与查询 ----
    void use() { glUseProgram(m_program); }
    GLuint id() const { return m_program; }
    bool isValid() const { return m_program != 0; }

    // ---- Uniform 传值 ----
    void setInt(const std::string &name, int v) { glUniform1i(loc(name), v); }
    void setFloat(const std::string &name, float v) { glUniform1f(loc(name), v); }
    void setVec3(const std::string &name, float x, float y, float z) { glUniform3f(loc(name), x, y, z); }
    void setVec3(const std::string &name, const float *v) { glUniform3fv(loc(name), 1, v); }
    void setMat3(const std::string &name, const float *v) { glUniformMatrix3fv(loc(name), 1, GL_FALSE, v); }
    void setMat4(const std::string &name, const float *v) { glUniformMatrix4fv(loc(name), 1, GL_FALSE, v); }

private:
    GLint loc(const std::string &name) { return glGetUniformLocation(m_program, name.c_str()); }

    bool check(GLuint id, const std::string &type)
    {
        GLint ok = 0;
        char log[1024];

        if (type != "PROGRAM")
        {
            glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
            if (!ok)
            {
                glGetShaderInfoLog(id, 1024, nullptr, log);
                std::cerr << "[Shader] Compile error (" << type << "):\n"
                          << log << std::endl;
                return false;
            }
        }
        else
        {
            glGetProgramiv(id, GL_LINK_STATUS, &ok);
            if (!ok)
            {
                glGetProgramInfoLog(id, 1024, nullptr, log);
                std::cerr << "[Shader] Link error:\n"
                          << log << std::endl;
                return false;
            }
        }
        return true;
    }

    GLuint m_program = 0;
};

#endif // GAME_ENGINE_RENDER_SHADER_H
#ifndef GAME_ENGINE_RENDER_SHADER_H
#define GAME_ENGINE_RENDER_SHADER_H

#include <QOpenGLFunctions_3_3_Core>
#include <QString>
#include <fstream>
#include <sstream>
#include <iostream>

class Shader : protected QOpenGLFunctions_3_3_Core
{
public:
    Shader() = default;
    Shader(const QString &v, const QString &f) { load(v, f); }
    ~Shader()
    {
        if (m_program)
        {
            glDeleteProgram(m_program);
        }
    }

    Shader(const Shader &) = delete;
    Shader &operator=(const Shader &) = delete;
    Shader(Shader &&o) noexcept : m_program(o.m_program) { o.m_program = 0; }
    Shader &operator=(Shader &&o) noexcept
    {
        if (this != &o)
        {
            if (m_program)
                glDeleteProgram(m_program);
            m_program = o.m_program;
            o.m_program = 0;
        }
        return *this;
    }

    bool load(const QString &vPath, const QString &fPath)
    {
        initializeOpenGLFunctions();
        std::string vCode, fCode;
        std::ifstream vf(vPath.toStdString()), ff(fPath.toStdString());
        if (!vf.is_open() || !ff.is_open())
            return false;
        std::stringstream vs, fs;
        vs << vf.rdbuf();
        fs << ff.rdbuf();
        vCode = vs.str();
        fCode = fs.str();
        const char *vc = vCode.c_str(), *fc = fCode.c_str();

        GLuint vsId = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vsId, 1, &vc, nullptr);
        glCompileShader(vsId);
        if (!check(vsId, "VERTEX"))
        {
            glDeleteShader(vsId);
            return false;
        }

        GLuint fsId = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fsId, 1, &fc, nullptr);
        glCompileShader(fsId);
        if (!check(fsId, "FRAGMENT"))
        {
            glDeleteShader(vsId);
            glDeleteShader(fsId);
            return false;
        }

        m_program = glCreateProgram();
        glAttachShader(m_program, vsId);
        glAttachShader(m_program, fsId);
        glLinkProgram(m_program);
        if (!check(m_program, "PROGRAM"))
        {
            glDeleteShader(vsId);
            glDeleteShader(fsId);
            return false;
        }

        glDeleteShader(vsId);
        glDeleteShader(fsId);
        return true;
    }

    void use() { glUseProgram(m_program); }
    GLuint id() const { return m_program; }
    bool isValid() const { return m_program != 0; }

    void setInt(const QString &n, int v) { glUniform1i(loc(n), v); }
    void setFloat(const QString &n, float v) { glUniform1f(loc(n), v); }
    void setVec3(const QString &n, float x, float y, float z) { glUniform3f(loc(n), x, y, z); }
    void setVec3(const QString &n, const float *v) { glUniform3fv(loc(n), 1, v); }
    void setMat3(const QString &n, const float *v) { glUniformMatrix3fv(loc(n), 1, GL_FALSE, v); }
    void setMat4(const QString &n, const float *v) { glUniformMatrix4fv(loc(n), 1, GL_FALSE, v); }

private:
    GLint loc(const QString &n) { return glGetUniformLocation(m_program, n.toStdString().c_str()); }
    bool check(GLuint s, const QString &type)
    {
        GLint ok;
        char log[1024];
        if (type != "PROGRAM")
        {
            glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
            if (!ok)
            {
                glGetShaderInfoLog(s, 1024, nullptr, log);
                std::cerr << "Shader compile " << type.toStdString() << ":\n"
                          << log << std::endl;
                return false;
            }
        }
        else
        {
            glGetProgramiv(s, GL_LINK_STATUS, &ok);
            if (!ok)
            {
                glGetProgramInfoLog(s, 1024, nullptr, log);
                std::cerr << "Program link:\n"
                          << log << std::endl;
                return false;
            }
        }
        return true;
    }
    GLuint m_program = 0;
};

#endif
