#pragma once
// The Room: textured floor/walls/poster plus bookshelf, lamp, plant and Decor props.
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <memory>
#include <vector>

#include "config.h"
#include "decor.h"
#include "mesh.h"
#include "polygon.h"
#include "procedural.h"

class Room {
public:
    explicit Room(const Primitives& p)
        : prim_(p),
          floor_(Mesh::quad(), loadTexture("wood.png", true, Procedural::wood)),
          wall_(Mesh::quad(), loadTexture("wallpaper.png", true, Procedural::wallpaper)),
          poster_(Mesh::quad(), loadTornTexture("poster.png", false))   // loaded posters are torn too
        {
        const float f = cfg::kFloorY;
        // Zabutons where the players sit (player in front, opponent across the table).
        extras_.push_back(std::make_unique<Zabuton>(prim_, glm::vec3(0, f, cfg::kSeatZ), glm::vec3(.62f, .10f, .14f)));
        extras_.push_back(std::make_unique<Zabuton>(prim_, glm::vec3(0, f, -cfg::kSeatZ), glm::vec3(.14f, .20f, .45f)));
        // Armor + sword in the back-right corner, turned to face the room.
        const glm::vec3 armorPos(6.2f, f, -6.2f);
        const float yaw = -45.f;
        const glm::vec3 forward(std::sin(glm::radians(yaw)), 0.f, std::cos(glm::radians(yaw)));
        extras_.push_back(std::make_unique<ArmorStand>(prim_, armorPos, yaw));
        extras_.push_back(std::make_unique<Sword>(prim_, armorPos + forward * 0.3f, yaw));
    }

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
        for (const auto& d : extras_) d->draw(s);   // zabutons, armor, sword
    }

private:
    // Decor 1: bookshelf against the back wall.
    void drawBookshelf(const Shader& s) const {
        const glm::vec3 dark(.30f, .16f, .09f), light(.68f, .43f, .22f);
        const float cx = -5.4f, cz = -6.6f;
        prim_.drawBox(s, {cx, 1.25f, -6.93f}, {1.5f, 2.5f, 0.04f}, dark * 0.8f);
        prim_.drawBox(s, {cx - .71f, 1.25f, cz}, {.08f, 2.5f, .7f}, dark);
        prim_.drawBox(s, {cx + .71f, 1.25f, cz}, {.08f, 2.5f, .7f}, dark);
        for (float y : {0.04f, 0.85f, 1.65f, 2.46f}) prim_.drawBox(s, {cx, y, cz}, {1.34f, .08f, .62f}, light);
        static const glm::vec3 pal[6] = {{.7f, .15f, .15f}, {.15f, .35f, .6f}, {.2f, .5f, .25f},
                                         {.8f, .65f, .2f},  {.45f, .2f, .5f},  {.85f, .85f, .8f}};
        const float bases[3] = {0.08f, 0.89f, 1.69f};
        for (int bay = 0; bay < 3; ++bay)
            for (int k = 0; k < 7; ++k) {
                float h = .45f + .07f * float((k * 5 + bay * 3) % 4);
                prim_.drawBox(s, {cx - .6f + k * .19f, bases[bay] + h * .5f, cz + .05f}, {.15f, h, .45f}, pal[(k + bay * 2) % 6]);
            }

        // Closed cookie jar on the top shelf
        const glm::vec3 glass(.65f, .78f, .72f);
        const glm::vec3 lid(.65f, .65f, .62f);
        const float jarX = cx + .42f;
        const float jarZ = cz - .22f;

        prim_.drawBox(s, {jarX, 2.7f, jarZ}, {.28f, .43f, .28f}, glass);    // jar body
        prim_.drawBox(s, {jarX, 2.93f, jarZ}, {.19f, .08f, .19f}, glass);   // jar neck
        prim_.drawBox(s, {jarX, 3.0f, jarZ}, {.24f, .07f, .24f}, lid);      // closed lid
        prim_.drawBox(s, {jarX, 3.05f, jarZ}, {.09f, .04f, .09f}, lid);     // lid knob

        // Cookies visible through the jar
        const glm::vec3 cookie(.72f, 1.1f, .18f);
        prim_.drawBox(s, {jarX - .06f, 2.61f, jarZ + .04f}, {.10f, .08f, .10f}, cookie);
        prim_.drawBox(s, {jarX + .05f, 2.67f, jarZ - .03f}, {.10f, .08f, .10f}, cookie);
    }

    // Decor 2: floor lamp.
    void drawFloorLamp(const Shader& s) const {
        const float x = 4.8f, z = -6.2f;
        prim_.drawBox(s, {x, 0.03f, z}, {.5f, .06f, .5f}, {.18f, .17f, .15f});
        prim_.drawBox(s, {x, 1.56f, z}, {.07f, 3.0f, .07f}, {.18f, .17f, .15f});
        prim_.drawBox(s, {x, 3.2f, z}, {.8f, .55f, .8f}, {.95f, .80f, .45f});
        prim_.drawBox(s, {x, 3.2f, z}, {.3f, .75f, .3f}, {1.f, .95f, .75f});
    }

    // Decor 3: potted plant.
    void drawPottedPlant(const Shader& s) const {
        const float x = 3.6f, z = -6.1f;
        prim_.drawBox(s, {x, .275f, z}, {.6f, .55f, .6f}, {.55f, .25f, .12f});
        prim_.drawBox(s, {x, .56f, z}, {.5f, .04f, .5f}, {.12f, .07f, .035f});
        prim_.drawBox(s, {x, .59f, z}, {.44f, .025f, .44f}, {.18f, .12f, .06f});

        glm::mat4 trunk = glm::translate(glm::mat4(1.f), {x, .95f, z});
        trunk = glm::rotate(trunk, glm::radians(-12.f), {0, 0, 1});
        trunk = glm::scale(trunk, {.13f, .85f, .13f});
        prim_.box.draw(s, trunk, {.30f, .16f, .07f});

        glm::mat4 upper = glm::translate(glm::mat4(1.f), {x + .08f, 1.42f, z});
        upper = glm::rotate(upper, glm::radians(18.f), {0, 0, 1});
        upper = glm::scale(upper, {.09f, .35f, .09f});
        prim_.box.draw(s, upper, {.32f, .18f, .08f});

        auto branch = [&](glm::vec3 pos, glm::vec3 scale, float angle) {
            glm::mat4 m = glm::translate(glm::mat4(1.f), pos);
            m = glm::rotate(m, glm::radians(angle), {0, 0, 1});
            m = glm::scale(m, scale);
            prim_.box.draw(s, m, {.30f, .16f, .07f});
        };
        branch({x - .30f, 1.05f, z}, {.10f, .48f, .10f}, -65.f);
        branch({x + .27f, 1.20f, z}, {.10f, .48f, .10f}, 65.f);
        branch({x + .10f, 1.48f, z}, {.08f, .35f, .08f}, -45.f);
        auto leaves = [&](glm::vec3 pos, glm::vec3 scale) {
            glm::mat4 m = glm::translate(glm::mat4(1.f), pos);
            m = glm::scale(m, scale);
            prim_.box.draw(s, m, {.12f, .38f, .12f});
        };
        leaves({x - .48f, 1.25f, z}, {.42f, .20f, .32f});
        leaves({x - .35f, 1.38f, z}, {.35f, .18f, .30f});
        leaves({x + .45f, 1.40f, z}, {.43f, .20f, .32f});
        leaves({x + .32f, 1.53f, z}, {.34f, .18f, .30f});
        leaves({x + .08f, 1.75f, z}, {.40f, .20f, .32f});
        leaves({x + .05f, 1.88f, z}, {.32f, .16f, .28f});
        leaves({x, 1.48f, z}, {.30f, .18f, .30f});
    }

    const Primitives& prim_;
    PolygonWithTexture floor_, wall_, poster_;
    std::vector<std::unique_ptr<Decor>> extras_;
};
