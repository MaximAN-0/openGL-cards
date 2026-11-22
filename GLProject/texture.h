#pragma once
// Texture (RAII, move-only; shared through std::shared_ptr).
// Note: STB_IMAGE_IMPLEMENTATION is defined once, in stb_impl.cpp.
#include <glad/glad.h>
#include <stb_image.h>

#include <memory>
#include <string>

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
