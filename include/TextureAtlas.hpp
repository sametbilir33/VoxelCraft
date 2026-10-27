#pragma once
#include <glad/gl.h>
#include "Block.hpp"

GLuint createBlockTextureAtlas(const char* blocksDirectory);
void bindBlockTextureAtlas(GLuint texture);
int blockTextureId(Block block, int face);
