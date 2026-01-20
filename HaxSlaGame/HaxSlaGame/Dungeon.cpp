#include "Dungeon.h"
#include "raymath.h"
#include <queue>
#include <math.h>

struct Point { int x, y; };

Dungeon::Dungeon() { Generate(); }

void Dungeon::Generate() {
    rooms.clear();
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            map[x][y] = TILE_WALL;
            discovered[x][y] = false;
        }
    }
    int roomCount = 8;
    for (int i = 0; i < roomCount; i++) {
        int w = GetRandomValue(4, 9), h = GetRandomValue(4, 9);
        int x = GetRandomValue(1, MAP_WIDTH - w - 1), y = GetRandomValue(1, MAP_HEIGHT - h - 1);
        Room newRoom = { x, y, w, h };
        for (int ry = y; ry < y + h; ry++) {
            for (int rx = x; rx < x + w; rx++) map[rx][ry] = TILE_FLOOR;
        }
        if (!rooms.empty()) {
            Vector2 prev = rooms.back().GetCenter();
            Vector2 curr = newRoom.GetCenter();
            DigCorridor((int)prev.x, (int)prev.y, (int)curr.x, (int)curr.y);
        }
        rooms.push_back(newRoom);
    }
    bool reachable[MAP_WIDTH][MAP_HEIGHT] = { false };
    std::queue<Point> q;
    Point start = { rooms[0].x + rooms[0].width / 2, rooms[0].y + rooms[0].height / 2 };
    q.push(start); reachable[start.x][start.y] = true;
    while (!q.empty()) {
        Point p = q.front(); q.pop();
        int dx[] = { 1, -1, 0, 0 }, dy[] = { 0, 0, 1, -1 };
        for (int i = 0; i < 4; i++) {
            int nx = p.x + dx[i], ny = p.y + dy[i];
            if (nx >= 0 && nx < MAP_WIDTH && ny >= 0 && ny < MAP_HEIGHT) {
                if (!reachable[nx][ny] && map[nx][ny] == TILE_FLOOR) {
                    reachable[nx][ny] = true; q.push({ nx, ny });
                }
            }
        }
    }
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (map[x][y] == TILE_FLOOR && !reachable[x][y]) map[x][y] = TILE_WALL;
        }
    }
}

void Dungeon::DigCorridor(int x1, int y1, int x2, int y2) {
    for (int x = (int)fmin(x1, x2); x <= (int)fmax(x1, x2); x++) map[x][y1] = TILE_FLOOR;
    for (int y = (int)fmin(y1, y2); y <= (int)fmax(y1, y2); y++) map[x2][y] = TILE_FLOOR;
}

void Dungeon::UpdateVisibility(Vector3 playerPos) {
    int px = (int)floor((playerPos.x + TILE_SIZE / 2.0f) / TILE_SIZE);
    int pz = (int)floor((playerPos.z + TILE_SIZE / 2.0f) / TILE_SIZE);
    int radius = 3;
    for (int y = pz - radius; y <= pz + radius; y++) {
        for (int x = px - radius; x <= px + radius; x++) {
            if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT) discovered[x][y] = true;
        }
    }
    for (auto& room : rooms) {
        if (room.Contains(px, pz)) {
            for (int ry = room.y; ry < room.y + room.height; ry++) {
                for (int rx = room.x; rx < room.x + room.width; rx++) discovered[rx][ry] = true;
            }
        }
    }
}

void Dungeon::Draw() {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (!discovered[x][y]) continue;
            Vector3 pos = { x * TILE_SIZE, 0.0f, y * TILE_SIZE };
            if (map[x][y] == TILE_WALL) {
                bool nearFloor = false;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int nx = x + dx, ny = y + dy;
                        if (nx >= 0 && nx < MAP_WIDTH && ny >= 0 && ny < MAP_HEIGHT && map[nx][ny] == TILE_FLOOR) nearFloor = true;
                    }
                }
                if (nearFloor) {
                    DrawCube(pos, TILE_SIZE, 2.0f, TILE_SIZE, GRAY);
                    DrawCubeWires(pos, TILE_SIZE, 2.0f, TILE_SIZE, DARKGRAY);
                }
            }
            else {
                DrawPlane(pos, { TILE_SIZE, TILE_SIZE }, DARKGREEN);
            }
        }
    }
}

bool Dungeon::IsWall(float x, float z) {
    int gx = (int)floor((x + TILE_SIZE / 2.0f) / TILE_SIZE);
    int gz = (int)floor((z + TILE_SIZE / 2.0f) / TILE_SIZE);
    if (gx < 0 || gx >= MAP_WIDTH || gz < 0 || gz >= MAP_HEIGHT) return true;
    return (map[gx][gz] == TILE_WALL);
}

bool Dungeon::CheckCollisionRadius(Vector3 pos, float radius) {
    float checkX[] = { pos.x - radius, pos.x + radius };
    float checkZ[] = { pos.z - radius, pos.z + radius };
    for (float cx : checkX) {
        for (float cz : checkZ) { if (IsWall(cx, cz)) return true; }
    }
    return false;
}

bool Dungeon::HasLineOfSight(Vector3 start, Vector3 end) {
    float dist = Vector3Distance(start, end);
    Vector3 dir = Vector3Normalize(Vector3Subtract(end, start));
    float step = 1.0f; float currentDist = step;
    while (currentDist < dist) {
        Vector3 checkPos = Vector3Add(start, Vector3Scale(dir, currentDist));
        if (IsWall(checkPos.x, checkPos.z)) return false;
        currentDist += step;
    }
    return true;
}

bool Dungeon::IsDiscovered(float x, float z) {
    int gx = (int)floor((x + TILE_SIZE / 2.0f) / TILE_SIZE);
    int gz = (int)floor((z + TILE_SIZE / 2.0f) / TILE_SIZE);
    if (gx < 0 || gx >= MAP_WIDTH || gz < 0 || gz >= MAP_HEIGHT) return false;
    return discovered[gx][gz];
}

Vector3 Dungeon::GetStartPosition() { return { rooms[0].x * TILE_SIZE, 0.5f, rooms[0].y * TILE_SIZE }; }

Vector3 Dungeon::GetRandomFloorPos() {
    if (rooms.empty()) return { 0, 0.5f, 0 };
    int r = GetRandomValue(0, (int)rooms.size() - 1);
    int rx = GetRandomValue(rooms[r].x, rooms[r].x + rooms[r].width - 1);
    int ry = GetRandomValue(rooms[r].y, rooms[r].y + rooms[r].height - 1);
    return { rx * TILE_SIZE, 0.5f, ry * TILE_SIZE };
}