#pragma once

#include <glm/glm.hpp>

class World;

class Player {
public:
    glm::vec3 position{48.0f, 32.0f, 48.0f};
    glm::vec3 velocity{0.0f};

    float width = 0.6f;
    float height = 1.8f;
    float eyeHeight = 1.62f;

    float moveSpeed = 5.0f;
    float jumpSpeed = 7.0f;
    float gravity = 22.0f;

    bool onGround = false;

    glm::vec3 eyePosition() const;

    void update(World& world, const glm::vec3& wishDirection, float dt);
    void jump();

private:
    bool collides(const World& world, const glm::vec3& pos) const;

    void moveX(World& world, float amount);
    void moveY(World& world, float amount);
    void moveZ(World& world, float amount);
};