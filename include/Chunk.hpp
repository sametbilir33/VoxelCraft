#pragma once
#include "Block.hpp"
#include <vector>
#include <glad/gl.h>
#include <glm/glm.hpp>

constexpr int CHUNK_SIZE = 16;
constexpr int CHUNK_HEIGHT = 64;

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
    glm::vec2 uv;
    float textureId;
};

class Chunk {
public:
    int chunkX, chunkZ;
    Block blocks[CHUNK_SIZE][CHUNK_HEIGHT][CHUNK_SIZE];
    
    GLuint vao = 0;
    GLuint vbo = 0;
    size_t vertexCount = 0;
    bool modified = true;

    Chunk(int cx, int cz);
    ~Chunk();

    void updateMesh(const class World& world);
    void render() const;
};
