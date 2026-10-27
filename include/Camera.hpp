#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    glm::vec3 position{48.0f, 30.0f, 48.0f};

    float yaw = -90.0f;
    float pitch = -18.0f;

    float sensitivity = 0.10f;

    glm::vec3 forward() const;
    glm::mat4 view() const;

    void mouseMove(float dx, float dy);
};