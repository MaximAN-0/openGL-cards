#pragma once
// Shader program (RAII) and the GLSL sources.
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <string>
#include <unordered_map>

inline constexpr const char* kVertexSrc = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in vec3 aNormal;
uniform mat4 uModel;
uniform mat4 uVP;
uniform vec2 uUVScale;
out vec2 vUV;
out vec3 vN;
void main(){
    vUV = aUV * uUVScale;
    vN  = mat3(uModel) * aNormal;
    gl_Position = uVP * uModel * vec4(aPos, 1.0);
}
)";

inline constexpr const char* kFragmentSrc = R"(
#version 330 core
in vec2 vUV;
in vec3 vN;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform vec3 uTint;
uniform int  uUseTexture;
void main(){
    vec4 tex = (uUseTexture == 1) ? texture(uTexture, vUV) : vec4(1.0);
    if (tex.a < 0.1) discard;                       // torn parts of the poster
    float d = abs(dot(normalize(vN), normalize(vec3(0.35, 1.0, 0.5))));
    float light = 0.55 + 0.55 * d;                  // cheap two-sided shading
    FragColor = vec4(min(tex.rgb * uTint * light, vec3(1.0)), 1.0);
}
)";

class Shader {
public:
    Shader(const char* vs, const char* fs) {
        GLuint v = compile(GL_VERTEX_SHADER, vs), f = compile(GL_FRAGMENT_SHADER, fs);
        id_ = glCreateProgram();
        glAttachShader(id_, v);
        glAttachShader(id_, f);
        glLinkProgram(id_);
        GLint ok = 0;
        glGetProgramiv(id_, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetProgramInfoLog(id_, 1024, nullptr, log);
            std::cerr << "Shader link error: " << log << "\n";
        }
        glDeleteShader(v);
        glDeleteShader(f);
    }
    ~Shader() { if (id_) glDeleteProgram(id_); }
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void use() const { glUseProgram(id_); }
    void setMat4(const char* n, const glm::mat4& m) const { glUniformMatrix4fv(loc(n), 1, GL_FALSE, glm::value_ptr(m)); }
    void setVec3(const char* n, const glm::vec3& v) const { glUniform3fv(loc(n), 1, glm::value_ptr(v)); }
    void setVec2(const char* n, const glm::vec2& v) const { glUniform2fv(loc(n), 1, glm::value_ptr(v)); }
    void setInt(const char* n, int v) const { glUniform1i(loc(n), v); }

private:
    static GLuint compile(GLenum type, const char* src) {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetShaderInfoLog(s, 1024, nullptr, log);
            std::cerr << "Shader compile error: " << log << "\n";
        }
        return s;
    }
    GLint loc(const char* n) const {
        auto it = cache_.find(n);
        if (it != cache_.end()) return it->second;
        GLint l = glGetUniformLocation(id_, n);
        cache_[n] = l;
        return l;
    }
    GLuint id_ = 0;
    mutable std::unordered_map<std::string, GLint> cache_;
};
