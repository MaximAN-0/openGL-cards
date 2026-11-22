#pragma once
// Room furniture built from the shared primitives: Decor base, Zabuton, ArmorStand, Sword.
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "polygon.h"
#include "shader.h"

// Anything in the room that draws itself from the shared primitives.
class Decor {
public:
    explicit Decor(const Primitives& p) : p_(p) {}
    virtual ~Decor() = default;
    virtual void draw(const Shader& s) const = 0;

protected:
    const Primitives& p_;
};

// Flat floor cushion for a seated player.
class Zabuton : public Decor {
public:
    Zabuton(const Primitives& p, const glm::vec3& floorPos, const glm::vec3& color)
        : Decor(p), pos_(floorPos), color_(color) {}
    void draw(const Shader& s) const override {
        const glm::vec3 base = pos_ + glm::vec3(0, .06f, 0), gold(.85f, .68f, .25f);
        p_.drawBox(s, base, {1.1f, .12f, 1.1f}, color_);                                  // body
        p_.drawBox(s, base + glm::vec3(0, .065f, 0), {.94f, .03f, .94f}, color_ * 1.3f);  // padded top
        p_.drawBox(s, base + glm::vec3(0, .09f, 0), {.1f, .03f, .1f}, gold);              // centre button
        for (float sx : {-1.f, 1.f})
            for (float sz : {-1.f, 1.f})
                p_.drawBox(s, base + glm::vec3(sx * .55f, 0, sz * .55f), {.08f, .1f, .08f}, gold);  // corner tassels
    }

private:
    glm::vec3 pos_, color_;
};

// Medieval suit of armor on a plinth, hands resting on a sword pommel.
class ArmorStand : public Decor {
public:
    ArmorStand(const Primitives& p, const glm::vec3& floorPos, float yawDeg) : Decor(p), pos_(floorPos), yaw_(yawDeg) {}
    void draw(const Shader& s) const override {
        const glm::mat4 m = glm::rotate(glm::translate(glm::mat4(1.f), pos_), glm::radians(yaw_), glm::vec3(0, 1, 0));
        const glm::vec3 steel(.62f, .64f, .68f), dark(.42f, .44f, .50f), shine(.80f, .82f, .86f);
        const glm::vec3 wood(.25f, .14f, .08f), plume(.70f, .10f, .10f), slit(.05f, .05f, .06f);
        auto part = [&](const glm::vec3& c, const glm::vec3& size, const glm::vec3& col) { p_.drawBoxLocal(s, m, c, size, col); };

        part({0, .05f, 0}, {1.0f, .1f, 1.0f}, wood);                          // plinth
        for (float sx : {-1.f, 1.f}) {
            part({sx * .13f, .15f, .05f}, {.2f, .1f, .34f}, dark);            // sabatons
            part({sx * .13f, .45f, 0}, {.14f, .5f, .14f}, steel);             // greaves
            part({sx * .13f, .73f, .02f}, {.18f, .12f, .18f}, shine);         // knee guards
            part({sx * .13f, .98f, 0}, {.16f, .4f, .16f}, steel);             // cuisses
            part({sx * .36f, 1.82f, 0}, {.24f, .14f, .28f}, shine);           // pauldrons
            p_.drawLimb(s, m, {sx * .34f, 1.75f, 0}, {sx * .38f, 1.45f, .12f}, .11f, steel);   // upper arms
            p_.drawLimb(s, m, {sx * .38f, 1.45f, .12f}, {sx * .04f, 1.5f, .30f}, .09f, dark);  // forearms -> grip
        }
        part({0, 1.26f, .01f}, {.5f, .2f, .32f}, dark);                       // tassets
        part({0, 1.6f, 0}, {.5f, .5f, .3f}, steel);                           // breastplate
        part({0, 1.6f, .16f}, {.06f, .42f, .04f}, shine);                     // chest ridge
        part({0, 1.92f, 0}, {.26f, .1f, .26f}, dark);                         // gorget
        part({0, 2.1f, 0}, {.26f, .3f, .28f}, steel);                         // helmet
        part({0, 2.1f, .145f}, {.2f, .035f, .02f}, slit);                     // visor slit
        part({0, 2.33f, 0}, {.04f, .2f, .3f}, plume);                         // crest
    }

private:
    glm::vec3 pos_;
    float yaw_;
};

// Broadsword standing point-down (placed in front of the armor's hands).
class Sword : public Decor {
public:
    Sword(const Primitives& p, const glm::vec3& floorPos, float yawDeg) : Decor(p), pos_(floorPos), yaw_(yawDeg) {}
    void draw(const Shader& s) const override {
        const glm::mat4 m = glm::rotate(glm::translate(glm::mat4(1.f), pos_), glm::radians(yaw_), glm::vec3(0, 1, 0));
        const glm::vec3 gold(.80f, .65f, .20f);
        p_.drawBoxLocal(s, m, {0, .74f, 0}, {.09f, 1.28f, .02f}, {.85f, .87f, .90f});     // blade
        p_.drawBoxLocal(s, m, {0, .80f, .012f}, {.025f, .90f, .005f}, {.60f, .62f, .66f}); // fuller groove
        p_.drawBoxLocal(s, m, {0, 1.38f, 0}, {.44f, .05f, .07f}, gold);                   // crossguard
        p_.drawBoxLocal(s, m, {0, 1.52f, 0}, {.05f, .22f, .05f}, {.30f, .16f, .09f});     // leather grip
        p_.drawBoxLocal(s, m, {0, 1.66f, 0}, {.10f, .08f, .10f}, gold);                   // pommel
    }

private:
    glm::vec3 pos_;
    float yaw_;
};
