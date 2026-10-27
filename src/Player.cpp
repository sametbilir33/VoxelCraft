
#include "Player.hpp"
#include "World.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace {
    constexpr float EPSILON = 0.001f;
    constexpr float MOVEMENT_EPSILON = 0.000001f;

    bool overlaps(
        float minA,
        float maxA,
        float minB,
        float maxB
    ) {
        return maxA > minB + EPSILON &&
               minA < maxB - EPSILON;
    }

    bool overlapsRaw(
        float minA,
        float maxA,
        float minB,
        float maxB
    ) {
        return maxA > minB && minA < maxB;
    }

    glm::vec3 blockCell(int x, int y, int z) {
        return glm::vec3(
            static_cast<float>(x),
            static_cast<float>(y),
            static_cast<float>(z)
        );
    }
}

glm::vec3 Player::eyePosition() const {
    return position + glm::vec3(0.0f, eyeHeight, 0.0f);
}

// Oyuncunun verilen konumda herhangi bir katı hitbox ile
// kesişip kesişmediğini kontrol eder.
// position, oyuncunun ayaklarının bulunduğu noktadır.
bool Player::collides(
    const World& world,
    const glm::vec3& pos
) const {
    const float halfWidth = width * 0.5f;

    const glm::vec3 playerMin{
        pos.x - halfWidth,
        pos.y,
        pos.z - halfWidth
    };

    const glm::vec3 playerMax{
        pos.x + halfWidth,
        pos.y + height,
        pos.z + halfWidth
    };

    const int minX = static_cast<int>(
        std::floor(playerMin.x)
    );
    const int maxX = static_cast<int>(
        std::floor(playerMax.x)
    );

    const int minY = static_cast<int>(
        std::floor(playerMin.y)
    );
    const int maxY = static_cast<int>(
        std::floor(playerMax.y)
    );

    const int minZ = static_cast<int>(
        std::floor(playerMin.z)
    );
    const int maxZ = static_cast<int>(
        std::floor(playerMax.z)
    );

    for (int x = minX; x <= maxX; ++x) {
        for (int y = minY; y <= maxY; ++y) {
            for (int z = minZ; z <= maxZ; ++z) {
                const Block block = world.getBlock(x, y, z);
                const BlockProperties properties =
                    blockProperties(block);

                if (!properties.solid)
                    continue;

                std::array<BlockAABB, 2> hitboxes{};
                const std::size_t count =
                    blockHitboxes(block, hitboxes);

                const glm::vec3 cell = blockCell(x, y, z);

                for (std::size_t i = 0; i < count; ++i) {
                    const glm::vec3 boxMin =
                        cell + hitboxes[i].min;

                    const glm::vec3 boxMax =
                        cell + hitboxes[i].max;

                    if (
                        overlapsRaw(
                            playerMin.x, playerMax.x,
                            boxMin.x, boxMax.x
                        ) &&
                        overlapsRaw(
                            playerMin.y, playerMax.y,
                            boxMin.y, boxMax.y
                        ) &&
                        overlapsRaw(
                            playerMin.z, playerMax.z,
                            boxMin.z, boxMax.z
                        )
                    ) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

// X ekseninde çarpışma kontrolü.
void Player::moveX(World& world, float amount) {
    if (std::abs(amount) < MOVEMENT_EPSILON)
        return;

    const float oldX = position.x;
    const float halfWidth = width * 0.5f;

    position.x += amount;

    const float sweptMinX =
        std::min(oldX, position.x) - halfWidth;

    const float sweptMaxX =
        std::max(oldX, position.x) + halfWidth;

    const int minX = static_cast<int>(
        std::floor(sweptMinX)
    );
    const int maxX = static_cast<int>(
        std::floor(sweptMaxX)
    );

    const int minY = static_cast<int>(
        std::floor(position.y)
    );
    const int maxY = static_cast<int>(
        std::floor(position.y + height)
    );

    const int minZ = static_cast<int>(
        std::floor(position.z - halfWidth)
    );
    const int maxZ = static_cast<int>(
        std::floor(position.z + halfWidth)
    );

    for (int x = minX; x <= maxX; ++x) {
        for (int y = minY; y <= maxY; ++y) {
            for (int z = minZ; z <= maxZ; ++z) {
                const Block block = world.getBlock(x, y, z);
                const BlockProperties properties =
                    blockProperties(block);

                if (!properties.solid)
                    continue;

                std::array<BlockAABB, 2> hitboxes{};
                const std::size_t count =
                    blockHitboxes(block, hitboxes);

                const glm::vec3 cell = blockCell(x, y, z);

                for (std::size_t i = 0; i < count; ++i) {
                    const glm::vec3 boxMin =
                        cell + hitboxes[i].min;

                    const glm::vec3 boxMax =
                        cell + hitboxes[i].max;

                    const bool overlapY = overlaps(
                        position.y,
                        position.y + height,
                        boxMin.y,
                        boxMax.y
                    );

                    const bool overlapZ = overlaps(
                        position.z - halfWidth,
                        position.z + halfWidth,
                        boxMin.z,
                        boxMax.z
                    );

                    if (!overlapY || !overlapZ)
                        continue;

                    if (amount > 0.0f) {
                        const bool crossed =
                            oldX + halfWidth <= boxMin.x + EPSILON &&
                            position.x + halfWidth > boxMin.x;

                        if (crossed) {
                            position.x = std::min(
                                position.x,
                                boxMin.x - halfWidth - EPSILON
                            );

                            velocity.x = 0.0f;
                        }
                    } else {
                        const bool crossed =
                            oldX - halfWidth >= boxMax.x - EPSILON &&
                            position.x - halfWidth < boxMax.x;

                        if (crossed) {
                            position.x = std::max(
                                position.x,
                                boxMax.x + halfWidth + EPSILON
                            );

                            velocity.x = 0.0f;
                        }
                    }
                }
            }
        }
    }
}

// Z ekseninde çarpışma kontrolü.
void Player::moveZ(World& world, float amount) {
    if (std::abs(amount) < MOVEMENT_EPSILON)
        return;

    const float oldZ = position.z;
    const float halfWidth = width * 0.5f;

    position.z += amount;

    const float sweptMinZ =
        std::min(oldZ, position.z) - halfWidth;

    const float sweptMaxZ =
        std::max(oldZ, position.z) + halfWidth;

    const int minX = static_cast<int>(
        std::floor(position.x - halfWidth)
    );
    const int maxX = static_cast<int>(
        std::floor(position.x + halfWidth)
    );

    const int minY = static_cast<int>(
        std::floor(position.y)
    );
    const int maxY = static_cast<int>(
        std::floor(position.y + height)
    );

    const int minZ = static_cast<int>(
        std::floor(sweptMinZ)
    );
    const int maxZ = static_cast<int>(
        std::floor(sweptMaxZ)
    );

    for (int x = minX; x <= maxX; ++x) {
        for (int y = minY; y <= maxY; ++y) {
            for (int z = minZ; z <= maxZ; ++z) {
                const Block block = world.getBlock(x, y, z);
                const BlockProperties properties =
                    blockProperties(block);

                if (!properties.solid)
                    continue;

                std::array<BlockAABB, 2> hitboxes{};
                const std::size_t count =
                    blockHitboxes(block, hitboxes);

                const glm::vec3 cell = blockCell(x, y, z);

                for (std::size_t i = 0; i < count; ++i) {
                    const glm::vec3 boxMin =
                        cell + hitboxes[i].min;

                    const glm::vec3 boxMax =
                        cell + hitboxes[i].max;

                    const bool overlapX = overlaps(
                        position.x - halfWidth,
                        position.x + halfWidth,
                        boxMin.x,
                        boxMax.x
                    );

                    const bool overlapY = overlaps(
                        position.y,
                        position.y + height,
                        boxMin.y,
                        boxMax.y
                    );

                    if (!overlapX || !overlapY)
                        continue;

                    if (amount > 0.0f) {
                        const bool crossed =
                            oldZ + halfWidth <= boxMin.z + EPSILON &&
                            position.z + halfWidth > boxMin.z;

                        if (crossed) {
                            position.z = std::min(
                                position.z,
                                boxMin.z - halfWidth - EPSILON
                            );

                            velocity.z = 0.0f;
                        }
                    } else {
                        const bool crossed =
                            oldZ - halfWidth >= boxMax.z - EPSILON &&
                            position.z - halfWidth < boxMax.z;

                        if (crossed) {
                            position.z = std::max(
                                position.z,
                                boxMax.z + halfWidth + EPSILON
                            );

                            velocity.z = 0.0f;
                        }
                    }
                }
            }
        }
    }
}

// Y ekseninde çarpışma kontrolü.
// Aşağı hareket sırasında bir yüzeye temas edilirse
// oyuncu zeminin üstüne yerleştirilir ve onGround güncellenir.
void Player::moveY(World& world, float amount) {
    if (std::abs(amount) < MOVEMENT_EPSILON)
        return;

    const float oldY = position.y;
    const float halfWidth = width * 0.5f;

    position.y += amount;

    const float sweptMinY =
        std::min(oldY, position.y);

    const float sweptMaxY =
        std::max(oldY + height, position.y + height);

    const int minX = static_cast<int>(
        std::floor(position.x - halfWidth)
    );
    const int maxX = static_cast<int>(
        std::floor(position.x + halfWidth)
    );

    const int minY = static_cast<int>(
        std::floor(sweptMinY)
    );
    const int maxY = static_cast<int>(
        std::floor(sweptMaxY)
    );

    const int minZ = static_cast<int>(
        std::floor(position.z - halfWidth)
    );
    const int maxZ = static_cast<int>(
        std::floor(position.z + halfWidth)
    );

    bool landed = false;

    for (int x = minX; x <= maxX; ++x) {
        for (int y = minY; y <= maxY; ++y) {
            for (int z = minZ; z <= maxZ; ++z) {
                const Block block = world.getBlock(x, y, z);
                const BlockProperties properties =
                    blockProperties(block);

                if (!properties.solid)
                    continue;

                std::array<BlockAABB, 2> hitboxes{};
                const std::size_t count =
                    blockHitboxes(block, hitboxes);

                const glm::vec3 cell = blockCell(x, y, z);

                for (std::size_t i = 0; i < count; ++i) {
                    const glm::vec3 boxMin =
                        cell + hitboxes[i].min;

                    const glm::vec3 boxMax =
                        cell + hitboxes[i].max;

                    const bool overlapX = overlaps(
                        position.x - halfWidth,
                        position.x + halfWidth,
                        boxMin.x,
                        boxMax.x
                    );

                    const bool overlapZ = overlaps(
                        position.z - halfWidth,
                        position.z + halfWidth,
                        boxMin.z,
                        boxMax.z
                    );

                    if (!overlapX || !overlapZ)
                        continue;

                    if (amount > 0.0f) {
                        // Başını tavana çarpma.
                        const bool crossed =
                            oldY + height <= boxMin.y + EPSILON &&
                            position.y + height > boxMin.y;

                        if (crossed) {
                            position.y = std::min(
                                position.y,
                                boxMin.y - height - EPSILON
                            );

                            velocity.y = 0.0f;
                        }
                    } else {
                        // Zemine veya yarım bloğun üstüne iniş.
                        const bool crossed =
                            oldY >= boxMax.y - EPSILON &&
                            position.y < boxMax.y;

                        if (crossed) {
                            position.y = std::max(
                                position.y,
                                boxMax.y + EPSILON
                            );

                            velocity.y = 0.0f;
                            landed = true;
                        }
                    }
                }
            }
        }
    }

    onGround = landed;
}

void Player::update(
    World& world,
    const glm::vec3& wishDirection,
    float dt
) {
    // Büyük kare sürelerinde duvarların içinden geçme riskini azaltır.
    dt = std::clamp(dt, 0.0f, 0.05f);

    if (dt <= 0.0f)
        return;

    // Yatay hareketi ayır ve çapraz hareket hızını sınırla.
    glm::vec3 horizontalWish{
        wishDirection.x,
        0.0f,
        wishDirection.z
    };

    const float wishLength = glm::length(horizontalWish);

    if (wishLength > 1.0f)
        horizontalWish /= wishLength;

    velocity.x = horizontalWish.x * moveSpeed;
    velocity.z = horizontalWish.z * moveSpeed;

    // Yerçekimi.
    velocity.y -= gravity * dt;

    // Eksenleri ayrı çözmek köşe çarpışmalarını kolaylaştırır.
    moveX(world, velocity.x * dt);
    moveZ(world, velocity.z * dt);
    moveY(world, velocity.y * dt);
}

void Player::jump() {
    if (!onGround)
        return;

    velocity.y = jumpSpeed;
    onGround = false;
}