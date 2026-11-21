#pragma once
// Player (human or opponent): hand layout, selection, first-person fan.
#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "camera.h"
#include "card.h"
#include "config.h"

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
    // Fan layout: hand floats in front of the (first-person) camera instead of lying on the table.
    void setFan(bool on, const CameraPose& cam) {
        if (!human_ || (!on && !fan_)) return;
        fan_ = on;
        if (on) {
            const glm::vec3 d = glm::normalize(cam.target - cam.eye);
            frame_.r = glm::normalize(glm::cross(d, glm::vec3(0, 1, 0)));
            frame_.u = glm::cross(frame_.r, d);
            frame_.n = -d;                                   // card normal points back at the camera
            frame_.pivot = cam.eye + d * cfg::kFanDist - frame_.u * cfg::kFanDrop;
            frame_.yaw = glm::degrees(std::atan2(-d.x, -d.z));
            frame_.tilt = 90.f - glm::degrees(std::asin(-d.y));
        }
        relayout(-1, 0.f, 0.6f);
    }
    std::string selectedName() const { return (human_ && !hand_.empty()) ? hand_[selected_].name() : "-"; }

    void update(float dt) { for (Card& c : hand_) c.update(dt); }
    void draw(const Shader& s) const { for (const Card& c : hand_) c.draw(s); }

private:
    Pose slot(int i, int n) const {
        Pose p;
        if (fan_) {
            // Cards pivot around one point below the view centre; leftmost leans furthest left.
            const float psi = ((n - 1) * 0.5f - i) * cfg::kFanSpread, a = glm::radians(psi);
            const glm::vec3 up = frame_.u * std::cos(a) - frame_.r * std::sin(a);
            p.position = frame_.pivot + up * (cfg::kCardH * cfg::kFanScale * 0.5f) + frame_.n * (i * 0.01f);
            p.yaw = frame_.yaw;
            p.tilt = frame_.tilt;
            p.roll = psi;
            p.scale = cfg::kFanScale;
            p.flip = 0.f;
            p.lift = up * 0.16f + frame_.n * 0.06f;   // a selected card slides out of the fan
            return p;
        }
        p.position = glm::vec3((i - (n - 1) * 0.5f) * cfg::kHandSpacing, cfg::kCardY + i * 0.002f, z_);
        p.yaw = yaw_;
        p.flip = human_ ? 0.f : 180.f;   // opponent's cards stay face down
        return p;
    }
    void relayout(int newIndex, float delay, float duration = 0.3f) {
        const int n = static_cast<int>(hand_.size());
        for (int i = 0; i < n; ++i) {
            if (i == newIndex) hand_[i].animateTo(slot(i, n), 0.7f, delay, 0.7f);
            else               hand_[i].animateTo(slot(i, n), duration);
        }
    }
    void refreshSelection() {
        for (size_t i = 0; i < hand_.size(); ++i) hand_[i].setSelected(human_ && int(i) == selected_);
    }

    std::string name_;
    struct FanFrame {
        glm::vec3 pivot{0.f}, r{1, 0, 0}, u{0, 1, 0}, n{0, 0, 1};   // camera-aligned basis
        float yaw = 0.f, tilt = 0.f;
    };
    bool human_;
    float z_, yaw_;
    int draws_ = 0, selected_ = 0;
    bool fan_ = false;
    FanFrame frame_;
    std::vector<Card> hand_;
};
