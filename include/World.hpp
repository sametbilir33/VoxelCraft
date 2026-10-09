#pragma once

#include "Chunk.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

struct PairHash {
    template <class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2>& p) const {
        const std::size_t h1 = std::hash<T1>{}(p.first);
        const std::size_t h2 = std::hash<T2>{}(p.second);

        return h1 ^ (h2 + 0x9e3779b9u + (h1 << 6) + (h1 >> 2));
    }
};

struct BlockPosition {
    int x;
    int y;
    int z;

    bool operator==(const BlockPosition&) const = default;
};

struct BlockPositionHash {
    std::size_t operator()(const BlockPosition& p) const {
        std::size_t h = std::hash<int>{}(p.x);
        h ^= std::hash<int>{}(p.y) + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(p.z) + 0x9e3779b9u + (h << 6) + (h >> 2);
        return h;
    }
};

class World {
public:
    static constexpr int LOAD_RADIUS = 3;
    static constexpr int LOAD_DIAMETER = LOAD_RADIUS * 2 + 1;

    std::unordered_map<std::pair<int, int>, Chunk, PairHash> chunks;
    std::uint32_t seed = 0;

    World();

    Block getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, Block block);

    // Başlangıç dünyasını oluşturur.
    void generateWorld();

    // Oyuncunun bulunduğu chunk merkez alınarak 7x7 alanı tutar.
    void updateStreaming(int centerChunkX, int centerChunkZ);

    void render();

    bool raycast(
        const glm::vec3& origin,
        const glm::vec3& direction,
        float maxDistance,
        glm::ivec3& hitBlock,
        glm::ivec3& normal
    );

private:
    // Chunk'lardan bağımsız saklanan oyuncu değişiklikleri.
    std::unordered_map<
        BlockPosition,
        Block,
        BlockPositionHash
    > blockEdits;

    float noise2D(float x, float z) const;
    float noise3D(float x, float y, float z) const;
    float terrainHeight(int x, int z) const;

    std::uint32_t hash2D(int x, int z) const;
    std::uint32_t hash3D(int x, int y, int z) const;
    float random01(int x, int z) const;

    bool isCave(int x, int y, int z, int surfaceHeight) const;

    void generateChunkTerrain(Chunk& chunk);
    void generateVegetation();
    void generateTree(int x, int y, int z);

    void writeGeneratedBlock(int x, int y, int z, Block block);
    void applyBlockEdits();

    void markChunkAndNeighborsModified(int x, int z);

    static int floorDiv(int value, int divisor);
};