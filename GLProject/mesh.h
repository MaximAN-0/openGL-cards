#pragma once
// Vertex, Mesh (quad / box generators) and Transform.
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>

struct Vertex { glm::vec3 pos; glm::vec2 uv; glm::vec3 normal; };

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    // Unit quad in the XY plane, normal +Z.
    static Mesh quad() {
        Mesh m;
        const glm::vec3 n(0, 0, 1);
        m.vertices = {{glm::vec3(-.5f, -.5f, 0), glm::vec2(0, 0), n}, {glm::vec3(.5f, -.5f, 0), glm::vec2(1, 0), n},
                      {glm::vec3(.5f, .5f, 0), glm::vec2(1, 1), n},   {glm::vec3(-.5f, .5f, 0), glm::vec2(0, 1), n}};
        m.indices = {0, 1, 2, 0, 2, 3};
        return m;
    }
    // Unit cube, 24 vertices (per-face normals).
    static Mesh box() {
        struct Face { glm::vec3 n, u, v; };
        const Face faces[6] = {
            {glm::vec3(1, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0)},
            {glm::vec3(-1, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0)},
            {glm::vec3(0, 1, 0), glm::vec3(1, 0, 0), glm::vec3(0, 0, -1)},
            {glm::vec3(0, -1, 0), glm::vec3(1, 0, 0), glm::vec3(0, 0, 1)},
            {glm::vec3(0, 0, 1), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0)},
            {glm::vec3(0, 0, -1), glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0)}};
        const float sx[4] = {-1, 1, 1, -1}, sy[4] = {-1, -1, 1, 1};
        Mesh m;
        for (const Face& f : faces) {
            const unsigned base = static_cast<unsigned>(m.vertices.size());
            for (int i = 0; i < 4; ++i)
                m.vertices.push_back({f.n * .5f + f.u * (sx[i] * .5f) + f.v * (sy[i] * .5f),
                                      glm::vec2(sx[i] > 0 ? 1.f : 0.f, sy[i] > 0 ? 1.f : 0.f), f.n});
            for (unsigned idx : {0u, 1u, 2u, 0u, 2u, 3u}) m.indices.push_back(base + idx);
        }
        return m;
    }
};

struct Transform {
    glm::vec3 position{0.f};
    glm::vec3 rotation{0.f};   // degrees, applied X then Y then Z
    glm::vec3 scale{1.f};
    glm::mat4 matrix() const {
        glm::mat4 m = glm::translate(glm::mat4(1.f), position);
        m = glm::rotate(m, glm::radians(rotation.x), glm::vec3(1, 0, 0));
        m = glm::rotate(m, glm::radians(rotation.y), glm::vec3(0, 1, 0));
        m = glm::rotate(m, glm::radians(rotation.z), glm::vec3(0, 0, 1));
        return glm::scale(m, scale);
    }
};
