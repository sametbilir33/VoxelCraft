
#include "TextureAtlas.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <png.h>

// ============================================================
// ATLAS AYARLARI
// ============================================================

namespace {

constexpr int TILE_SIZE = 16;
constexpr int ATLAS_COLS = 4;
constexpr int ATLAS_ROWS = 4;
constexpr int ATLAS_WIDTH = TILE_SIZE * ATLAS_COLS;
constexpr int ATLAS_HEIGHT = TILE_SIZE * ATLAS_ROWS;
constexpr int TILE_COUNT = ATLAS_COLS * ATLAS_ROWS;

// Tile ID'leri eski dokularla geriye dönük uyumludur.
struct TileInfo {
    const char* filename;
    std::array<std::uint8_t, 4> fallback;
};

constexpr std::array<TileInfo, TILE_COUNT> TILES = {{
    {"dirt.png",         {121,  85,  58, 255}}, // 0
    {"grass.png",        { 91, 160,  52, 255}}, // 1
    {"grass_side.png",   {107, 130,  55, 255}}, // 2
    {"stone.png",        {125, 125, 125, 255}}, // 3
    {"oak_log.png",      {116,  83,  48, 255}}, // 4
    {"leaves.png",       { 55, 125,  45, 220}}, // 5
    {"oak_log_top.png",  {153, 119,  77, 255}}, // 6
    {"short_grass.png",  { 70, 160,  45,   0}}, // 7
    {"cobblestone.png",  {110, 110, 110, 255}}, // 8
    {"sand.png",         {218, 204, 151, 255}}, // 9
    {"glass.png",        {190, 220, 230,  90}}, // 10
    {"glowstone.png",    {255, 208, 100, 255}}, // 11
    {"planks.png",       {160, 116,  65, 255}}, // 12
    {"torch.png",        {255, 190,  60, 255}}, // 13
    {"water.png",        { 55, 115, 220, 150}}, // 14
    {nullptr,            {255,   0, 255, 255}}  // 15: ayrılmış hücre
}};

void makeFallbackTile(
    std::vector<std::uint8_t>& pixels,
    int tileId
) {
    const auto color = TILES[tileId].fallback;

    for (int y = 0; y < TILE_SIZE; ++y) {
        for (int x = 0; x < TILE_SIZE; ++x) {
            const int i = (y * TILE_SIZE + x) * 4;

            // Eksik dokular için fark edilebilir dama deseni.
            const bool checker = ((x / 4) + (y / 4)) % 2 == 0;
            const float factor = checker ? 1.0f : 0.72f;

            pixels[i + 0] = static_cast<std::uint8_t>(color[0] * factor);
            pixels[i + 1] = static_cast<std::uint8_t>(color[1] * factor);
            pixels[i + 2] = static_cast<std::uint8_t>(color[2] * factor);
            pixels[i + 3] = color[3];
        }
    }
}

bool loadPngTile(
    const std::string& path,
    std::vector<std::uint8_t>& output
) {
    png_image image{};
    image.version = PNG_IMAGE_VERSION;

    if (!png_image_begin_read_from_file(&image, path.c_str())) {
        return false;
    }

    image.format = PNG_FORMAT_RGBA;

    std::vector<std::uint8_t> source(
        PNG_IMAGE_SIZE(image)
    );

    if (!png_image_finish_read(
            &image,
            nullptr,
            source.data(),
            0,
            nullptr
        )) {
        png_image_free(&image);
        return false;
    }

    const int sourceWidth = static_cast<int>(image.width);
    const int sourceHeight = static_cast<int>(image.height);

    if (sourceWidth <= 0 || sourceHeight <= 0) {
        png_image_free(&image);
        return false;
    }

    output.resize(TILE_SIZE * TILE_SIZE * 4);

    // Nearest-neighbor ölçekleme. Pixel-art dokuların
    // keskin kenarları korunur.
    for (int y = 0; y < TILE_SIZE; ++y) {
        for (int x = 0; x < TILE_SIZE; ++x) {
            const int sx = std::min(
                sourceWidth - 1,
                x * sourceWidth / TILE_SIZE
            );

            // PNG üstten başlar; OpenGL atlasını alttan
            // başlayan koordinatlarla hazırlıyoruz.
            const int sy = std::min(
                sourceHeight - 1,
                (TILE_SIZE - 1 - y) * sourceHeight / TILE_SIZE
            );

            const std::size_t src =
                (static_cast<std::size_t>(sy) * sourceWidth + sx) * 4;

            const std::size_t dst =
                (static_cast<std::size_t>(y) * TILE_SIZE + x) * 4;

            output[dst + 0] = source[src + 0];
            output[dst + 1] = source[src + 1];
            output[dst + 2] = source[src + 2];
            output[dst + 3] = source[src + 3];
        }
    }

    png_image_free(&image);
    return true;
}

} // namespace

// ============================================================
// BLOK YÜZEYİ -> TEXTURE ID
// face: 0=-Z, 1=+Z, 2=-X, 3=+X, 4=+Y, 5=-Y
// ============================================================

int blockTextureId(Block block, int face) {
    switch (block) {
        case Block::Grass:
            if (face == 4) return 1;
            if (face == 5) return 0;
            return 2;

        case Block::Dirt:
            return 0;

        case Block::Stone:
            return 3;

        case Block::Wood:
            return (face == 4 || face == 5) ? 6 : 4;

        case Block::Leaves:
            return 5;

        case Block::ShortGrass:
            return 7;

        case Block::Cobblestone:
            return 8;

        case Block::Sand:
            return 9;

        case Block::Glass:
            return 10;

        case Block::Glowstone:
            return 11;

        case Block::Planks:
            return 12;

        case Block::Slab:
            return 12;

        case Block::OakLog:
            return (face == 4 || face == 5) ? 6 : 4;

        case Block::Torch:
            return 13;

        case Block::Water:
            return 14;

        case Block::Air:
        default:
            return 0;
    }
}

// ============================================================
// ATLAS OLUŞTUR
// ============================================================

GLuint createBlockTextureAtlas(const char* blocksDirectory) {
    if (blocksDirectory == nullptr) {
        throw std::invalid_argument(
            "Texture atlas dizini nullptr olamaz."
        );
    }

    std::vector<std::uint8_t> atlas(
        ATLAS_WIDTH * ATLAS_HEIGHT * 4,
        0
    );

    for (int tileId = 0; tileId < TILE_COUNT; ++tileId) {
        std::vector<std::uint8_t> tile;
        bool loaded = false;

        if (TILES[tileId].filename != nullptr) {
            const std::string path =
                std::string(blocksDirectory) + "/" +
                TILES[tileId].filename;

            loaded = loadPngTile(path, tile);
        }

        if (!loaded) {
            tile.resize(TILE_SIZE * TILE_SIZE * 4);
            makeFallbackTile(tile, tileId);
        }

        const int cellX = tileId % ATLAS_COLS;
        const int cellY = tileId / ATLAS_COLS;

        for (int y = 0; y < TILE_SIZE; ++y) {
            for (int x = 0; x < TILE_SIZE; ++x) {
                const std::size_t src =
                    (static_cast<std::size_t>(y) * TILE_SIZE + x) * 4;

                const std::size_t dst =
                    (
                        static_cast<std::size_t>(cellY * TILE_SIZE + y)
                        * ATLAS_WIDTH
                        + cellX * TILE_SIZE + x
                    ) * 4;

                atlas[dst + 0] = tile[src + 0];
                atlas[dst + 1] = tile[src + 1];
                atlas[dst + 2] = tile[src + 2];
                atlas[dst + 3] = tile[src + 3];
            }
        }
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        ATLAS_WIDTH,
        ATLAS_HEIGHT,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        atlas.data()
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        GL_NEAREST
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MAG_FILTER,
        GL_NEAREST
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_S,
        GL_CLAMP_TO_EDGE
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_T,
        GL_CLAMP_TO_EDGE
    );

    glBindTexture(GL_TEXTURE_2D, 0);

    return texture;
}

// ============================================================
// ATLAS'I KULLAN
// ============================================================

void bindBlockTextureAtlas(GLuint texture) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
}