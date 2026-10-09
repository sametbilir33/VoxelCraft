
#include "Chunk.hpp"
#include "World.hpp"
#include "TextureAtlas.hpp"

#include <array>
#include <vector>
#include <cstddef>
#include <cmath>

namespace {

constexpr float EPSILON = 0.001f;

// ============================================================
// YÜZ UV KOORDİNATLARI
// Face sırası:
// 0 = -Z, 1 = +Z, 2 = -X, 3 = +X, 4 = +Y, 5 = -Y
// ============================================================

const glm::vec2 FACE_UV[6][4] = {
// 0: -Z
{
    {0.0f, 1.0f}, {1.0f, 1.0f},
    {1.0f, 0.0f}, {0.0f, 0.0f}
},

    // 1: +Z
{
    {0.0f, 0.0f}, {1.0f, 0.0f},
    {1.0f, 1.0f}, {0.0f, 1.0f}
},

    // 2: -X
    {
        {0.0f, 0.0f}, {1.0f, 0.0f},
        {1.0f, 1.0f}, {0.0f, 1.0f}
    },

    // 3: +X
    {
        {0.0f, 0.0f}, {0.0f, 1.0f},
        {1.0f, 1.0f}, {1.0f, 0.0f}
    },

    // 4: +Y, üst yüz
    {
        {0.0f, 0.0f}, {0.0f, 1.0f},
        {1.0f, 1.0f}, {1.0f, 0.0f}
    },

    // 5: -Y, alt yüz
    {
        {0.0f, 0.0f}, {1.0f, 0.0f},
        {1.0f, 1.0f}, {0.0f, 1.0f}
    }
};

// Çapraz bitkiler için UV koordinatları.
const glm::vec2 PLANT_UV[4] = {
    {0.0f, 0.0f},
    {1.0f, 0.0f},
    {1.0f, 1.0f},
    {0.0f, 1.0f}
};

// Küp köşe sıralaması.
const int FACE_INDICES[6][4] = {
    {3, 2, 1, 0}, // -Z
    {4, 5, 6, 7}, // +Z
    {0, 4, 7, 3}, // -X
    {1, 2, 6, 5}, // +X
    {3, 7, 6, 2}, // +Y
    {0, 1, 5, 4}  // -Y
};

const glm::ivec3 NEIGHBOR_OFFSETS[6] = {
    { 0,  0, -1},
    { 0,  0,  1},
    {-1,  0,  0},
    { 1,  0,  0},
    { 0,  1,  0},
    { 0, -1,  0}
};

glm::vec3 blockColor(Block block, int face) {
    switch (block) {
        case Block::Grass:
            return face == 4
                ? glm::vec3(0.25f, 0.72f, 0.18f)
                : glm::vec3(1.0f);

        case Block::Dirt:
        case Block::Stone:
        case Block::Cobblestone:
        case Block::Wood:
        case Block::OakLog:
        case Block::Planks:
        case Block::Leaves:
        case Block::ShortGrass:
        case Block::Sand:
        case Block::Glass:
        case Block::Glowstone:
        case Block::Torch:
        case Block::Water:
            return glm::vec3(1.0f);

        default:
            return glm::vec3(1.0f);
    }
}

void appendVertex(
    std::vector<Vertex>& vertices,
    const glm::vec3& position,
    const glm::vec3& color,
    const glm::vec2& uv,
    int textureId
) {
    vertices.push_back({
        position,
        color,
        uv,
        static_cast<float>(textureId)
    });
}

// ============================================================
// QUAD EKLE
// face: 0-5 normal blok yüzleri, -1 bitki yüzleri
// ============================================================

void appendQuad(
    std::vector<Vertex>& vertices,
    const glm::vec3 positions[4],
    const glm::vec3& color,
    int textureId,
    int face,
    bool doubleSided = false
) {
    constexpr int TRIANGLES[6] = {
        0, 1, 2,
        0, 2, 3
    };

    const glm::vec2* uv =
        (face >= 0 && face < 6)
            ? FACE_UV[face]
            : PLANT_UV;

    for (int i : TRIANGLES) {
        appendVertex(
            vertices,
            positions[i],
            color,
            uv[i],
            textureId
        );
    }

    if (doubleSided) {
        constexpr int REVERSE_TRIANGLES[6] = {
            2, 1, 0,
            3, 2, 0
        };

        for (int i : REVERSE_TRIANGLES) {
            appendVertex(
                vertices,
                positions[i],
                color,
                uv[i],
                textureId
            );
        }
    }
}

// ============================================================
// ÇAPRAZ BİTKİ OLUŞTUR
// ============================================================

void appendCrossPlant(
    std::vector<Vertex>& vertices,
    const glm::vec3& base,
    int textureId
) {
    constexpr float inset = 0.12f;
    constexpr float top = 0.88f;

    const float x0 = base.x + inset;
    const float x1 = base.x + 1.0f - inset;
    const float y0 = base.y;
    const float y1 = base.y + top;
    const float z0 = base.z + inset;
    const float z1 = base.z + 1.0f - inset;

    const glm::vec3 quadA[4] = {
        {x0, y0, z0},
        {x1, y0, z1},
        {x1, y1, z1},
        {x0, y1, z0}
    };

    const glm::vec3 quadB[4] = {
        {x1, y0, z0},
        {x0, y0, z1},
        {x0, y1, z1},
        {x1, y1, z0}
    };

    const glm::vec3 color(1.0f);

    appendQuad(
        vertices,
        quadA,
        color,
        textureId,
        -1,
        true
    );

    appendQuad(
        vertices,
        quadB,
        color,
        textureId,
        -1,
        true
    );
}

} // namespace

// ============================================================
// CHUNK
// ============================================================

Chunk::Chunk(int cx, int cz)
    : chunkX(cx), chunkZ(cz) {
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                blocks[x][y][z] = Block::Air;
            }
        }
    }
}

Chunk::~Chunk() {
    if (vbo != 0) {
        glDeleteBuffers(1, &vbo);
    }

    if (vao != 0) {
        glDeleteVertexArrays(1, &vao);
    }
}

// ============================================================
// MESH OLUŞTUR
// ============================================================

void Chunk::updateMesh(const World& world) {
    std::vector<Vertex> vertices;
    vertices.reserve(CHUNK_SIZE * CHUNK_SIZE * 6 * 6);

    const int worldOriginX = chunkX * CHUNK_SIZE;
    const int worldOriginZ = chunkZ * CHUNK_SIZE;

    static const glm::vec3 cube[8] = {
        {0, 0, 0},
        {1, 0, 0},
        {1, 1, 0},
        {0, 1, 0},
        {0, 0, 1},
        {1, 0, 1},
        {1, 1, 1},
        {0, 1, 1}
    };

    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                const Block block = blocks[x][y][z];

                if (block == Block::Air) {
                    continue;
                }

                const glm::vec3 origin(
                    static_cast<float>(worldOriginX + x),
                    static_cast<float>(y),
                    static_cast<float>(worldOriginZ + z)
                );

                const BlockProperties properties =
                    blockProperties(block);

                if (properties.shape == BlockShape::Cross) {
                    appendCrossPlant(
                        vertices,
                        origin,
                        blockTextureId(block, 0)
                    );
                    continue;
                }

                glm::vec3 localMin(0.0f);
                glm::vec3 localMax(1.0f);

                if (properties.shape == BlockShape::Slab) {
                    localMax.y = 0.5f;
                } else if (properties.shape == BlockShape::Torch) {
                    localMin = glm::vec3(
                        0.375f, 0.0f, 0.375f
                    );
                    localMax = glm::vec3(
                        0.625f, 0.75f, 0.625f
                    );
                }

                glm::vec3 shapeVertices[8];

                for (int i = 0; i < 8; ++i) {
                    shapeVertices[i] = origin + glm::vec3(
                        cube[i].x == 0.0f
                            ? localMin.x : localMax.x,
                        cube[i].y == 0.0f
                            ? localMin.y : localMax.y,
                        cube[i].z == 0.0f
                            ? localMin.z : localMax.z
                    );
                }

                for (int face = 0; face < 6; ++face) {
                    const glm::ivec3 offset =
                        NEIGHBOR_OFFSETS[face];

                    const Block neighbor = world.getBlock(
                        worldOriginX + x + offset.x,
                        y + offset.y,
                        worldOriginZ + z + offset.z
                    );

                    if (neighbor != Block::Air &&
                        blockProperties(neighbor).occludesFaces &&
                        properties.shape == BlockShape::Cube) {
                        continue;
                    }

                    if (properties.shape == BlockShape::Slab &&
                        face == 5 &&
                        neighbor != Block::Air) {
                        continue;
                    }

                    const glm::vec3 color =
                        blockColor(block, face);

                    const int textureId =
                        blockTextureId(block, face);

                    glm::vec3 quad[4];

                    for (int i = 0; i < 4; ++i) {
                        quad[i] =
                            shapeVertices[FACE_INDICES[face][i]];
                    }

                    // ÖNEMLİ: face parametresi burada gönderiliyor.
                    appendQuad(
                        vertices,
                        quad,
                        color,
                        textureId,
                        face,
                        false
                    );
                }
            }
        }
    }

    if (vao == 0) {
        glGenVertexArrays(1, &vao);
    }

    if (vbo == 0) {
        glGenBuffers(1, &vbo);
    }

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(
            vertices.size() * sizeof(Vertex)
        ),
        vertices.empty() ? nullptr : vertices.data(),
        GL_STATIC_DRAW
    );

    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<void*>(
            offsetof(Vertex, position)
        )
    );
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<void*>(
            offsetof(Vertex, color)
        )
    );
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(
        2,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<void*>(
            offsetof(Vertex, uv)
        )
    );
    glEnableVertexAttribArray(2);

    glVertexAttribPointer(
        3,
        1,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<void*>(
            offsetof(Vertex, textureId)
        )
    );
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);

    vertexCount = vertices.size();
    modified = false;
}

// ============================================================
// CHUNK ÇİZ
// ============================================================

void Chunk::render() const {
    if (vao == 0 || vertexCount == 0) {
        return;
    }

    glBindVertexArray(vao);

    glDrawArrays(
        GL_TRIANGLES,
        0,
        static_cast<GLsizei>(vertexCount)
    );

    glBindVertexArray(0);
}