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
        prim_.drawFlat(s, {0, .94f, 0}, {7.35f, 4.65f}, {.22f, .34f, .25f});                 // felt
        prim_.drawFlat(s, {0, .945f, cfg::kPlayerZ}, {6.4f, 1.6f}, {.12f, .30f, .18f});      // player's zone
        prim_.drawFlat(s, {0, .945f, cfg::kOpponentZ}, {6.4f, 1.6f}, {.25f, .20f, .16f});    // opponent's zone
        prim_.drawFlat(s, {cfg::kStockX, .945f, 0}, {1.05f, 1.45f}, {.10f, .15f, .10f});     // stock slot
        prim_.drawFlat(s, {cfg::kDiscardX, .945f, 0}, {1.05f, 1.45f}, {.16f, .12f, .10f});   // discard slot
    }

private:
    const Primitives& prim_;
};
