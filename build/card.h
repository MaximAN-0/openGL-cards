#pragma once
// Card pose, pose animation, and the Card itself.
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "config.h"
#include "polygon.h"
#include "shader.h"

struct Pose {
    glm::vec3 position{0.f};
    float yaw = 0.f;    // rotation around the vertical axis (degrees)
    float flip = 0.f;   // 0 = face up, 180 = face down
    float tilt = 0.f;   // 0 = lying flat on the table, 90 = standing upright facing +Z
    float roll = 0.f;   // in-plane rotation of the card (used for the fan)
    float scale = 1.f;  // uniform card scale
    glm::vec3 lift = cfg::kSelectOffset;   // offset applied when the card is selected
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
        p.tilt = glm::mix(from_.tilt, to_.tilt, e);
        p.roll = glm::mix(from_.roll, to_.roll, e);
        p.scale = glm::mix(from_.scale, to_.scale, e);
        p.lift = glm::mix(from_.lift, to_.lift, e);
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
        glm::mat4 m = glm::translate(glm::mat4(1.f), pose_.position + pose_.lift * select_);
        m = glm::rotate(m, glm::radians(pose_.yaw), glm::vec3(0, 1, 0));
        m = glm::rotate(m, glm::radians(-90.f + pose_.tilt), glm::vec3(1, 0, 0)); // flat on the table -> upright
        m = glm::rotate(m, glm::radians(pose_.roll), glm::vec3(0, 0, 1));         // fan rotation
        m = glm::rotate(m, glm::radians(pose_.flip), glm::vec3(0, 1, 0));         // flip around the card's long axis
        const glm::vec3 size(cfg::kCardW * pose_.scale, cfg::kCardH * pose_.scale, 1.f), lift(0, 0, 0.002f);
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
