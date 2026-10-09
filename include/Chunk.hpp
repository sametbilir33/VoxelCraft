#pragma once

#include "Block.hpp"
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <cstddef>

// One vertical 16x16x16 section, addressed by (chunkX, sectionY, chunkZ).
constexpr int CHUNK_SIZE = 16;
constexpr int SUBCHUNK_HEIGHT = 16;
constexpr int WORLD_MIN_Y = -64;
constexpr int WORLD_MAX_Y = 63;

struct Vertex { glm::vec3 position, color; glm::vec2 uv; float textureId; };

class Chunk {
public:
    int chunkX, chunkY, chunkZ;
    Block blocks[CHUNK_SIZE][SUBCHUNK_HEIGHT][CHUNK_SIZE]{};
    GLuint vao = 0, vbo = 0;
    std::size_t vertexCount = 0;
    bool modified = true;

    Chunk(int cx, int cy, int cz);
    ~Chunk();
    Chunk(const Chunk&) = delete;
    Chunk& operator=(const Chunk&) = delete;
    void updateMesh(const class World& world);
    void render() const;
};
