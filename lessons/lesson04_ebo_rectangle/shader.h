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
    Shader(const QString &vertexPath, const QString &fragmentPath) { load(vertexPath, fragmentPath); }

    bool load(const QString &vertexPath, const QString &fragmentPath)
    {
        initializeOpenGLFunctions();
        std::string vertexCode, fragmentCode;
        std::ifstream vFile, fFile;
        vFile.open(vertexPath.toStdString());
        fFile.open(fragmentPath.toStdString());
        if (!vFile.is_open() || !fFile.is_open())
        {
            std::cerr << "ERROR: Failed to open shader files!" << std::endl;
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

        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vCode, nullptr);
        glCompileShader(vs);
        checkCompileErrors(vs, "VERTEX");

        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fCode, nullptr);
        glCompileShader(fs);
        checkCompileErrors(fs, "FRAGMENT");

        m_program = glCreateProgram();
        glAttachShader(m_program, vs);
        glAttachShader(m_program, fs);
        glLinkProgram(m_program);
        checkCompileErrors(m_program, "PROGRAM");
        glDeleteShader(vs);
        glDeleteShader(fs);
        return true;
    }

    void use() { glUseProgram(m_program); }
    GLuint id() const { return m_program; }

    void setFloat(const QString &name, float value)
    {
        glUniform1f(glGetUniformLocation(m_program, name.toStdString().c_str()), value);
    }
    void setVec4(const QString &name, float x, float y, float z, float w)
    {
        glUniform4f(glGetUniformLocation(m_program, name.toStdString().c_str()), x, y, z, w);
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
                std::cerr << "ERROR: " << type.toStdString() << " compile:\n"
                          << infoLog << std::endl;
            }
        }
        else
        {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success)
            {
                glGetProgramInfoLog(shader, 1024, nullptr, infoLog);
                std::cerr << "ERROR: Program link:\n"
                          << infoLog << std::endl;
            }
        }
    }

    GLuint m_program = 0;
};

#endif
