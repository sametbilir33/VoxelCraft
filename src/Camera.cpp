#include "Camera.hpp"
#include <cmath>
#include <algorithm>

glm::vec3 Camera::forward() const {
    float radPitch = glm::radians(pitch);
    float radYaw = glm::radians(yaw);
    return glm::normalize(glm::vec3(
        std::cos(radPitch) * std::cos(radYaw),
        std::sin(radPitch),
        std::cos(radPitch) * std::sin(radYaw)
    ));
}

glm::mat4 Camera::view() const {
    return glm::lookAt(position, position + forward(), glm::vec3(0.0f, 1.0f, 0.0f));
}

void Camera::mouseMove(float dx, float dy) {
    yaw += dx * sensitivity;
    pitch -= dy * sensitivity;
    pitch = std::clamp(pitch, -89.0f, 89.0f);
}
