#pragma once
#include "Chunk.hpp"
#include <glm/glm.hpp>
#include <cstdint>
#include <unordered_map>

struct ChunkPosition { int x, y, z; bool operator==(const ChunkPosition&) const = default; };
struct ChunkPositionHash { std::size_t operator()(const ChunkPosition& p) const; };
struct BlockPosition { int x, y, z; bool operator==(const BlockPosition&) const = default; };
struct BlockPositionHash { std::size_t operator()(const BlockPosition& p) const; };

struct WorldSettings {
    bool oceansEnabled = true;
    bool cavesEnabled = true;
    int seaLevel = 18;
    float terrainAmplitude = 1.0f;
    // Higher values make oceans rarer while preserving their large scale.
    float oceanThreshold = 0.48f;
    float caveDensity = 1.0f;
};

class World {
public:
    static constexpr int LOAD_RADIUS = 3;
    static constexpr int LOAD_DIAMETER = LOAD_RADIUS * 2 + 1;
    std::unordered_map<ChunkPosition, Chunk, ChunkPositionHash> chunks;
    std::uint32_t seed = 0;
    WorldSettings settings;
    explicit World(WorldSettings worldSettings = {});

    static int floorDiv(int value, int divisor);
    static int floorMod(int value, int divisor);
    static bool inWorldY(int y) { return y >= WORLD_MIN_Y && y <= WORLD_MAX_Y; }
    Block getBlock(int x, int y, int z) const;
    // Returns false when the request is outside the world or leaves the cell
    // unchanged, so callers do not cause needless subchunk/mesh work.
    bool setBlock(int x, int y, int z, Block block);
    void generateWorld();
    void updateStreaming(int centerChunkX, int centerChunkZ, float playerY);
    void render(const glm::mat4& viewProjection);
    bool raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
                 glm::ivec3& hitBlock, glm::ivec3& normal);

private:
    std::unordered_map<BlockPosition, Block, BlockPositionHash> blockEdits;
    float noise2D(float x, float z) const;
    float noise3D(float x, float y, float z) const;
    float terrainHeight(int x, int z) const;
    float oceanFactor(int x, int z) const;
    std::uint32_t hash2D(int x, int z) const;
    std::uint32_t hash3D(int x, int y, int z) const;
    float random01(int x, int z) const;
    bool isCave(int x, int y, int z, int surfaceHeight) const;
    Block generatedBlockAt(int x, int y, int z) const;
    Block generatedBlockAt(int x, int y, int z, int surfaceHeight) const;
    Chunk& ensureChunk(int cx, int cy, int cz);
    void generateChunkTerrain(Chunk& chunk);
    void markChunkAndNeighborsModified(int x, int y, int z);
    bool isVisible(const Chunk& chunk, const glm::mat4& viewProjection) const;
};
