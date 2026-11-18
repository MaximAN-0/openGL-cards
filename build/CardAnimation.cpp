
#pragma once
#include <glm/glm.hpp>
#include <algorithm>

class CardAnimation
{
private:
    glm::vec3 start;
    glm::vec3 end;
    glm::vec3 current;

    float elapsed = 0.0f;
    float duration = 0.7f;

    bool active = false;

public:
    void startAnimation(
        glm::vec3 from,
        glm::vec3 to,
        float time = 0.7f)
    {
        start = from;
        end = to;
        current = from;

        elapsed = 0.0f;
        duration = time;
        active = true;
    }

    void update(float deltaTime)
    {
        if (!active)
            return;

        elapsed += deltaTime;

        float t = std::clamp(elapsed / duration, 0.0f, 1.0f);

        // Smooth ease-out
        float smooth = 1.0f - pow(1.0f - t, 3.0f);

        current = glm::mix(start, end, smooth);

        if (t >= 1.0f)
            active = false;
    }

    glm::vec3 getPosition() const
    {
        return current;
    }

    bool isActive() const
    {
        return active;
    }
};
