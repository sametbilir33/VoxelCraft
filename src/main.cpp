#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <vector>
#include <algorithm>

#include "Shader.hpp"
#include "Camera.hpp"
#include "World.hpp"
#include "Player.hpp"
#include "TextureAtlas.hpp"

#include <array>
#include <cstddef>

#include <cmath>
#include <iomanip>
#include <memory>
#include <string>

constexpr int WIDTH = 1280;
constexpr int HEIGHT = 720;

Camera camera;
Player player;

World* activeWorld = nullptr;

float lastX = WIDTH / 2.0f;
float lastY = HEIGHT / 2.0f;

bool firstMouse = true;

int selectedSlot = 1;

// GUI / preview
GLuint previewVAO = 0;
GLuint previewVBO = 0;

GLuint crosshairVAO = 0;
GLuint crosshairVBO = 0;
GLuint outlineVAO = 0;
GLuint outlineVBO = 0;
GLuint blockAtlasTexture = 0;

// ============================================================
// ANA MENU
// ============================================================

WorldSettings pendingWorldSettings;
bool menuActive = true;
bool startWorldRequested = false;
GLuint menuVAO = 0;
GLuint menuVBO = 0;

void appendMenuRect(std::vector<Vertex>& vertices, float x0, float y0, float x1, float y1, bool textured = false) {
    const glm::vec2 uv[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    const glm::vec3 p[4] = {{x0,y0,0}, {x1,y0,0}, {x1,y1,0}, {x0,y1,0}};
    constexpr int indices[6] = {0, 1, 2, 0, 2, 3};
    for (int i : indices) vertices.push_back({p[i], glm::vec3(1), textured ? uv[i] : glm::vec2(0), 0.0f});
}

std::array<unsigned char, 7> menuGlyph(char c) {
    switch (c) {
        case 'A': return {14,17,17,31,17,17,17}; case 'B': return {30,17,17,30,17,17,30};
        case 'C': return {15,16,16,16,16,16,15}; case 'D': return {30,17,17,17,17,17,30};
        case 'E': return {31,16,16,30,16,16,31}; case 'G': return {15,16,16,23,17,17,15};
        case 'H': return {17,17,17,31,17,17,17}; case 'I': return {31,4,4,4,4,4,31};
        case 'K': return {17,18,20,24,20,18,17}; case 'L': return {16,16,16,16,16,16,31};
        case 'M': return {17,27,21,21,17,17,17}; case 'N': return {17,25,21,19,17,17,17};
        case 'O': return {14,17,17,17,17,17,14}; case 'R': return {30,17,17,30,20,18,17};
        case 'S': return {15,16,16,14,1,1,30};  case 'T': return {31,4,4,4,4,4,4};
        case 'U': return {17,17,17,17,17,17,14}; case 'V': return {17,17,17,17,17,10,4};
        case 'X': return {17,17,10,4,10,17,17}; case 'Y': return {17,17,10,4,4,4,4};
        case 'Z': return {31,1,2,4,8,16,31}; case '0': return {14,17,19,21,25,17,14};
        case '1': return {4,12,4,4,4,4,14}; case '2': return {14,17,1,2,4,8,31};
        case '3': return {30,1,1,14,1,1,30}; case '4': return {2,6,10,18,31,2,2};
        case '5': return {31,16,16,30,1,1,30}; case '6': return {14,16,16,30,17,17,14};
        case '7': return {31,1,2,4,8,8,8}; case '8': return {14,17,17,14,17,17,14};
        case '9': return {14,17,17,15,1,1,14}; case '+': return {0,4,4,31,4,4,0};
        case '-': return {0,0,0,31,0,0,0}; case ':': return {0,4,0,0,4,0,0};
        default: return {0,0,0,0,0,0,0};
    }
}

void appendMenuText(std::vector<Vertex>& vertices, const std::string& text, float centerX, float baselineY, float scale) {
    const float width = static_cast<float>(text.size()) * 6.0f * scale;
    float x = centerX - width * 0.5f;
    for (char raw : text) {
        const auto glyph = menuGlyph(raw);
        for (int row = 0; row < 7; ++row) for (int col = 0; col < 5; ++col)
            if (glyph[row] & (1 << (4 - col)))
                appendMenuRect(vertices, x + col * scale, baselineY - row * scale,
                               x + (col + 1) * scale, baselineY - (row + 1) * scale);
        x += 6.0f * scale;
    }
}

void drawMenuBatch(Shader& shader, const std::vector<Vertex>& vertices, const glm::vec3& tint, bool textured) {
    if (menuVAO == 0) { glGenVertexArrays(1, &menuVAO); glGenBuffers(1, &menuVBO); }
    glBindVertexArray(menuVAO); glBindBuffer(GL_ARRAY_BUFFER, menuVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)), vertices.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position))); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, color))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, uv))); glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, textureId))); glEnableVertexAttribArray(3);
    shader.use();
    shader.setMat4("uProjection", glm::mat4(1.0f)); shader.setMat4("uView", glm::mat4(1.0f)); shader.setMat4("uModel", glm::mat4(1.0f));
    glUniform1i(glGetUniformLocation(shader.id, "uUseTexture"), textured ? 1 : 0);
    glUniform1i(glGetUniformLocation(shader.id, "uAtlas"), 0);
    glUniform3fv(glGetUniformLocation(shader.id, "uTint"), 1, &tint[0]);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    glBindVertexArray(0);
}


void renderMainMenu(Shader& shader, int width, int height) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    // Arka plan
    std::vector<Vertex> background;
    appendMenuRect(background, -1.0f, -1.0f, 1.0f, 1.0f, true);
    bindBlockTextureAtlas(blockAtlasTexture);
    drawMenuBatch(shader, background, glm::vec3(0.42f, 0.34f, 0.25f), true);

    // Ana panel
    std::vector<Vertex> panels;
    appendMenuRect(panels, -0.55f, -0.88f, 0.55f, 0.78f);

    // Başlat butonu
    appendMenuRect(panels, -0.30f, 0.48f, 0.30f, 0.64f);

    // Ayar satırları
    const float rows[] = {
        0.32f, 0.12f, -0.08f, -0.28f, -0.48f, -0.66f
    };

    for (float y : rows) {
        appendMenuRect(
            panels,
            -0.43f, y - 0.055f,
             0.43f, y + 0.055f
        );
    }

    drawMenuBatch(
        shader, panels,
        glm::vec3(0.13f, 0.10f, 0.07f), false
    );

    // Menü yazıları
    std::vector<Vertex> text;

    appendMenuText(text, "VOXELCRAFT", 0.0f, 0.84f, 0.022f);
    appendMenuText(text, "DUNYAYA GIR", 0.0f, 0.585f, 0.014f);

    appendMenuText(
        text,
        std::string("DENIZ: ") +
        (pendingWorldSettings.oceansEnabled ? "ACIK" : "KAPALI"),
        0.0f, 0.345f, 0.012f
    );

    appendMenuText(
        text,
        std::string("MAGARA: ") +
        (pendingWorldSettings.cavesEnabled ? "ACIK" : "KAPALI"),
        0.0f, 0.145f, 0.012f
    );

    appendMenuText(text, "YUKSEKLIK  -  +", 0.0f, -0.055f, 0.012f);
    appendMenuText(text, "DENIZ ORANI -  +", 0.0f, -0.255f, 0.012f);
    appendMenuText(text, "MAGARA ORANI -  +", 0.0f, -0.455f, 0.012f);
    appendMenuText(text, "DENIZ SEVIYE -  +", 0.0f, -0.635f, 0.012f);

    appendMenuText(text, "SOL TIKLA AYARLARI DEGISTIR",
                   0.0f, -0.81f, 0.008f);

    drawMenuBatch(
        shader, text,
        glm::vec3(0.96f, 0.91f, 0.78f), false
    );

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}


void handleMenuClick(GLFWwindow* window, double xpos, double ypos) {
    int w, h;
    glfwGetWindowSize(window, &w, &h);

    if (w <= 0 || h <= 0)
        return;

    const float x = static_cast<float>(xpos / w * 2.0 - 1.0);
    const float y = static_cast<float>(1.0 - ypos / h * 2.0);

    // Panel dışındaki tıklamaları yok say.
    if (x < -0.55f || x > 0.55f ||
        y < -0.88f || y > 0.78f) {
        return;
    }

    // Dünyaya gir
    if (x >= -0.30f && x <= 0.30f &&
        y >= 0.48f && y <= 0.64f) {
        startWorldRequested = true;
        return;
    }

    // Denizleri aç / kapat
    if (y >= 0.265f && y <= 0.375f) {
        pendingWorldSettings.oceansEnabled =
            !pendingWorldSettings.oceansEnabled;
        return;
    }

    // Mağaraları aç / kapat
    if (y >= 0.065f && y <= 0.175f) {
        pendingWorldSettings.cavesEnabled =
            !pendingWorldSettings.cavesEnabled;
        return;
    }

    // Arazi yüksekliği
    if (y >= -0.135f && y <= -0.025f) {
        pendingWorldSettings.terrainAmplitude =
            std::clamp(
                pendingWorldSettings.terrainAmplitude +
                    (x < 0.0f ? -0.1f : 0.1f),
                0.4f, 1.8f
            );
        return;
    }

    // Deniz oranı
    if (y >= -0.335f && y <= -0.225f) {
        pendingWorldSettings.oceanThreshold =
            std::clamp(
                pendingWorldSettings.oceanThreshold +
                    (x < 0.0f ? 0.05f : -0.05f),
                0.20f, 0.75f
            );
        return;
    }

    // Mağara yoğunluğu
    if (y >= -0.535f && y <= -0.425f) {
        pendingWorldSettings.caveDensity =
            std::clamp(
                pendingWorldSettings.caveDensity +
                    (x < 0.0f ? -0.1f : 0.1f),
                0.2f, 1.5f
            );
        return;
    }

    // Deniz seviyesi
    if (y >= -0.715f && y <= -0.605f) {
        pendingWorldSettings.seaLevel =
            std::clamp(
                pendingWorldSettings.seaLevel +
                    (x < 0.0f ? -1 : 1),
                4, 35
            );
        return;
    }
}



// ============================================================
// SEÇİLİ BLOK
// ============================================================


Block selectedBlock() {
    switch (selectedSlot) {
        case 1: return Block::Grass;
        case 2: return Block::Dirt;
        case 3: return Block::Stone;
        case 4: return Block::Wood;
        case 5: return Block::Leaves;
        case 6: return Block::Slab;
        case 7: return Block::Torch;
        case 8: return Block::Cobblestone;
        case 9: return Block::Sand;
        default: return Block::Grass;
    }
}


// ============================================================
// MOUSE
// ============================================================

void mouseCallback(
    GLFWwindow*,
    double xpos,
    double ypos
) {
    if (menuActive) return;
    if (firstMouse) {
        lastX = static_cast<float>(xpos);
        lastY = static_cast<float>(ypos);
        firstMouse = false;
    }

    float dx =
        static_cast<float>(xpos) - lastX;

    float dy =
        static_cast<float>(ypos) - lastY;

    lastX = static_cast<float>(xpos);
    lastY = static_cast<float>(ypos);

    camera.mouseMove(dx, dy);
}


// ============================================================
// OYUNCU BLOKLA ÇAKIŞIYOR MU?
// ============================================================


bool playerIntersectsBlock(
    const Player& p,
    const glm::ivec3& block,
    Block blockType
) {
    std::array<BlockAABB, 2> hitboxes{};
    const std::size_t count = blockHitboxes(blockType, hitboxes);

    // Fiziksel hitbox yoksa oyuncuyla fiziksel çarpışma olmaz.
    if (count == 0)
        return false;

    const float halfWidth = p.width * 0.5f;

    const glm::vec3 playerMin{
        p.position.x - halfWidth,
        p.position.y,
        p.position.z - halfWidth
    };

    const glm::vec3 playerMax{
        p.position.x + halfWidth,
        p.position.y + p.height,
        p.position.z + halfWidth
    };

    const glm::vec3 cell{
        static_cast<float>(block.x),
        static_cast<float>(block.y),
        static_cast<float>(block.z)
    };

    for (std::size_t i = 0; i < count; ++i) {
        const glm::vec3 boxMin = cell + hitboxes[i].min;
        const glm::vec3 boxMax = cell + hitboxes[i].max;

        if (
            playerMax.x > boxMin.x &&
            playerMin.x < boxMax.x &&
            playerMax.y > boxMin.y &&
            playerMin.y < boxMax.y &&
            playerMax.z > boxMin.z &&
            playerMin.z < boxMax.z
        ) {
            return true;
        }
    }

    return false;
}


// ============================================================
// BLOK KIRMA / KOYMA
// ============================================================

void mouseButtonCallback(
    GLFWwindow* window,
    int button,
    int action,
    int
) {
    if (menuActive) {
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
            double x, y; glfwGetCursorPos(window, &x, &y);
            handleMenuClick(window, x, y);
        }
        return;
    }
    if (!activeWorld)
        return;

    // Mouse buttons do not send GLFW_REPEAT, but accepting only PRESS makes
    // the single-action behaviour explicit if input handling changes later.
    if (action != GLFW_PRESS)
        return;

    if (button != GLFW_MOUSE_BUTTON_LEFT &&
        button != GLFW_MOUSE_BUTTON_RIGHT) {
        return;
    }

    glm::ivec3 hit;
    glm::ivec3 normal;

    if (!activeWorld->raycast(
            camera.position,
            camera.forward(),
            6.0f,
            hit,
            normal
        )) {
        return;
    }

    // Sol tık = kır
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        // raycast only returns selectable, non-Air blocks.
        activeWorld->setBlock(
            hit.x,
            hit.y,
            hit.z,
            Block::Air
        );

        return;
    }

    // Sağ tık = koy
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {

        glm::ivec3 place = hit + normal;

        if (!World::inWorldY(place.y)) {
            return;
        }

        const Block placeBlock = selectedBlock();

if (playerIntersectsBlock(player, place, placeBlock)) {
    return;
}

        // The ray hit is never Air. The adjacent cell must be actual empty
        // space; this deliberately does not replace plants/water yet.
        if (activeWorld->getBlock(place.x, place.y, place.z) != Block::Air) {
            return;
        }

        activeWorld->setBlock(
            place.x,
            place.y,
            place.z,
            placeBlock
        );
    }
}


// ============================================================
// INPUT
// ============================================================

void processInput(
    GLFWwindow* window,
    float dt,
    World& world
) {
    if (glfwGetKey(
            window,
            GLFW_KEY_ESCAPE
        ) == GLFW_PRESS) {

        glfwSetWindowShouldClose(
            window,
            true
        );
    }

    glm::vec3 fwd = camera.forward();

    fwd.y = 0.0f;

    if (glm::length(fwd) > 0.001f)
        fwd = glm::normalize(fwd);

    glm::vec3 right =
        glm::cross(
            fwd,
            glm::vec3(0.0f, 1.0f, 0.0f)
        );

    glm::vec3 move(0.0f);

    if (glfwGetKey(
            window,
            GLFW_KEY_W
        ) == GLFW_PRESS) {

        move += fwd;
    }

    if (glfwGetKey(
            window,
            GLFW_KEY_S
        ) == GLFW_PRESS) {

        move -= fwd;
    }

    if (glfwGetKey(
            window,
            GLFW_KEY_D
        ) == GLFW_PRESS) {

        move += right;
    }

    if (glfwGetKey(
            window,
            GLFW_KEY_A
        ) == GLFW_PRESS) {

        move -= right;
    }

    player.update(
        world,
        move,
        dt
    );


    // Space = zıpla

    static bool previousSpace = false;

    bool currentSpace =
        glfwGetKey(
            window,
            GLFW_KEY_SPACE
        ) == GLFW_PRESS;

    if (currentSpace && !previousSpace)
        player.jump();

    previousSpace = currentSpace;


    
    // 1-9 blok seçimi
    for (int i = 0; i < 9; ++i) {
        const int key = GLFW_KEY_1 + i;

        if (glfwGetKey(window, key) == GLFW_PRESS) {
            selectedSlot = i + 1;
        }
    }

    camera.position =
        player.eyePosition();
}


// ============================================================
// 3D BLOK PREVIEW RENGİ
// ============================================================

glm::vec3 blockPreviewColor(
    Block block,
    int face
) {
    switch (block) {

        case Block::Grass:

            if (face == 4)
                return glm::vec3(
                    0.25f,
                    0.72f,
                    0.18f
                );

            return glm::vec3(
                0.50f,
                0.32f,
                0.14f
            );


        case Block::Dirt:

            return glm::vec3(
                0.45f,
                0.28f,
                0.12f
            );


        case Block::Stone:

            return glm::vec3(
                0.48f,
                0.50f,
                0.53f
            );


        case Block::Wood:

            return glm::vec3(
                0.38f,
                0.25f,
                0.12f
            );


        case Block::Leaves:

            return glm::vec3(
                0.15f,
                0.50f,
                0.15f
            );


        default:

            return glm::vec3(1.0f);
    }
}


// ============================================================
// PREVIEW CUBE OLUŞTUR
// ============================================================

void createPreviewCube() {
    std::vector<Vertex> vertices;
    vertices.reserve(36);

    // Küpün 8 köşesi.
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

    // Yüzlerin köşe sıralaması Chunk.cpp ile uyumlu.
    static const int faces[6][4] = {
        {3, 2, 1, 0}, // -Z
        {4, 5, 6, 7}, // +Z
        {0, 4, 7, 3}, // -X
        {1, 2, 6, 5}, // +X
        {3, 7, 6, 2}, // +Y
        {0, 1, 5, 4}  // -Y
    };

    // Her yüz için ayrı UV koordinatları.
    // Her yüzün 4 köşesine ait UV değerleri kullanılır.
    static const glm::vec2 faceUV[6][4] = {
        {
            {0.0f, 1.0f}, {1.0f, 1.0f},
            {1.0f, 0.0f}, {0.0f, 0.0f}
        },
        {
            {0.0f, 0.0f}, {1.0f, 0.0f},
            {1.0f, 1.0f}, {0.0f, 1.0f}
        },
        {
            {0.0f, 0.0f}, {1.0f, 0.0f},
            {1.0f, 1.0f}, {0.0f, 1.0f}
        },
        {
            {0.0f, 0.0f}, {0.0f, 1.0f},
            {1.0f, 1.0f}, {1.0f, 0.0f}
        },
        {
            {0.0f, 0.0f}, {0.0f, 1.0f},
            {1.0f, 1.0f}, {1.0f, 0.0f}
        },
        {
            {0.0f, 0.0f}, {1.0f, 0.0f},
            {1.0f, 1.0f}, {0.0f, 1.0f}
        }
    };

    static const glm::vec2 torchUV[4] = {
        {0.375f, 0.0f}, {0.625f, 0.0f},
        {0.625f, 1.0f}, {0.375f, 1.0f}
    };

    const Block block = selectedBlock();
    const BlockProperties properties = blockProperties(block);
    const glm::vec2* previewUV =
        properties.shape == BlockShape::Torch ? torchUV : nullptr;

    // Varsayılan boyut: tam blok.
    glm::vec3 localMin(0.0f);
    glm::vec3 localMax(1.0f);

    // Slab: yarım yükseklik.
    if (properties.shape == BlockShape::Slab) {
        localMax.y = 0.5f;
    }
    // Torch: ince ve kısa.
    else if (properties.shape == BlockShape::Torch) {
        localMin = glm::vec3(0.375f, 0.0f, 0.375f);
        localMax = glm::vec3(0.625f, 0.75f, 0.625f);
    }

    // Her yüz iki üçgenden oluşur.
    static const int triangleIndices[6] = {
        0, 1, 2,
        0, 2, 3
    };

    for (int face = 0; face < 6; ++face) {
        const glm::vec3 color = blockPreviewColor(block, face);
        const float tileId =
            static_cast<float>(blockTextureId(block, face));

        for (int i = 0; i < 6; ++i) {
            // Üçgen indisleri yüzün dört köşesine işaret eder.
            const int cornerIndex = triangleIndices[i];
            const int vertexIndex = faces[face][cornerIndex];

            // Blok şekline göre köşe konumunu ölçekle.
            const glm::vec3 unitPosition = cube[vertexIndex];

            const glm::vec3 localPosition(
                localMin.x + unitPosition.x * (localMax.x - localMin.x),
                localMin.y + unitPosition.y * (localMax.y - localMin.y),
                localMin.z + unitPosition.z * (localMax.z - localMin.z)
            );

            // Önizlemenin merkezi dünya orijini olsun.
            const glm::vec3 centeredPosition =
                localPosition - glm::vec3(0.5f);

            vertices.push_back({
                centeredPosition,
                color,
                previewUV ? previewUV[cornerIndex] : faceUV[face][cornerIndex],
                tileId
            });
        }
    }

    if (previewVAO == 0) {
        glGenVertexArrays(1, &previewVAO);
    }

    if (previewVBO == 0) {
        glGenBuffers(1, &previewVBO);
    }

    glBindVertexArray(previewVAO);
    glBindBuffer(GL_ARRAY_BUFFER, previewVBO);

    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
        vertices.data(),
        GL_DYNAMIC_DRAW
    );

    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, position))
    );
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, color))
    );
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(
        2, 2, GL_FLOAT, GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, uv))
    );
    glEnableVertexAttribArray(2);

    glVertexAttribPointer(
        3, 1, GL_FLOAT, GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, textureId))
    );
    glEnableVertexAttribArray(3);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

// ============================================================
// CROSSHAIR
// ============================================================

void createCrosshair() {

    const float size = 0.018f;

    float vertices[] = {

        -size,  0.0f, 0.0f,
         size,  0.0f, 0.0f,

         0.0f, -size, 0.0f,
         0.0f,  size, 0.0f
    };


    glGenVertexArrays(
        1,
        &crosshairVAO
    );

    glGenBuffers(
        1,
        &crosshairVBO
    );


    glBindVertexArray(
        crosshairVAO
    );

    glBindBuffer(
        GL_ARRAY_BUFFER,
        crosshairVBO
    );


    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(vertices),
        vertices,
        GL_STATIC_DRAW
    );


    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        3 * sizeof(float),
        nullptr
    );

    glEnableVertexAttribArray(0);


    glBindVertexArray(0);
}


// ============================================================
// CROSSHAIR ÇİZ
// ============================================================

void renderCrosshair(
    Shader& shader
) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);


    shader.use();
    glUniform1i(glGetUniformLocation(shader.id, "uUseTexture"), 0);
    glUniform3f(glGetUniformLocation(shader.id, "uTint"), 1.0f, 1.0f, 1.0f);

    shader.setMat4(
        "uProjection",
        glm::mat4(1.0f)
    );

    shader.setMat4(
        "uView",
        glm::mat4(1.0f)
    );

    shader.setMat4(
        "uModel",
        glm::mat4(1.0f)
    );


    glBindVertexArray(
        crosshairVAO
    );


    glDrawArrays(
        GL_LINES,
        0,
        4
    );


    glBindVertexArray(0);


    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

// ============================================================
// BAKILAN BLOĞUN SEÇİM ÇERÇEVESİ
// ============================================================
void renderTargetOutline(Shader& shader, World& world) {
    glm::ivec3 hit, normal;

    if (!world.raycast(
            camera.position,
            camera.forward(),
            6.0f,
            hit,
            normal
        )) {
        return;
    }

    const float e = 0.003f;
    const float x0 = static_cast<float>(hit.x) - e;
    const float y0 = static_cast<float>(hit.y) - e;
    const float z0 = static_cast<float>(hit.z) - e;
    const float x1 = static_cast<float>(hit.x + 1) + e;
    const float y1 = static_cast<float>(hit.y + 1) + e;
    const float z1 = static_cast<float>(hit.z + 1) + e;
    const glm::vec3 p[] = {
        {x0,y0,z0},{x1,y0,z0}, {x1,y0,z0},{x1,y0,z1},
        {x1,y0,z1},{x0,y0,z1}, {x0,y0,z1},{x0,y0,z0},
        {x0,y1,z0},{x1,y1,z0}, {x1,y1,z0},{x1,y1,z1},
        {x1,y1,z1},{x0,y1,z1}, {x0,y1,z1},{x0,y1,z0},
        {x0,y0,z0},{x0,y1,z0}, {x1,y0,z0},{x1,y1,z0},
        {x1,y0,z1},{x1,y1,z1}, {x0,y0,z1},{x0,y1,z1}
    };
    if (!outlineVAO) glGenVertexArrays(1, &outlineVAO);
    if (!outlineVBO) glGenBuffers(1, &outlineVBO);
    glBindVertexArray(outlineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, outlineVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(p), p, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glEnableVertexAttribArray(0);

    shader.use();
    glUniform1i(glGetUniformLocation(shader.id, "uUseTexture"), 0);
    glUniform3f(glGetUniformLocation(shader.id, "uTint"), 0.05f, 0.05f, 0.05f);
    shader.setMat4("uProjection", glm::perspective(glm::radians(75.0f),
        static_cast<float>(std::max(1, WIDTH)) / static_cast<float>(std::max(1, HEIGHT)), 0.1f, 500.0f));
    // The main loop's projection may use a resized window, so set it from the current viewport below.
    GLint viewport[4]; glGetIntegerv(GL_VIEWPORT, viewport);
    shader.setMat4("uProjection", glm::perspective(glm::radians(75.0f),
        static_cast<float>(std::max(1, viewport[2])) / static_cast<float>(std::max(1, viewport[3])), 0.1f, 500.0f));
    shader.setMat4("uView", camera.view());
    shader.setMat4("uModel", glm::mat4(1.0f));
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);
    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, 24);
    glLineWidth(1.0f);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glBindVertexArray(0);
}

// ============================================================
// 3D BLOK PREVIEW ÇİZ
// ============================================================

void renderBlockPreview(
    Shader& shader,
    int width,
    int height
) {
    // Seçilen blok değişmiş olabilir.
    // Her frame güncellemek basit ve şimdilik yeterli.
    createPreviewCube();


    const int previewSize = 150;

    const int margin = 25;


    // OpenGL viewport koordinatı alttan başlar.
    const int viewportX =
        width - previewSize - margin;

    const int viewportY =
        height - previewSize - margin;


    glViewport(
        viewportX,
        viewportY,
        previewSize,
        previewSize
    );


    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);


    shader.use();
    glUniform1i(glGetUniformLocation(shader.id, "uUseTexture"), 1);
    glUniform1i(glGetUniformLocation(shader.id, "uAtlas"), 0);
    glUniform3f(glGetUniformLocation(shader.id, "uTint"), 1.0f, 1.0f, 1.0f);
    bindBlockTextureAtlas(blockAtlasTexture);

    glm::mat4 projection =
        glm::perspective(
            glm::radians(45.0f),
            1.0f,
            0.1f,
            100.0f
        );


    glm::mat4 view =
        glm::lookAt(
            glm::vec3(
                2.8f,
                2.2f,
                2.8f
            ),

            glm::vec3(0.0f),

            glm::vec3(
                0.0f,
                1.0f,
                0.0f
            )
        );


    float time =
        static_cast<float>(
            glfwGetTime()
        );


    glm::mat4 model =
        glm::rotate(
            glm::mat4(1.0f),

            time * glm::radians(35.0f),

            glm::vec3(
                0.0f,
                1.0f,
                0.0f
            )
        );


    model =
        glm::rotate(
            model,

            glm::radians(-12.0f),

            glm::vec3(
                1.0f,
                0.0f,
                0.0f
            )
        );


    shader.setMat4(
        "uProjection",
        projection
    );

    shader.setMat4(
        "uView",
        view
    );

    shader.setMat4(
        "uModel",
        model
    );


    glBindVertexArray(
        previewVAO
    );


    glDrawArrays(
        GL_TRIANGLES,
        0,
        36
    );


    glBindVertexArray(0);


    // Ana viewport'a geri dön.
    glViewport(
        0,
        0,
        width,
        height
    );
}


// ============================================================
// MAIN
// ============================================================

int main() {

    if (!glfwInit())
        return -1;


    glfwWindowHint(
        GLFW_CONTEXT_VERSION_MAJOR,
        3
    );

    glfwWindowHint(
        GLFW_CONTEXT_VERSION_MINOR,
        3
    );

    glfwWindowHint(
        GLFW_OPENGL_PROFILE,
        GLFW_OPENGL_CORE_PROFILE
    );


    GLFWwindow* window =
        glfwCreateWindow(
            WIDTH,
            HEIGHT,
            "VoxelCraft",
            nullptr,
            nullptr
        );


    if (!window) {

        glfwTerminate();

        return -1;
    }


    glfwMakeContextCurrent(window);

    glfwSwapInterval(1);


    glfwSetCursorPosCallback(
        window,
        mouseCallback
    );


    glfwSetMouseButtonCallback(
        window,
        mouseButtonCallback
    );


    glfwSetInputMode(
        window,
        GLFW_CURSOR,
        GLFW_CURSOR_NORMAL
    );


    if (!gladLoadGL(
            glfwGetProcAddress
        )) {

        std::cerr
            << "GLAD yuklenemedi!"
            << std::endl;


        glfwDestroyWindow(window);
        glfwTerminate();

        return -1;
    }


    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);


    try {

        Shader shader(
            "shaders/block.vert",
            "shaders/block.frag"
        );

        blockAtlasTexture = createBlockTextureAtlas("assets/blocks");

        std::unique_ptr<World> world;

        const auto startWorld = [&]() {
            world = std::make_unique<World>(pendingWorldSettings);
            activeWorld = world.get();
            int spawnY = WORLD_MAX_Y;
            while (spawnY >= WORLD_MIN_Y &&
                   !blockProperties(world->getBlock(48, spawnY, 48)).solid) {
                --spawnY;
            }
            ++spawnY;
            while (spawnY <= WORLD_MAX_Y &&
                   world->getBlock(48, spawnY, 48) == Block::Water) {
                ++spawnY;
            }
            player.position = glm::vec3(48.0f, static_cast<float>(spawnY), 48.0f);
            player.velocity = glm::vec3(0.0f);
            camera.position = player.eyePosition();
            firstMouse = true;
            menuActive = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        };


        // GUI VAO'ları
        createCrosshair();
        createPreviewCube();


        double lastTime =
            glfwGetTime();


        while (
            !glfwWindowShouldClose(window)
        ) {

            double current =
                glfwGetTime();


            float dt =
                static_cast<float>(
                    current - lastTime
                );


            lastTime = current;


            // Delta time çok büyürse fizik patlamasın.
            if (dt > 0.05f)
                dt = 0.05f;

            glfwPollEvents();

            if (startWorldRequested) {
                startWorldRequested = false;
                startWorld();
            }

            int w;
            int h;
            glfwGetFramebufferSize(window, &w, &h);
            if (w <= 0 || h <= 0) continue;

            if (menuActive) {
                glViewport(0, 0, w, h);
                glClearColor(0.12f, 0.09f, 0.06f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                renderMainMenu(shader, w, h);
                glfwSwapBuffers(window);
                continue;
            }


            processInput(
                window,
                dt,
                *world
            );


            const int playerChunkX = static_cast<int>(
    std::floor(player.position.x / CHUNK_SIZE)
);

const int playerChunkZ = static_cast<int>(
    std::floor(player.position.z / CHUNK_SIZE)
);

world->updateStreaming(playerChunkX, playerChunkZ, player.position.y);


            // =================================================
            // DÜNYA
            // =================================================

            glViewport(
                0,
                0,
                w,
                h
            );


            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);


            glClearColor(
                0.48f,
                0.70f,
                0.92f,
                1.0f
            );


            glClear(
                GL_COLOR_BUFFER_BIT |
                GL_DEPTH_BUFFER_BIT
            );


            shader.use();
            glUniform1i(glGetUniformLocation(shader.id, "uUseTexture"), 1);
            glUniform1i(glGetUniformLocation(shader.id, "uAtlas"), 0);
            glUniform3f(glGetUniformLocation(shader.id, "uTint"), 1.0f, 1.0f, 1.0f);
            bindBlockTextureAtlas(blockAtlasTexture);

            glm::mat4 projection =
                glm::perspective(
                    glm::radians(75.0f),

                    static_cast<float>(w) /
                        static_cast<float>(h),

                    0.1f,
                    500.0f
                );


            shader.setMat4(
                "uProjection",
                projection
            );


            shader.setMat4(
                "uView",
                camera.view()
            );


            shader.setMat4(
                "uModel",
                glm::mat4(1.0f)
            );


            world->render(projection * camera.view());

            renderTargetOutline(shader, *world);

            // =================================================
            // SAĞ ÜST 3D BLOK
            // =================================================

            renderBlockPreview(
                shader,
                w,
                h
            );


            // =================================================
            // CROSSHAIR
            // =================================================

            glViewport(
                0,
                0,
                w,
                h
            );


            renderCrosshair(shader);


            // =================================================
            // FRAME
            // =================================================

            glfwSwapBuffers(window);
        }

    }
    catch (
        const std::exception& e
    ) {

        std::cerr
            << "Hata: "
            << e.what()
            << std::endl;
    }


    activeWorld = nullptr;


    if (previewVAO)
        glDeleteVertexArrays(
            1,
            &previewVAO
        );


    if (previewVBO)
        glDeleteBuffers(
            1,
            &previewVBO
        );


    if (crosshairVAO)
        glDeleteVertexArrays(
            1,
            &crosshairVAO
        );


    if (crosshairVBO)
        glDeleteBuffers(
            1,
            &crosshairVBO
        );
    if (outlineVAO) glDeleteVertexArrays(1, &outlineVAO);
    if (outlineVBO) glDeleteBuffers(1, &outlineVBO);
    if (blockAtlasTexture) glDeleteTextures(1, &blockAtlasTexture);

    glfwDestroyWindow(window);

    glfwTerminate();


    return 0;
}
