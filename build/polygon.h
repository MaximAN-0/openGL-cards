#pragma once
// GPU geometry: Polygon (flat colour), PolygonWithTexture, and shared Primitives.
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <cstddef>
#include <memory>
#include <utility>

#include "mesh.h"
#include "shader.h"
#include "texture.h"

// Flat-coloured geometry. Owns its GPU buffers; move-only.
class Polygon {
public:
    explicit Polygon(const Mesh& mesh) { upload(mesh); }
    virtual ~Polygon() { release(); }
    Polygon(const Polygon&) = delete;
    Polygon& operator=(const Polygon&) = delete;
    Polygon(Polygon&& o) noexcept { swap(o); }
    Polygon& operator=(Polygon&& o) noexcept {
        if (this != &o) { release(); swap(o); }
        return *this;
    }

    // Template method: common uniforms -> material hook -> draw call.
    void draw(const Shader& s, const glm::mat4& model, const glm::vec3& tint = glm::vec3(1.f),
              const glm::vec2& uvScale = glm::vec2(1.f)) const {
        s.setMat4("uModel", model);
        s.setVec3("uTint", tint);
        s.setVec2("uUVScale", uvScale);
        applyMaterial(s);
        glBindVertexArray(vao_);
        glDrawElements(GL_TRIANGLES, count_, GL_UNSIGNED_INT, nullptr);
    }

protected:
    virtual void applyMaterial(const Shader& s) const { s.setInt("uUseTexture", 0); }

private:
    void upload(const Mesh& m) {
        count_ = static_cast<GLsizei>(m.indices.size());
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glGenBuffers(1, &ebo_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, m.vertices.size() * sizeof(Vertex), m.vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, m.indices.size() * sizeof(unsigned), m.indices.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
        glBindVertexArray(0);
    }
    void release() {
        if (ebo_) glDeleteBuffers(1, &ebo_);
        if (vbo_) glDeleteBuffers(1, &vbo_);
        if (vao_) glDeleteVertexArrays(1, &vao_);
        ebo_ = vbo_ = vao_ = 0;
        count_ = 0;
    }
    void swap(Polygon& o) noexcept {
        std::swap(vao_, o.vao_); std::swap(vbo_, o.vbo_); std::swap(ebo_, o.ebo_); std::swap(count_, o.count_);
    }
    GLuint vao_ = 0, vbo_ = 0, ebo_ = 0;
    GLsizei count_ = 0;
};

// Same geometry, but sampled from a (shared) texture.
class PolygonWithTexture : public Polygon {
public:
    PolygonWithTexture(const Mesh& mesh, std::shared_ptr<Texture> tex) : Polygon(mesh), tex_(std::move(tex)) {}
    void setTexture(std::shared_ptr<Texture> t) { tex_ = std::move(t); }

protected:
    void applyMaterial(const Shader& s) const override {
        s.setInt("uUseTexture", tex_ ? 1 : 0);
        if (tex_) tex_->bind(0);
    }

private:
    std::shared_ptr<Texture> tex_;
};

// Shared unit quad + unit box used for all untextured furniture.
struct Primitives {
    Polygon quad{Mesh::quad()};
    Polygon box{Mesh::box()};

    void drawBox(const Shader& s, const glm::vec3& pos, const glm::vec3& size, const glm::vec3& color) const {
        box.draw(s, Transform{pos, glm::vec3(0.f), size}.matrix(), color);
    }
    void drawFlat(const Shader& s, const glm::vec3& pos, const glm::vec2& size, const glm::vec3& color) const {
        quad.draw(s, Transform{pos, glm::vec3(-90.f, 0.f, 0.f), glm::vec3(size, 1.f)}.matrix(), color);
    }
    // Box positioned inside a parent coordinate frame (used by multi-part props).
    void drawBoxLocal(const Shader& s, const glm::mat4& parent, const glm::vec3& pos, const glm::vec3& size,
                      const glm::vec3& color) const {
        box.draw(s, parent * Transform{pos, glm::vec3(0.f), size}.matrix(), color);
    }
    // Square-section bar running from point a to point b (arms, etc.).
    void drawLimb(const Shader& s, const glm::mat4& parent, const glm::vec3& a, const glm::vec3& b, float thickness,
                  const glm::vec3& color) const {
        const glm::vec3 d = b - a;
        const float len = glm::length(d);
        if (len < 1e-4f) return;
        const glm::vec3 y = d / len;
        const glm::vec3 ref = std::fabs(y.y) > 0.99f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
        const glm::vec3 x = glm::normalize(glm::cross(ref, y));
        const glm::vec3 z = glm::cross(x, y);
        glm::mat4 m(glm::vec4(x, 0.f), glm::vec4(y, 0.f), glm::vec4(z, 0.f), glm::vec4((a + b) * 0.5f, 1.f));
        m = glm::scale(m, glm::vec3(thickness, len, thickness));
        box.draw(s, parent * m, color);
    }
};
