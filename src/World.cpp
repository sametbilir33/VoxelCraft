#include "World.hpp"
#include "TextureAtlas.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <utility>
#include <vector>

namespace {

float smoothstep(float t) {
    return t * t * (3.0f - 2.0f * t);
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float distanceSquared(const glm::vec3& a, const glm::vec3& b) {
    const glm::vec3 d = a - b;
    return glm::dot(d, d);
}

// Ray ile eksenlere paralel bir AABB'nin kesişimi.
bool rayBox(
    const glm::vec3& origin,
    const glm::vec3& direction,
    const glm::vec3& boxMin,
    const glm::vec3& boxMax,
    float maxDistance,
    float& hitDistance,
    glm::ivec3& hitNormal
) {
    float tMin = 0.0f;
    float tMax = maxDistance;
    glm::ivec3 normal(0);

    for (int axis = 0; axis < 3; ++axis) {
        const float o = origin[axis];
        const float d = direction[axis];
        const float mn = boxMin[axis];
        const float mx = boxMax[axis];

        if (std::abs(d) < 1e-7f) {
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

        if (t1 > tMin) {
            tMin = t1;
            normal = glm::ivec3(0);
            normal[axis] = sign;
        }

        tMax = std::min(tMax, t2);

        if (tMin > tMax) {
            return false;
        }
    }

    if (tMax < 0.0f || tMin > maxDistance) {
        return false;
    }

    hitDistance = tMin;
    hitNormal = normal;
    return true;
}

} // namespace

// ------------------------------------------------------------
// DÜNYA
// ------------------------------------------------------------

World::World() {
    std::random_device rd;
    seed = (static_cast<std::uint32_t>(rd()) << 16) ^ rd();

    std::cout << "World seed: " << seed << '\n';

    generateWorld();
}

int World::floorDiv(int value, int divisor) {
    int result = value / divisor;
    const int remainder = value % divisor;

    if (remainder != 0 && ((remainder < 0) != (divisor < 0))) {
        --result;
    }

    return result;
}

// ------------------------------------------------------------
// DETERMINISTIK HASH VE GÜRÜLTÜ
// ------------------------------------------------------------

std::uint32_t World::hash2D(int x, int z) const {
    std::uint32_t h = seed;

    h ^= static_cast<std::uint32_t>(x) * 0x8da6b343u;
    h ^= static_cast<std::uint32_t>(z) * 0xd8163841u;

    h ^= h >> 13;
    h *= 0x85ebca6bu;
    h ^= h >> 16;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;

    return h;
}

std::uint32_t World::hash3D(int x, int y, int z) const {
    std::uint32_t h = hash2D(x, z);
    h ^= static_cast<std::uint32_t>(y) * 0xcb1ab31fu;

    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    h *= 0x297a2d39u;
    h ^= h >> 15;

    return h;
}

float World::random01(int x, int z) const {
    return static_cast<float>(hash2D(x, z) & 0x00FFFFFFu)
        / static_cast<float>(0x01000000u);
}

float World::noise2D(float x, float z) const {
    const int ix = static_cast<int>(std::floor(x));
    const int iz = static_cast<int>(std::floor(z));

    const float fx = smoothstep(x - static_cast<float>(ix));
    const float fz = smoothstep(z - static_cast<float>(iz));

    const auto value = [this](int px, int pz) {
        return random01(px, pz) * 2.0f - 1.0f;
    };

    const float a = lerp(
        value(ix, iz),
        value(ix + 1, iz),
        fx
    );

    const float b = lerp(
        value(ix, iz + 1),
        value(ix + 1, iz + 1),
        fx
    );

    return lerp(a, b, fz);
}

float World::noise3D(float x, float y, float z) const {
    const int ix = static_cast<int>(std::floor(x));
    const int iy = static_cast<int>(std::floor(y));
    const int iz = static_cast<int>(std::floor(z));

    const float fx = smoothstep(x - static_cast<float>(ix));
    const float fy = smoothstep(y - static_cast<float>(iy));
    const float fz = smoothstep(z - static_cast<float>(iz));

    const auto value = [this](int px, int py, int pz) {
        return static_cast<float>(
            hash3D(px, py, pz) & 0x00FFFFFFu
        ) / static_cast<float>(0x00800000u) - 1.0f;
    };

    const float c000 = value(ix,     iy,     iz);
    const float c100 = value(ix + 1, iy,     iz);
    const float c010 = value(ix,     iy + 1, iz);
    const float c110 = value(ix + 1, iy + 1, iz);
    const float c001 = value(ix,     iy,     iz + 1);
    const float c101 = value(ix + 1, iy,     iz + 1);
    const float c011 = value(ix,     iy + 1, iz + 1);
    const float c111 = value(ix + 1, iy + 1, iz + 1);

    const float x00 = lerp(c000, c100, fx);
    const float x10 = lerp(c010, c110, fx);
    const float x01 = lerp(c001, c101, fx);
    const float x11 = lerp(c011, c111, fx);

    const float y0 = lerp(x00, x10, fy);
    const float y1 = lerp(x01, x11, fy);

    return lerp(y0, y1, fz);
}

float World::terrainHeight(int x, int z) const {
    float total = 0.0f;
    float amplitude = 1.0f;
    float frequency = 0.015f;
    float amplitudeSum = 0.0f;

    for (int octave = 0; octave < 5; ++octave) {
        total += noise2D(
            static_cast<float>(x) * frequency,
            static_cast<float>(z) * frequency
        ) * amplitude;

        amplitudeSum += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }

    const float broadHills = noise2D(
        static_cast<float>(x) * 0.006f,
        static_cast<float>(z) * 0.006f
    );

    const float normalized = total / amplitudeSum;

    const float height =
        14.0f +
        normalized * 22.0f +
        broadHills * 8.0f;

    return std::clamp(
        std::floor(height),
        3.0f,
        static_cast<float>(CHUNK_HEIGHT - 8)
    );
}

// ------------------------------------------------------------
// KÜÇÜK MAĞARALAR
// ------------------------------------------------------------

bool World::isCave(
    int x,
    int y,
    int z,
    int surfaceHeight
) const {
    // Yüzeye ve dünyanın alt sınırına yakın bölgelerde
    // mağara oluşumunu sınırla.
    if (y < 5 || y >= surfaceHeight - 3) {
        return false;
    }

    // İki farklı ölçekteki gürültüyü birlikte kullanmak,
    // devasa boşlukları azaltır ve kısa tünel kümeleri üretir.
    const float broad = noise3D(
        static_cast<float>(x) * 0.105f,
        static_cast<float>(y) * 0.14f,
        static_cast<float>(z) * 0.105f
    );

    const float detail = noise3D(
        static_cast<float>(x) * 0.23f + 17.0f,
        static_cast<float>(y) * 0.21f + 31.0f,
        static_cast<float>(z) * 0.23f + 11.0f
    );

    return broad > 0.57f && detail > -0.12f;
}

// ------------------------------------------------------------
// BLOK OKUMA VE YAZMA
// ------------------------------------------------------------

Block World::getBlock(int x, int y, int z) const {
    if (y < 0 || y >= CHUNK_HEIGHT) {
        return Block::Air;
    }

    const int cx = floorDiv(x, CHUNK_SIZE);
    const int cz = floorDiv(z, CHUNK_SIZE);

    const auto it = chunks.find({cx, cz});

    if (it == chunks.end()) {
        return Block::Air;
    }

    const BlockPosition position{x, y, z};
    const auto edit = blockEdits.find(position);

    if (edit != blockEdits.end()) {
        return edit->second;
    }

    const int lx = x - cx * CHUNK_SIZE;
    const int lz = z - cz * CHUNK_SIZE;

    return it->second.blocks[lx][y][lz];
}

void World::markChunkAndNeighborsModified(int x, int z) {
    const int cx = floorDiv(x, CHUNK_SIZE);
    const int cz = floorDiv(z, CHUNK_SIZE);

    constexpr int offsets[5][2] = {
        { 0,  0},
        {-1,  0},
        { 1,  0},
        { 0, -1},
        { 0,  1}
    };

    for (const auto& offset : offsets) {
        const auto it = chunks.find({
            cx + offset[0],
            cz + offset[1]
        });

        if (it != chunks.end()) {
            it->second.modified = true;
        }
    }
}

void World::setBlock(int x, int y, int z, Block block) {
    if (y < 0 || y >= CHUNK_HEIGHT) {
        return;
    }

    const BlockPosition position{x, y, z};

    // Değişiklik chunk belleğinden bağımsız saklanır.
    blockEdits[position] = block;

    const int cx = floorDiv(x, CHUNK_SIZE);
    const int cz = floorDiv(z, CHUNK_SIZE);

    auto it = chunks.find({cx, cz});

    if (it == chunks.end()) {
        return;
    }

    const int lx = x - cx * CHUNK_SIZE;
    const int lz = z - cz * CHUNK_SIZE;

    it->second.blocks[lx][y][lz] = block;

    markChunkAndNeighborsModified(x, z);
}

// Arazi üretiminde oyuncu değişikliklerini kaydetmeden yaz.
void World::writeGeneratedBlock(int x, int y, int z, Block block) {
    if (y < 0 || y >= CHUNK_HEIGHT) {
        return;
    }

    const int cx = floorDiv(x, CHUNK_SIZE);
    const int cz = floorDiv(z, CHUNK_SIZE);

    auto it = chunks.find({cx, cz});

    if (it == chunks.end()) {
        return;
    }

    const int lx = x - cx * CHUNK_SIZE;
    const int lz = z - cz * CHUNK_SIZE;

    it->second.blocks[lx][y][lz] = block;
    it->second.modified = true;
}

// ------------------------------------------------------------
// CHUNK ARAZİSİ
// ------------------------------------------------------------

void World::generateChunkTerrain(Chunk& chunk) {
    const int originX = chunk.chunkX * CHUNK_SIZE;
    const int originZ = chunk.chunkZ * CHUNK_SIZE;

    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            const int wx = originX + x;
            const int wz = originZ + z;

            const int height = static_cast<int>(
                terrainHeight(wx, wz)
            );

            for (int y = 0; y <= height; ++y) {
                if (isCave(wx, y, wz, height)) {
                    chunk.blocks[x][y][z] = Block::Air;
                } else if (y == height) {
                    chunk.blocks[x][y][z] = Block::Grass;
                } else if (y >= height - 3) {
                    chunk.blocks[x][y][z] = Block::Dirt;
                } else {
                    chunk.blocks[x][y][z] = Block::Stone;
                }
            }

            // Short grass az miktarda ve yalnızca yüzeyde çıkar.
            if (
                chunk.blocks[x][height][z] == Block::Grass &&
                height + 1 < CHUNK_HEIGHT &&
                random01(wx + 7919, wz - 1543) < 0.035f
            ) {
                chunk.blocks[x][height + 1][z] = Block::ShortGrass;
            }
        }
    }

    chunk.modified = true;
}

// ------------------------------------------------------------
// AĞAÇLAR VE YÜZEY BİTKİLERİ
// ------------------------------------------------------------

void World::generateTree(int x, int y, int z) {
    const int treeHeight =
        4 + static_cast<int>(random01(x + 97, z - 53) * 2.0f);

    for (int i = 0; i < treeHeight; ++i) {
        writeGeneratedBlock(x, y + i, z, Block::Wood);
    }

    const int top = y + treeHeight - 1;

    for (int dx = -2; dx <= 2; ++dx) {
        for (int dz = -2; dz <= 2; ++dz) {
            for (int dy = -1; dy <= 2; ++dy) {
                const int spread = std::abs(dx) + std::abs(dz);

                if (spread > 3) {
                    continue;
                }

                if (dy == 2 && spread > 1) {
                    continue;
                }

                const int lx = x + dx;
                const int ly = top + dy;
                const int lz = z + dz;

                if (ly < 0 || ly >= CHUNK_HEIGHT) {
                    continue;
                }

                if (getBlock(lx, ly, lz) == Block::Air) {
                    writeGeneratedBlock(lx, ly, lz, Block::Leaves);
                }
            }
        }
    }
}

void World::generateVegetation() {
    // Ağaçların chunk sınırını aşabilmesi için her yüklü
    // chunk'ın yakın çevresindeki aday merkezleri incelenir.
    for (const auto& entry : chunks) {
        const Chunk& chunk = entry.second;

        const int minX = chunk.chunkX * CHUNK_SIZE;
        const int maxX = minX + CHUNK_SIZE - 1;
        const int minZ = chunk.chunkZ * CHUNK_SIZE;
        const int maxZ = minZ + CHUNK_SIZE - 1;

        const int gridMinX = floorDiv(minX - 3, 12);
        const int gridMaxX = floorDiv(maxX + 3, 12);
        const int gridMinZ = floorDiv(minZ - 3, 12);
        const int gridMaxZ = floorDiv(maxZ + 3, 12);

        for (int gx = gridMinX; gx <= gridMaxX; ++gx) {
            for (int gz = gridMinZ; gz <= gridMaxZ; ++gz) {
                if (random01(gx + 231, gz - 491) > 0.13f) {
                    continue;
                }

                const int x = gx * 12 +
                    static_cast<int>(random01(gx, gz + 91) * 12.0f);

                const int z = gz * 12 +
                    static_cast<int>(random01(gx - 71, gz) * 12.0f);

                // Ağaç tabanını yalnızca bir kez üret.
                if (
                    x < minX || x > maxX ||
                    z < minZ || z > maxZ
                ) {
                    continue;
                }

                const int surface = static_cast<int>(
                    terrainHeight(x, z)
                );

                if (getBlock(x, surface, z) != Block::Grass) {
                    continue;
                }

                if (getBlock(x, surface + 1, z) != Block::Air &&
                    getBlock(x, surface + 1, z) != Block::ShortGrass) {
                    continue;
                }

                generateTree(x, surface + 1, z);
            }
        }
    }
}

// Oyuncunun düzenlemelerini prosedürel üretimin üzerine uygula.
void World::applyBlockEdits() {
    for (const auto& entry : blockEdits) {
        const BlockPosition& p = entry.first;

        if (p.y < 0 || p.y >= CHUNK_HEIGHT) {
            continue;
        }

        const int cx = floorDiv(p.x, CHUNK_SIZE);
        const int cz = floorDiv(p.z, CHUNK_SIZE);

        auto it = chunks.find({cx, cz});

        if (it == chunks.end()) {
            continue;
        }

        const int lx = p.x - cx * CHUNK_SIZE;
        const int lz = p.z - cz * CHUNK_SIZE;

        it->second.blocks[lx][p.y][lz] = entry.second;
        it->second.modified = true;
    }
}

// ------------------------------------------------------------
// 7x7 CHUNK AKTARIMI
// ------------------------------------------------------------

void World::generateWorld() {
    updateStreaming(0, 0);
}

void World::updateStreaming(int centerChunkX, int centerChunkZ) {
    constexpr int MAX_NEW_CHUNKS_PER_FRAME = 1;

    bool changed = false;
    int createdThisFrame = 0;

    // Alan dışındaki chunk'ları kaldır.
    for (auto it = chunks.begin(); it != chunks.end();) {
        const int cx = it->first.first;
        const int cz = it->first.second;

        if (
            std::abs(cx - centerChunkX) > LOAD_RADIUS ||
            std::abs(cz - centerChunkZ) > LOAD_RADIUS
        ) {
            constexpr int neighbors[4][2] = {
                {-1, 0}, {1, 0}, {0, -1}, {0, 1}
            };

            for (const auto& n : neighbors) {
                auto neighbor = chunks.find({
                    cx + n[0], cz + n[1]
                });

                if (neighbor != chunks.end()) {
                    neighbor->second.modified = true;
                }
            }

            it = chunks.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }

    // Her karede en fazla bir yeni chunk üret.
    for (int cx = centerChunkX - LOAD_RADIUS;
         cx <= centerChunkX + LOAD_RADIUS &&
         createdThisFrame < MAX_NEW_CHUNKS_PER_FRAME;
         ++cx) {

        for (int cz = centerChunkZ - LOAD_RADIUS;
             cz <= centerChunkZ + LOAD_RADIUS &&
             createdThisFrame < MAX_NEW_CHUNKS_PER_FRAME;
             ++cz) {

            const std::pair<int, int> key{cx, cz};

            if (chunks.find(key) != chunks.end()) {
                continue;
            }

            auto [it, inserted] = chunks.try_emplace(key, cx, cz);

            if (!inserted) {
                continue;
            }

            generateChunkTerrain(it->second);
            applyBlockEdits();

            // Yalnızca yeni chunk'ın komşularını işaretle.
            constexpr int neighbors[4][2] = {
                {-1, 0}, {1, 0}, {0, -1}, {0, 1}
            };

            for (const auto& n : neighbors) {
                auto neighbor = chunks.find({
                    cx + n[0], cz + n[1]
                });

                if (neighbor != chunks.end()) {
                    neighbor->second.modified = true;
                }
            }

            ++createdThisFrame;
            changed = true;
        }
    }

    // Ağaçları yalnızca 7x7 alan tamamlandığında üret.
    // Her yeni chunk oluşturulduğunda tüm dünyayı tarama.
    if (changed && chunks.size() == LOAD_DIAMETER * LOAD_DIAMETER) {
        generateVegetation();
        applyBlockEdits();
    }
}

// ------------------------------------------------------------
// ÇİZİM
// ------------------------------------------------------------

void World::render() {
    constexpr int MAX_MESH_UPDATES_PER_FRAME = 2;
    int updated = 0;

    for (auto& entry : chunks) {
        Chunk& chunk = entry.second;

        if (chunk.modified &&
            updated < MAX_MESH_UPDATES_PER_FRAME) {
            chunk.updateMesh(*this);
            ++updated;
        }

        chunk.render();
    }
}

// ------------------------------------------------------------
// RAYCAST
// ------------------------------------------------------------

bool World::raycast(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxDistance,
    glm::ivec3& hitBlock,
    glm::ivec3& normal
) {
    if (maxDistance <= 0.0f) {
        return false;
    }

    const glm::vec3 end = origin + direction * maxDistance;

    const int minX = static_cast<int>(
        std::floor(std::min(origin.x, end.x))
    );
    const int maxX = static_cast<int>(
        std::floor(std::max(origin.x, end.x))
    );

    const int minY = std::max(
        0,
        static_cast<int>(std::floor(std::min(origin.y, end.y)))
    );
    const int maxY = std::min(
        CHUNK_HEIGHT - 1,
        static_cast<int>(std::floor(std::max(origin.y, end.y)))
    );

    const int minZ = static_cast<int>(
        std::floor(std::min(origin.z, end.z))
    );
    const int maxZ = static_cast<int>(
        std::floor(std::max(origin.z, end.z))
    );

    float closestDistance = maxDistance;
    bool found = false;

    for (int x = minX; x <= maxX; ++x) {
        for (int y = minY; y <= maxY; ++y) {
            for (int z = minZ; z <= maxZ; ++z) {
                const Block block = getBlock(x, y, z);
                const BlockProperties properties =
                    blockProperties(block);

                if (!properties.selectable) {
                    continue;
                }

                std::array<BlockAABB, 2> boxes{};
                std::size_t count = blockHitboxes(block, boxes);

                // Çapraz bitkiler ve su için seçilebilir bir
                // hedef hacmi kullan.
                if (count == 0) {
                    boxes[0] = {
                        glm::vec3(0.0f),
                        glm::vec3(1.0f)
                    };
                    count = 1;
                }

                const glm::vec3 cell(
                    static_cast<float>(x),
                    static_cast<float>(y),
                    static_cast<float>(z)
                );

                for (std::size_t i = 0; i < count; ++i) {
                    float distance = 0.0f;
                    glm::ivec3 hitNormal(0);

                    if (!rayBox(
                        origin,
                        direction,
                        cell + boxes[i].min,
                        cell + boxes[i].max,
                        closestDistance,
                        distance,
                        hitNormal
                    )) {
                        continue;
                    }

                    if (distance < 0.0f || distance > closestDistance) {
                        continue;
                    }

                    closestDistance = distance;
                    hitBlock = glm::ivec3(x, y, z);
                    normal = hitNormal;
                    found = true;
                }
            }
        }
    }

    return found;
}