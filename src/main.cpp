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
    GLFWwindow*,
    int button,
    int action,
    int
) {
    if (!activeWorld)
        return;

    if (action != GLFW_PRESS)
        return;

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

        if (place.y < 0 ||
            place.y >= CHUNK_HEIGHT) {
            return;
        }

        const Block placeBlock = selectedBlock();

if (playerIntersectsBlock(player, place, placeBlock)) {
    return;
}

        if (activeWorld->getBlock(
                place.x,
                place.y,
                place.z
            ) != Block::Air) {
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

    const Block block = selectedBlock();
    const BlockProperties properties = blockProperties(block);

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
                faceUV[face][cornerIndex],
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
// DEBUG: KAMERA YÖNÜ, HEDEF BLOK VE BLOK YÜZÜ
// ============================================================

void debugTargetInfo(
    World& world,
    const glm::ivec3& hit,
    const glm::ivec3& normal
) {
    static bool initialized = false;
    static glm::ivec3 lastHit(-999999);
    static glm::ivec3 lastNormal(0);
    static int lastDirection = -1;

    const glm::vec3 forward = glm::normalize(camera.forward());

    // Kuzey: -Z, Doğu: +X, Güney: +Z, Batı: -X
    float angle = glm::degrees(
        std::atan2(forward.x, -forward.z)
    );

    if (angle < 0.0f)
        angle += 360.0f;

    const char* directions[] = {
        "N", "NE", "E", "SE",
        "S", "SW", "W", "NW"
    };

    const int direction = static_cast<int>(
        std::floor((angle + 22.5f) / 45.0f)
    ) % 8;

    // Chunk.cpp içindeki yüz sıralamasıyla aynı.
    int face = -1;
    const char* faceName = "UNKNOWN";

    if (normal.z < 0) {
        face = 0;
        faceName = "-Z / NORTH";
    } else if (normal.z > 0) {
        face = 1;
        faceName = "+Z / SOUTH";
    } else if (normal.x < 0) {
        face = 2;
        faceName = "-X / WEST";
    } else if (normal.x > 0) {
        face = 3;
        faceName = "+X / EAST";
    } else if (normal.y > 0) {
        face = 4;
        faceName = "+Y / TOP";
    } else if (normal.y < 0) {
        face = 5;
        faceName = "-Y / BOTTOM";
    }

    // Aynı bilgi her karede konsolu doldurmasın.
    const bool changed =
        !initialized ||
        hit != lastHit ||
        normal != lastNormal ||
        direction != lastDirection;

    if (!changed)
        return;

    initialized = true;
    lastHit = hit;
    lastNormal = normal;
    lastDirection = direction;

    const Block block = world.getBlock(
        hit.x,
        hit.y,
        hit.z
    );

    std::cout
        << "\n========== VOXEL DEBUG ==========\n"
        << "Kamera yonu : " << directions[direction]
        << " (" << angle << " derece)\n"
        << "Forward     : "
        << forward.x << ", "
        << forward.y << ", "
        << forward.z << '\n'
        << "Hedef blok  : " << blockName(block) << '\n'
        << "Koordinat   : "
        << hit.x << ", "
        << hit.y << ", "
        << hit.z << '\n'
        << "Hedef yuz   : " << faceName << '\n'
        << "Normal      : "
        << normal.x << ", "
        << normal.y << ", "
        << normal.z << '\n';

    if (face >= 0) {
        std::cout
            << "Atlas tile  : "
            << blockTextureId(block, face)
            << '\n';
    }

    std::cout
        << "=================================\n";
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

    debugTargetInfo(world, hit, normal);

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
        GLFW_CURSOR_DISABLED
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

        World world;

        activeWorld = &world;


        player.position =
            glm::vec3(
                48.0f,
                40.0f,
                48.0f
            );


        camera.position =
            player.eyePosition();


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


            processInput(
                window,
                dt,
                world
            );


            const int playerChunkX = static_cast<int>(
    std::floor(player.position.x / CHUNK_SIZE)
);

const int playerChunkZ = static_cast<int>(
    std::floor(player.position.z / CHUNK_SIZE)
);

world.updateStreaming(playerChunkX, playerChunkZ);

            glfwPollEvents();


            int w;
            int h;


            glfwGetFramebufferSize(
                window,
                &w,
                &h
            );


            if (w <= 0 || h <= 0)
                continue;


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


            world.render();

            renderTargetOutline(shader, world);

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