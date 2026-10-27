#pragma once
#include <glad/gl.h>
#include <string>
#include <glm/glm.hpp>

class Shader {
public:
    GLuint id = 0;

    Shader(const std::string& vertexPath, const std::string& fragmentPath);
    ~Shader();

    void use() const;
    void setMat4(const std::string& name, const glm::mat4& mat) const;

private:
    std::string readFile(const std::string& path);
    GLuint compile(GLenum type, const std::string& source);
};
