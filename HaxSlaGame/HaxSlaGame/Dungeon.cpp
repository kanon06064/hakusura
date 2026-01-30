#include "Dungeon.h"
#include "raymath.h"
#include <math.h>

bool Room::Contains(int gx, int gy) { return (gx >= x && gx < x + width && gy >= y && gy < y + height); }
Dungeon::Dungeon() { Generate(true); }
void Dungeon::Generate(bool homeMode) {
    isHome = homeMode; rooms.clear();
    stairsDownPos = Vector3{ -999,-999,-999 }; stairsUpPos = Vector3{ -999,-999,-999 }; storageBoxPos = Vector3{ -999,-999,-999 };
    for (int y = 0; y < MAP_HEIGHT; y++) for (int x = 0; x < MAP_WIDTH; x++) { map[x][y] = 1; discovered[x][y] = homeMode; }
    if (homeMode) {
        Room c = { 15, 15, 10, 10 }; rooms.push_back(c);
        for (int ry = c.y; ry < c.y + c.height; ry++) for (int rx = c.x; rx < c.x + c.width; rx++) map[rx][ry] = 0;
        stairsDownPos = Vector3{ 20.0f * TILE_SIZE, 0.1f, 24.0f * TILE_SIZE };
        storageBoxPos = Vector3{ 17.0f * TILE_SIZE, 0.5f, 17.0f * TILE_SIZE };
    }
    else {
        for (int i = 0; i < 8; i++) {
            int w = GetRandomValue(5, 10), h = GetRandomValue(5, 10);
            int x = GetRandomValue(1, MAP_WIDTH - w - 1), y = GetRandomValue(1, MAP_HEIGHT - h - 1);
            Room nr = { x, y, w, h };
            for (int ry = y; ry < y + h; ry++) for (int rx = x; rx < x + w; rx++) map[rx][ry] = 0;
            if (!rooms.empty()) DigCorridor(rooms.back().x + rooms.back().width / 2, rooms.back().y + rooms.back().height / 2, x + w / 2, y + h / 2);
            rooms.push_back(nr);
        }
        stairsUpPos = Vector3{ (float)rooms[0].x * TILE_SIZE + 1.2f, 0.1f, (float)rooms[0].y * TILE_SIZE + 1.2f };
        stairsDownPos = Vector3{ (float)rooms.back().x * TILE_SIZE + 1.2f, 0.1f, (float)rooms.back().y * TILE_SIZE + 1.2f };
    }
}
void Dungeon::DigCorridor(int x1, int y1, int x2, int y2) {
    for (int x = (int)fminf((float)x1, (float)x2); x <= (int)fmaxf((float)x1, (float)x2); x++) map[x][y1] = 0;
    for (int y = (int)fminf((float)y1, (float)y2); y <= (int)fmaxf((float)y1, (float)y2); y++) map[x2][y] = 0;
}
void Dungeon::UpdateVisibility(Vector3 p) {
    if (isHome) return;
    int gx = (int)floorf((p.x + TILE_SIZE / 2.0f) / TILE_SIZE), gz = (int)floorf((p.z + TILE_SIZE / 2.0f) / TILE_SIZE);
    for (int y = gz - 4; y <= gz + 4; y++) for (int x = gx - 4; x <= gx + 4; x++) if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT) discovered[x][y] = true;
}
void Dungeon::Draw() {
    for (int y = 0; y < MAP_HEIGHT; y++) for (int x = 0; x < MAP_WIDTH; x++) {
        if (!discovered[x][y]) continue;
        Vector3 pos = Vector3{ x * TILE_SIZE, 0.0f, y * TILE_SIZE };
        if (map[x][y] == 1) DrawCube(pos, TILE_SIZE, 2.0f, TILE_SIZE, GRAY);
        else DrawPlane(pos, Vector2{ TILE_SIZE, TILE_SIZE }, isHome ? DARKBLUE : DARKGREEN);
    }
    if (stairsDownPos.x != -999 && IsDiscovered(stairsDownPos.x, stairsDownPos.z)) DrawCube(stairsDownPos, 1.2f, 0.1f, 1.2f, GOLD);
    if (stairsUpPos.x != -999 && IsDiscovered(stairsUpPos.x, stairsUpPos.z)) DrawCube(stairsUpPos, 1.2f, 0.1f, 1.2f, SKYBLUE);
    if (isHome && storageBoxPos.x != -999) { DrawCube(storageBoxPos, 1.0f, 1.0f, 1.0f, BROWN); DrawCubeWires(storageBoxPos, 1.0f, 1.0f, 1.0f, BLACK); }
}
bool Dungeon::IsWall(float x, float z) {
    int gx = (int)floorf((x + TILE_SIZE / 2.0f) / TILE_SIZE), gz = (int)floorf((z + TILE_SIZE / 2.0f) / TILE_SIZE);
    if (gx < 0 || gx >= MAP_WIDTH || gz < 0 || gz >= MAP_HEIGHT) return true;
    return map[gx][gz] == 1;
}
bool Dungeon::CheckCollisionRadius(Vector3 p, float r) { return IsWall(p.x - r, p.z) || IsWall(p.x + r, p.z) || IsWall(p.x, p.z - r) || IsWall(p.x, p.z + r); }
bool Dungeon::HasLineOfSight(Vector3 s, Vector3 e) {
    float dist = Vector3Distance(s, e); Vector3 dir = Vector3Normalize(Vector3Subtract(e, s));
    for (float d = 0.5f; d < dist; d += 0.5f) { Vector3 p = Vector3Add(s, Vector3Scale(dir, d)); if (IsWall(p.x, p.z)) return false; }
    return true;
}
bool Dungeon::IsDiscovered(float x, float z) {
    int gx = (int)floorf((x + TILE_SIZE / 2.0f) / TILE_SIZE), gz = (int)floorf((z + TILE_SIZE / 2.0f) / TILE_SIZE);
    return (gx >= 0 && gx < MAP_WIDTH && gz >= 0 && gz < MAP_HEIGHT) ? discovered[gx][gz] : false;
}
Vector3 Dungeon::GetStartPosition() { return Vector3{ (float)rooms[0].x * TILE_SIZE + 1.0f, 0.5f, (float)rooms[0].y * TILE_SIZE + 1.0f }; }
Vector3 Dungeon::GetRandomFloorPos() {
    int r = GetRandomValue(0, (int)rooms.size() - 1);
    return Vector3{ (float)GetRandomValue(rooms[r].x, rooms[r].x + rooms[r].width - 1) * TILE_SIZE, 0.5f, (float)GetRandomValue(rooms[r].y, rooms[r].y + rooms[r].height - 1) * TILE_SIZE };
}