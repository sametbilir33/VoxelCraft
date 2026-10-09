#include "World.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>

namespace {
std::size_t mix(std::size_t a, std::size_t b) { return a ^ (b + 0x9e3779b9u + (a << 6) + (a >> 2)); }
float smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }
float lerp(float a, float b, float t) { return a + (b - a) * t; }
bool rayBox(const glm::vec3& o, const glm::vec3& d, const glm::vec3& mn, const glm::vec3& mx, float maxD, float& hit, glm::ivec3& n) {
    float lo = 0.0f, hi = maxD; n = glm::ivec3(0);
    for (int a = 0; a < 3; ++a) {
        if (std::abs(d[a]) < 1e-7f) { if (o[a] < mn[a] || o[a] > mx[a]) return false; continue; }
        float t1 = (mn[a] - o[a]) / d[a], t2 = (mx[a] - o[a]) / d[a]; int sign = -1;
        if (t1 > t2) { std::swap(t1, t2); sign = 1; }
        if (t1 > lo) { lo = t1; n = glm::ivec3(0); n[a] = sign; }
        hi = std::min(hi, t2); if (lo > hi) return false;
    }
    hit = lo; return hi >= 0.0f && lo <= maxD;
}
}

std::size_t ChunkPositionHash::operator()(const ChunkPosition& p) const { return mix(mix(std::hash<int>{}(p.x), std::hash<int>{}(p.y)), std::hash<int>{}(p.z)); }
std::size_t BlockPositionHash::operator()(const BlockPosition& p) const { return mix(mix(std::hash<int>{}(p.x), std::hash<int>{}(p.y)), std::hash<int>{}(p.z)); }

World::World(WorldSettings worldSettings) : settings(worldSettings) { std::random_device rd; seed = (std::uint32_t(rd()) << 16) ^ rd(); std::cout << "World seed: " << seed << '\n'; generateWorld(); }
int World::floorDiv(int v, int d) { int q = v / d, r = v % d; return q - (r < 0 ? 1 : 0); }
int World::floorMod(int v, int d) { int r = v % d; return r < 0 ? r + d : r; }
std::uint32_t World::hash2D(int x, int z) const { std::uint32_t h=seed; h^=std::uint32_t(x)*0x8da6b343u; h^=std::uint32_t(z)*0xd8163841u; h^=h>>13; h*=0x85ebca6bu; h^=h>>16; h*=0xc2b2ae35u; return h^(h>>16); }
std::uint32_t World::hash3D(int x,int y,int z) const { std::uint32_t h=hash2D(x,z)^std::uint32_t(y)*0xcb1ab31fu; h^=h>>15; h*=0x2c1b3c6du; h^=h>>12; h*=0x297a2d39u; return h^(h>>15); }
float World::random01(int x,int z) const { return float(hash2D(x,z)&0xffffffu)/float(0x1000000u); }
float World::noise2D(float x,float z) const { int ix=int(std::floor(x)), iz=int(std::floor(z)); float fx=smoothstep(x-ix), fz=smoothstep(z-iz); auto v=[this](int a,int b){return random01(a,b)*2-1;}; return lerp(lerp(v(ix,iz),v(ix+1,iz),fx),lerp(v(ix,iz+1),v(ix+1,iz+1),fx),fz); }
float World::noise3D(float x,float y,float z) const { int ix=int(std::floor(x)),iy=int(std::floor(y)),iz=int(std::floor(z)); float fx=smoothstep(x-ix),fy=smoothstep(y-iy),fz=smoothstep(z-iz); auto v=[this](int a,int b,int c){return float(hash3D(a,b,c)&0xffffffu)/float(0x800000u)-1;}; float x00=lerp(v(ix,iy,iz),v(ix+1,iy,iz),fx),x10=lerp(v(ix,iy+1,iz),v(ix+1,iy+1,iz),fx),x01=lerp(v(ix,iy,iz+1),v(ix+1,iy,iz+1),fx),x11=lerp(v(ix,iy+1,iz+1),v(ix+1,iy+1,iz+1),fx); return lerp(lerp(x00,x10,fy),lerp(x01,x11,fy),fz); }
float World::oceanFactor(int x, int z) const {
    if (!settings.oceansEnabled) return 0.0f;
    // Very low frequency continental noise creates connected, large oceans
    // rather than isolated one-chunk ponds.
    const float continental = noise2D(x * 0.0018f, z * 0.0018f);
    // Only the deepest continental lows become oceans. The low frequency is
    // deliberately unchanged, so the fewer oceans remain large.
    return smoothstep(std::clamp((-continental - settings.oceanThreshold) / 0.30f, 0.0f, 1.0f));
}

float World::terrainHeight(int x,int z) const {
    float total=0,amp=1,freq=.015f,sum=0;
    for(int i=0;i<5;++i){ total+=noise2D(x*freq,z*freq)*amp; sum+=amp; amp*=.5f; freq*=2; }
    const float land = 14.0f + (total/sum)*22.0f*settings.terrainAmplitude + noise2D(x*.006f,z*.006f)*8.0f*settings.terrainAmplitude;
    const float SEA_LEVEL = static_cast<float>(settings.seaLevel);
    const float oceanFloor = SEA_LEVEL - 6.0f + noise2D(x*.025f, z*.025f)*2.5f;
    return std::clamp(std::floor(lerp(land, oceanFloor, oceanFactor(x, z))), 3.0f, 55.0f);
}

bool World::isCave(int x,int y,int z,int surface) const {
    if (!settings.cavesEnabled) return false;
    if (y < WORLD_MIN_Y + 2 || y > surface) return false;

    // Rare, deterministic shafts connect some underground systems to the
    // surface. Their 64-block grid keeps entrances sparse and readable.
    if (y >= surface - 18) {
        for (int gx = floorDiv(x, 64) - 1; gx <= floorDiv(x, 64) + 1; ++gx) {
            for (int gz = floorDiv(z, 64) - 1; gz <= floorDiv(z, 64) + 1; ++gz) {
                if (random01(gx + 503, gz - 911) > 0.12f * settings.caveDensity) continue;
                const int ex = gx * 64 + int(random01(gx - 73, gz + 31) * 64.0f);
                const int ez = gz * 64 + int(random01(gx + 19, gz - 47) * 64.0f);
                if (oceanFactor(ex, ez) > 0.30f) continue;
                const int entranceSurface = int(terrainHeight(ex, ez));
                const int dx = x - ex, dz = z - ez;
                if (dx * dx + dz * dz <= 4 && y >= entranceSurface - 18 && y <= entranceSurface)
                    return true;
            }
        }
    }

    // Two coherent low-frequency fields make connected chambers/tunnels;
    // the old high-frequency threshold produced peppered, random holes.
    if (y >= surface - 3) return false;
    const float chamber = noise3D(x*.045f, y*.055f, z*.045f);
    const float detail = noise3D(x*.090f + 31.0f, y*.095f + 17.0f, z*.090f - 11.0f);
    return chamber > 0.46f + (1.0f - settings.caveDensity) * 0.18f && detail > -0.20f;
}

Block World::generatedBlockAt(int x,int y,int z) const {
    if (!inWorldY(y)) return Block::Air;
    return generatedBlockAt(x, y, z, int(terrainHeight(x, z)));
}

Block World::generatedBlockAt(int x, int y, int z, int surface) const {
    if (!inWorldY(y)) return Block::Air;
    const int SEA_LEVEL = settings.seaLevel;
    if (y > surface) {
        if (settings.oceansEnabled && y <= SEA_LEVEL) return Block::Water;
        // No tree can extend farther than this. This fast path removes the
        // expensive nearby-tree scan for almost all air subchunk cells.
        if (y > surface + 8) return Block::Air;
        // Same deterministic tree placement as the former vegetation pass,
        // evaluated on demand so a tree never forces distant sections alive.
        const int gridX = floorDiv(x, 12);
        const int gridZ = floorDiv(z, 12);
        for (int gx = gridX - 1; gx <= gridX + 1; ++gx) {
            for (int gz = gridZ - 1; gz <= gridZ + 1; ++gz) {
                if (random01(gx + 231, gz - 491) > 0.13f) continue;
                const int tx = gx * 12 + int(random01(gx, gz + 91) * 12.0f);
                const int tz = gz * 12 + int(random01(gx - 71, gz) * 12.0f);
                const int baseSurface = int(terrainHeight(tx, tz));
                if (settings.oceansEnabled && (baseSurface <= SEA_LEVEL + 2 || oceanFactor(tx, tz) > 0.30f)) continue;
                const int base = baseSurface + 1;
                const int treeHeight = 4 + int(random01(tx + 97, tz - 53) * 2.0f);
                if (x == tx && z == tz && y >= base && y < base + treeHeight)
                    return Block::Wood;
                const int top = base + treeHeight - 1;
                const int dx = std::abs(x - tx), dz = std::abs(z - tz);
                if (dx <= 2 && dz <= 2 && y >= top - 1 && y <= top + 2 &&
                    dx + dz <= 3 && !(y == top + 2 && dx + dz > 1))
                    return Block::Leaves;
            }
        }
        if (y == surface + 1 && random01(x + 7919, z - 1543) < .035f)
            return Block::ShortGrass;
        return Block::Air;
    }
    if (isCave(x,y,z,surface)) return Block::Air;
    // Ocean floors and a narrow shore band are sandy.
    if (y == surface) return (settings.oceansEnabled && (surface <= SEA_LEVEL + 2 || oceanFactor(x, z) > 0.30f))
        ? Block::Sand : Block::Grass;
    if (settings.oceansEnabled && surface <= SEA_LEVEL + 2 && y >= surface - 4) return Block::Sand;
    return y >= surface-3 ? Block::Dirt : Block::Stone;
}
Chunk& World::ensureChunk(int cx,int cy,int cz) {
    ChunkPosition p{cx,cy,cz}; auto it=chunks.find(p); if(it != chunks.end()) return it->second;
    auto [created, ok] = chunks.try_emplace(p,cx,cy,cz); generateChunkTerrain(created->second);
    for (int dx=-1;dx<=1;++dx) for(int dy=-1;dy<=1;++dy) for(int dz=-1;dz<=1;++dz) if(std::abs(dx)+std::abs(dy)+std::abs(dz)==1) { auto n=chunks.find({cx+dx,cy+dy,cz+dz}); if(n!=chunks.end()) n->second.modified=true; }
    return created->second;
}
void World::generateChunkTerrain(Chunk& c) {
    int ox=c.chunkX*CHUNK_SIZE, oy=c.chunkY*SUBCHUNK_HEIGHT, oz=c.chunkZ*CHUNK_SIZE;
    // Height is constant for a vertical column. Calculating it once instead
    // of once per block removes thousands of octave-noise evaluations per
    // generated subchunk.
    for(int x=0;x<CHUNK_SIZE;++x) for(int z=0;z<CHUNK_SIZE;++z) {
        const int wx=ox+x, wz=oz+z;
        const int surface = int(terrainHeight(wx, wz));
        for(int y=0;y<SUBCHUNK_HEIGHT;++y) {
            const int wy=oy+y;
            const Block block=generatedBlockAt(wx, wy, wz, surface);
            auto edit=blockEdits.find({wx,wy,wz});
            c.blocks[x][y][z]=edit==blockEdits.end()?block:edit->second;
        }
    }
    c.modified=true;
}
Block World::getBlock(int x,int y,int z) const {
    if(!inWorldY(y)) return Block::Air; auto edit=blockEdits.find({x,y,z}); if(edit!=blockEdits.end()) return edit->second;
    int cx=floorDiv(x,CHUNK_SIZE),cy=floorDiv(y,SUBCHUNK_HEIGHT),cz=floorDiv(z,CHUNK_SIZE); auto it=chunks.find({cx,cy,cz});
    if(it==chunks.end()) return generatedBlockAt(x,y,z);
    return it->second.blocks[floorMod(x,CHUNK_SIZE)][floorMod(y,SUBCHUNK_HEIGHT)][floorMod(z,CHUNK_SIZE)];
}
void World::markChunkAndNeighborsModified(int x,int y,int z) { int cx=floorDiv(x,16),cy=floorDiv(y,16),cz=floorDiv(z,16); for(int i=0;i<7;++i){ static const int o[7][3]={{0,0,0},{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}}; auto it=chunks.find({cx+o[i][0],cy+o[i][1],cz+o[i][2]}); if(it!=chunks.end()) it->second.modified=true; } }
bool World::setBlock(int x, int y, int z, Block block) {
    if (!inWorldY(y) || getBlock(x, y, z) == block) {
        return false;
    }

    blockEdits[{x, y, z}] = block;
    Chunk& chunk = ensureChunk(
        floorDiv(x, CHUNK_SIZE),
        floorDiv(y, SUBCHUNK_HEIGHT),
        floorDiv(z, CHUNK_SIZE)
    );
    chunk.blocks[floorMod(x, CHUNK_SIZE)]
                [floorMod(y, SUBCHUNK_HEIGHT)]
                [floorMod(z, CHUNK_SIZE)] = block;

    // Only this section and its six face-neighbours can gain/lose a face.
    markChunkAndNeighborsModified(x, y, z);
    return true;
}
void World::generateWorld() { updateStreaming(3, 3, 32.0f); }
void World::updateStreaming(int ccx,int ccz,float py) {
    int playerSection=floorDiv(int(std::floor(py)),SUBCHUNK_HEIGHT);
    for(auto it=chunks.begin();it!=chunks.end();) { const auto&p=it->first; if(std::abs(p.x-ccx)>LOAD_RADIUS || std::abs(p.z-ccz)>LOAD_RADIUS || (std::abs(p.y-playerSection)>2 && p.y!=floorDiv(int(terrainHeight(p.x*16+8,p.z*16+8)),16))) it=chunks.erase(it); else ++it; }
    // Surface and player-adjacent sections are the only sections materialized. At most four each frame avoids stalls.
    int made=0;
    // Center-first shells make the spawn/player column available before the
    // outer edge of the retained 7x7 area.
    for (int radius = 0; radius <= LOAD_RADIUS && made < 4; ++radius) {
        for (int dx = -radius; dx <= radius && made < 4; ++dx) {
            for (int dz = -radius; dz <= radius && made < 4; ++dz) {
                if (std::max(std::abs(dx), std::abs(dz)) != radius) continue;
                const int x = ccx + dx, z = ccz + dz;
                const int surfaceSection = floorDiv(int(terrainHeight(x * 16 + 8, z * 16 + 8)), 16);
                for (int sy : {surfaceSection, playerSection, playerSection - 1, playerSection + 1}) {
                    if (sy * 16 < WORLD_MIN_Y || sy * 16 > WORLD_MAX_Y || chunks.contains({x, sy, z})) continue;
                    ensureChunk(x, sy, z);
                    if (++made >= 4) break;
                }
            }
        }
    }
}
bool World::isVisible(const Chunk& c, const glm::mat4& clip) const {
    // GLM indexes matrices as [column][row].  Frustum extraction, however,
    // is defined in terms of rows of projection * view.  Adding clip[3] and
    // clip[0] directly mixed columns and produced camera-angle-dependent,
    // invalid planes.
    const glm::vec4 row0(clip[0][0], clip[1][0], clip[2][0], clip[3][0]);
    const glm::vec4 row1(clip[0][1], clip[1][1], clip[2][1], clip[3][1]);
    const glm::vec4 row2(clip[0][2], clip[1][2], clip[2][2], clip[3][2]);
    const glm::vec4 row3(clip[0][3], clip[1][3], clip[2][3], clip[3][3]);
    glm::vec4 planes[6] = {
        row3 + row0, row3 - row0, // left, right
        row3 + row1, row3 - row1, // bottom, top
        row3 + row2, row3 - row2  // near, far
    };

    const glm::vec3 minimum(c.chunkX * CHUNK_SIZE,
                            c.chunkY * SUBCHUNK_HEIGHT,
                            c.chunkZ * CHUNK_SIZE);
    const glm::vec3 center = minimum + glm::vec3(CHUNK_SIZE, SUBCHUNK_HEIGHT, CHUNK_SIZE) * 0.5f;
    const glm::vec3 halfExtent(CHUNK_SIZE * 0.5f, SUBCHUNK_HEIGHT * 0.5f, CHUNK_SIZE * 0.5f);

    for (glm::vec4& plane : planes) {
        const glm::vec3 unnormalizedNormal(plane);
        const float length = glm::length(unnormalizedNormal);
        if (length <= 1e-6f) {
            // A degenerate plane must never discard all world geometry.
            continue;
        }
        plane /= length;
        const glm::vec3 normal(plane);
        const float signedDistance = glm::dot(normal, center) + plane.w;
        const float projectedRadius = glm::dot(glm::abs(normal), halfExtent);
        if (signedDistance < -projectedRadius) {
            return false;
        }
    }
    return true;
}
void World::render(const glm::mat4& vp) { int updates=0; for(auto& e:chunks) { Chunk& c=e.second; if(c.modified && updates<2) { c.updateMesh(*this); ++updates; } if(isVisible(c,vp)) c.render(); } }
bool World::raycast(const glm::vec3&o,const glm::vec3&d,float maxD,glm::ivec3& hit,glm::ivec3& normal) { glm::vec3 end=o+d*maxD; int minX=int(std::floor(std::min(o.x,end.x))),maxX=int(std::floor(std::max(o.x,end.x))),minY=std::max(WORLD_MIN_Y,int(std::floor(std::min(o.y,end.y)))),maxY=std::min(WORLD_MAX_Y,int(std::floor(std::max(o.y,end.y)))),minZ=int(std::floor(std::min(o.z,end.z))),maxZ=int(std::floor(std::max(o.z,end.z))); float closest=maxD; bool found=false; for(int x=minX;x<=maxX;++x)for(int y=minY;y<=maxY;++y)for(int z=minZ;z<=maxZ;++z){Block b=getBlock(x,y,z);if(!blockProperties(b).selectable)continue;std::array<BlockAABB,2> boxes{};auto count=blockHitboxes(b,boxes);if(!count){boxes[0]={glm::vec3(0),glm::vec3(1)};count=1;}for(std::size_t i=0;i<count;++i){float dist;glm::ivec3 n;if(rayBox(o,d,glm::vec3(x,y,z)+boxes[i].min,glm::vec3(x,y,z)+boxes[i].max,closest,dist,n)&&dist>=0&&dist<=closest){closest=dist;hit={x,y,z};normal=n;found=true;}}}return found; }
