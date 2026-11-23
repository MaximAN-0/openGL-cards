#pragma once
// The stock pile.
#include <algorithm>
#include <random>
#include <utility>
#include <vector>

#include "card.h"
#include "config.h"

class Deck {
public:
    void add(Card c) { cards_.push_back(std::move(c)); }
    void shuffle() { std::shuffle(cards_.begin(), cards_.end(), rng_); restack(); }
    bool empty() const { return cards_.empty(); }
    size_t size() const { return cards_.size(); }
    Card drawTop() {
        Card c = std::move(cards_.back());
        cards_.pop_back();
        return c;   // keeps the pose it had on the stock pile
    }
    void draw(const Shader& s) const { for (const Card& c : cards_) c.draw(s); }

private:
    void restack() {
        for (size_t i = 0; i < cards_.size(); ++i) {
            Pose p;
            p.position = glm::vec3(cfg::kStockX, cfg::kCardY + i * cfg::kLayerStep, 0.f);
            p.yaw = (float((i * 37) % 7) - 3.f) * 0.6f;   // tiny jitter so the pile looks hand-stacked
            p.flip = 180.f;                                // face down
            cards_[i].setPose(p);
        }
    }
    std::vector<Card> cards_;
    std::mt19937 rng_{std::random_device{}()};
};
