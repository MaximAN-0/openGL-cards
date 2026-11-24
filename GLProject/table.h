#pragma once
// The card table.
#include "config.h"
#include "polygon.h"
#include "shader.h"

class Table {
public:
    explicit Table(const Primitives& p) : prim_(p) {}
    void draw(const Shader& s) const {
        prim_.drawBox(s, {0, .75f, 0}, {7.5f, .35f, 4.8f}, {.28f, .13f, .07f});
        for (float sx : {-1.f, 1.f})
            for (float sz : {-1.f, 1.f})
                prim_.drawBox(s, {sx * 3.4f, .2475f, sz * 2.1f}, {.3f, .655f, .3f}, {.22f, .10f, .05f});
        drawRim(s);
        prim_.drawFlat(s, {0, .94f, 0}, {7.2f, 4.5f}, {.22f, .34f, .25f});                    // felt

        // Both players get an identical zone with a gold border; the piles sit between them, each with its own colour.
        const glm::vec3 gold(.75f, .62f, .28f), zone(.15f, .27f, .19f);
        for (float z : {cfg::kPlayerZ, cfg::kOpponentZ}) {
            prim_.drawFlat(s, {0, .945f, z}, {6.4f, 1.6f}, zone);
            frame(s, {0, z}, {6.4f, 1.6f}, .035f, gold);
        }
        prim_.drawFlat(s, {cfg::kStockX, .945f, 0}, {1.05f, 1.45f}, {.10f, .15f, .12f});     // stock slot
        frame(s, {cfg::kStockX, 0}, {1.05f, 1.45f}, .03f, {.35f, .50f, .80f});
        prim_.drawFlat(s, {cfg::kDiscardX, .945f, 0}, {1.05f, 1.45f}, {.16f, .11f, .11f});   // discard slot
        frame(s, {cfg::kDiscardX, 0}, {1.05f, 1.45f}, .03f, {.75f, .30f, .30f});
        prim_.drawFlat(s, {0, .95f, 0}, {3.4f, .03f}, gold * .7f);                           // centre divider between the piles
    }

private:
    // Raised wooden rim around the felt.
    void drawRim(const Shader& s) const {
        const glm::vec3 rim(.20f, .09f, .045f);
        for (float sz : {-1.f, 1.f}) prim_.drawBox(s, {0, .955f, sz * 2.325f}, {7.5f, .06f, .15f}, rim);
        for (float sx : {-1.f, 1.f}) prim_.drawBox(s, {sx * 3.675f, .955f, 0}, {.15f, .06f, 4.5f}, rim);
    }
    // Thin outline just inside a rectangle on the felt.
    void frame(const Shader& s, const glm::vec2& c, const glm::vec2& size, float t, const glm::vec3& col) const {
        const float y = .95f;
        for (float sz : {-1.f, 1.f}) prim_.drawFlat(s, {c.x, y, c.y + sz * (size.y - t) * .5f}, {size.x, t}, col);
        for (float sx : {-1.f, 1.f}) prim_.drawFlat(s, {c.x + sx * (size.x - t) * .5f, y, c.y}, {t, size.y - 2 * t}, col);
    }
    const Primitives& prim_;
};
