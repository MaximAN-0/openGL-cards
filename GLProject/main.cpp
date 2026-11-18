// =============================================================================
//  Card Room - modern OpenGL (3.3 core), C++17, OOP design
//
//  Controls
//    1 / 2        top-down view / table view (camera slides between them)
//    0            toggle cinematic "idle look-around" camera
//    G            both players draw one card (opponent is delayed by 0.5 s)
//    LEFT / RIGHT select a card in your hand (it slides up)
//    D            discard the selected card to the discard pile
//    ESC          quit
//
//  Assets are OPTIONAL. If a PNG is missing a procedural texture is generated:
//    assets/wood.png  assets/wallpaper.png  assets/torn_poster.png (needs alpha)
//    assets/cards/back.png  assets/cards/<name>.png  (the major arcana cards, 22 in total)
// =============================================================================
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef ASSET_DIR
#define ASSET_DIR "../assets/"
#endif

// ----------------------------------------------------------------------------
//  Configuration / layout constants
// ----------------------------------------------------------------------------
namespace cfg {
constexpr int   kWindowW = 1280, kWindowH = 800;
constexpr int   kMaxDraws = 5;
constexpr float kOpponentDelay = 0.5f;
constexpr float kCardW = 0.82f, kCardH = 1.22f;
constexpr float kCardY = 0.965f;          // resting height of a card above the table
constexpr float kLayerStep = 0.01f;      // thickness of one card in a pile
constexpr float kStockX = -2.7f, kDiscardX = 2.7f;
constexpr float kPlayerZ = 1.5f, kOpponentZ = -1.5f;
constexpr float kHandSpacing = 1.0f;
constexpr float kFloorY = -0.08f;
const glm::vec3 kSelectOffset(0.0f, 0.28f, -0.35f);  // "slide up" for a selected card
}  // namespace cfg

inline float smootherstep(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

// ----------------------------------------------------------------------------
//  Shader (RAII)
// ----------------------------------------------------------------------------
static const char* kVertexSrc = R"(
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

static const char* kFragmentSrc = R"(
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

// ----------------------------------------------------------------------------
//  Texture (RAII, move-only; shared through std::shared_ptr)
// ----------------------------------------------------------------------------
class Texture {
public:
    Texture(int w, int h, const unsigned char* rgba, bool repeat) {
        glGenTextures(1, &id_);
        glBindTexture(GL_TEXTURE_2D, id_);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        glGenerateMipmap(GL_TEXTURE_2D);
        const GLint wrap = repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    ~Texture() { if (id_) glDeleteTextures(1, &id_); }
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& o) noexcept : id_(o.id_) { o.id_ = 0; }
    Texture& operator=(Texture&& o) noexcept {
        if (this != &o) { if (id_) glDeleteTextures(1, &id_); id_ = o.id_; o.id_ = 0; }
        return *this;
    }
    void bind(GLuint unit = 0) const { glActiveTexture(GL_TEXTURE0 + unit); glBindTexture(GL_TEXTURE_2D, id_); }

    static std::shared_ptr<Texture> fromFile(const std::string& path, bool repeat) {
        stbi_set_flip_vertically_on_load(1);
        int w, h, c;
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &c, 4);
        if (!data) return nullptr;
        auto tex = std::make_shared<Texture>(w, h, data, repeat);
        stbi_image_free(data);
        return tex;
    }

private:
    GLuint id_ = 0;
};

// ----------------------------------------------------------------------------
//  Mesh + Polygon + PolygonWithTexture (VAO/VBO/EBO owned & cleaned up in dtor)
// ----------------------------------------------------------------------------
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
};

// ----------------------------------------------------------------------------
//  Procedural textures (used when PNG assets are not present)
// ----------------------------------------------------------------------------
namespace Procedural {

struct Image {
    int w, h;
    std::vector<unsigned char> px;
    Image(int w_, int h_) : w(w_), h(h_), px(static_cast<size_t>(w_) * h_ * 4, 255) {}
    void set(int x, int y, const glm::vec3& c, float a = 1.f) {
        unsigned char* p = &px[(static_cast<size_t>(y) * w + x) * 4];
        p[0] = (unsigned char)(std::clamp(c.x, 0.f, 1.f) * 255.f + .5f);
        p[1] = (unsigned char)(std::clamp(c.y, 0.f, 1.f) * 255.f + .5f);
        p[2] = (unsigned char)(std::clamp(c.z, 0.f, 1.f) * 255.f + .5f);
        p[3] = (unsigned char)(std::clamp(a, 0.f, 1.f) * 255.f + .5f);
    }
    std::shared_ptr<Texture> upload(bool repeat) const { return std::make_shared<Texture>(w, h, px.data(), repeat); }
};

inline float hash2(int x, int y) {
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (h & 0xFFFFu) / 65535.f;
}
inline float vnoise(float x, float y) {
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float fx = x - xi, fy = y - yi;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    return glm::mix(glm::mix(hash2(xi, yi), hash2(xi + 1, yi), fx),
                    glm::mix(hash2(xi, yi + 1), hash2(xi + 1, yi + 1), fx), fy);
}
inline glm::vec3 hsv(float h, float s, float v) {
    h -= std::floor(h);
    glm::vec3 rgb = glm::clamp(glm::vec3(std::fabs(h * 6 - 3) - 1, 2 - std::fabs(h * 6 - 2), 2 - std::fabs(h * 6 - 4)), 0.f, 1.f);
    return ((rgb - 1.f) * s + 1.f) * v;
}

inline Image wood() {
    Image img(512, 512);
    for (int y = 0; y < 512; ++y)
        for (int x = 0; x < 512; ++x) {
            int plank = y / 64;
            float tone = 0.7f + 0.4f * hash2(plank, 3);
            float g = vnoise(x * 0.012f, y * 0.20f + plank * 13.f);
            float ring = 0.5f + 0.5f * std::sin(g * 18.f + y * 0.15f);
            glm::vec3 c = glm::vec3(0.60f, 0.39f, 0.21f) * (tone * (0.78f + 0.27f * ring));
            int joint = (int)(hash2(plank, 9) * 512);
            if ((y % 64) < 2 || std::abs(x - joint) < 2) c *= 0.45f;
            img.set(x, y, c);
        }
    return img;
}

inline Image wallpaper() {
    Image img(256, 256);
    for (int y = 0; y < 256; ++y)
        for (int x = 0; x < 256; ++x) {
            int s = x % 64;
            glm::vec3 c = s < 32 ? glm::vec3(.88f, .82f, .67f) : glm::vec3(.80f, .72f, .55f);
            if (s == 0 || s == 32) c = glm::vec3(.58f, .43f, .32f);
            if (s >= 32 && std::abs(s - 48) + std::abs(y % 64 - 32) < 11) c = glm::vec3(.66f, .47f, .38f);
            if (s < 32 && std::abs(s - 16) + std::abs((y + 32) % 64 - 32) < 6) c = glm::vec3(.78f, .70f, .52f);
            img.set(x, y, c * (0.95f + 0.05f * hash2(x, y)));
        }
    return img;
}

// Poster that is torn away along its right side and at the top-right corner.
inline Image tornPoster() {
    const int W = 256, H = 384;
    Image img(W, H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const float ytop = float(H - 1 - y), u = x / float(W - 1), v = y / float(H - 1);
            const float edge = W * 0.80f + 16.f * (vnoise(y * 0.06f, 3.f) - 0.5f) * 2.f + 6.f * (hash2(y / 2, 5) - 0.5f);
            const bool corner = x > W * 0.55f && ytop < (x - W * 0.55f) * 0.75f + 6.f * vnoise(x * 0.1f, 9.f);
            if (x > edge || corner) { img.set(x, y, glm::vec3(0.f), 0.f); continue; }

            glm::vec3 c = glm::mix(glm::vec3(.10f, .18f, .32f), glm::vec3(.55f, .20f, .25f), v);
            if (x < 10 || ytop < 10 || y < 10) c = glm::vec3(.92f, .80f, .45f);
            float du = std::fabs(u - .42f), dv = std::fabs(v - .58f);
            if (du + dv * .66f < .22f) c = glm::vec3(.95f, .82f, .30f);
            if (du + dv * .66f < .15f) c = glm::vec3(.75f, .15f, .18f);
            if (v > .14f && v < .24f && u > .08f) c = glm::vec3(.93f, .90f, .80f);
            if (((v > .84f && v < .88f) || (v > .89f && v < .92f)) && u > .1f && hash2(x / 6, int(v * 50)) > .35f)
                c = glm::vec3(.95f, .90f, .70f);
            // wear: blotchy fading, stains, scratches, folds
            c *= 0.65f + 0.45f * vnoise(x * .05f, y * .05f);
            if (vnoise(x * .02f + 7.f, y * .02f) > .62f) c = glm::mix(c, glm::vec3(.45f, .35f, .2f), .5f);
            if (hash2(x, y) > .985f) c = glm::vec3(.9f, .88f, .8f);
            if (std::abs(x - 90) < 1 || std::abs(y - 190) < 1) c *= 1.25f;
            // fibrous paper edge along the tear
            float d = edge - x;
            if (d < 7.f) c = glm::mix(glm::vec3(.93f, .90f, .82f), c, std::clamp(d / 7.f, 0.f, 1.f));
            img.set(x, y, c);
        }
    return img;
}

inline Image cardBack() {
    Image img(256, 384);
    for (int y = 0; y < 384; ++y)
        for (int x = 0; x < 256; ++x) {
            float u = x / 255.f, v = y / 383.f;
            glm::vec3 c(.10f, .15f, .35f);
            if (u < .06f || u > .94f || v < .04f || v > .96f) c = glm::vec3(.92f, .85f, .60f);
            else {
                if ((x + y) % 32 < 2 || (x - y + 1024) % 32 < 2) c = glm::vec3(.78f, .65f, .30f);
                float r = glm::length(glm::vec2((u - .5f) * (2.f / 3.f), v - .5f) * 1.5f);
                if (r < .14f) c = glm::vec3(.70f, .15f, .20f);
                if (r < .08f) c = glm::vec3(.95f, .80f, .30f);
            }
            img.set(x, y, c);
        }
    return img;
}

// Simple cartoon character portrait: head, eyes, mouth, body and a hat/halo.
inline Image character(int idx) {
    Image img(256, 384);
    const float hue = idx / 12.f;
    const glm::vec3 bgA = hsv(hue, .55f, .85f), bgB = hsv(hue + .08f, .70f, .30f), skin = hsv(hue + .5f, .30f, .97f),
                    acc = hsv(hue + .33f, .85f, .95f), body = hsv(hue + .66f, .60f, .70f);
    for (int y = 0; y < 384; ++y)
        for (int x = 0; x < 256; ++x) {
            const float X = (x - 127.5f) / 384.f, Y = y / 384.f;
            glm::vec3 c = glm::mix(bgB, bgA, Y);
            c += 0.25f * std::max(0.f, 1.f - glm::length(glm::vec2(X, Y - .55f)) * 2.4f);
            float bx = X / .28f, by = (Y - .20f) / .17f;
            if (bx * bx + by * by < 1.f) c = body;
            if (X * X + (Y - .55f) * (Y - .55f) < .17f * .17f) c = skin;
            for (float s : {-1.f, 1.f}) {
                float dx = X - s * .07f, dy = Y - .58f;
                if (dx * dx + dy * dy < .03f * .03f) c = glm::vec3(.08f);
            }
            if (Y < .50f && Y > .455f && (X / .08f) * (X / .08f) + ((Y - .50f) / .045f) * ((Y - .50f) / .045f) < 1.f)
                c = glm::vec3(.25f, .05f, .08f);
            switch (idx % 4) {
                case 0:  // wizard hat
                    if (Y > .69f && Y < .95f && std::fabs(X) < (.95f - Y) * .55f) c = acc;
                    if (Y > .67f && Y < .70f && std::fabs(X) < .24f) c = acc * .8f;
                    break;
                case 1:  // horns
                    for (float s : {-1.f, 1.f})
                        if (Y > .69f && Y < .88f && std::fabs(X - s * .12f) < (.88f - Y) * .28f) c = acc;
                    break;
                case 2:  // crown
                    if (Y > .70f && Y < .76f + .05f * (((int)std::floor((X + .15f) * 20.f)) & 1) && std::fabs(X) < .15f) c = acc;
                    break;
                case 3: {  // halo
                    float rx = X / .15f, ry = (Y - .78f) / .04f, r = rx * rx + ry * ry;
                    if (r < 1.f && r > .45f) c = acc;
                } break;
            }
            if (Y < .09f) c = glm::vec3(.95f, .92f, .82f);
            if (Y < .065f && Y > .03f && std::fabs(X) < .2f && hash2(x / 5, y / 5) > .3f) c = acc * .55f;
            if (x < 8 || x > 247 || y < 8 || y > 375) c = glm::vec3(.92f, .78f, .35f);
            img.set(x, y, c);
        }
    return img;
}
}  // namespace Procedural

inline std::shared_ptr<Texture> loadTexture(const std::string& rel, bool repeat,
                                            const std::function<Procedural::Image()>& fallback) {
    if (auto t = Texture::fromFile(std::string(ASSET_DIR) + rel, repeat)) return t;
    std::cout << "[assets] " << rel << " not found - using a procedural texture\n";
    return fallback().upload(repeat);
}

// ----------------------------------------------------------------------------
//  Cameras
// ----------------------------------------------------------------------------
struct CameraPose { glm::vec3 eye, target, up; };

inline CameraPose lerpPose(const CameraPose& a, const CameraPose& b, float t) {
    return {glm::mix(a.eye, b.eye, t), glm::mix(a.target, b.target, t), glm::normalize(glm::mix(a.up, b.up, t))};
}

// Preset poses (coordinates preserved from the original program).
inline CameraPose topDownPose() { return {glm::vec3(0, 10.5f, 0.01f), glm::vec3(0, 0.8f, 0), glm::vec3(0, 0, -1)}; }
inline CameraPose tablePose()   { return {glm::vec3(6.5f, 5.0f, 6.5f), glm::vec3(0, 0.8f, 0), glm::vec3(0, 1, 0)}; }

class Camera {
public:
    explicit Camera(const CameraPose& p, float fovDeg = 48.f) : pose_(p), fov_(fovDeg) {}
    virtual ~Camera() = default;

    virtual void update(float dt) {
        if (!moving_) return;
        t_ += dt;
        float k = std::min(t_ / dur_, 1.f);
        pose_ = lerpPose(from_, to_, smootherstep(k));
        if (k >= 1.f) moving_ = false;
    }
    void setPose(const CameraPose& p) { pose_ = p; moving_ = false; }
    void transitionTo(const CameraPose& p, float seconds = 0.9f) {
        from_ = pose_; to_ = p; dur_ = seconds; t_ = 0.f; moving_ = true;
    }
    const CameraPose& pose() const { return pose_; }
    glm::mat4 view() const { return glm::lookAt(pose_.eye, pose_.target, pose_.up); }
    glm::mat4 projection(float aspect) const { return glm::perspective(glm::radians(fov_), aspect, 0.1f, 100.f); }

protected:
    CameraPose pose_, from_{}, to_{};
    float fov_, dur_ = 1.f, t_ = 0.f;
    bool moving_ = false;
};

// Idle "look-around" camera: slides between points of interest and rests on each.
class CinematicCamera : public Camera {
public:
    CinematicCamera() : Camera(waypoints().front()) {}

    void begin(const CameraPose& from) {
        pose_ = from; a_ = from; idx_ = 0; b_ = waypoints()[0];
        segDur_ = 2.2f; segT_ = 0.f; holding_ = false; clock_ = 0.f; moving_ = false;
    }
    void update(float dt) override {
        clock_ += dt;
        segT_ += dt;
        if (!holding_) {
            float k = std::min(segT_ / segDur_, 1.f);
            pose_ = lerpPose(a_, b_, smootherstep(k));
            if (k >= 1.f) { holding_ = true; segT_ = 0.f; }
        } else {
            // gentle breathing sway that fades in/out so slides start cleanly
            float amp = std::sin(glm::pi<float>() * std::min(segT_ / kHold, 1.f));
            pose_ = b_;
            pose_.eye    += amp * glm::vec3(std::sin(clock_ * .6f) * .12f, std::sin(clock_ * .45f) * .05f, 0.f);
            pose_.target += amp * glm::vec3(std::sin(clock_ * .35f) * .10f, std::cos(clock_ * .5f) * .05f, 0.f);
            if (segT_ >= kHold) {
                a_ = b_;
                idx_ = (idx_ + 1) % waypoints().size();
                b_ = waypoints()[idx_];
                segDur_ = kSlide; segT_ = 0.f; holding_ = false;
            }
        }
    }

private:
    static const std::vector<CameraPose>& waypoints() {
        static const std::vector<CameraPose> w = {
            {glm::vec3(0, 4.8f, 6.2f),      glm::vec3(0, 0.9f, 0),         glm::vec3(0, 1, 0)},  // table overview
            {glm::vec3(-1.0f, 2.2f, -1.5f), glm::vec3(-5.4f, 1.4f, -6.6f), glm::vec3(0, 1, 0)},  // bookshelf
            {glm::vec3(1.0f, 2.8f, -2.5f),  glm::vec3(1.0f, 2.9f, -6.9f),  glm::vec3(0, 1, 0)},  // torn poster
            {glm::vec3(2.0f, 2.6f, -1.5f),  glm::vec3(4.6f, 1.8f, -6.1f),  glm::vec3(0, 1, 0)},  // plant + lamp
            {glm::vec3(-6.2f, 3.0f, 5.5f),  glm::vec3(3.0f, 1.2f, -3.0f),  glm::vec3(0, 1, 0)},  // side sweep
            {glm::vec3(3.5f, 2.4f, 3.5f),   glm::vec3(-1.0f, 1.0f, 0.5f),  glm::vec3(0, 1, 0)},  // table close-up
        };
        return w;
    }
    static constexpr float kSlide = 4.5f, kHold = 1.8f;
    CameraPose a_{}, b_{};
    size_t idx_ = 0;
    float segT_ = 0.f, segDur_ = 2.f, clock_ = 0.f;
    bool holding_ = false;
};

// ----------------------------------------------------------------------------
//  Card animation, Card, Deck
// ----------------------------------------------------------------------------
struct Pose {
    glm::vec3 position{0.f};
    float yaw = 0.f;    // rotation around the vertical axis (degrees)
    float flip = 0.f;   // 0 = face up, 180 = face down
};

class CardAnimation {
public:
    void start(const Pose& from, const Pose& to, float duration, float delay, float arc) {
        from_ = from; to_ = to; duration_ = std::max(duration, 0.01f); delay_ = delay; arc_ = arc;
        elapsed_ = 0.f; active_ = true;
    }
    // Change the destination of an animation that has not begun moving yet.
    void retarget(const Pose& to) { to_ = to; }
    bool active() const { return active_; }
    bool waiting() const { return active_ && delay_ > 0.f; }

    Pose update(float dt) {
        if (!active_) return to_;
        if (delay_ > 0.f) {
            delay_ -= dt;
            if (delay_ > 0.f) return from_;
            dt = -delay_;
            delay_ = 0.f;
        }
        elapsed_ += dt;
        const float t = std::min(elapsed_ / duration_, 1.f), e = smootherstep(t);
        if (t >= 1.f) { active_ = false; return to_; }
        Pose p;
        p.position = glm::mix(from_.position, to_.position, e);
        p.position.y += arc_ * std::sin(glm::pi<float>() * e);   // lift off the table while travelling
        p.yaw = glm::mix(from_.yaw, to_.yaw, e);
        p.flip = glm::mix(from_.flip, to_.flip, e);
        return p;
    }

private:
    Pose from_, to_;
    float duration_ = 1.f, delay_ = 0.f, elapsed_ = 0.f, arc_ = 0.f;
    bool active_ = false;
};

class Card {
public:
    Card(std::string name, std::shared_ptr<PolygonWithTexture> front, std::shared_ptr<PolygonWithTexture> back)
        : name_(std::move(name)), front_(std::move(front)), back_(std::move(back)) {}

    const std::string& name() const { return name_; }
    void setPose(const Pose& p) { pose_ = p; }
    const Pose& pose() const { return pose_; }
    void setSelected(bool s) { selected_ = s; }

    void animateTo(const Pose& target, float duration, float delay = 0.f, float arc = 0.f) {
        if (anim_.waiting()) anim_.retarget(target);   // keep the pending delay (e.g. opponent's 0.5 s)
        else anim_.start(pose_, target, duration, delay, arc);
    }
    void update(float dt) {
        if (anim_.active()) pose_ = anim_.update(dt);
        select_ += ((selected_ ? 1.f : 0.f) - select_) * std::min(1.f, dt * 12.f);   // smooth slide up/down
    }
    void draw(const Shader& s) const {
        glm::mat4 m = glm::translate(glm::mat4(1.f), pose_.position + cfg::kSelectOffset * select_);
        m = glm::rotate(m, glm::radians(pose_.yaw), glm::vec3(0, 1, 0));
        m = glm::rotate(m, glm::radians(-90.f), glm::vec3(1, 0, 0));      // lay the quad flat
        m = glm::rotate(m, glm::radians(pose_.flip), glm::vec3(0, 1, 0)); // flip around the card's long axis
        const glm::vec3 size(cfg::kCardW, cfg::kCardH, 1.f), lift(0, 0, 0.002f);
        const glm::vec3 tint = glm::mix(glm::vec3(1.f), glm::vec3(1.25f, 1.2f, 1.0f), select_);
        front_->draw(s, glm::scale(glm::translate(m, lift), size), tint);
        glm::mat4 b = glm::rotate(m, glm::radians(180.f), glm::vec3(0, 1, 0));
        back_->draw(s, glm::scale(glm::translate(b, lift), size), tint);
    }

private:
    std::string name_;
    std::shared_ptr<PolygonWithTexture> front_, back_;
    Pose pose_;
    CardAnimation anim_;
    float select_ = 0.f;
    bool selected_ = false;
};

class Deck {
public:
    void add(Card c) { cards_.push_back(std::move(c)); }
    void shuffle() { std::shuffle(cards_.begin(), cards_.end(), rng_); restack(); }
    bool empty() const { return cards_.empty(); }
    size_t size() const { return cards_.size(); }
    Card drawTop() {
        Card c = std::move(cards_.back());
        cards_.pop_back();
        return c;   // keeps the pose it had on the stock pile
    }
    void draw(const Shader& s) const { for (const Card& c : cards_) c.draw(s); }

private:
    void restack() {
        for (size_t i = 0; i < cards_.size(); ++i) {
            Pose p;
            p.position = glm::vec3(cfg::kStockX, cfg::kCardY + i * cfg::kLayerStep, 0.f);
            p.yaw = (float((i * 37) % 7) - 3.f) * 0.6f;   // tiny jitter so the pile looks hand-stacked
            p.flip = 180.f;                                // face down
            cards_[i].setPose(p);
        }
    }
    std::vector<Card> cards_;
    std::mt19937 rng_{std::random_device{}()};
};

// ----------------------------------------------------------------------------
//  Player (human or opponent)
// ----------------------------------------------------------------------------
class Player {
public:
    Player(std::string name, bool human, float rowZ, float yaw)
        : name_(std::move(name)), human_(human), z_(rowZ), yaw_(yaw) {}

    bool reserveDraw() { if (draws_ >= cfg::kMaxDraws) return false; ++draws_; return true; }
    int draws() const { return draws_; }
    size_t handSize() const { return hand_.size(); }

    void receive(Card card, float delay) {
        hand_.push_back(std::move(card));
        if (human_ && hand_.size() == 1) selected_ = 0;
        relayout(static_cast<int>(hand_.size()) - 1, delay);
        refreshSelection();
    }
    std::optional<Card> discardSelected() {
        if (!human_ || hand_.empty()) return std::nullopt;
        Card out = std::move(hand_[selected_]);
        hand_.erase(hand_.begin() + selected_);
        selected_ = hand_.empty() ? 0 : std::clamp(selected_, 0, static_cast<int>(hand_.size()) - 1);
        out.setSelected(false);
        relayout(-1, 0.f);
        refreshSelection();
        return out;
    }
    void moveSelection(int delta) {
        if (!human_ || hand_.empty()) return;
        selected_ = std::clamp(selected_ + delta, 0, static_cast<int>(hand_.size()) - 1);
        refreshSelection();
    }
    std::string selectedName() const { return (human_ && !hand_.empty()) ? hand_[selected_].name() : "-"; }

    void update(float dt) { for (Card& c : hand_) c.update(dt); }
    void draw(const Shader& s) const { for (const Card& c : hand_) c.draw(s); }

private:
    Pose slot(int i, int n) const {
        Pose p;
        p.position = glm::vec3((i - (n - 1) * 0.5f) * cfg::kHandSpacing, cfg::kCardY + i * 0.002f, z_);
        p.yaw = yaw_;
        p.flip = human_ ? 0.f : 180.f;   // opponent's cards stay face down
        return p;
    }
    void relayout(int newIndex, float delay) {
        const int n = static_cast<int>(hand_.size());
        for (int i = 0; i < n; ++i) {
            if (i == newIndex) hand_[i].animateTo(slot(i, n), 0.7f, delay, 0.7f);
            else               hand_[i].animateTo(slot(i, n), 0.3f);
        }
    }
    void refreshSelection() {
        for (size_t i = 0; i < hand_.size(); ++i) hand_[i].setSelected(human_ && int(i) == selected_);
    }

    std::string name_;
    bool human_;
    float z_, yaw_;
    int draws_ = 0, selected_ = 0;
    std::vector<Card> hand_;
};

// ----------------------------------------------------------------------------
//  Game
// ----------------------------------------------------------------------------
class Game {
public:
    explicit Game(std::vector<Card> cards)
        : player_("You", true, cfg::kPlayerZ, 0.f), opponent_("Opponent", false, cfg::kOpponentZ, 180.f) {
        for (Card& c : cards) deck_.add(std::move(c));
        deck_.shuffle();
    }

    // One round: the player draws now, the opponent draws 0.5 s later (face down).
    void drawRound() {
        if (deck_.size() < 2 || !player_.reserveDraw()) return;   // nothing happens after the 6th draw
        opponent_.reserveDraw();
        player_.receive(deck_.drawTop(), 0.f);
        opponent_.receive(deck_.drawTop(), cfg::kOpponentDelay);
    }
    void discardSelected() {
        if (auto card = player_.discardSelected()) {
            Pose to;
            to.position = glm::vec3(cfg::kDiscardX, cfg::kCardY + discard_.size() * cfg::kLayerStep, 0.f);
            to.yaw = (float(rng_() % 25) - 12.f);
            to.flip = 0.f;
            card->animateTo(to, 0.6f, 0.f, 0.6f);
            discard_.push_back(std::move(*card));
        }
    }
    void moveSelection(int d) { player_.moveSelection(d); }

    void update(float dt) {
        player_.update(dt);
        opponent_.update(dt);
        for (Card& c : discard_) c.update(dt);
    }
    void draw(const Shader& s) const {
        deck_.draw(s);
        for (const Card& c : discard_) c.draw(s);
        player_.draw(s);
        opponent_.draw(s);
    }
    std::string status() const {
        char buf[256];
        std::snprintf(buf, sizeof buf, "Card Room | stock %zu | you %d/%d | opponent %d/%d | discarded %zu | selected: %s",
                      deck_.size(), player_.draws(), cfg::kMaxDraws, opponent_.draws(), cfg::kMaxDraws,
                      discard_.size(), player_.selectedName().c_str());
        return buf;
    }

private:
    Deck deck_;
    Player player_, opponent_;
    std::vector<Card> discard_;
    std::mt19937 rng_{std::random_device{}()};
};

inline std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

// One texture per character, one shared back texture.
inline std::vector<Card> makeCharacterDeck() {
    static const char* kNames[22] = {"0_theFool", "1_theMagician", "2_theHighPriestess", "3_theEmpress", "4_theEmperor",  "5_theHierophant",
                                     "6_theLovers",  "7_theChariot",  "8_justice", "9_theHermit", "10_wheelOfFortune", "11_strength",
                                     "12_theHangedMan", "13_death",  "14_temperance", "15_devil", "16_theTower", "17_theStar",
                                     "18_theMoon", "19_theSun", "20_judgment", "21_theWorld"};
    auto back = std::make_shared<PolygonWithTexture>(Mesh::quad(), loadTexture("../assets/cards/back.png", false, Procedural::cardBack));
    std::vector<Card> cards;
    for (int i = 0; i < 22; ++i) {
        auto tex = loadTexture("../assets/cards/" + toLower(kNames[i]) + ".png", false, [i] { return Procedural::character(i); });
        cards.emplace_back(kNames[i], std::make_shared<PolygonWithTexture>(Mesh::quad(), tex), back);
    }
    return cards;
}

// ----------------------------------------------------------------------------
//  Room and Table
// ----------------------------------------------------------------------------
class Room {
public:
    explicit Room(const Primitives& p)
        : prim_(p),
          floor_(Mesh::quad(), loadTexture("wood.png", true, Procedural::wood)),
          wall_(Mesh::quad(), loadTexture("wallpaper.png", true, Procedural::wallpaper)),
          poster_(Mesh::quad(), loadTexture("torn_poster.png", false, Procedural::tornPoster)) {}

    void draw(const Shader& s) const {
        const float wallY = cfg::kFloorY + 4.f;            // walls are 8 high and meet the floor
        const glm::vec2 wallUV(14.f / 3.5f, 8.f / 3.5f);
        floor_.draw(s, Transform{glm::vec3(0, cfg::kFloorY, 0), glm::vec3(-90, 0, 0), glm::vec3(14, 14, 1)}.matrix(),
                    glm::vec3(1.f), glm::vec2(4.f));
        const glm::vec3 one(14, 8, 1);
        wall_.draw(s, Transform{glm::vec3(0, wallY, -7), glm::vec3(0), one}.matrix(), glm::vec3(1.f), wallUV);
        wall_.draw(s, Transform{glm::vec3(0, wallY, 7), glm::vec3(0), one}.matrix(), glm::vec3(1.f), wallUV);
        wall_.draw(s, Transform{glm::vec3(7, wallY, 0), glm::vec3(0, 90, 0), one}.matrix(), glm::vec3(1.f), wallUV);
        wall_.draw(s, Transform{glm::vec3(-7, wallY, 0), glm::vec3(0, 90, 0), one}.matrix(), glm::vec3(1.f), wallUV);
        // Torn poster on the back wall (the right side has been ripped away).
        poster_.draw(s, Transform{glm::vec3(1.0f, 2.9f, -6.92f), glm::vec3(0), glm::vec3(2.0f, 3.0f, 1)}.matrix());
        drawBookshelf(s);
        drawFloorLamp(s);
        drawPottedPlant(s);
    }

private:
    // Decor 1: bookshelf against the back wall.
    void drawBookshelf(const Shader& s) const {
        const glm::vec3 dark(.30f, .16f, .09f), light(.68f, .43f, .22f);
        const float cx = -5.4f, cz = -6.6f;
        prim_.drawBox(s, {cx, 1.25f, -6.93f}, {1.5f, 2.5f, 0.04f}, dark * 0.8f);
        prim_.drawBox(s, {cx - .71f, 1.25f, cz}, {.08f, 2.5f, .7f}, dark);
        prim_.drawBox(s, {cx + .71f, 1.25f, cz}, {.08f, 2.5f, .7f}, dark);
        for (float y : {0.04f, 0.85f, 1.65f, 2.46f}) prim_.drawBox(s, {cx, y, cz}, {1.5f, .08f, .7f}, light);
        static const glm::vec3 pal[6] = {{.7f, .15f, .15f}, {.15f, .35f, .6f}, {.2f, .5f, .25f},
                                         {.8f, .65f, .2f},  {.45f, .2f, .5f},  {.85f, .85f, .8f}};
        const float bases[3] = {0.08f, 0.89f, 1.69f};
        for (int bay = 0; bay < 3; ++bay)
            for (int k = 0; k < 7; ++k) {
                float h = .45f + .07f * float((k * 5 + bay * 3) % 4);
                prim_.drawBox(s, {cx - .6f + k * .19f, bases[bay] + h * .5f, cz + .05f}, {.15f, h, .45f}, pal[(k + bay * 2) % 6]);
            }
    }
    // Decor 2: floor lamp.
    void drawFloorLamp(const Shader& s) const {
        const float x = 5.6f, z = -6.2f;
        prim_.drawBox(s, {x, 0.03f, z}, {.5f, .06f, .5f}, {.18f, .17f, .15f});
        prim_.drawBox(s, {x, 1.56f, z}, {.07f, 3.0f, .07f}, {.18f, .17f, .15f});
        prim_.drawBox(s, {x, 3.2f, z}, {.8f, .55f, .8f}, {.95f, .80f, .45f});
        prim_.drawBox(s, {x, 3.2f, z}, {.3f, .75f, .3f}, {1.f, .95f, .75f});
    }
    // Decor 3: potted plant.
    void drawPottedPlant(const Shader& s) const {
        const float x = 3.6f, z = -6.1f;
        prim_.drawBox(s, {x, .275f, z}, {.6f, .55f, .6f}, {.65f, .32f, .2f});
        prim_.drawBox(s, {x, .56f, z}, {.5f, .04f, .5f}, {.15f, .09f, .05f});
        for (int k = 0; k < 9; ++k) {
            const float yawA = glm::radians(k * 40.f), tilt = glm::radians(18.f + (k % 3) * 13.f);
            const float L = .9f + .15f * (k % 3);
            const glm::vec3 dir(std::sin(tilt) * std::cos(yawA), std::cos(tilt), std::sin(tilt) * std::sin(yawA));
            glm::mat4 m = glm::translate(glm::mat4(1.f), glm::vec3(x, .58f, z) + dir * (L * .5f));
            m = glm::rotate(m, -yawA, glm::vec3(0, 1, 0));
            m = glm::rotate(m, -tilt, glm::vec3(0, 0, 1));
            m = glm::scale(m, glm::vec3(.22f, L, .02f));
            prim_.box.draw(s, m, glm::vec3(.12f + .04f * (k % 3), .45f + .06f * (k % 2), .18f));
        }
    }

    const Primitives& prim_;
    PolygonWithTexture floor_, wall_, poster_;
};

class Table {
public:
    explicit Table(const Primitives& p) : prim_(p) {}
    void draw(const Shader& s) const {
        prim_.drawBox(s, {0, .75f, 0}, {7.5f, .35f, 4.8f}, {.28f, .13f, .07f});
        for (float sx : {-1.f, 1.f})
            for (float sz : {-1.f, 1.f})
                prim_.drawBox(s, {sx * 3.4f, .2475f, sz * 2.1f}, {.3f, .655f, .3f}, {.22f, .10f, .05f});
        prim_.drawFlat(s, {0, .94f, 0}, {7.35f, 4.65f}, {.22f, .34f, .25f});                 // felt
        prim_.drawFlat(s, {0, .945f, cfg::kPlayerZ}, {6.4f, 1.6f}, {.12f, .30f, .18f});      // player's zone
        prim_.drawFlat(s, {0, .945f, cfg::kOpponentZ}, {6.4f, 1.6f}, {.25f, .20f, .16f});    // opponent's zone
        prim_.drawFlat(s, {cfg::kStockX, .945f, 0}, {1.05f, 1.45f}, {.10f, .15f, .10f});     // stock slot
        prim_.drawFlat(s, {cfg::kDiscardX, .945f, 0}, {1.05f, 1.45f}, {.16f, .12f, .10f});   // discard slot
    }

private:
    const Primitives& prim_;
};

// ----------------------------------------------------------------------------
//  Application: window, input, main loop
// ----------------------------------------------------------------------------
class Application {
public:
    Application() {
        if (!glfwInit()) throw std::runtime_error("GLFW initialization failed");
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_SAMPLES, 4);
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
        window_ = glfwCreateWindow(cfg::kWindowW, cfg::kWindowH, "Card Room", nullptr, nullptr);
        if (!window_) throw std::runtime_error("Window creation failed");
        glfwMakeContextCurrent(window_);
        glfwSwapInterval(1);
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) throw std::runtime_error("GLAD initialization failed");
        glfwGetFramebufferSize(window_, &fbW_, &fbH_);

        glfwSetWindowUserPointer(window_, this);
        glfwSetKeyCallback(window_, [](GLFWwindow* w, int key, int, int action, int) {
            static_cast<Application*>(glfwGetWindowUserPointer(w))->onKey(key, action);
        });
        glfwSetFramebufferSizeCallback(window_, [](GLFWwindow* w, int fw, int fh) {
            auto* a = static_cast<Application*>(glfwGetWindowUserPointer(w));
            a->fbW_ = fw; a->fbH_ = fh;
            glViewport(0, 0, fw, fh);
        });

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_MULTISAMPLE);

        shader_ = std::make_unique<Shader>(kVertexSrc, kFragmentSrc);
        prims_ = std::make_unique<Primitives>();
        room_ = std::make_unique<Room>(*prims_);
        table_ = std::make_unique<Table>(*prims_);
        game_ = std::make_unique<Game>(makeCharacterDeck());

        std::cout << "\n1/2 views | 0 cinematic | G draw | LEFT/RIGHT select | D discard | ESC quit\n";
    }
    ~Application() {
        // GL objects must die while the context still exists.
        game_.reset(); table_.reset(); room_.reset(); prims_.reset(); shader_.reset();
        if (window_) glfwDestroyWindow(window_);
        glfwTerminate();
    }
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void run() {
        double last = glfwGetTime();
        while (!glfwWindowShouldClose(window_)) {
            const double now = glfwGetTime();
            const float dt = static_cast<float>(std::min(now - last, 0.05));
            last = now;
            glfwPollEvents();
            update(dt);
            render();
            glfwSwapBuffers(window_);
        }
    }

private:
    void onKey(int key, int action) {
        if (action == GLFW_RELEASE) return;
        const bool press = (action == GLFW_PRESS);
        switch (key) {
            case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(window_, GLFW_TRUE); break;
            case GLFW_KEY_1: if (press) setView(1); break;
            case GLFW_KEY_2: if (press) setView(2); break;
            case GLFW_KEY_0: if (press) toggleCinematic(); break;
            case GLFW_KEY_G: if (press) game_->drawRound(); break;
            case GLFW_KEY_D: if (press) game_->discardSelected(); break;
            case GLFW_KEY_LEFT:  game_->moveSelection(-1); break;
            case GLFW_KEY_RIGHT: game_->moveSelection(+1); break;
            default: break;
        }
    }
    void setView(int v) {
        lastFixedView_ = v;
        if (active_ == &cinematic_) { camera_.setPose(cinematic_.pose()); active_ = &camera_; }
        camera_.transitionTo(v == 1 ? topDownPose() : tablePose());
    }
    void toggleCinematic() {
        if (active_ == &cinematic_) { setView(lastFixedView_); return; }
        cinematic_.begin(active_->pose());
        active_ = &cinematic_;
    }
    void update(float dt) {
        active_->update(dt);
        game_->update(dt);
        const std::string t = game_->status();
        if (t != title_) { title_ = t; glfwSetWindowTitle(window_, t.c_str()); }
    }
    void render() {
        glClearColor(0.08f, 0.09f, 0.12f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        const float aspect = fbH_ > 0 ? float(fbW_) / float(fbH_) : 1.f;
        shader_->use();
        shader_->setInt("uTexture", 0);
        shader_->setMat4("uVP", active_->projection(aspect) * active_->view());
        room_->draw(*shader_);
        table_->draw(*shader_);
        game_->draw(*shader_);
    }

    GLFWwindow* window_ = nullptr;
    int fbW_ = cfg::kWindowW, fbH_ = cfg::kWindowH;
    std::unique_ptr<Shader> shader_;
    std::unique_ptr<Primitives> prims_;
    std::unique_ptr<Room> room_;
    std::unique_ptr<Table> table_;
    std::unique_ptr<Game> game_;
    Camera camera_{topDownPose()};
    CinematicCamera cinematic_;
    Camera* active_ = &camera_;
    int lastFixedView_ = 1;
    std::string title_;
};

int main() {
    try {
        Application app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        glfwTerminate();
        return 1;
    }
    return 0;
}