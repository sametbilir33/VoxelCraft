
#include "World.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

namespace {

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float smoothstep(float t) {
    return t * t * (3.0f - 2.0f * t);
}

}

World::World() {
    std::random_device rd;

    seed =
        (static_cast<std::uint32_t>(rd()) << 16) ^
        static_cast<std::uint32_t>(rd());

    std::cout << "World seed: " << seed << '\n';

    generateWorld();
}

std::uint32_t World::hash2D(int x, int z) const {
    std::uint32_t h = seed;

    h ^= static_cast<std::uint32_t>(x) + 0x9e3779b9u +
         (h << 6) + (h >> 2);

    h ^= static_cast<std::uint32_t>(z) + 0x9e3779b9u +
         (h << 6) + (h >> 2);

    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;

    return h;
}

float World::random01(int x, int z) const {
    return static_cast<float>(hash2D(x, z)) /
           static_cast<float>(UINT32_MAX);
}

float World::noise2D(float x, float z) const {
    const int x0 = static_cast<int>(std::floor(x));
    const int z0 = static_cast<int>(std::floor(z));

    const int x1 = x0 + 1;
    const int z1 = z0 + 1;

    const float fx = x - static_cast<float>(x0);
    const float fz = z - static_cast<float>(z0);

    const float sx = smoothstep(fx);
    const float sz = smoothstep(fz);

    const float n00 = random01(x0, z0);
    const float n10 = random01(x1, z0);
    const float n01 = random01(x0, z1);
    const float n11 = random01(x1, z1);

    const float nx0 = lerp(n00, n10, sx);
    const float nx1 = lerp(n01, n11, sx);

    return lerp(nx0, nx1, sz);
}

float World::terrainHeight(int x, int z) const {
    float value = 0.0f;
    float amplitude = 1.0f;
    float frequency = 0.015f;
    float amplitudeSum = 0.0f;

    for (int octave = 0; octave < 5; ++octave) {
        value += noise2D(
            static_cast<float>(x) * frequency,
            static_cast<float>(z) * frequency
        ) * amplitude;

        amplitudeSum += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }

    value /= amplitudeSum;

    const float hills = noise2D(
        static_cast<float>(x) * 0.006f,
        static_cast<float>(z) * 0.006f
    );

    const float height =
        14.0f +
        value * 22.0f +
        hills * 8.0f;

    return std::clamp(
        height,
        3.0f,
        static_cast<float>(CHUNK_HEIGHT - 8)
    );
}

Block World::getBlock(int x, int y, int z) const {
    if (y < 0 || y >= CHUNK_HEIGHT) {
        return Block::Air;
    }

    const int cx = static_cast<int>(
        std::floor(static_cast<float>(x) / CHUNK_SIZE)
    );

    const int cz = static_cast<int>(
        std::floor(static_cast<float>(z) / CHUNK_SIZE)
    );

    const auto it = chunks.find({cx, cz});

    if (it == chunks.end()) {
        return Block::Air;
    }

    const int lx = x - cx * CHUNK_SIZE;
    const int lz = z - cz * CHUNK_SIZE;

    return it->second.blocks[lx][y][lz];
}

void World::setBlock(int x, int y, int z, Block block) {
    if (y < 0 || y >= CHUNK_HEIGHT) {
        return;
    }

    const int cx = static_cast<int>(
        std::floor(static_cast<float>(x) / CHUNK_SIZE)
    );

    const int cz = static_cast<int>(
        std::floor(static_cast<float>(z) / CHUNK_SIZE)
    );

    auto it = chunks.find({cx, cz});

    if (it == chunks.end()) {
        return;
    }

    const int lx = x - cx * CHUNK_SIZE;
    const int lz = z - cz * CHUNK_SIZE;

    it->second.blocks[lx][y][lz] = block;
    it->second.modified = true;

    // Chunk sınırlarında komşu mesh'lerini de güncelle.
    if (lx == 0) {
        auto n = chunks.find({cx - 1, cz});
        if (n != chunks.end()) {
            n->second.modified = true;
        }
    }

    if (lx == CHUNK_SIZE - 1) {
        auto n = chunks.find({cx + 1, cz});
        if (n != chunks.end()) {
            n->second.modified = true;
        }
    }

    if (lz == 0) {
        auto n = chunks.find({cx, cz - 1});
        if (n != chunks.end()) {
            n->second.modified = true;
        }
    }

    if (lz == CHUNK_SIZE - 1) {
        auto n = chunks.find({cx, cz + 1});
        if (n != chunks.end()) {
            n->second.modified = true;
        }
    }
}

void World::generateTree(int x, int y, int z) {
    const int trunkHeight =
        4 + static_cast<int>(random01(x + 91, z + 37) * 3.0f);

    if (y + trunkHeight + 3 >= CHUNK_HEIGHT) {
        return;
    }

    // Gövde.
    for (int ty = 0; ty < trunkHeight; ++ty) {
        if (getBlock(x, y + ty, z) == Block::Air) {
            setBlock(x, y + ty, z, Block::Wood);
        }
    }

    const int crownBottom = y + trunkHeight - 2;
    const int crownTop = y + trunkHeight + 1;

    for (int ty = crownBottom; ty <= crownTop; ++ty) {
        const int radius = (ty == crownTop) ? 1 : 2;

        for (int ox = -radius; ox <= radius; ++ox) {
            for (int oz = -radius; oz <= radius; ++oz) {
                if (
                    std::abs(ox) == radius &&
                    std::abs(oz) == radius &&
                    random01(
                        x + ox * 17 + ty * 31,
                        z + oz * 13 + ty * 19
                    ) < 0.45f
                ) {
                    continue;
                }

                if (getBlock(x + ox, ty, z + oz) == Block::Air) {
                    setBlock(x + ox, ty, z + oz, Block::Leaves);
                }
            }
        }
    }
}

void World::generateWorld() {
    const int radius = 3;

    // Önce bütün chunk'ları oluştur.
    for (int cx = -radius; cx <= radius; ++cx) {
        for (int cz = -radius; cz <= radius; ++cz) {
            chunks.emplace(
                std::make_pair(cx, cz),
                Chunk(cx, cz)
            );
        }
    }

    // Arazi üretimi.
    for (auto& pair : chunks) {
        Chunk& chunk = pair.second;

        const int wxStart = chunk.chunkX * CHUNK_SIZE;
        const int wzStart = chunk.chunkZ * CHUNK_SIZE;

        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                const int wx = wxStart + x;
                const int wz = wzStart + z;

                const int h = static_cast<int>(terrainHeight(wx, wz));

                for (int y = 0; y <= h; ++y) {
                    Block block = Block::Stone;

                    if (y == h) {
                        block = Block::Grass;
                    } else if (y >= h - 3) {
                        block = Block::Dirt;
                    }

                    chunk.blocks[x][y][z] = block;
                }
            }
        }
    }

    // Ağaçları ayrı aşamada üret.
    constexpr int TREE_GRID = 9;

    const int worldMin = -radius * CHUNK_SIZE;
    const int worldMax = (radius + 1) * CHUNK_SIZE - 1;

    const int cellMin =
        static_cast<int>(
            std::floor(static_cast<float>(worldMin) / TREE_GRID)
        ) - 1;

    const int cellMax =
        static_cast<int>(
            std::floor(static_cast<float>(worldMax) / TREE_GRID)
        ) + 1;

    std::vector<glm::ivec2> treePositions;

    for (int gz = cellMin; gz <= cellMax; ++gz) {
        for (int gx = cellMin; gx <= cellMax; ++gx) {
            // Yaklaşık %32 ağaç adayı.
            if (random01(gx * 31, gz * 73) > 0.32f) {
                continue;
            }

            const float rx = random01(gx * 97 + 11, gz * 53 + 7);
            const float rz = random01(gx * 43 + 19, gz * 89 + 3);

            const int x = gx * TREE_GRID +
                          static_cast<int>(rx * TREE_GRID);

            const int z = gz * TREE_GRID +
                          static_cast<int>(rz * TREE_GRID);

            if (
                x < worldMin + 3 || x > worldMax - 3 ||
                z < worldMin + 3 || z > worldMax - 3
            ) {
                continue;
            }

            bool tooClose = false;

            for (const auto& existing : treePositions) {
                const int dx = x - existing.x;
                const int dz = z - existing.y;

                if (dx * dx + dz * dz < 25) {
                    tooClose = true;
                    break;
                }
            }

            if (tooClose) {
                continue;
            }

            const int groundY = static_cast<int>(terrainHeight(x, z));

            if (getBlock(x, groundY, z) != Block::Grass) {
                continue;
            }

            treePositions.emplace_back(x, z);
            generateTree(x, groundY + 1, z);
        }
    }

    // Bütün mesh'leri en son üret.
    for (auto& pair : chunks) {
        pair.second.modified = true;
        pair.second.updateMesh(*this);
    }

    std::cout
        << "World generated. Trees: "
        << treePositions.size()
        << '\n';
}

void World::render() {
    for (auto& pair : chunks) {
        if (pair.second.modified) {
            pair.second.updateMesh(*this);
        }

        pair.second.render();
    }
}

bool World::raycast(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxDistance,
    glm::ivec3& hitBlock,
    glm::ivec3& normal
) {
    if (
        maxDistance <= 0.0f ||
        glm::length(direction) < 0.0001f
    ) {
        return false;
    }

    const glm::vec3 dir = glm::normalize(direction);

    // Ray ile AABB kesişimi.
    auto rayBox = [](
        const glm::vec3& rayOrigin,
        const glm::vec3& rayDir,
        const glm::vec3& boxMin,
        const glm::vec3& boxMax,
        float maxT,
        float& hitT,
        glm::ivec3& hitNormal
    ) -> bool {
        float tNear = 0.0f;
        float tFar = maxT;
        glm::ivec3 nearNormal(0);

        for (int axis = 0; axis < 3; ++axis) {
            const float o = rayOrigin[axis];
            const float d = rayDir[axis];
            const float mn = boxMin[axis];
            const float mx = boxMax[axis];

            if (std::abs(d) < 0.000001f) {
                if (o < mn || o > mx) {
                    return false;
                }

                continue;
            }

            float t1 = (mn - o) / d;
            float t2 = (mx - o) / d;

            int sign = -1;

            if (t1 > t2) {
                std::swap(t1, t2);
                sign = 1;
            }

            if (t1 > tNear) {
                tNear = t1;
                nearNormal = glm::ivec3(0);
                nearNormal[axis] = sign;
            }

            tFar = std::min(tFar, t2);

            if (tNear > tFar) {
                return false;
            }
        }

        if (tFar < 0.0f || tNear > maxT) {
            return false;
        }

        hitT = std::max(0.0f, tNear);
        hitNormal = nearNormal;

        // Ray kutunun içinden başlıyorsa çıkış yönüne göre normal üret.
        if (hitNormal == glm::ivec3(0)) {
            int axis = 0;

            if (std::abs(rayDir.y) > std::abs(rayDir.x)) {
                axis = 1;
            }

            if (std::abs(rayDir.z) > std::abs(rayDir[axis])) {
                axis = 2;
            }

            hitNormal[axis] = rayDir[axis] > 0.0f ? -1 : 1;
        }

        return true;
    };

    // Ray ile üçgen kesişimi. Cross biçimli bitkilerde kullanılır.
    auto rayTriangle = [](
        const glm::vec3& rayOrigin,
        const glm::vec3& rayDir,
        const glm::vec3& a,
        const glm::vec3& b,
        const glm::vec3& c,
        float maxT,
        float& hitT,
        glm::ivec3& hitNormal
    ) -> bool {
        constexpr float EPS = 0.000001f;

        const glm::vec3 edge1 = b - a;
        const glm::vec3 edge2 = c - a;
        const glm::vec3 p = glm::cross(rayDir, edge2);
        const float determinant = glm::dot(edge1, p);

        if (std::abs(determinant) < EPS) {
            return false;
        }

        const float inverseDeterminant = 1.0f / determinant;
        const glm::vec3 tvec = rayOrigin - a;

        const float u =
            glm::dot(tvec, p) * inverseDeterminant;

        if (u < 0.0f || u > 1.0f) {
            return false;
        }

        const glm::vec3 q = glm::cross(tvec, edge1);

        const float v =
            glm::dot(rayDir, q) * inverseDeterminant;

        if (v < 0.0f || u + v > 1.0f) {
            return false;
        }

        const float t =
            glm::dot(edge2, q) * inverseDeterminant;

        if (t < 0.0f || t > maxT) {
            return false;
        }

        hitT = t;

        const glm::vec3 n = glm::normalize(
            glm::cross(edge1, edge2)
        );

        // Bitki yüzeyinin normalini baskın eksene yuvarla.
        const glm::vec3 absN = glm::abs(n);

        if (absN.x >= absN.y && absN.x >= absN.z) {
            hitNormal = glm::ivec3(
                n.x >= 0.0f ? 1 : -1, 0, 0
            );
        } else if (absN.y >= absN.x && absN.y >= absN.z) {
            hitNormal = glm::ivec3(
                0, n.y >= 0.0f ? 1 : -1, 0
            );
        } else {
            hitNormal = glm::ivec3(
                0, 0, n.z >= 0.0f ? 1 : -1
            );
        }

        return true;
    };

    const glm::vec3 end = origin + dir * maxDistance;

    const int minX = static_cast<int>(
        std::floor(std::min(origin.x, end.x))
    ) - 1;

    const int maxX = static_cast<int>(
        std::floor(std::max(origin.x, end.x))
    ) + 1;

    const int minY = std::max(
        0,
        static_cast<int>(
            std::floor(std::min(origin.y, end.y))
        ) - 1
    );

    const int maxY = std::min(
        CHUNK_HEIGHT - 1,
        static_cast<int>(
            std::floor(std::max(origin.y, end.y))
        ) + 1
    );

    const int minZ = static_cast<int>(
        std::floor(std::min(origin.z, end.z))
    ) - 1;

    const int maxZ = static_cast<int>(
        std::floor(std::max(origin.z, end.z))
    ) + 1;

    float closestT = maxDistance + 1.0f;
    bool found = false;

    // En yakın isabetin normalini burada sakla.
    glm::ivec3 closestNormal(0);

    for (int x = minX; x <= maxX; ++x) {
        for (int y = minY; y <= maxY; ++y) {
            for (int z = minZ; z <= maxZ; ++z) {
                const Block block = getBlock(x, y, z);

                if (block == Block::Air) {
                    continue;
                }

                const BlockProperties properties =
                    blockProperties(block);

                if (!properties.selectable) {
                    continue;
                }

                const glm::vec3 cellOrigin(
                    static_cast<float>(x),
                    static_cast<float>(y),
                    static_cast<float>(z)
                );

                if (properties.shape == BlockShape::Cross) {
                    constexpr float inset = 0.12f;
                    constexpr float top = 0.88f;

                    const float x0 = x + inset;
                    const float x1 = x + 1.0f - inset;
                    const float y0 = static_cast<float>(y);
                    const float y1 = y + top;
                    const float z0 = z + inset;
                    const float z1 = z + 1.0f - inset;

                    const glm::vec3 a0(x0, y0, z0);
                    const glm::vec3 a1(x1, y0, z1);
                    const glm::vec3 a2(x1, y1, z1);
                    const glm::vec3 a3(x0, y1, z0);

                    const glm::vec3 b0(x1, y0, z0);
                    const glm::vec3 b1(x0, y0, z1);
                    const glm::vec3 b2(x0, y1, z1);
                    const glm::vec3 b3(x1, y1, z0);

                    const glm::vec3 triangles[4][3] = {
                        {a0, a1, a2},
                        {a0, a2, a3},
                        {b0, b1, b2},
                        {b0, b2, b3}
                    };

                    for (const auto& tri : triangles) {
                        float t = 0.0f;
                        glm::ivec3 n(0);

                        if (rayTriangle(
                                origin,
                                dir,
                                tri[0],
                                tri[1],
                                tri[2],
                                std::min(maxDistance, closestT),
                                t,
                                n
                            ) && t < closestT) {
                            closestT = t;
                            hitBlock = glm::ivec3(x, y, z);
                            closestNormal = n;
                            found = true;
                        }
                    }

                    continue;
                }

                std::array<BlockAABB, 2> hitboxes{};
                std::size_t count = blockHitboxes(block, hitboxes);

                // Su gibi fiziksel hitbox'ı olmayan seçilebilir bloklar
                // için tam blok alanını hedef olarak kullan.
                if (count == 0) {
                    hitboxes[0] = {
                        glm::vec3(0.0f),
                        glm::vec3(1.0f)
                    };

                    count = 1;
                }

                for (std::size_t i = 0; i < count; ++i) {
                    float t = 0.0f;
                    glm::ivec3 n(0);

                    if (rayBox(
                            origin,
                            dir,
                            cellOrigin + hitboxes[i].min,
                            cellOrigin + hitboxes[i].max,
                            std::min(maxDistance, closestT),
                            t,
                            n
                        ) && t < closestT) {
                        closestT = t;
                        hitBlock = glm::ivec3(x, y, z);
                        closestNormal = n;
                        found = true;
                    }
                }
            }
        }
    }

    if (!found || closestT > maxDistance) {
        return false;
    }

    // Yalnızca en yakın isabetin normali döndürülür.
    normal = closestNormal;

    return true;
}