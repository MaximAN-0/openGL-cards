#pragma once
// Procedural textures (used when PNG assets are not present) and the asset loader.
#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "config.h"
#include "texture.h"

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

// Load ASSET_DIR/<rel>; if the file is missing, generate a procedural texture instead.
inline std::shared_ptr<Texture> loadTexture(const std::string& rel, bool repeat,
                                            const std::function<Procedural::Image()>& fallback) {
    if (auto t = Texture::fromFile(std::string(ASSET_DIR) + rel, repeat)) return t;
    std::cout << "[assets] " << rel << " not found - using a procedural texture\n";
    return fallback().upload(repeat);
}
