cmake_minimum_required(VERSION 3.20)

project(VoxelCraft C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(PNG REQUIRED)

add_executable(VoxelCraft
    src/main.cpp
    src/Shader.cpp
    src/TextureAtlas.cpp
    src/Camera.cpp
    src/Chunk.cpp
    src/World.cpp
    src/Player.cpp

    external/glad/src/gl.c
)

target_include_directories(VoxelCraft PRIVATE
    include
    external/glad/include
)

target_link_libraries(VoxelCraft PRIVATE
    glfw
    glm
    PNG::PNG
)