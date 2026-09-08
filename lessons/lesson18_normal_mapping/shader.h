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
    bool load(const QString &vp, const QString &fp)
    {
        initializeOpenGLFunctions();
        std::string vc, fc;
        std::ifstream vf, ff;
        vf.open(vp.toStdString());
        ff.open(fp.toStdString());
        if (!vf.is_open() || !ff.is_open())
        {
            std::cerr << "ERROR: shader file!" << std::endl;
            return false;
        }
        std::stringstream vs, fs;
        vs << vf.rdbuf();
        fs << ff.rdbuf();
        vc = vs.str();
        fc = fs.str();
        vf.close();
        ff.close();
        const char *vcs = vc.c_str(), *fcs = fc.c_str();
        GLuint v = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(v, 1, &vcs, nullptr);
        glCompileShader(v);
        check(v, "VERTEX");
        GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(f, 1, &fcs, nullptr);
        glCompileShader(f);
        check(f, "FRAGMENT");
        m_prog = glCreateProgram();
        glAttachShader(m_prog, v);
        glAttachShader(m_prog, f);
        glLinkProgram(m_prog);
        check(m_prog, "PROGRAM");
        glDeleteShader(v);
        glDeleteShader(f);
        return true;
    }
    void use() { glUseProgram(m_prog); }
    GLuint id() const { return m_prog; }
    void setInt(const QString &n, int v) { glUniform1i(glGetUniformLocation(m_prog, n.toStdString().c_str()), v); }
    void setFloat(const QString &n, float v) { glUniform1f(glGetUniformLocation(m_prog, n.toStdString().c_str()), v); }
    void setVec3(const QString &n, float x, float y, float z) { glUniform3f(glGetUniformLocation(m_prog, n.toStdString().c_str()), x, y, z); }
    void setMat4(const QString &n, const float *v) { glUniformMatrix4fv(glGetUniformLocation(m_prog, n.toStdString().c_str()), 1, GL_FALSE, v); }

private:
    void check(GLuint s, const QString &t)
    {
        GLint ok;
        char log[1024];
        if (t != "PROGRAM")
        {
            glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
            if (!ok)
            {
                glGetShaderInfoLog(s, 1024, nullptr, log);
                std::cerr << "ERR:" << t.toStdString() << ":\n"
                          << log << std::endl;
            }
        }
        else
        {
            glGetProgramiv(s, GL_LINK_STATUS, &ok);
            if (!ok)
            {
                glGetProgramInfoLog(s, 1024, nullptr, log);
                std::cerr << "ERR:Link:\n"
                          << log << std::endl;
            }
        }
    }
    GLuint m_prog = 0;
};
#endif
