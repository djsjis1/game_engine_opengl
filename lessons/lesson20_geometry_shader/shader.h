#ifndef SHADER_H
#define SHADER_H
#include <QOpenGLFunctions_3_3_Core>
#include <QString>
#include <fstream>
#include <sstream>
#include <iostream>
class Shader : protected QOpenGLFunctions_3_3_Core
{
public:
    Shader() = default;
    Shader(const QString &vp, const QString &fp) { load(vp, fp); }
    Shader(const QString &vp, const QString &gp, const QString &fp) { load(vp, gp, fp); }
    bool load(const QString &vp, const QString &fp)
    {
        initializeOpenGLFunctions();
        m_vSrc = readFile(vp);
        m_fSrc = readFile(fp);
        if (m_vSrc.empty() || m_fSrc.empty())
            return false;
        return link(m_vSrc.c_str(), nullptr, m_fSrc.c_str());
    }
    // 带几何着色器的版本
    bool load(const QString &vp, const QString &gp, const QString &fp)
    {
        initializeOpenGLFunctions();
        m_vSrc = readFile(vp);
        m_gSrc = readFile(gp);
        m_fSrc = readFile(fp);
        if (m_vSrc.empty() || m_gSrc.empty() || m_fSrc.empty())
            return false;
        return link(m_vSrc.c_str(), m_gSrc.c_str(), m_fSrc.c_str());
    }
    void use() { glUseProgram(m_prog); }
    void setInt(const QString &n, int v) { glUniform1i(glGetUniformLocation(m_prog, n.toStdString().c_str()), v); }
    void setFloat(const QString &n, float v) { glUniform1f(glGetUniformLocation(m_prog, n.toStdString().c_str()), v); }
    void setVec3(const QString &n, float x, float y, float z) { glUniform3f(glGetUniformLocation(m_prog, n.toStdString().c_str()), x, y, z); }
    void setMat4(const QString &n, const float *v) { glUniformMatrix4fv(glGetUniformLocation(m_prog, n.toStdString().c_str()), 1, GL_FALSE, v); }

private:
    std::string readFile(const QString &path)
    {
        std::ifstream f(path.toStdString());
        if (!f.is_open())
        {
            std::cerr << "ERR:open " << path.toStdString() << std::endl;
            return "";
        }
        std::stringstream ss;
        ss << f.rdbuf();
        f.close();
        return ss.str();
    }
    std::string m_vSrc, m_gSrc, m_fSrc;
    bool link(const char *vs, const char *gs, const char *fs)
    {
        GLuint v = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(v, 1, &vs, nullptr);
        glCompileShader(v);
        check(v, "VS");
        GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(f, 1, &fs, nullptr);
        glCompileShader(f);
        check(f, "FS");
        GLuint g = 0;
        if (gs)
        {
            g = glCreateShader(GL_GEOMETRY_SHADER);
            glShaderSource(g, 1, &gs, nullptr);
            glCompileShader(g);
            check(g, "GS");
        }
        m_prog = glCreateProgram();
        glAttachShader(m_prog, v);
        if (gs)
            glAttachShader(m_prog, g);
        glAttachShader(m_prog, f);
        glLinkProgram(m_prog);
        check(m_prog, "LINK");
        glDeleteShader(v);
        glDeleteShader(f);
        if (gs)
            glDeleteShader(g);
        return true;
    }
    void check(GLuint s, const QString &t)
    {
        GLint ok;
        char log[1024];
        if (t != "LINK")
        {
            glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
            if (!ok)
            {
                glGetShaderInfoLog(s, 1024, nullptr, log);
                std::cerr << t.toStdString() << ":" << log << std::endl;
            }
        }
        else
        {
            glGetProgramiv(s, GL_LINK_STATUS, &ok);
            if (!ok)
            {
                glGetProgramInfoLog(s, 1024, nullptr, log);
                std::cerr << t.toStdString() << ":" << log << std::endl;
            }
        }
    }
    GLuint m_prog = 0;
};
#endif
