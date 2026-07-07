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
            std::cerr << "ERROR: Failed to open shader files!" << std::endl;
            return false;
        }
        std::stringstream vs, fs;
        vs << vf.rdbuf();
        fs << ff.rdbuf();
        vc = vs.str();
        fc = fs.str();
        vf.close();
        ff.close();
        const char *vcs = vc.c_str();
        const char *fcs = fc.c_str();

        GLuint v = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(v, 1, &vcs, nullptr);
        glCompileShader(v);
        checkError(v, "VERTEX");
        GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(f, 1, &fcs, nullptr);
        glCompileShader(f);
        checkError(f, "FRAGMENT");
        m_program = glCreateProgram();
        glAttachShader(m_program, v);
        glAttachShader(m_program, f);
        glLinkProgram(m_program);
        checkError(m_program, "PROGRAM");
        glDeleteShader(v);
        glDeleteShader(f);
        return true;
    }

    void use() { glUseProgram(m_program); }
    GLuint id() const { return m_program; }

    void setInt(const QString &n, int val) { glUniform1i(glGetUniformLocation(m_program, n.toStdString().c_str()), val); }
    void setFloat(const QString &n, float val) { glUniform1f(glGetUniformLocation(m_program, n.toStdString().c_str()), val); }

private:
    void checkError(GLuint s, const QString &t)
    {
        GLint ok;
        char log[1024];
        if (t != "PROGRAM")
        {
            glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
            if (!ok)
            {
                glGetShaderInfoLog(s, 1024, nullptr, log);
                std::cerr << "ERROR: " << t.toStdString() << ":\n"
                          << log << std::endl;
            }
        }
        else
        {
            glGetProgramiv(s, GL_LINK_STATUS, &ok);
            if (!ok)
            {
                glGetProgramInfoLog(s, 1024, nullptr, log);
                std::cerr << "ERROR: Program:\n"
                          << log << std::endl;
            }
        }
    }
    GLuint m_program = 0;
};

#endif
