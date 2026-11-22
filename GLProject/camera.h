#pragma once
// Camera poses, the transitioning Camera, and the idle CinematicCamera.
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include "config.h"

struct CameraPose { glm::vec3 eye, target, up; };

inline CameraPose lerpPose(const CameraPose& a, const CameraPose& b, float t) {
    return {glm::mix(a.eye, b.eye, t), glm::mix(a.target, b.target, t), glm::normalize(glm::mix(a.up, b.up, t))};
}

// Preset poses (coordinates preserved from the original program).
inline CameraPose topDownPose() { return {glm::vec3(0, 10.5f, 0.01f), glm::vec3(0, 0.8f, 0), glm::vec3(0, 0, -1)}; }
inline CameraPose tablePose()   { return {glm::vec3(6.5f, 5.0f, 6.5f), glm::vec3(0, 0.8f, 0), glm::vec3(0, 1, 0)}; }
// View 4: view 2 mirrored through the table centre (opposite corner, behind the opponent).
inline CameraPose tableMirrorPose() { return {glm::vec3(-6.5f, 5.0f, -6.5f), glm::vec3(0, 0.8f, 0), glm::vec3(0, 1, 0)}; }
// View 3: seated on the player's zabuton, looking across the table.
inline CameraPose firstPersonPose() { return {glm::vec3(0, 2.0f, 3.9f), glm::vec3(0, 0.9f, 0.5f), glm::vec3(0, 1, 0)}; }

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
            {glm::vec3(2.0f, 2.6f, -1.5f),  glm::vec3(4.2f, 1.8f, -6.1f),  glm::vec3(0, 1, 0)},  // plant + lamp
            {glm::vec3(2.5f, 2.4f, -1.0f),  glm::vec3(6.2f, 1.2f, -6.2f),  glm::vec3(0, 1, 0)},  // armor + sword
            {glm::vec3(3.5f, 2.4f, 1.0f),   glm::vec3(0.0f, 0.2f, -3.5f),  glm::vec3(0, 1, 0)},  // opponent's zabuton
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
