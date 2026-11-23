#pragma once
// Game rules/state and the card-deck factory.
#include <algorithm>
#include <cstdio>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "card.h"
#include "config.h"
#include "deck.h"
#include "player.h"
#include "polygon.h"
#include "procedural.h"

class Game {
public:
    explicit Game(std::vector<Card> cards)
        : player_("You", true, cfg::kPlayerZ, 0.f), opponent_("Opponent", false, cfg::kOpponentZ, 180.f) {
        for (Card& c : cards) deck_.add(std::move(c));
        deck_.shuffle();
    }

    // One round: the player draws now, the opponent draws 0.5 s later (face down).
    void drawRound() {
        if (deck_.size() < 2 || !player_.reserveDraw()) return;   // nothing happens after the 6th draw
        opponent_.reserveDraw();
        player_.receive(deck_.drawTop(), 0.f);
        opponent_.receive(deck_.drawTop(), cfg::kOpponentDelay);
    }
    void discardSelected() {
        if (auto card = player_.discardSelected()) {
            Pose to;
            to.position = glm::vec3(cfg::kDiscardX, cfg::kCardY + discard_.size() * cfg::kLayerStep, 0.f);
            to.yaw = (float(rng_() % 25) - 12.f);
            to.flip = 0.f;
            card->animateTo(to, 0.6f, 0.f, 0.6f);
            discard_.push_back(std::move(*card));
        }
    }
    void moveSelection(int d) { player_.moveSelection(d); }
    void setFanView(bool on, const CameraPose& cam) { player_.setFan(on, cam); }

    void update(float dt) {
        player_.update(dt);
        opponent_.update(dt);
        for (Card& c : discard_) c.update(dt);
    }
    void draw(const Shader& s) const {
        deck_.draw(s);
        for (const Card& c : discard_) c.draw(s);
        player_.draw(s);
        opponent_.draw(s);
    }
    std::string status() const {
        char buf[256];
        std::snprintf(buf, sizeof buf, "Card Room | stock %zu | you %d/%d | opponent %d/%d | discarded %zu | selected: %s",
                      deck_.size(), player_.draws(), cfg::kMaxDraws, opponent_.draws(), cfg::kMaxDraws,
                      discard_.size(), player_.selectedName().c_str());
        return buf;
    }

private:
    Deck deck_;
    Player player_, opponent_;
    std::vector<Card> discard_;
    std::mt19937 rng_{std::random_device{}()};
};

inline std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

// One texture per character, one shared back texture.
inline std::vector<Card> makeCharacterDeck() {
    static const char* kNames[22] = {"0_theFool", "1_theMagician", "2_theHighPriestess", "3_theEmpress", "4_theEmperor",  "5_theHierophant",
                                     "6_theLovers",  "7_theChariot",  "8_justice", "9_theHermit", "10_wheelOfFortune", "11_strength",
                                     "12_theHangedMan", "13_death",  "14_temperance", "15_devil", "16_theTower", "17_theStar",
                                     "18_theMoon", "19_theSun", "20_judgement", "21_theWorld"};
    auto back = std::make_shared<PolygonWithTexture>(Mesh::quad(), loadTexture("cards/back.png", false, Procedural::cardBack));
    std::vector<Card> cards;
    for (int i = 0; i < 22; ++i) {
        auto tex = loadTexture("cards/" + toLower(kNames[i]) + ".png", false, [i] { return Procedural::character(i); });
        cards.emplace_back(kNames[i], std::make_shared<PolygonWithTexture>(Mesh::quad(), tex), back);
    }
    return cards;
}
