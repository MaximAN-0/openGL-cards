#pragma once
// Configuration / layout constants shared by the whole project.
#include <algorithm>
#include <glm/glm.hpp>

#ifndef ASSET_DIR
#define ASSET_DIR "assets/"
#endif

namespace cfg {
constexpr int   kWindowW = 1280, kWindowH = 800;
constexpr int   kMaxDraws = 5;
constexpr float kOpponentDelay = 0.5f;
constexpr float kCardW = 0.82f, kCardH = 1.22f;
constexpr float kCardY = 0.965f;          // resting height of a card above the table
constexpr float kLayerStep = 0.012f;      // thickness of one card in a pile
constexpr float kStockX = -2.7f, kDiscardX = 2.7f;
constexpr float kPlayerZ = 1.5f, kOpponentZ = -1.5f;
constexpr float kHandSpacing = 1.0f;
constexpr float kFloorY = -0.08f;
inline const glm::vec3 kSelectOffset(0.0f, 0.28f, -0.35f);  // "slide up" for a selected card
constexpr float kSeatZ = 3.5f;        // zabuton distance from the table centre (both players)
constexpr float kFanDist = 1.5f;      // fan pivot distance in front of the first-person camera
constexpr float kFanDrop = 0.56f;     // how far below the view centre the fan pivot sits
constexpr float kFanScale = 0.45f;    // fanned cards are drawn smaller
constexpr float kFanSpread = 11.0f;   // degrees between neighbouring fanned cards
}  // namespace cfg

inline float smootherstep(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}
