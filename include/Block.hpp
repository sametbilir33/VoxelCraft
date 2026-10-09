
#pragma once

#include <cstdint>
#include <array>
#include <cstddef>

#include <glm/glm.hpp>

// ============================================================
// BLOK TÜRLERİ
// ============================================================

enum class Block : std::uint8_t {
    Air = 0,

    Grass,
    Dirt,
    Stone,
    Wood,
    Leaves,

    ShortGrass,
    Cobblestone,
    Sand,
    Glass,
    Glowstone,
    Planks,
    OakLog,
    Torch,
    Water,
    Slab
};

// ============================================================
// BLOK ŞEKİLLERİ
// ============================================================

enum class BlockShape : std::uint8_t {
    Empty,
    Cube,
    Cross,
    Slab,
    Torch
};

// Bir blok içindeki normalize edilmiş AABB.
// Örneğin tam küp: min=(0,0,0), max=(1,1,1).
struct BlockAABB {
    glm::vec3 min;
    glm::vec3 max;
};

// ============================================================
// BLOK ÖZELLİKLERİ
// ============================================================

struct BlockProperties {
    BlockShape shape;
    bool solid;
    bool transparent;
    bool selectable;
    bool occludesFaces;
    bool replaceable;
    float height;
};

inline constexpr BlockProperties blockProperties(Block block) {
    switch (block) {
        case Block::Air:
            return {
                BlockShape::Empty,
                false, true, false, false, true, 0.0f
            };

        case Block::ShortGrass:
            return {
                BlockShape::Cross,
                false, true, true, false, true, 0.8f
            };

        case Block::Torch:
            return {
                BlockShape::Torch,
                false, true, true, false, true, 0.75f
            };

        case Block::Water:
            return {
                BlockShape::Cube,
                false, true, true, false, true, 1.0f
            };

        case Block::Leaves:
            return {
                BlockShape::Cube,
                true, true, true, false, false, 1.0f
            };

        case Block::Glass:
            return {
                BlockShape::Cube,
                true, true, true, false, false, 1.0f
            };

        case Block::Slab:
            return {
                BlockShape::Slab,
                true, false, true, true, false, 0.5f
            };

        case Block::Grass:
        case Block::Dirt:
        case Block::Stone:
        case Block::Wood:
        case Block::Cobblestone:
        case Block::Sand:
        case Block::Glowstone:
        case Block::Planks:
        case Block::OakLog:
        default:
            return {
                BlockShape::Cube,
                true, false, true, true, false, 1.0f
            };
    }
}

// ============================================================
// BLOK HITBOX'LARI
// Koordinatlar blok hücresine göre 0..1 aralığındadır.
// ============================================================

inline std::size_t blockHitboxes(
    Block block,
    std::array<BlockAABB, 2>& out
) {
    switch (block) {
        case Block::Air:
        case Block::ShortGrass:
        case Block::Water:
            return 0;

        case Block::Slab:
            out[0] = {
                glm::vec3(0.0f, 0.0f, 0.0f),
                glm::vec3(1.0f, 0.5f, 1.0f)
            };
            return 1;

        case Block::Torch:
            out[0] = {
                glm::vec3(0.375f, 0.0f, 0.375f),
                glm::vec3(0.625f, 0.75f, 0.625f)
            };
            return 1;

        default:
            out[0] = {
                glm::vec3(0.0f, 0.0f, 0.0f),
                glm::vec3(1.0f, 1.0f, 1.0f)
            };
            return 1;
    }
}

// ============================================================
// BLOK ADLARI
// ============================================================

inline const char* blockName(Block block) {
    switch (block) {
        case Block::Air:        return "Air";
        case Block::Grass:      return "Grass";
        case Block::Dirt:       return "Dirt";
        case Block::Stone:      return "Stone";
        case Block::Wood:       return "Wood";
        case Block::Leaves:     return "Leaves";
        case Block::ShortGrass: return "Short Grass";
        case Block::Cobblestone:return "Cobblestone";
        case Block::Sand:       return "Sand";
        case Block::Glass:      return "Glass";
        case Block::Glowstone:  return "Glowstone";
        case Block::Planks:     return "Planks";
        case Block::OakLog:     return "Oak Log";
        case Block::Torch:      return "Torch";
        case Block::Water:      return "Water";
        case Block::Slab:   return "Slab";
        default:                return "Unknown";
    }
}