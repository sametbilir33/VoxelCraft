#pragma once

#include "Chunk.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <unordered_map>
#include <utility>

struct PairHash {
    template <class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2>& p) const {
        return std::hash<T1>{}(p.first) ^
               (std::hash<T2>{}(p.second) << 1);
    }
};

class World {
public:
    std::unordered_map<std::pair<int, int>, Chunk, PairHash> chunks;

    std::uint32_t seed;

    World();

    Block getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, Block block);

    void generateWorld();
    void render();

    bool raycast(
        const glm::vec3& origin,
        const glm::vec3& direction,
        float maxDistance,
        glm::ivec3& hitBlock,
        glm::ivec3& normal
    );

private:
    float noise2D(float x, float z) const;
    float terrainHeight(int x, int z) const;

    std::uint32_t hash2D(int x, int z) const;
    float random01(int x, int z) const;

    void generateTree(int x, int y, int z);
};